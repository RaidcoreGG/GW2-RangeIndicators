#ifndef SETTINGS_H
#define SETTINGS_H

#include <mutex>
#include <vector>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

struct RangeIndicator
{
	unsigned int RGBA;
	float Radius;
	bool IsVisible;
	float VOffset;
	float Arc;
	float Thickness;
};

extern const char* IS_VISIBLE;
extern const char* IS_HITBOX_VISIBLE;
extern const char* HITBOX_RGBA;
extern const char* RANGE_INDICATORS;

namespace Settings
{
	extern std::mutex	Mutex;
	extern std::mutex	RangesMutex;
	extern json			Settings;

	/* Loads the settings. */
	void Load(std::filesystem::path aPath);
	/* Saves the settings. */
	void Save(std::filesystem::path aPath);

	extern bool IsVisible;
	extern bool IsHitboxVisible;
	extern unsigned int HitboxRGBA;
	extern std::vector<RangeIndicator> RangeIndicators;
}

#endif
