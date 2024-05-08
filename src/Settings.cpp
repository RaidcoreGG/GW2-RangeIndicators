#include "Settings.h"

#include "Shared.h"

#include <filesystem>
#include <fstream>

const char* IS_HITBOX_VISIBLE = "IsHitboxVisible";
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
				true
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				240,
				true
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				360,
				true
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				600,
				true
			});

			RangeIndicators.push_back(RangeIndicator{
				0xFFFFFFFF,
				900,
				true
			});

			for (RangeIndicator& ri : Settings::RangeIndicators)
			{
				if (!ri.IsVisible) { continue; }

				json jRi{};
				jRi["RGBA"] = ri.RGBA;
				jRi["Radius"] = ri.Radius;
				jRi["IsVisible"] = ri.IsVisible;

				Settings::Settings[RANGE_INDICATORS].push_back(jRi);
			}

			Settings::Settings[IS_HITBOX_VISIBLE] = true;

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

		if (!Settings[IS_HITBOX_VISIBLE].is_null())
		{
			Settings[IS_HITBOX_VISIBLE].get_to<bool>(IsHitboxVisible);
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
				ri["RGBA"].get_to(rangeIndicator.RGBA);
				ri["Radius"].get_to(rangeIndicator.Radius);
				ri["IsVisible"].get_to(rangeIndicator.IsVisible);

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

	bool IsHitboxVisible = true;
	std::vector<RangeIndicator> RangeIndicators;
}