#pragma once

#include "karma/audio/audio.hpp"
#include "karma/components/audio_listener.h"
#include "karma/components/audio_source.h"
#include "karma/components/transform.h"
#include "karma/ecs/world.h"
#include <optional>
#include <unordered_map>

namespace karma::ecs {
namespace components = karma::components;

class AudioSyncSystem {
public:
    void update(karma::ecs::World &world, Audio *audio) {
#ifdef KARMA_SERVER
        (void)world;
        (void)audio;
        return;
#else
        if (!audio) {
            return;
        }

        auto &transforms = world.storage<components::TransformComponent>();
        auto &sources = world.storage<components::AudioSourceComponent>();

        for (auto it = sources_.begin(); it != sources_.end(); ) {
            if (!sources.has(it->first)) {
                it = sources_.erase(it);
            } else {
                ++it;
            }
        }

        auto &listeners = world.storage<components::AudioListenerComponent>();
        for (const auto entity : listeners.denseEntities()) {
            const components::AudioListenerComponent &listener = listeners.get(entity);
            if (!listener.active) {
                continue;
            }
            if (!transforms.has(entity)) {
                continue;
            }
            const auto &transform = transforms.get(entity);
            audio->setListenerPosition(transform.position);
            audio->setListenerRotation(transform.rotation);
            break;
        }

        for (const auto entity : sources.denseEntities()) {
            const components::AudioSourceComponent &source = sources.get(entity);
            if (source.clip_key.empty()) {
                continue;
            }
            auto &state = sources_[entity];
            if (!state.clip.has_value()) {
                state.clip = audio->loadClip(source.clip_key);
            }
            if (source.play_on_start && !state.started && state.clip.has_value()) {
                glm::vec3 position{0.0f};
                if (transforms.has(entity)) {
                    position = transforms.get(entity).position;
                }
                state.clip->play(position, source.gain);
                state.started = true;
            }
        }
#endif
    }

private:
    struct SourceState {
        std::optional<AudioClip> clip{};
        bool started = false;
    };
    std::unordered_map<karma::ecs::Entity, SourceState> sources_;
};

} // namespace karma::ecs
