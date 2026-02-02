#pragma once

#include "audio/backend.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class ClientEngine;

class AudioClip {
public:
    AudioClip() = delete;
    AudioClip(const AudioClip&) = default;
    AudioClip(AudioClip&&) noexcept = default;
    AudioClip& operator=(const AudioClip&) = default;
    AudioClip& operator=(AudioClip&&) noexcept = default;
    ~AudioClip() = default;

    void play(const glm::vec3& position, float volume = 1.0f) const;

private:
    friend class Audio;
    explicit AudioClip(std::shared_ptr<audio_backend::Clip> data);

    std::shared_ptr<audio_backend::Clip> data_;
};

class Audio {

public:
    Audio();
    ~Audio();

    AudioClip loadClip(const std::string& filepath, int maxInstances = 5);
    void setListenerPosition(const glm::vec3& position);
    void setListenerRotation(const glm::quat& rotation);

private:
    std::shared_ptr<audio_backend::Clip> createClip(const std::string& filepath, int maxInstances);

    std::unique_ptr<audio_backend::Backend> backend_;
    std::unordered_map<std::string, std::weak_ptr<audio_backend::Clip>> clipCache_;
};
