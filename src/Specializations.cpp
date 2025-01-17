#include "Specializations.h"

static const std::unordered_map<unsigned int, std::string> specializationNames = {
    {1, "Mesmer"},        {2, "Necromancer"},   {3, "Revenant"},     {4, "Warrior"},      {5, "Druid"},
    {6, "Engineer"},      {7, "Daredevil"},     {8, "Ranger"},       {9, "Revenant"},     {10, "Mesmer"},
    {11, "Warrior"},      {12, "Revenant"},     {13, "Guardian"},    {14, "Revenant"},    {15, "Revenant"},
    {16, "Guardian"},     {17, "Elementalist"}, {18, "Berserker"},   {19, "Necromancer"}, {20, "Thief"},
    {21, "Engineer"},     {22, "Warrior"},      {23, "Mesmer"},      {24, "Mesmer"},      {25, "Ranger"},
    {26, "Elementalist"}, {27, "Dragonhunter"}, {28, "Thief"},       {29, "Engineer"},    {30, "Ranger"},
    {31, "Elementalist"}, {32, "Ranger"},       {33, "Ranger"},      {34, "Reaper"},      {35, "Thief"},
    {36, "Warrior"},      {37, "Elementalist"}, {38, "Engineer"},    {39, "Necromancer"}, {40, "Chronomancer"},
    {41, "Elementalist"}, {42, "Guardian"},     {43, "Scrapper"},    {44, "Thief"},       {45, "Mesmer"},
    {46, "Guardian"},     {47, "Engineer"},     {48, "Tempest"},     {49, "Guardian"},    {50, "Necromancer"},
    {51, "Warrior"},      {52, "Herald"},       {53, "Necromancer"}, {54, "Thief"},       {55, "Soulbeast"},
    {56, "Weaver"},       {57, "Holosmith"},    {58, "Deadeye"},     {59, "Mirage"},      {60, "Scourge"},
    {61, "Spellbreaker"}, {62, "Firebrand"},    {63, "Renegade"},    {64, "Harbinger"},   {65, "Willbender"},
    {66, "Virtuoso"},     {67, "Catalyst"},     {68, "Bladesworn"},  {69, "Vindicator"},  {70, "Mechanist"},
    {71, "Specter"},      {72, "Untamed"}};

std::string MumbleIdentToSpecString(Mumble::Identity *identity)
{
    return specializationNames.count(identity->Specialization) ? specializationNames.at(identity->Specialization) : "Unknown";
}

std::string SpecToString(unsigned int aSpec)
{
    return specializationNames.count(aSpec) ? specializationNames.at(aSpec) : "Unknown";
}
