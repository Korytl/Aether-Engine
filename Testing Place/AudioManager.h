#pragma once

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <memory>

class AudioManager
{
public:
	static AudioManager& GetInstance()
	{
		static AudioManager instance;
		return instance;
	}

	//Initialize
	bool Initialize();
	void Shutdown();

	//Audio loading
	bool LoadMusic(const std::string& musicID, const std::string& filepath);
	bool LoadSound(const std::string& soundID, const std::string& filepath);

	//Music controls
	void PlayMusic(const std::string& musicID, bool loop = true, float volume = 50.0f);
	void StopMusic();
	void PauseMusic();
	void ResumeMusic();

	//Sound effects
	void PlaySound(const std::string& soundID, float volume = 100.0f);
	void StopAllSounds();

	//Volume controls
	void SetMasterVolume(float volume); //0-100
	void SetMusicVolume(float volume); //0-100
	void SetSoundVolume(float volume); //0-100

	//Getters
	float GetMasterVolume() const { return masterVolume; }
	float GetMusicVolume() const { return musicVolume; }
	float GetSoundVolume() const { return soundVolume; }
	bool IsMusicPlaying() const;

	//Update - clean up finished sounds
	void Update();

private:
	AudioManager() : masterVolume(100.0f), musicVolume(50.0f), soundVolume(100.0f),
		currentMusicID("")
	{

	}

	~AudioManager()
	{
		Shutdown();
	}

	//Disable copy
	AudioManager(const AudioManager&) = delete;
	AudioManager& operator = (const AudioManager&) = delete;

	std::unordered_map<std::string, std::unique_ptr<sf::Music>>musicTracks;
	std::unordered_map<std::string, sf::SoundBuffer>soundBuffers;
	std::vector<std::unique_ptr<sf::Sound>> activeSounds;

	float masterVolume;
	float musicVolume;
	float soundVolume;
	std::string currentMusicID;
};