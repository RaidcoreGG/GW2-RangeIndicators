#ifndef SHARED_H
#define SHARED_H

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "RTAPI/RTAPI.hpp"

extern AddonAPI*            APIDefs;
extern Mumble::Data*        MumbleLink;
extern Mumble::Identity*    MumbleIdentity;
extern NexusLinkData*       NexusLink;
extern RTAPI::RealTimeData* RTDATA;

#endif
