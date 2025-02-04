#include "Settings.h"

#include "Shared.h"

#include <filesystem>
#include <fstream>

const char* IS_VISIBLE = "IsVisible";
const char* IN_COMBAT_ONLY = "InCombatOnly";
const char* IS_HITBOX_VISIBLE = "IsHitboxVisible";
const char* ALWAYS_SHOW_HITBOX = "AlwaysShowHitbox";
const char* HITBOX_RGBA = "HitboxRGBA";
const char* RANGE_INDICATORS = "RangeIndicators";
const char* FILTER_SPECIALIZATION = "FilterSpecialization";
const char* FILTER_PROFESSION = "FilterProfession";
const char* SORT_BY_PROFESSION = "SortByProfession";
const char* TEXT_ON_CIRCLE = "TextOnCircle";
const char* TEXT_DISPLAY_MODE = "TextDisplayMode";

// Shortcuts
const char* SHORTCUT_MENU_ENABLED = "ShortcutMenuEnabled";
const char* SHORTCUT_COMBAT_TOGGLE = "CombatToggle";
const char* SHORTCUT_HITBOX_TOGGLE = "HitboxToggle";
const char* SHORTCUT_ALWAYS_SHOW_HITBOX_TOGGLE = "AlwaysShowHitboxToggle";
const char* SHORTCUT_FILTER_SPECIALIZATION_TOGGLE = "FilterSpecializationToggle";
const char* SHORTCUT_FILTER_PROFESSION_TOGGLE = "FilterProfessionToggle";
const char* SHORTCUT_SORT_BY_PROFESSION_TOGGLE = "SortByProfessionToggle";
const char* SHORTCUT_TEXT_ON_CIRCLE_TOGGLE = "TextOnCircleToggle";

namespace Settings
{
	std::mutex	Mutex;
	std::mutex	RangesMutex;
	json		Settings = json::object();

	void Load(std::filesystem::path aPath)
	{
		if (!std::filesystem::exists(aPath))
		{
			/* add default range indicators then return */

			std::lock_guard<std::mutex> lock(RangesMutex);

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				130,
				true,
				0,
				360,
				1,
				"ALL",
				""
				});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				240,
				true,
				0,
				360,
				1,
				"ALL",
				""
				});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				360,
				true,
				0,
				360,
				1,
				"ALL",
				""
				});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				600,
				true,
				0,
				360,
				1,
				"ALL",
				""
				});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				900,
				true,
				0,
				360,
				1,
				"ALL",
				""
				});

			for (RangeIndicator& ri : Settings::RangeIndicators)
			{
				if (!ri.IsVisible) { continue; }

				json jRi{};
				jRi["RGBA"] = ri.RGBA;
				jRi["Radius"] = ri.Radius;
				jRi["IsVisible"] = ri.IsVisible;
				jRi["VOffset"] = ri.VOffset;
				jRi["Arc"] = ri.Arc;
				jRi["Thickness"] = ri.Thickness;
				jRi["Specialization"] = ri.Specialization;
				jRi["Name"] = ri.Name;

				Settings::Settings[RANGE_INDICATORS].push_back(jRi);
			}

			Settings::Settings[IS_HITBOX_VISIBLE] = true;
			Settings::Settings[HITBOX_RGBA] = 0xFFFFFFFF;

			return;
		}

		Settings::Mutex.lock();
		{
			try
			{
				std::ifstream file(aPath);
				Settings = json::parse(file);
				file.close();
			}
			catch (json::parse_error& ex)
			{
				APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Settings.json could not be parsed.");
				APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", ex.what());
			}
		}

		// General settings
		if (!Settings[IS_VISIBLE].is_null())
		{
			Settings[IS_VISIBLE].get_to<bool>(IsVisible);
		}

		if (!Settings[IN_COMBAT_ONLY].is_null())
		{
			Settings[IN_COMBAT_ONLY].get_to<bool>(InCombatOnly);
		}

		if (!Settings[IS_HITBOX_VISIBLE].is_null())
		{
			Settings[IS_HITBOX_VISIBLE].get_to<bool>(IsHitboxVisible);
		}

		if (!Settings[HITBOX_RGBA].is_null())
		{
			Settings[HITBOX_RGBA].get_to<unsigned int>(HitboxRGBA);
		}

		if (!Settings[FILTER_SPECIALIZATION].is_null())
		{
			Settings[FILTER_SPECIALIZATION].get_to<bool>(FilterSpecialization);
		}

		if (!Settings[FILTER_PROFESSION].is_null())
		{
			Settings[FILTER_PROFESSION].get_to<bool>(FilterProfession);
		}

		if (!Settings[SORT_BY_PROFESSION].is_null())
		{
			Settings[SORT_BY_PROFESSION].get_to<bool>(SortByProfession);
		}

		if (!Settings[TEXT_ON_CIRCLE].is_null())
		{
			Settings[TEXT_ON_CIRCLE].get_to<bool>(TextOnCircle);
		}

		// Shortcuts
		if (!Settings[SHORTCUT_MENU_ENABLED].is_null())
		{
			Settings[SHORTCUT_MENU_ENABLED].get_to<bool>(ShortcutMenuEnabled);
		}

		if (!Settings[SHORTCUT_COMBAT_TOGGLE].is_null())
		{
			Settings[SHORTCUT_COMBAT_TOGGLE].get_to<bool>(CombatToggle);
		}

		if (!Settings[SHORTCUT_HITBOX_TOGGLE].is_null())
		{
			Settings[SHORTCUT_HITBOX_TOGGLE].get_to<bool>(HitboxToggle);
		}

		if (!Settings[SHORTCUT_ALWAYS_SHOW_HITBOX_TOGGLE].is_null())
		{
			Settings[SHORTCUT_ALWAYS_SHOW_HITBOX_TOGGLE].get_to<bool>(AlwaysShowHitboxToggle);
		}

		if (!Settings[SHORTCUT_FILTER_SPECIALIZATION_TOGGLE].is_null())
		{
			Settings[SHORTCUT_FILTER_SPECIALIZATION_TOGGLE].get_to<bool>(FilterSpecializationToggle);
		}

		if (!Settings[SHORTCUT_FILTER_PROFESSION_TOGGLE].is_null())
		{
			Settings[SHORTCUT_FILTER_PROFESSION_TOGGLE].get_to<bool>(FilterProfessionToggle);
		}

		if (!Settings[SHORTCUT_SORT_BY_PROFESSION_TOGGLE].is_null())
		{
			Settings[SHORTCUT_SORT_BY_PROFESSION_TOGGLE].get_to<bool>(SortByProfessionToggle);
		}

		if (!Settings[SHORTCUT_TEXT_ON_CIRCLE_TOGGLE].is_null())
		{
			Settings[SHORTCUT_TEXT_ON_CIRCLE_TOGGLE].get_to<bool>(TextOnCircleToggle);
		}

		// Range Indicators

		if (Settings.contains(RANGE_INDICATORS) && Settings[RANGE_INDICATORS].is_array())
		{
			for (const auto& jRi : Settings[RANGE_INDICATORS])
			{
				RangeIndicator ri;

				// Set defaults first
				ri.RGBA = 0xFFFFFFFF;
				ri.Radius = 360;
				ri.Arc = 360;
				ri.IsVisible = true;
				ri.VOffset = 0;
				ri.Thickness = 1;
				ri.Specialization = "ALL";
				ri.Name = "";

				// Load values with null checks and type validation
				if (!jRi["RGBA"].is_null()) {
					try {
						ri.RGBA = jRi["RGBA"].get<unsigned int>();
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid RGBA value in settings.json");
					}
				}

				if (!jRi["Radius"].is_null()) {
					try {
						ri.Radius = jRi["Radius"].get<float>();
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid Radius value in settings.json");
					}
				}

				if (!jRi["Arc"].is_null()) {
					try {
						ri.Arc = jRi["Arc"].get<float>();
						if (ri.Arc < 0) { ri.Arc = 0; }
						if (ri.Arc > 360) { ri.Arc = 360; }
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid Arc value in settings.json");
					}
				}

				if (!jRi["IsVisible"].is_null()) {
					try {
						ri.IsVisible = jRi["IsVisible"].get<bool>();
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid IsVisible value in settings.json");
					}
				}

				if (!jRi["VOffset"].is_null()) {
					try {
						ri.VOffset = jRi["VOffset"].get<float>();
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid VOffset value in settings.json");
					}
				}

				if (!jRi["Thickness"].is_null()) {
					try {
						ri.Thickness = jRi["Thickness"].get<float>();
						if (ri.Thickness < 1) { ri.Thickness = 1; }
						if (ri.Thickness > 25) { ri.Thickness = 25; }
					}
					catch (...) {
						APIDefs->Log(ELogLevel_WARNING, "RangeIndicators", "Invalid Thickness value in settings.json");
					}
				}

				// Handle Specialization field
				if (jRi.contains("Specialization") && !jRi["Specialization"].is_null()) {
					ri.Specialization = jRi["Specialization"].get<std::string>();
				}

				// Handle Name field
				if (jRi.contains("Name") && !jRi["Name"].is_null()) {
					std::string name = jRi["Name"].get<std::string>();
					if (name.length() > MAX_NAME_LENGTH) {
						name = name.substr(0, MAX_NAME_LENGTH);
					}
					ri.Name = name;
				}

				RangeIndicators.push_back(ri);
			}
		}

		if (!Settings[TEXT_DISPLAY_MODE].is_null())
		{
			Settings[TEXT_DISPLAY_MODE].get_to<int>(reinterpret_cast<int&>(TextDisplayMode));
		}

		Settings::Mutex.unlock();
	}
	void Save(std::filesystem::path aPath)
	{
		Settings::Mutex.lock();
		{
			std::ofstream file(aPath);
			file << Settings.dump(1, '\t') << std::endl;
			file.close();
		}
		Settings::Mutex.unlock();
	}

	bool IsVisible = true;
	bool InCombatOnly = false;
	bool IsHitboxVisible = true;
	bool AlwaysShowHitbox = false;
	unsigned int HitboxRGBA = 0xFFFFFFFF;
	bool FilterSpecialization = false;
	bool FilterProfession = false;
	bool SortByProfession = false;
	std::vector<RangeIndicator> RangeIndicators;
	bool TextOnCircle = false;
	TextMode TextDisplayMode = TextMode::Radius;

	// Shortcuts
	bool ShortcutMenuEnabled = true;
	bool CombatToggle = true;
	bool HitboxToggle = true;
	bool AlwaysShowHitboxToggle = false;
	bool FilterSpecializationToggle = false;
	bool FilterProfessionToggle = false;
	bool SortByProfessionToggle = false;
	bool TextOnCircleToggle = false;
}