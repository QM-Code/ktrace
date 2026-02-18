#include "audio/backends/backend_factory_internal.hpp"
#include "audio/backends/spatialization_internal.hpp"

#include "karma/common/config/store.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/common/logging/logging.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(KARMA_HAS_AUDIO_SDL3AUDIO)
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include <mutex>
#endif

namespace karma::audio_backend {
namespace {

#if defined(KARMA_HAS_AUDIO_SDL3AUDIO)

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

bool ShouldForceInitFail() {
    return EnvFlagEnabled("KARMA_SDL3AUDIO_FORCE_INIT_FAIL");
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

    if (karma::common::config::ConfigStore::Initialized()) {
        const auto resolved = karma::common::config::ConfigStore::ResolveAssetPath(std::string(asset_path), {});
        if (!resolved.empty() && IsRegularFile(resolved)) {
            return TryCanonical(resolved);
        }

        if (!asset_path.starts_with("assets.")) {
            std::string prefixed_key = "assets.";
            prefixed_key += asset_path;
            const auto prefixed = karma::common::config::ConfigStore::ResolveAssetPath(prefixed_key, {});
            if (!prefixed.empty() && IsRegularFile(prefixed)) {
                return TryCanonical(prefixed);
            }
        }
    }

    try {
        const auto resolved = karma::common::data::Resolve(requested);
        if (IsRegularFile(resolved)) {
            return TryCanonical(resolved);
        }
    } catch (...) {
        // Some tests run without a configured data root. Leave unresolved and
        // let the backend synthesize fallback audio.
    }

    return std::nullopt;
}

std::shared_ptr<const DecodedClip> DecodeWavClip(const std::filesystem::path& path,
                                                 int sample_rate,
                                                 int channels) {
    if (sample_rate <= 0 || channels <= 0) {
        return nullptr;
    }

    SDL_AudioSpec src_spec{};
    Uint8* src_data = nullptr;
    Uint32 src_len = 0;
    if (!SDL_LoadWAV(path.string().c_str(), &src_spec, &src_data, &src_len)) {
        return nullptr;
    }
    const auto src_guard = std::unique_ptr<Uint8, decltype(&SDL_free)>(src_data, SDL_free);

    SDL_AudioSpec dst_spec{};
    dst_spec.format = SDL_AUDIO_F32;
    dst_spec.channels = static_cast<Uint8>(channels);
    dst_spec.freq = sample_rate;

    Uint8* converted_data = nullptr;
    int converted_len = 0;
    if (!SDL_ConvertAudioSamples(&src_spec,
                                 src_data,
                                 static_cast<int>(src_len),
                                 &dst_spec,
                                 &converted_data,
                                 &converted_len)) {
        return nullptr;
    }
    const auto converted_guard = std::unique_ptr<Uint8, decltype(&SDL_free)>(converted_data, SDL_free);

    if (converted_len <= 0) {
        return nullptr;
    }

    const size_t sample_count = static_cast<size_t>(converted_len) / sizeof(float);
    if (sample_count < static_cast<size_t>(channels)) {
        return nullptr;
    }

    auto clip = std::make_shared<DecodedClip>();
    clip->samples.resize(sample_count);
    std::memcpy(clip->samples.data(), converted_data, sample_count * sizeof(float));
    clip->sample_rate = sample_rate;
    clip->channels = channels;
    clip->frame_count = sample_count / static_cast<size_t>(channels);
    if (clip->frame_count == 0) {
        return nullptr;
    }

    return clip;
}

class Sdl3AudioBackend final : public Backend {
 public:
    const char* name() const override {
        return "sdl3audio";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        if (ShouldForceInitFail()) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: forced init failure via KARMA_SDL3AUDIO_FORCE_INIT_FAIL");
            return false;
        }

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: failed to init SDL audio subsystem: {}",
                        SDL_GetError());
            return false;
        }
        owns_sdl_audio_subsystem_ = true;

        SDL_AudioSpec desired{};
        desired.format = SDL_AUDIO_F32;
        desired.channels = kDefaultChannels;
        desired.freq = kDefaultSampleRate;
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &desired,
                                            &Sdl3AudioBackend::AudioStreamCallback,
                                            this);
        if (!stream_) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: failed to open playback stream: {}",
                        SDL_GetError());
            shutdown();
            return false;
        }

        if (!SDL_ResumeAudioStreamDevice(stream_)) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: failed to resume stream: {}",
                        SDL_GetError());
            shutdown();
            return false;
        }

        sample_rate_ = static_cast<float>(desired.freq);
        channel_count_ = desired.channels;
        initialized_ = true;
        KARMA_TRACE("audio.sdl3audio",
                    "AudioBackend[sdl3audio]: initialized sample_rate={} channels={}",
                    desired.freq,
                    desired.channels);
        if (channel_count_ > 2) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: output channels={} using spatial routing policy={} "
                        "(channels>=2 use stereo-derived fallback)",
                        channel_count_,
                        detail::SpatialRoutingPolicyName());
        }
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
        if (stream_) {
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
        if (owns_sdl_audio_subsystem_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            owns_sdl_audio_subsystem_ = false;
        }
        initialized_ = false;
        KARMA_TRACE("audio.sdl3audio", "AudioBackend[sdl3audio]: shutdown");
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
        KARMA_TRACE_CHANGED("audio.sdl3audio",
                            std::to_string(voice_count) + ":" + std::to_string(frame_play_requests_),
                            "AudioBackend[sdl3audio]: active voices={} frame_requests={}",
                            voice_count,
                            frame_play_requests_);
    }

    void setListener(const ListenerState& state) override {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        listener_ = detail::SanitizeListenerState(state);
    }

    void playOneShot(const PlayRequest& request) override {
        if (AddVoice(request, false) != kInvalidVoiceId) {
            ++frame_play_requests_;
        }
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
        std::optional<glm::vec3> world_position{};
        std::shared_ptr<const DecodedClip> clip{};
        double frame_cursor = 0.0;
        float tone_frequency_hz = 440.0f;
        float tone_phase = 0.0f;
        bool use_tone_fallback = false;
        bool loop = false;
        uint64_t tone_samples_remaining = 0;
    };

    static void SDLCALL AudioStreamCallback(void* userdata,
                                            SDL_AudioStream* stream,
                                            int additional_amount,
                                            int /*total_amount*/) {
        if (!userdata || additional_amount <= 0) {
            return;
        }

        auto* backend = static_cast<Sdl3AudioBackend*>(userdata);
        backend->MixAndSubmit(stream, additional_amount);
    }

    VoiceId AddVoice(const PlayRequest& request, bool /*controllable*/) {
        if (!initialized_) {
            return kInvalidVoiceId;
        }
        const auto world_position = detail::SanitizeEmitterPosition(request.world_position);
        if (request.world_position.has_value() && !world_position.has_value()) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: rejected voice with non-finite world_position");
            return kInvalidVoiceId;
        }
        if (!std::isfinite(request.gain) || !std::isfinite(request.pitch)) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: rejected voice with non-finite gain/pitch");
            return kInvalidVoiceId;
        }

        VoiceState voice{};
        voice.gain = ClampGain(request.gain);
        voice.pitch = ClampPitch(request.pitch);
        voice.loop = request.loop;
        voice.world_position = world_position;
        voice.clip = ResolveClip(request.asset_path);

        if (!voice.clip) {
            voice.use_tone_fallback = true;
            voice.tone_frequency_hz = HashToFrequency(request.asset_path);
            if (!voice.loop) {
                voice.tone_samples_remaining = static_cast<uint64_t>(kOneShotDurationSeconds * sample_rate_);
            }
            if (!request.asset_path.empty()) {
                KARMA_TRACE("audio.sdl3audio",
                            "AudioBackend[sdl3audio]: unresolved/unsupported clip '{}' (WAV required), using synthesized fallback",
                            request.asset_path);
            }
        }

        const VoiceId id = next_voice_id_++;
        std::lock_guard<std::mutex> lock(voices_mutex_);
        voices_[id] = std::move(voice);
        return id;
    }

    void MixAndSubmit(SDL_AudioStream* stream, int additional_amount) {
        if (channel_count_ <= 0 || sample_rate_ <= 0.0f) {
            return;
        }

        const int bytes_per_frame = static_cast<int>(sizeof(float) * channel_count_);
        const int frames = additional_amount / bytes_per_frame;
        if (frames <= 0) {
            return;
        }

        const ListenerState listener = CurrentListener();
        std::vector<float> mixed(static_cast<size_t>(frames * channel_count_), 0.0f);
        {
            std::lock_guard<std::mutex> lock(voices_mutex_);
            for (auto it = voices_.begin(); it != voices_.end();) {
                VoiceState& voice = it->second;
                const bool finished = voice.clip ? MixDecodedVoice(voice, mixed, frames, listener)
                                                 : MixToneVoice(voice, mixed, frames, listener);

                if (finished) {
                    it = voices_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        for (float& value : mixed) {
            value = std::clamp(value, -1.0f, 1.0f);
        }

        if (!SDL_PutAudioStreamData(stream,
                                    mixed.data(),
                                    static_cast<int>(mixed.size() * sizeof(float)))) {
            KARMA_TRACE("audio.sdl3audio",
                        "AudioBackend[sdl3audio]: failed to submit stream data: {}",
                        SDL_GetError());
        }
    }

    bool MixDecodedVoice(VoiceState& voice,
                         std::vector<float>& mixed,
                         int frame_count,
                         const ListenerState& listener) const {
        if (!voice.clip || voice.clip->frame_count == 0) {
            return true;
        }
        const auto spatial_gains = detail::ComputeSpatialGains(listener, voice.world_position);

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
                const float channel_gain = detail::ChannelSpatialGain(spatial_gains, channel, channel_count_);
                mixed[dst_base + static_cast<size_t>(channel)] += sample * voice.gain * channel_gain;
            }

            voice.frame_cursor += static_cast<double>(voice.pitch);
            if (voice.loop && voice.frame_cursor >= static_cast<double>(clip_frames)) {
                voice.frame_cursor = std::fmod(voice.frame_cursor, static_cast<double>(clip_frames));
            }
        }

        return !voice.loop && voice.frame_cursor >= static_cast<double>(clip_frames);
    }

    bool MixToneVoice(VoiceState& voice,
                      std::vector<float>& mixed,
                      int frame_count,
                      const ListenerState& listener) const {
        const float phase_step = (kTwoPi * voice.tone_frequency_hz * voice.pitch) / sample_rate_;
        const auto spatial_gains = detail::ComputeSpatialGains(listener, voice.world_position);

        for (int frame = 0; frame < frame_count; ++frame) {
            if (!voice.loop && voice.tone_samples_remaining == 0) {
                return true;
            }

            const float sample = std::sin(voice.tone_phase) * voice.gain;
            const size_t base = static_cast<size_t>(frame * channel_count_);
            for (int channel = 0; channel < channel_count_; ++channel) {
                const float channel_gain = detail::ChannelSpatialGain(spatial_gains, channel, channel_count_);
                mixed[base + static_cast<size_t>(channel)] += sample * channel_gain;
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

    ListenerState CurrentListener() const {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        return listener_;
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

        const auto decoded = DecodeWavClip(*resolved_path,
                                           static_cast<int>(sample_rate_),
                                           channel_count_);
        if (!decoded) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);
        clip_cache_[cache_key] = decoded;
        KARMA_TRACE("audio.sdl3audio",
                    "AudioBackend[sdl3audio]: loaded clip '{}' frames={} rate={} channels={}",
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
    mutable std::mutex listener_mutex_{};
    SDL_AudioStream* stream_ = nullptr;
    float sample_rate_ = static_cast<float>(kDefaultSampleRate);
    int channel_count_ = kDefaultChannels;
    bool initialized_ = false;
    bool owns_sdl_audio_subsystem_ = false;
};

#else

class Sdl3AudioBackendStub final : public Backend {
 public:
    const char* name() const override { return "sdl3audio"; }

    bool init() override {
        KARMA_TRACE("audio.sdl3audio", "AudioBackend[sdl3audio]: unavailable (not compiled)");
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

std::unique_ptr<Backend> CreateSdl3AudioBackend() {
#if defined(KARMA_HAS_AUDIO_SDL3AUDIO)
    return std::make_unique<Sdl3AudioBackend>();
#else
    return std::make_unique<Sdl3AudioBackendStub>();
#endif
}

} // namespace karma::audio_backend
