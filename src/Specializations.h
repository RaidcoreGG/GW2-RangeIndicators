#pragma once
#ifndef SPECIALIZATIONS_H
#define SPECIALIZATIONS_H

#include <string>
#include <unordered_map>
#include "Shared.h"

extern const std::unordered_map<unsigned int, std::string> specializationNames;

std::string MumbleIdentToSpecString(Mumble::Identity* identity);
#endif