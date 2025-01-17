#pragma once
#ifndef SPECIALIZATIONS_H
#define SPECIALIZATIONS_H

#include <string>
#include <unordered_map>
#include "Shared.h"

namespace Specializations
{
    extern const std::unordered_map<unsigned int, std::string> specializationNames;
    extern const std::vector<std::string> distinctSpecializationNames;

    std::string MumbleIdentToSpecString(Mumble::Identity *identity);
    std::string SpecToString(unsigned int aSpec);
    std::string EliteSpecToCoreSpec(std::string aSpec);
}
#endif