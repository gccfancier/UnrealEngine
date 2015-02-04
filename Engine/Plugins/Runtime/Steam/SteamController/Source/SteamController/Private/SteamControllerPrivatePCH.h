// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "ISteamControllerPlugin.h"

#if WITH_STEAM_CONTROLLER

#include "InputDevice.h"

// Disable crazy warnings that claim that standard C library is "deprecated".
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#include <steam/steam_api.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // WITH_STEAM_CONTROLLER

