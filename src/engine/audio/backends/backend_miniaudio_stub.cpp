#include "audio/backends/backend_factory_internal.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(KARMA_HAS_AUDIO_MINIAUDIO)
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <mutex>
#endif

namespace karma::audio_backend {
namespace {

#if defined(KARMA_HAS_AUDIO_MINIAUDIO)

constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannels = 2;
constexpr float kOneShotDurationSeconds = 0.25f;
constexpr float kTwoPi = 6.28318530717958647692f;

struct DecodedClip {
    std::vector<float> samples{};
    uint64_t frame_count = 0;
    int sample_rate = kDefaultSampleRate;
    int channels = kDefaultChannels;
};

float ClampGain(float gain) {
    return std::clamp(gain, 0.0f, 2.0f);
}

float ClampPitch(float pitch) {
    return std::clamp(pitch, 0.125f, 8.0f);
}

float HashToFrequency(std::string_view asset_path) {
    if (asset_path.empty()) {
        return 440.0f;
    }

    uint32_t hash = 2166136261u;
    for (const unsigned char ch : asset_path) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return 220.0f + static_cast<float>(hash % 660u);
}

bool EnvFlagEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value) {
        return false;
    }

    const std::string_view text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON";
}

bool ShouldForceNullBackend() {
    return EnvFlagEnabled("KARMA_MINIAUDIO_FORCE_NULL");
}

bool ShouldForceInitFail() {
    return EnvFlagEnabled("KARMA_MINIAUDIO_FORCE_INIT_FAIL");
}

std::filesystem::path TryCanonical(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }

    canonical = std::filesystem::absolute(path, ec);
    if (!ec) {
        return canonical;
    }

    return path;
}

bool IsRegularFile(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec) && !ec;
}

std::optional<std::filesystem::path> ResolveAudioAssetPath(std::string_view asset_path) {
    if (asset_path.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path requested(asset_path);

    if (requested.is_absolute() && IsRegularFile(requested)) {
        return TryCanonical(requested);
    }

    if (IsRegularFile(requested)) {
        return TryCanonical(requested);
    }

    if (karma::config::ConfigStore::Initialized()) {
        const auto resolved = karma::config::ConfigStore::ResolveAssetPath(std::string(asset_path), {});
        if (!resolved.empty() && IsRegularFile(resolved)) {
            return TryCanonical(resolved);
        }

        if (!asset_path.starts_with("assets.")) {
            std::string prefixed_key = "assets.";
            prefixed_key += asset_path;
            const auto prefixed = karma::config::ConfigStore::ResolveAssetPath(prefixed_key, {});
            if (!prefixed.empty() && IsRegularFile(prefixed)) {
                return TryCanonical(prefixed);
            }
        }
    }

    try {
        const auto resolved = karma::data::Resolve(requested);
        if (IsRegularFile(resolved)) {
            return TryCanonical(resolved);
        }
    } catch (...) {
        // Some tests run without a configured data root. Leave unresolved and
        // let the backend synthesize fallback audio.
    }

    return std::nullopt;
}

std::shared_ptr<const DecodedClip> DecodeClip(const std::filesystem::path& path,
                                              int sample_rate,
                                              int channels) {
    if (sample_rate <= 0 || channels <= 0) {
        return nullptr;
    }

    const ma_decoder_config decoder_config = ma_decoder_config_init(
        ma_format_f32,
        static_cast<ma_uint32>(channels),
        static_cast<ma_uint32>(sample_rate));

    ma_decoder decoder{};
    if (ma_decoder_init_file(path.string().c_str(), &decoder_config, &decoder) != MA_SUCCESS) {
        return nullptr;
    }

    struct DecoderScope {
        ma_decoder* decoder = nullptr;
        ~DecoderScope() {
            if (decoder) {
                ma_decoder_uninit(decoder);
            }
        }
    } decoder_scope{&decoder};

    constexpr ma_uint64 kChunkFrames = 2048;
    std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * static_cast<size_t>(channels));
    std::vector<float> samples;

    while (true) {
        ma_uint64 frames_read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder,
                                                            chunk.data(),
                                                            kChunkFrames,
                                                            &frames_read);
        if (result != MA_SUCCESS && result != MA_AT_END) {
            return nullptr;
        }

        if (frames_read == 0) {
            break;
        }

        const size_t sample_count = static_cast<size_t>(frames_read) * static_cast<size_t>(channels);
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + sample_count);

        if (result == MA_AT_END) {
            break;
        }
    }

    if (samples.empty()) {
        return nullptr;
    }

    auto clip = std::make_shared<DecodedClip>();
    clip->samples = std::move(samples);
    clip->sample_rate = sample_rate;
    clip->channels = channels;
    clip->frame_count = clip->samples.size() / static_cast<size_t>(channels);
    if (clip->frame_count == 0) {
        return nullptr;
    }

    return clip;
}

class MiniaudioBackend final : public Backend {
 public:
    const char* name() const override {
        return "miniaudio";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        if (ShouldForceInitFail()) {
            KARMA_TRACE("audio.miniaudio",
                        "AudioBackend[miniaudio]: forced init failure via KARMA_MINIAUDIO_FORCE_INIT_FAIL");
            return false;
        }

        if (!InitContextAndDevice()) {
            shutdown();
            return false;
        }

        initialized_ = true;
        KARMA_TRACE("audio.miniaudio",
                    "AudioBackend[miniaudio]: initialized sample_rate={} channels={} backend={}",
                    sample_rate_,
                    channel_count_,
                    using_null_backend_ ? "null" : "default");
        return true;
    }

    void shutdown() override {
        {
            std::lock_guard<std::mutex> lock(voices_mutex_);
            voices_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            clip_cache_.clear();
        }

        if (device_initialized_) {
            ma_device_uninit(&device_);
            device_initialized_ = false;
        }
        if (context_initialized_) {
            ma_context_uninit(&context_);
            context_initialized_ = false;
        }

        initialized_ = false;
        using_null_backend_ = false;
        KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: shutdown");
    }

    void beginFrame(float dt) override {
        (void)dt;
        frame_play_requests_ = 0;
    }

    void update(float dt) override {
        (void)dt;
    }

    void endFrame() override {
        size_t voice_count = 0;
        {
            std::lock_guard<std::mutex> lock(voices_mutex_);
            voice_count = voices_.size();
        }
        KARMA_TRACE_CHANGED("audio.miniaudio",
                            std::to_string(voice_count) + ":" + std::to_string(frame_play_requests_),
                            "AudioBackend[miniaudio]: active voices={} frame_requests={}",
                            voice_count,
                            frame_play_requests_);
    }

    void setListener(const ListenerState& state) override {
        listener_ = state;
    }

    void playOneShot(const PlayRequest& request) override {
        AddVoice(request, false);
        ++frame_play_requests_;
    }

    VoiceId startVoice(const PlayRequest& request) override {
        const VoiceId id = AddVoice(request, true);
        if (id == kInvalidVoiceId) {
            return kInvalidVoiceId;
        }
        ++frame_play_requests_;
        return id;
    }

    void stopVoice(VoiceId voice) override {
        std::lock_guard<std::mutex> lock(voices_mutex_);
        voices_.erase(voice);
    }

  private:
    struct VoiceState {
        float gain = 1.0f;
        float pitch = 1.0f;
        std::shared_ptr<const DecodedClip> clip{};
        double frame_cursor = 0.0;
        float tone_frequency_hz = 440.0f;
        float tone_phase = 0.0f;
        bool use_tone_fallback = false;
        bool loop = false;
        uint64_t tone_samples_remaining = 0;
    };

    bool InitContextAndDevice() {
        if (ShouldForceNullBackend()) {
            const ma_backend null_backend_list[] = {ma_backend_null};
            if (ma_context_init(null_backend_list, 1, nullptr, &context_) != MA_SUCCESS) {
                KARMA_TRACE("audio.miniaudio",
                            "AudioBackend[miniaudio]: failed to init forced-null context");
                return false;
            }
            context_initialized_ = true;
            if (!InitDeviceForContext(&context_)) {
                return false;
            }
            using_null_backend_ = true;
            return true;
        }

        if (ma_context_init(nullptr, 0, nullptr, &context_) != MA_SUCCESS) {
            KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: failed to init default context");
            return false;
        }
        context_initialized_ = true;

        if (InitDeviceForContext(&context_)) {
            using_null_backend_ = false;
            return true;
        }

        ma_context_uninit(&context_);
        context_initialized_ = false;

        const ma_backend null_backend_list[] = {ma_backend_null};
        if (ma_context_init(null_backend_list, 1, nullptr, &context_) != MA_SUCCESS) {
            KARMA_TRACE("audio.miniaudio",
                        "AudioBackend[miniaudio]: failed to init null-backend context");
            return false;
        }
        context_initialized_ = true;

        if (!InitDeviceForContext(&context_)) {
            return false;
        }
        using_null_backend_ = true;
        return true;
    }

    bool InitDeviceForContext(ma_context* context) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = kDefaultChannels;
        config.sampleRate = kDefaultSampleRate;
        config.dataCallback = &MiniaudioBackend::DataCallback;
        config.pUserData = this;

        if (ma_device_init(context, &config, &device_) != MA_SUCCESS) {
            KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: failed to init playback device");
            return false;
        }
        device_initialized_ = true;

        if (ma_device_start(&device_) != MA_SUCCESS) {
            KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: failed to start playback device");
            ma_device_uninit(&device_);
            device_initialized_ = false;
            return false;
        }

        sample_rate_ = static_cast<float>(device_.sampleRate);
        channel_count_ = static_cast<int>(device_.playback.channels);
        return true;
    }

    static void DataCallback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
        (void)input;
        if (!device || !output || frame_count == 0) {
            return;
        }

        auto* backend = static_cast<MiniaudioBackend*>(device->pUserData);
        if (!backend) {
            return;
        }

        backend->MixToOutput(static_cast<float*>(output), static_cast<int>(frame_count));
    }

    VoiceId AddVoice(const PlayRequest& request, bool /*controllable*/) {
        if (!initialized_) {
            return kInvalidVoiceId;
        }

        VoiceState voice{};
        voice.gain = ClampGain(request.gain);
        voice.pitch = ClampPitch(request.pitch);
        voice.loop = request.loop;
        voice.clip = ResolveClip(request.asset_path);

        if (!voice.clip) {
            voice.use_tone_fallback = true;
            voice.tone_frequency_hz = HashToFrequency(request.asset_path);
            if (!voice.loop) {
                voice.tone_samples_remaining = static_cast<uint64_t>(kOneShotDurationSeconds * sample_rate_);
            }
            if (!request.asset_path.empty()) {
                KARMA_TRACE("audio.miniaudio",
                            "AudioBackend[miniaudio]: unresolved/unsupported clip '{}' (decoder failed), using synthesized fallback",
                            request.asset_path);
            }
        }

        const VoiceId id = next_voice_id_++;
        std::lock_guard<std::mutex> lock(voices_mutex_);
        voices_[id] = std::move(voice);
        return id;
    }

    void MixToOutput(float* output, int frame_count) {
        if (!output || frame_count <= 0 || channel_count_ <= 0 || sample_rate_ <= 0.0f) {
            return;
        }

        const size_t sample_count = static_cast<size_t>(frame_count * channel_count_);
        std::fill(output, output + sample_count, 0.0f);

        std::lock_guard<std::mutex> lock(voices_mutex_);
        for (auto it = voices_.begin(); it != voices_.end();) {
            VoiceState& voice = it->second;
            const bool finished = voice.clip ? MixDecodedVoice(voice, output, frame_count)
                                             : MixToneVoice(voice, output, frame_count);

            if (finished) {
                it = voices_.erase(it);
            } else {
                ++it;
            }
        }

        for (size_t i = 0; i < sample_count; ++i) {
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
    }

    bool MixDecodedVoice(VoiceState& voice, float* output, int frame_count) const {
        if (!voice.clip || voice.clip->frame_count == 0) {
            return true;
        }

        const auto& clip = *voice.clip;
        const uint64_t clip_frames = clip.frame_count;

        for (int frame = 0; frame < frame_count; ++frame) {
            if (!voice.loop && voice.frame_cursor >= static_cast<double>(clip_frames)) {
                return true;
            }

            uint64_t frame0 = static_cast<uint64_t>(voice.frame_cursor);
            const double frac = voice.frame_cursor - static_cast<double>(frame0);
            uint64_t frame1 = frame0 + 1;

            if (voice.loop) {
                frame0 %= clip_frames;
                frame1 %= clip_frames;
            } else {
                if (frame0 >= clip_frames) {
                    return true;
                }
                if (frame1 >= clip_frames) {
                    frame1 = clip_frames - 1;
                }
            }

            const size_t dst_base = static_cast<size_t>(frame * channel_count_);
            const int source_channels = std::max(1, clip.channels);

            for (int channel = 0; channel < channel_count_; ++channel) {
                const int source_channel = std::min(channel, source_channels - 1);
                const size_t sample0_index =
                    static_cast<size_t>(frame0 * static_cast<uint64_t>(source_channels) + source_channel);
                const size_t sample1_index =
                    static_cast<size_t>(frame1 * static_cast<uint64_t>(source_channels) + source_channel);
                if (sample0_index >= clip.samples.size() || sample1_index >= clip.samples.size()) {
                    return true;
                }

                const float sample0 = clip.samples[sample0_index];
                const float sample1 = clip.samples[sample1_index];
                const float sample =
                    static_cast<float>((1.0 - frac) * static_cast<double>(sample0) +
                                       frac * static_cast<double>(sample1));
                output[dst_base + static_cast<size_t>(channel)] += sample * voice.gain;
            }

            voice.frame_cursor += static_cast<double>(voice.pitch);
            if (voice.loop && voice.frame_cursor >= static_cast<double>(clip_frames)) {
                voice.frame_cursor = std::fmod(voice.frame_cursor, static_cast<double>(clip_frames));
            }
        }

        return !voice.loop && voice.frame_cursor >= static_cast<double>(clip_frames);
    }

    bool MixToneVoice(VoiceState& voice, float* output, int frame_count) const {
        const float phase_step = (kTwoPi * voice.tone_frequency_hz * voice.pitch) / sample_rate_;

        for (int frame = 0; frame < frame_count; ++frame) {
            if (!voice.loop && voice.tone_samples_remaining == 0) {
                return true;
            }

            const float sample = std::sin(voice.tone_phase) * voice.gain;
            const size_t base = static_cast<size_t>(frame * channel_count_);
            for (int channel = 0; channel < channel_count_; ++channel) {
                output[base + static_cast<size_t>(channel)] += sample;
            }

            voice.tone_phase += phase_step;
            if (voice.tone_phase >= kTwoPi) {
                voice.tone_phase = std::fmod(voice.tone_phase, kTwoPi);
            }

            if (!voice.loop && voice.tone_samples_remaining > 0) {
                --voice.tone_samples_remaining;
            }
        }

        return !voice.loop && voice.tone_samples_remaining == 0;
    }

    std::shared_ptr<const DecodedClip> ResolveClip(std::string_view asset_path) {
        const auto resolved_path = ResolveAudioAssetPath(asset_path);
        if (!resolved_path.has_value()) {
            return nullptr;
        }
        const std::string cache_key = resolved_path->string();

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            if (const auto it = clip_cache_.find(cache_key); it != clip_cache_.end()) {
                if (const auto existing = it->second.lock()) {
                    return existing;
                }
            }
        }

        const auto decoded = DecodeClip(*resolved_path,
                                        static_cast<int>(sample_rate_),
                                        channel_count_);
        if (!decoded) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);
        clip_cache_[cache_key] = decoded;
        KARMA_TRACE("audio.miniaudio",
                    "AudioBackend[miniaudio]: loaded clip '{}' frames={} rate={} channels={}",
                    cache_key,
                    decoded->frame_count,
                    decoded->sample_rate,
                    decoded->channels);
        return decoded;
    }

    VoiceId next_voice_id_ = 1;
    uint32_t frame_play_requests_ = 0;
    ListenerState listener_{};
    std::unordered_map<VoiceId, VoiceState> voices_{};
    std::unordered_map<std::string, std::weak_ptr<const DecodedClip>> clip_cache_{};
    std::mutex voices_mutex_{};
    std::mutex cache_mutex_{};
    ma_context context_{};
    ma_device device_{};
    float sample_rate_ = static_cast<float>(kDefaultSampleRate);
    int channel_count_ = kDefaultChannels;
    bool initialized_ = false;
    bool context_initialized_ = false;
    bool device_initialized_ = false;
    bool using_null_backend_ = false;
};

#else

class MiniaudioBackendStub final : public Backend {
 public:
    const char* name() const override { return "miniaudio"; }

    bool init() override {
        KARMA_TRACE("audio.miniaudio", "AudioBackend[miniaudio]: unavailable (not compiled)");
        return false;
    }

    void shutdown() override {}
    void beginFrame(float) override {}
    void update(float) override {}
    void endFrame() override {}
    void setListener(const ListenerState&) override {}
    void playOneShot(const PlayRequest&) override {}
    VoiceId startVoice(const PlayRequest&) override { return kInvalidVoiceId; }
    void stopVoice(VoiceId) override {}
};

#endif

} // namespace

std::unique_ptr<Backend> CreateMiniaudioBackend() {
#if defined(KARMA_HAS_AUDIO_MINIAUDIO)
    return std::make_unique<MiniaudioBackend>();
#else
    return std::make_unique<MiniaudioBackendStub>();
#endif
}

} // namespace karma::audio_backend
