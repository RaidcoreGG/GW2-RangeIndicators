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

		if (Settings.contains(RANGE_INDICATORS) && Settings[RANGE_INDICATORS].is_array())
		{
			for (const auto& jRi : Settings[RANGE_INDICATORS])
			{
				RangeIndicator ri;
				ri.RGBA = jRi["RGBA"].get<unsigned int>();
				ri.Radius = jRi["Radius"].get<float>();
				ri.Arc = jRi["Arc"].get<float>();
				ri.IsVisible = jRi["IsVisible"].get<bool>();
				ri.VOffset = jRi["VOffset"].get<float>();
				ri.Thickness = jRi["Thickness"].get<float>();
				ri.Specialization = jRi["Specialization"].get<std::string>();
				
				// Handle Name field, which might not exist in older settings files
				if (jRi.contains("Name")) {
					std::string name = jRi["Name"].get<std::string>();
					if (name.length() > MAX_NAME_LENGTH) {
						name = name.substr(0, MAX_NAME_LENGTH);  // Leave room for null terminator
					}
					ri.Name = name;
				} else {
					ri.Name = "";  // Default to empty string if Name field doesn't exist
				}

				RangeIndicators.push_back(ri);
			}
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
}