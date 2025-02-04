#ifndef SETTINGS_H
#define SETTINGS_H

#include <mutex>
#include <vector>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

constexpr size_t MAX_NAME_LENGTH = 127;  // 127 chars + null terminator = 128 bytes

struct RangeIndicator
{
	unsigned int RGBA;
	float Radius;
	bool IsVisible;
	float VOffset;
	float Arc;
	float Thickness;
	std::string Specialization;
	std::string Name;
};

enum class TextMode {
    Radius = 0,
    Name = 1,
    NameAndRadius = 2
};

extern const char* IS_VISIBLE;
extern const char* IN_COMBAT_ONLY;
extern const char* IS_HITBOX_VISIBLE;
extern const char* ALWAYS_SHOW_HITBOX;
extern const char* HITBOX_RGBA;
extern const char* RANGE_INDICATORS;
extern const char* FILTER_SPECIALIZATION;
extern const char* FILTER_PROFESSION;
extern const char* SORT_BY_PROFESSION;
extern const char* TEXT_ON_CIRCLE;
extern const char* TEXT_DISPLAY_MODE;

// Shortcuts
extern const char* SHORTCUT_MENU_ENABLED;
extern const char* SHORTCUT_COMBAT_TOGGLE;
extern const char* SHORTCUT_HITBOX_TOGGLE;
extern const char* SHORTCUT_ALWAYS_SHOW_HITBOX_TOGGLE;
extern const char* SHORTCUT_FILTER_SPECIALIZATION_TOGGLE;
extern const char* SHORTCUT_FILTER_PROFESSION_TOGGLE;
extern const char* SHORTCUT_SORT_BY_PROFESSION_TOGGLE;
extern const char* SHORTCUT_TEXT_ON_CIRCLE_TOGGLE;


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
	extern bool InCombatOnly;
	extern bool IsHitboxVisible;
	extern bool AlwaysShowHitbox;
	extern unsigned int HitboxRGBA;
	extern bool FilterSpecialization;
	extern bool FilterProfession;
	extern bool SortByProfession;
	extern std::vector<RangeIndicator> RangeIndicators;
	extern bool TextOnCircle;
	extern TextMode TextDisplayMode;

	// Shortcuts
	extern bool ShortcutMenuEnabled;
	extern bool CombatToggle;
	extern bool HitboxToggle;
	extern bool AlwaysShowHitboxToggle;
	extern bool FilterSpecializationToggle;
	extern bool FilterProfessionToggle;
	extern bool SortByProfessionToggle;
	extern bool TextOnCircleToggle;
}

#endif
