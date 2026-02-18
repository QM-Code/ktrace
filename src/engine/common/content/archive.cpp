#include "karma/common/content/archive.hpp"

#include "karma/common/logging/logging.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <miniz.h>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr int kMaxArchiveEntries = 4096;
constexpr uint64_t kMaxArchiveTotalUncompressedBytes = 1024ULL * 1024ULL * 1024ULL; // 1 GiB
constexpr uint64_t kMaxArchiveFileUncompressedBytes = 256ULL * 1024ULL * 1024ULL; // 256 MiB
constexpr uint16_t kZipHostUnix = 3;
constexpr uint32_t kPosixTypeMask = 0170000u;
constexpr uint32_t kPosixTypeRegular = 0100000u;
constexpr uint32_t kPosixTypeDirectory = 0040000u;

struct PreparedArchiveEntry {
    int index = 0;
    fs::path relative_path{};
    bool is_directory = false;
    uint64_t uncompressed_size = 0;
};

std::optional<fs::path> NormalizeArchiveEntryPath(const char* raw_name) {
    if (!raw_name || *raw_name == '\0') {
        return std::nullopt;
    }

    std::string sanitized(raw_name);
    std::replace(sanitized.begin(), sanitized.end(), '\\', '/');

    fs::path path = fs::path(sanitized).lexically_normal();
    if (path.empty() || path == ".") {
        return std::nullopt;
    }

    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return std::nullopt;
    }

    for (const auto& component : path) {
        if (component == "..") {
            return std::nullopt;
        }
    }

    return path;
}

bool BuildExtractionPlan(mz_zip_archive& zip,
                         std::vector<PreparedArchiveEntry>& entries,
                         uint64_t& total_uncompressed_bytes) {
    const int num_entries = mz_zip_reader_get_num_files(&zip);
    if (num_entries < 0) {
        spdlog::error("WorldArchive: Invalid zip entry count");
        return false;
    }
    if (num_entries > kMaxArchiveEntries) {
        spdlog::error("WorldArchive: Rejecting archive with {} entries (max {})",
                      num_entries,
                      kMaxArchiveEntries);
        return false;
    }

    entries.clear();
    entries.reserve(static_cast<size_t>(num_entries));
    total_uncompressed_bytes = 0;

    for (int i = 0; i < num_entries; ++i) {
        mz_zip_archive_file_stat file_stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) {
            spdlog::error("WorldArchive: Failed to get file stat for index {}", i);
            return false;
        }

        const auto normalized_path = NormalizeArchiveEntryPath(file_stat.m_filename);
        if (!normalized_path.has_value()) {
            spdlog::error("WorldArchive: Rejecting unsafe archive path '{}'",
                          file_stat.m_filename ? file_stat.m_filename : "(null)");
            return false;
        }

        const bool is_directory = mz_zip_reader_is_file_a_directory(&zip, i);
        const uint16_t host_system = static_cast<uint16_t>(
            (static_cast<uint32_t>(file_stat.m_version_made_by) >> 8) & 0xFFu);
        const uint32_t unix_mode = static_cast<uint32_t>((file_stat.m_external_attr >> 16) & 0xFFFFu);
        if (host_system == kZipHostUnix && (unix_mode & kPosixTypeMask) != 0) {
            const uint32_t posix_type = unix_mode & kPosixTypeMask;
            const bool mode_is_directory = posix_type == kPosixTypeDirectory;
            const bool mode_is_regular = posix_type == kPosixTypeRegular;
            const bool supported_type = is_directory ? mode_is_directory : mode_is_regular;
            if (!supported_type) {
                spdlog::error(
                    "WorldArchive: Rejecting unsupported archive entry type '{}' (mode={:#o}, directory={})",
                    normalized_path->string(),
                    unix_mode,
                    is_directory ? 1 : 0);
                return false;
            }
        }

        const uint64_t uncompressed_size = static_cast<uint64_t>(file_stat.m_uncomp_size);
        if (!is_directory && uncompressed_size > kMaxArchiveFileUncompressedBytes) {
            spdlog::error("WorldArchive: Rejecting oversized archive entry '{}' ({} bytes, max {})",
                          normalized_path->string(),
                          uncompressed_size,
                          kMaxArchiveFileUncompressedBytes);
            return false;
        }

        if (!is_directory) {
            if (uncompressed_size > (kMaxArchiveTotalUncompressedBytes - total_uncompressed_bytes)) {
                spdlog::error("WorldArchive: Rejecting archive total size overflow at '{}' (max {} bytes)",
                              normalized_path->string(),
                              kMaxArchiveTotalUncompressedBytes);
                return false;
            }
            total_uncompressed_bytes += uncompressed_size;
        }

        entries.push_back(PreparedArchiveEntry{
            .index = i,
            .relative_path = *normalized_path,
            .is_directory = is_directory,
            .uncompressed_size = uncompressed_size
        });
    }

    return true;
}

void ZipDirectory(const fs::path& input_dir, const fs::path& output_zip) {
    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
        throw std::runtime_error("Input is not a directory");
    }

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, output_zip.string().c_str(), 0)) {
        throw std::runtime_error("Failed to create zip file");
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            fs::path abs_path = entry.path();
            fs::path rel_path = fs::relative(abs_path, input_dir);
            std::string zip_path = rel_path.generic_string();

            if (!mz_zip_writer_add_file(
                    &zip,
                    zip_path.c_str(),
                    abs_path.string().c_str(),
                    nullptr,
                    0,
                    MZ_DEFAULT_LEVEL)) {
                throw std::runtime_error("Failed to add file: " + zip_path);
            }
        }

        if (!mz_zip_writer_finalize_archive(&zip)) {
            throw std::runtime_error("Failed to finalize zip");
        }

        mz_zip_writer_end(&zip);
    } catch (...) {
        mz_zip_writer_end(&zip);
        throw;
    }
}

karma::common::content::ArchiveBytes ReadArchiveFile(const fs::path& zip_path) {
    if (!fs::exists(zip_path)) {
        throw std::runtime_error("World zip file not found: " + zip_path.string());
    }

    auto file_size = fs::file_size(zip_path);
    karma::common::content::ArchiveBytes data(file_size);
    std::ifstream file(zip_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open zip file: " + zip_path.string());
    }

    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size));
    if (!file) {
        throw std::runtime_error("Failed to read zip file: " + zip_path.string());
    }

    return data;
}
} // namespace

namespace karma::common::content {

ArchiveBytes BuildWorldArchive(const fs::path& world_dir) {
    const fs::path input_dir(world_dir);
    fs::path output_zip = input_dir;
    output_zip += ".zip";
    ZipDirectory(input_dir, output_zip);
    return ReadArchiveFile(output_zip);
}

bool ExtractWorldArchive(const ArchiveBytes& data, const fs::path& dest_dir) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        spdlog::error("WorldArchive: Failed to open zip from memory");
        return false;
    }

    std::vector<PreparedArchiveEntry> entries;
    uint64_t total_uncompressed_bytes = 0;
    if (!BuildExtractionPlan(zip, entries, total_uncompressed_bytes)) {
        mz_zip_reader_end(&zip);
        return false;
    }

    for (const auto& entry : entries) {
        const fs::path out_path = dest_dir / entry.relative_path;
        std::error_code ec;
        if (entry.is_directory) {
            fs::create_directories(out_path, ec);
            if (ec) {
                spdlog::error("WorldArchive: Failed to create directory '{}': {}",
                              out_path.string(),
                              ec.message());
                mz_zip_reader_end(&zip);
                return false;
            }
            continue;
        }

        const auto parent_path = out_path.parent_path();
        if (!parent_path.empty()) {
            fs::create_directories(parent_path, ec);
            if (ec) {
                spdlog::error("WorldArchive: Failed to create directory '{}': {}",
                              parent_path.string(),
                              ec.message());
                mz_zip_reader_end(&zip);
                return false;
            }
        }

        if (!mz_zip_reader_extract_to_file(&zip, entry.index, out_path.string().c_str(), 0)) {
            spdlog::error("WorldArchive: Failed to extract '{}'", entry.relative_path.string());
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    mz_zip_reader_end(&zip);
    KARMA_TRACE("world",
                "WorldArchive: Unzipped {} entries ({} bytes) to {}",
                entries.size(),
                total_uncompressed_bytes,
                dest_dir.string());
    return true;
}

std::optional<karma::common::serialization::Value> ReadWorldJsonFile(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    try {
        karma::common::serialization::Value data;
        file >> data;
        return data;
    } catch (const std::exception& e) {
        spdlog::error("WorldArchive: Failed to parse JSON {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

} // namespace karma::common::content
