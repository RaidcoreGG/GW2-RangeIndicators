#include "Settings.h"

#include "Shared.h"

#include <filesystem>
#include <fstream>

const char* IS_VISIBLE = "IsVisible";
const char* IS_HITBOX_VISIBLE = "IsHitboxVisible";
const char* HITBOX_RGBA = "HitboxRGBA";
const char* RANGE_INDICATORS = "RangeIndicators";

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
				360
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				240,
				true,
				0,
				360
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				360,
				true,
				0,
				360
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				600,
				true,
				0,
				360
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				900,
				true,
				0,
				360
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

		if (!Settings[IS_HITBOX_VISIBLE].is_null())
		{
			Settings[IS_HITBOX_VISIBLE].get_to<bool>(IsHitboxVisible);
		}

		if (!Settings[HITBOX_RGBA].is_null())
		{
			Settings[HITBOX_RGBA].get_to<unsigned int>(HitboxRGBA);
		}

		if (!Settings[RANGE_INDICATORS].is_null())
		{
			std::lock_guard<std::mutex> lock(RangesMutex);
			for (json ri : Settings[RANGE_INDICATORS])
			{
				if (ri.is_null())
				{
					continue;
				}

				RangeIndicator rangeIndicator{};
				if (!ri["RGBA"].is_null()) { ri["RGBA"].get_to(rangeIndicator.RGBA); }
				if (!ri["Radius"].is_null()) { ri["Radius"].get_to(rangeIndicator.Radius); }
				if (!ri["IsVisible"].is_null()) { ri["IsVisible"].get_to(rangeIndicator.IsVisible); }
				if (!ri["VOffset"].is_null()) { ri["VOffset"].get_to(rangeIndicator.VOffset); }
				if (!ri["Arc"].is_null()) { ri["Arc"].get_to(rangeIndicator.Arc); } else { rangeIndicator.Arc = 360; }

				RangeIndicators.push_back(rangeIndicator);
			}

			std::sort(RangeIndicators.begin(), RangeIndicators.end(), [](RangeIndicator lhs, RangeIndicator rhs)
				{
					return lhs.Radius < rhs.Radius;
				});

			Settings[RANGE_INDICATORS].clear();

			for (RangeIndicator ri : RangeIndicators)
			{
				json jRi{};
				jRi["RGBA"] = ri.RGBA;
				jRi["Radius"] = ri.Radius;
				jRi["IsVisible"] = ri.IsVisible;
				jRi["VOffset"] = ri.VOffset;
				jRi["Arc"] = ri.Arc;
				Settings[RANGE_INDICATORS].push_back(jRi);
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
	bool IsHitboxVisible = true;
	unsigned int HitboxRGBA = 0xFFFFFFFF;
	std::vector<RangeIndicator> RangeIndicators;
}