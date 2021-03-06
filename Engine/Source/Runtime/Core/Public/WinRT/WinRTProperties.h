// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	WinRTProperties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatformProperties.h"

struct FWinRTPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* DisplayName()
	{
		return "Windows RT";
	}

	static FORCEINLINE const char* PlatformName()
	{
		return "WinRT";
	}
	static FORCEINLINE const char* IniPlatformName()
	{
		return "WinRT";
	}
	static FORCEINLINE bool HasEditorOnlyData()
	{
		return false;
	}              
	static FORCEINLINE bool SupportsTessellation()
	{
		return true;
	}
	static FORCEINLINE bool RequiresCookedData()
	{
		return true;
	}
	static FORCEINLINE const char* GetPhysicsFormat()
	{
		return "PhysXWinRT";
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}
};


