#include "AudioManager.h"
#include <algorithm>
#include <iostream>

bool AudioManager::Initialize()
{
    std::cout << "Audio manager initialized" << std::endl;
    return true;
}

void AudioManager::Shutdown()
{
    StopMusic();
    StopAllSounds();
    musicTracks.clear();
    soundBuffers.clear();
    activeSounds.clear();
}

bool AudioManager::LoadMusic(const std::string& musicID, const std::string& filepath)
{
    auto music = std::make_unique<sf::Music>();

    if (!music->openFromFile(filepath))
    {
        std::cerr << "Failed to load music: " << filepath << std::endl;
        return false;
    }

    musicTracks[musicID] = std::move(music);
    std::cout << "Loaded music: " << musicID << " from" << filepath << std::endl;
    return true;
}

bool AudioManager::LoadSound(const std::string& soundID, const std::string& filepath) {
    sf::SoundBuffer buffer;

    if (!buffer.loadFromFile(filepath)) {
        std::cerr << "Failed to load sound: " << filepath << std::endl;
        return false;
    }

    soundBuffers[soundID] = buffer;
    std::cout << "Loaded sound: " << soundID << " from " << filepath << std::endl;
    return true;
}

void AudioManager::PlayMusic(const std::string& musicID, bool loop, float volume) {
    auto it = musicTracks.find(musicID);
    if (it == musicTracks.end()) {
        std::cerr << "Music not found: " << musicID << std::endl;
        return;
    }

    // Stop current music if playing
    StopMusic();

    currentMusicID = musicID;
    it->second->setLoop(loop);
    it->second->setVolume(volume * musicVolume / 100.0f * masterVolume / 100.0f);
    it->second->play();

    std::cout << "Playing music: " << musicID << std::endl;
}

void AudioManager::StopMusic() {
    if (!currentMusicID.empty()) {
        auto it = musicTracks.find(currentMusicID);
        if (it != musicTracks.end()) {
            it->second->stop();
        }
        currentMusicID = "";
    }
}

void AudioManager::PauseMusic() {
    if (!currentMusicID.empty()) {
        auto it = musicTracks.find(currentMusicID);
        if (it != musicTracks.end()) {
            it->second->pause();
        }
    }
}

void AudioManager::ResumeMusic() {
    if (!currentMusicID.empty()) {
        auto it = musicTracks.find(currentMusicID);
        if (it != musicTracks.end() && it->second->getStatus() == sf::Music::Paused) {
            it->second->play();
        }
    }
}

void AudioManager::PlaySound(const std::string& soundID, float volume) {
    auto it = soundBuffers.find(soundID);
    if (it == soundBuffers.end()) {
        std::cerr << "Sound not found: " << soundID << std::endl;
        return;
    }

    auto sound = std::make_unique<sf::Sound>();
    sound->setBuffer(it->second);
    sound->setVolume(volume * soundVolume / 100.0f * masterVolume / 100.0f);
    sound->play();

    activeSounds.push_back(std::move(sound));
}

void AudioManager::StopAllSounds() {
    for (auto& sound : activeSounds) {
        sound->stop();
    }
    activeSounds.clear();
}

void AudioManager::SetMasterVolume(float volume) {
    masterVolume = std::clamp(volume, 0.0f, 100.0f);

    // Update music volume
    if (!currentMusicID.empty()) {
        auto it = musicTracks.find(currentMusicID);
        if (it != musicTracks.end()) {
            it->second->setVolume(musicVolume * masterVolume / 100.0f);
        }
    }
}

void AudioManager::SetMusicVolume(float volume) {
    musicVolume = std::clamp(volume, 0.0f, 100.0f);

    // Update current music
    if (!currentMusicID.empty()) {
        auto it = musicTracks.find(currentMusicID);
        if (it != musicTracks.end()) {
            it->second->setVolume(musicVolume * masterVolume / 100.0f);
        }
    }
}

void AudioManager::SetSoundVolume(float volume) {
    soundVolume = std::clamp(volume, 0.0f, 100.0f);
}

bool AudioManager::IsMusicPlaying() const {
    if (currentMusicID.empty()) return false;

    auto it = musicTracks.find(currentMusicID);
    if (it != musicTracks.end()) {
        return it->second->getStatus() == sf::Music::Playing;
    }
    return false;
}

void AudioManager::Update() {
    // Remove finished sounds
    activeSounds.erase(
        std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<sf::Sound>& sound) {
                return sound->getStatus() == sf::Sound::Stopped;
            }),
        activeSounds.end()
    );
}