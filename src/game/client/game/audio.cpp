#include "game.hpp"

#include "client/net/connection.hpp"
#include "karma/audio/backend.hpp"
#include "karma/audio/audio_system.hpp"

namespace bz3 {

void Game::onAudioEvent(client::net::AudioEvent event) {
    switch (event) {
        case client::net::AudioEvent::PlayerSpawn:
            playOneShotAsset("assets.audio.player.Spawn", 1.0f, 1.0f);
            break;
        case client::net::AudioEvent::PlayerDeath:
            playOneShotAsset("assets.audio.player.Die", 1.0f, 1.0f);
            break;
        case client::net::AudioEvent::ShotFire:
            playOneShotAsset("assets.audio.shot.Fire", 0.65f, 1.0f);
            break;
        default:
            break;
    }
}

void Game::playOneShotAsset(const char* asset_key, float gain, float pitch) {
    if (!audio || !asset_key || *asset_key == '\0') {
        return;
    }

    karma::audio_backend::PlayRequest request{};
    request.asset_path = asset_key;
    request.gain = gain;
    request.pitch = pitch;
    request.loop = false;
    audio->playOneShot(request);
}

} // namespace bz3
