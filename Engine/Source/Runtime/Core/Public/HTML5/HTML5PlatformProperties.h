// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	HTML5Properties.h - Basic static properties of a platform 
	These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform
==================================================================================*/

#pragma once

#include "GenericPlatformProperties.h"


/**
 * Implements HTML5 platform properties.
 */
struct FHTML5PlatformProperties
	: public FGenericPlatformProperties
{
	static FORCEINLINE const char* DisplayName()
	{
		return "HTML5";
	}

	static FORCEINLINE const char* GetPhysicsFormat( )
	{
		return "PhysXPC";
	}

	static FORCEINLINE bool HasEditorOnlyData( )
	{
		return false;
	}

	static FORCEINLINE uint32 MaxGpuSkinBones( )
	{
		return 20;
	}

	static FORCEINLINE const char* PlatformName()
	{
		return "HTML5";
	}

	static FORCEINLINE const char* IniPlatformName()
	{
		return "HTML5";
	}

	static FORCEINLINE bool RequiresCookedData( )
	{
		return true;
	}

	static FORCEINLINE bool SupportsBuildTarget( EBuildTargets::Type BuildTarget )
	{
		return (BuildTarget == EBuildTargets::Game);
	}

	static FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return false;
	}

	static FORCEINLINE bool SupportsTextureStreaming()
	{
		return false;
	}

	static FORCEINLINE bool SupportsVertexShaderTextureSampling()
	{
		return false;
	}

	static FORCEINLINE bool HasFixedResolution()
	{
		return false;
	}
};
