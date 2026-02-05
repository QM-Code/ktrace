#include "karma/renderer/backend.hpp"

#include "karma/common/logging.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/platform/window.hpp"

#include <spdlog/spdlog.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

#include <glm/glm.hpp>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <fstream>
#include <unordered_map>
#include <array>
#include <vector>

namespace karma::renderer_backend {
namespace {

struct PosNormalVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;

    static bgfx::VertexLayout layout() {
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        return layout;
    }
};

bgfx::ShaderHandle loadShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return BGFX_INVALID_HANDLE;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size + 1));
    if (!file.read(reinterpret_cast<char*>(mem->data), size)) {
        return BGFX_INVALID_HANDLE;
    }
    mem->data[mem->size - 1] = '\0';
    return bgfx::createShader(mem);
}

struct Mesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t num_indices = 0;
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
};

struct Material {
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

class BgfxCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override {
        spdlog::error("BGFX fatal: {}:{} code={} {}", filePath ? filePath : "(null)", line, static_cast<int>(code),
                      str ? str : "");
        std::abort();
    }

    void traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList) override {
        if (!karma::logging::ShouldTraceChannel("render.bgfx.internal")) {
            return;
        }
        std::array<char, 2048> stack_buf{};
        va_list args_copy;
        va_copy(args_copy, argList);
        int needed = std::vsnprintf(stack_buf.data(), stack_buf.size(), format, args_copy);
        va_end(args_copy);
        if (needed < 0) {
            return;
        }
        std::string message;
        if (static_cast<size_t>(needed) < stack_buf.size()) {
            message.assign(stack_buf.data(), static_cast<size_t>(needed));
        } else {
            std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
            va_list args_copy2;
            va_copy(args_copy2, argList);
            std::vsnprintf(heap_buf.data(), heap_buf.size(), format, args_copy2);
            va_end(args_copy2);
            message.assign(heap_buf.data(), static_cast<size_t>(needed));
        }
        KARMA_TRACE("render.bgfx.internal", "{}:{}: {}", filePath ? filePath : "(null)", line, message.c_str());
    }

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

} // namespace

bgfx::TextureHandle createWhiteTexture() {
    const uint32_t white = 0xffffffff;
    const bgfx::Memory* mem = bgfx::copy(&white, sizeof(white));
    return bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}

class BgfxBackend final : public Backend {
 public:
    explicit BgfxBackend(karma::platform::Window& window) {
        karma::platform::NativeWindowHandle native = window.nativeHandle();
        bgfx::PlatformData pd{};
        if (native.is_wayland && native.wayland_surface && native.display) {
            pd.nwh = native.wayland_surface;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Wayland;
            KARMA_TRACE("render.bgfx", "using Wayland surface handle");
        } else if (native.window) {
            pd.nwh = native.window;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Default;
            KARMA_TRACE("render.bgfx", "using default native window handle");
        } else {
            spdlog::error("Graphics(Bgfx): failed to resolve native window handle");
        }
        KARMA_TRACE("render.bgfx",
                    "platform data type={} nwh={} ndt={}",
                    static_cast<int>(pd.type), pd.nwh, pd.ndt);
        if (!pd.ndt || !pd.nwh) {
            spdlog::error("Graphics(Bgfx): missing native display/window handle (ndt={}, nwh={})", pd.ndt, pd.nwh);
            return;
        }

        bgfx::Init init{};
        init.type = bgfx::RendererType::Vulkan;
        init.vendorId = BGFX_PCI_ID_NONE;
        init.platformData = pd;
        init.callback = &callback_;
        window.getFramebufferSize(width_, height_);
        init.resolution.width = static_cast<uint32_t>(width_);
        init.resolution.height = static_cast<uint32_t>(height_);
        init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER;
        KARMA_TRACE("render.bgfx",
                    "init renderer=Vulkan size={}x{} reset=0x{:08x}",
                    init.resolution.width, init.resolution.height, init.resolution.reset);
        if (!bgfx::init(init)) {
            spdlog::error("BGFX init failed");
            return;
        }
        KARMA_TRACE("render.bgfx", "init success renderer={}",
                    bgfx::getRendererName(bgfx::getRendererType()));
        initialized_ = true;
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        const auto vs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/vs_mesh.bin").string();
        const auto fs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/fs_mesh.bin").string();
        auto vsh = loadShader(vs_path);
        auto fsh = loadShader(fs_path);
        if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
            spdlog::error("Failed to load BGFX shaders: {} {}", vs_path, fs_path);
        } else {
            program_ = bgfx::createProgram(vsh, fsh, true);
        }
        KARMA_TRACE("render.bgfx", "program valid={}", bgfx::isValid(program_) ? 1 : 0);
        layout_ = PosNormalVertex::layout();
        u_color_ = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
        u_light_dir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
        u_light_color_ = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
        u_ambient_color_ = bgfx::createUniform("u_ambientColor", bgfx::UniformType::Vec4);
        u_unlit_ = bgfx::createUniform("u_unlit", bgfx::UniformType::Vec4);
        s_tex_ = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
        white_tex_ = createWhiteTexture();
    }

    ~BgfxBackend() override {
        if (!initialized_) {
            return;
        }
        for (auto& [id, mesh] : meshes_) {
            if (bgfx::isValid(mesh.vbh)) bgfx::destroy(mesh.vbh);
            if (bgfx::isValid(mesh.ibh)) bgfx::destroy(mesh.ibh);
        }
        for (auto& [id, material] : materials_) {
            (void)material;
        }
        if (bgfx::isValid(program_)) bgfx::destroy(program_);
        if (bgfx::isValid(u_color_)) bgfx::destroy(u_color_);
        if (bgfx::isValid(u_light_dir_)) bgfx::destroy(u_light_dir_);
        if (bgfx::isValid(u_light_color_)) bgfx::destroy(u_light_color_);
        if (bgfx::isValid(u_ambient_color_)) bgfx::destroy(u_ambient_color_);
        if (bgfx::isValid(u_unlit_)) bgfx::destroy(u_unlit_);
        if (bgfx::isValid(s_tex_)) bgfx::destroy(s_tex_);
        if (bgfx::isValid(white_tex_)) bgfx::destroy(white_tex_);
        bgfx::shutdown();
    }

    void beginFrame(int width, int height, float dt) override {
        (void)dt;
        if (!initialized_) {
            return;
        }
        width_ = width;
        height_ = height;
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        bgfx::touch(0);
    }

    void endFrame() override {
        if (!initialized_) {
            return;
        }
        bgfx::frame();
    }

    renderer::MeshId createMesh(const renderer::MeshData& mesh) override {
        if (!initialized_) {
            return renderer::kInvalidMesh;
        }
        KARMA_TRACE("render.bgfx", "createMesh vertices={} indices={}",
                    mesh.positions.size(), mesh.indices.size());
        std::vector<PosNormalVertex> vertices;
        vertices.reserve(mesh.positions.size());
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            const glm::vec3& p = mesh.positions[i];
            const glm::vec3 n = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec2 uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2(0.0f, 0.0f);
            vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
        }
        const bgfx::Memory* vmem = bgfx::copy(vertices.data(), sizeof(PosNormalVertex) * vertices.size());
        const bgfx::Memory* imem = bgfx::copy(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

        Mesh out;
        out.vbh = bgfx::createVertexBuffer(vmem, layout_);
        out.ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
        out.num_indices = static_cast<uint32_t>(mesh.indices.size());
        if (mesh.albedo && !mesh.albedo->pixels.empty()) {
            out.tex = bgfx::createTexture2D(
                static_cast<uint16_t>(mesh.albedo->width),
                static_cast<uint16_t>(mesh.albedo->height),
                false,
                1,
                bgfx::TextureFormat::RGBA8,
                0,
                bgfx::copy(mesh.albedo->pixels.data(),
                           static_cast<uint32_t>(mesh.albedo->pixels.size())));
            KARMA_TRACE("render.bgfx", "createMesh texture={} {}x{}",
                        bgfx::isValid(out.tex) ? 1 : 0,
                        mesh.albedo->width, mesh.albedo->height);
        }

        renderer::MeshId id = next_mesh_id_++;
        meshes_[id] = out;
        return id;
    }

    void destroyMesh(renderer::MeshId mesh) override {
        auto it = meshes_.find(mesh);
        if (it == meshes_.end()) return;
        if (bgfx::isValid(it->second.vbh)) bgfx::destroy(it->second.vbh);
        if (bgfx::isValid(it->second.ibh)) bgfx::destroy(it->second.ibh);
        if (bgfx::isValid(it->second.tex)) bgfx::destroy(it->second.tex);
        meshes_.erase(it);
    }

    renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) override {
        if (!initialized_) {
            return renderer::kInvalidMaterial;
        }
        Material out;
        out.color[0] = material.base_color.r;
        out.color[1] = material.base_color.g;
        out.color[2] = material.base_color.b;
        out.color[3] = material.base_color.a;
        renderer::MaterialId id = next_material_id_++;
        materials_[id] = out;
        return id;
    }

    void destroyMaterial(renderer::MaterialId material) override {
        materials_.erase(material);
    }

    void submit(const renderer::DrawItem& item) override {
        if (!initialized_) {
            return;
        }
        draw_items_.push_back(item);
    }

    void renderFrame() override {
        renderLayer(0);
    }

    void renderLayer(renderer::LayerId layer) override {
        if (!initialized_ || !bgfx::isValid(program_)) {
            return;
        }

        float view[16];
        float proj[16];
        bx::mtxLookAt(view, bx::Vec3(camera_.position.x, camera_.position.y, camera_.position.z),
                      bx::Vec3(camera_.target.x, camera_.target.y, camera_.target.z));
        // BGFX uses left-handed view/projection by default; mirror X to match the
        // engine's right-handed camera conventions, then flip culling below.
        float mirror[16];
        float view_mirror[16];
        bx::mtxScale(mirror, -1.0f, 1.0f, 1.0f);
        bx::mtxMul(view_mirror, view, mirror);
        bx::mtxProj(proj, camera_.fov_y_degrees, float(width_) / float(height_),
                    camera_.near_clip, camera_.far_clip, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view_mirror, proj);

        for (const auto& item : draw_items_) {
            if (item.layer != layer) {
                continue;
            }
            auto mesh_it = meshes_.find(item.mesh);
            if (mesh_it == meshes_.end()) continue;
            auto mat_it = materials_.find(item.material);
            if (mat_it == materials_.end()) continue;

            bgfx::setVertexBuffer(0, mesh_it->second.vbh);
            bgfx::setIndexBuffer(mesh_it->second.ibh);
            bgfx::setTransform(&item.transform[0][0]);
            bgfx::setUniform(u_color_, mat_it->second.color);
            const float light_dir[4] = {light_.direction.x, light_.direction.y, light_.direction.z, 0.0f};
            const float light_color[4] = {light_.color.r, light_.color.g, light_.color.b, light_.color.a};
            const float ambient_color[4] = {light_.ambient.r, light_.ambient.g, light_.ambient.b, light_.ambient.a};
            const float unlit[4] = {light_.unlit, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(u_light_dir_, light_dir);
            bgfx::setUniform(u_light_color_, light_color);
            bgfx::setUniform(u_ambient_color_, ambient_color);
            bgfx::setUniform(u_unlit_, unlit);
            if (bgfx::isValid(mesh_it->second.tex)) {
                bgfx::setTexture(0, s_tex_, mesh_it->second.tex);
            } else if (bgfx::isValid(white_tex_)) {
                bgfx::setTexture(0, s_tex_, white_tex_);
            }
            const uint64_t cull_cw = (BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK) | BGFX_STATE_CULL_CW;
            bgfx::setState(cull_cw | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z);
            bgfx::submit(0, program_);
        }

        // TODO(bz3-rewrite): keep per-layer queues so multiple layers can render in a frame.
        draw_items_.clear();
    }

    void setCamera(const renderer::CameraData& camera) override {
        camera_ = camera;
    }

    void setDirectionalLight(const renderer::DirectionalLightData& light) override {
        light_ = light;
    }

    bool isValid() const override {
        return initialized_;
    }

 private:
    BgfxCallback callback_{};
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_dir_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ambient_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_unlit_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle white_tex_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout_{};

    std::unordered_map<renderer::MeshId, Mesh> meshes_;
    std::unordered_map<renderer::MaterialId, Material> materials_;
    std::vector<renderer::DrawItem> draw_items_;

    renderer::CameraData camera_{};
    renderer::DirectionalLightData light_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateBackend(karma::platform::Window& window) {
#if defined(KARMA_RENDER_BACKEND_BGFX)
    return std::make_unique<BgfxBackend>(window);
#else
    (void)window;
    return nullptr;
#endif
}

} // namespace karma::renderer_backend
