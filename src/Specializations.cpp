#include "specializations.h"
#include <set>

namespace Specializations {

	// Core professions and their elite specializations
	struct ProfessionSpecs {
		std::string coreProfession;
		std::vector<std::string> eliteSpecs;
	};

	static const std::vector<ProfessionSpecs> professionSpecializations = {
		{"Guardian", {"Dragonhunter", "Firebrand", "Willbender", "Luminary"}},
		{"Warrior", {"Berserker", "Spellbreaker", "Bladesworn", "Paragon"}},
		{"Engineer", {"Scrapper", "Holosmith", "Mechanist", "Amalgam"}},
		{"Ranger", {"Druid", "Soulbeast", "Untamed", "Galeshot"}},
		{"Thief", {"Daredevil", "Deadeye", "Specter", "Antiquary"}},
		{"Elementalist", {"Tempest", "Weaver", "Catalyst", "Evoker"}},
		{"Mesmer", {"Chronomancer", "Mirage", "Virtuoso", "Troubadour"}},
		{"Necromancer", {"Reaper", "Scourge", "Harbinger", "Ritualist"}},
		{"Revenant", {"Herald", "Renegade", "Vindicator", "Conduit"}} };

	std::string EliteSpecToCoreSpec(std::string aSpec)
	{
		for (const auto& profession : professionSpecializations)
		{
			if (std::find(profession.eliteSpecs.begin(), profession.eliteSpecs.end(), aSpec) != profession.eliteSpecs.end())
				return profession.coreProfession;
		}
		return "Unknown";
	}

	// Specialization IDs map for mumble identity
	static const std::unordered_map<unsigned int, std::string> specializationNames = {
	{1, "Mesmer"},        {2, "Necromancer"},   {3, "Revenant"},
	{4, "Warrior"},       {5, "Druid"},         {6, "Engineer"},
	{7, "Daredevil"},     {8, "Ranger"},        {9, "Revenant"},
	{10, "Mesmer"},       {11, "Warrior"},      {12, "Revenant"},
	{13, "Guardian"},     {14, "Revenant"},     {15, "Revenant"},
	{16, "Guardian"},     {17, "Elementalist"}, {18, "Berserker"},
	{19, "Necromancer"},  {20, "Thief"},        {21, "Engineer"},
	{22, "Warrior"},      {23, "Mesmer"},       {24, "Mesmer"},
	{25, "Ranger"},       {26, "Elementalist"}, {27, "Dragonhunter"},
	{28, "Thief"},        {29, "Engineer"},     {30, "Ranger"},
	{31, "Elementalist"}, {32, "Ranger"},       {33, "Ranger"},
	{34, "Reaper"},       {35, "Thief"},        {36, "Warrior"},
	{37, "Elementalist"}, {38, "Engineer"},     {39, "Necromancer"},
	{40, "Chronomancer"}, {41, "Elementalist"}, {42, "Guardian"},
	{43, "Scrapper"},     {44, "Thief"},        {45, "Mesmer"},
	{46, "Guardian"},     {47, "Engineer"},     {48, "Tempest"},
	{49, "Guardian"},     {50, "Necromancer"},  {51, "Warrior"},
	{52, "Herald"},       {53, "Necromancer"},  {54, "Thief"},
	{55, "Soulbeast"},    {56, "Weaver"},       {57, "Holosmith"},
	{58, "Deadeye"},      {59, "Mirage"},       {60, "Scourge"},
	{61, "Spellbreaker"}, {62, "Firebrand"},    {63, "Renegade"},
	{64, "Harbinger"},    {65, "Willbender"},   {66, "Virtuoso"},
	{67, "Catalyst"},     {68, "Bladesworn"},   {69, "Vindicator"},
	{70, "Mechanist"},    {71, "Specter"},      {72, "Untamed"},
	{73, "Troubadour"},   {74, "Paragon"},      {75, "Amalgam"},
	{76, "Ritualist"},    {77, "Antiquary"},    {78, "Galeshot"},
	{79, "Conduit"},      {80, "Evoker"},       {81, "Luminary"}};

	const std::vector<std::string> distinctSpecializationNames = []()
		{
			std::set<std::string> unique;
			for (const auto& pair : specializationNames)
			{
				unique.insert(pair.second);
			}
			return std::vector<std::string>(unique.begin(), unique.end());
		}();

	std::string MumbleIdentToSpecString(Mumble::Identity* identity)
	{
		return specializationNames.count(identity->Specialization) ? specializationNames.at(identity->Specialization) : "Unknown";
	}

	std::string SpecToString(unsigned int aSpec)
	{
		return specializationNames.count(aSpec) ? specializationNames.at(aSpec) : "Unknown";
	}
} // namespace Specializations