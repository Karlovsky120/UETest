// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	GenericPlatformProperties.h: Generic platform properties, used a base class for platforms.
		These are shared between:
		the runtime platform - via FPlatformProperties
		the target platforms - via ITargetPlatform

==============================================================================================*/

#pragma once


/**
 * Base class for platform properties.
 */
struct FGenericPlatformProperties
{
	/**
	 * Gets the platform's user friendly display name.
	 *
	 * @return The display name string.
	 */
	static FORCEINLINE const char* DisplayName()
	{
		return "";
	}

	/**
	 * Gets the platform's physics format.
	 *
	 * @return The physics format name.
	 */
	static FORCEINLINE const char* GetPhysicsFormat()
	{
		return "PhysXPC";
	}

	/**
	 * Gets whether this platform has Editor-only data.
	 *
	 * @return true if the platform has Editor-only data, false otherwise.
	 */
	static FORCEINLINE bool HasEditorOnlyData()
	{
		return true;
	}

	/**
	 * Gets the name of this platform when loading INI files. Defaults to PlatformName.
	 *
	 * Note: MUST be implemented per platform.
	 *
	 * @return Platform name.
	 */
	static const char* IniPlatformName();

	/**
	 * Gets whether this is a game only platform.
	 *
	 * @return true if this is a game only platform, false otherwise.
	 */
	static FORCEINLINE bool IsGameOnly()
	{
		return false;
	}

	/**
	 * Gets whether this is a server only platform.
	 *
	 * @return true if this is a server only platform, false otherwise.
	 */
	static FORCEINLINE bool IsServerOnly()
	{
		return false;
	}

	/**
	 * Gets whether this is a client only (no capability to run the game without connecting to a server) platform.
	 *
	 * @return true if this is a client only platform, false otherwise.
	 */
	static FORCEINLINE bool IsClientOnly()
	{
		return false;
	}

	/**
	 *	Gets whether this was a monolithic build or not
	 */
	static FORCEINLINE bool IsMonolithicBuild()
	{
		return IS_MONOLITHIC;
	}

	/**
	 *	Gets whether this was a program or not
	 */
	static FORCEINLINE bool IsProgram()
	{
		return IS_PROGRAM;
	}

	/**
	 * Gets whether this is a Little Endian platform.
	 *
	 * @return true if the platform is Little Endian, false otherwise.
	 */
	static FORCEINLINE bool IsLittleEndian()
	{
		return true;
	}

	/**
	 * Returns the maximum bones the platform supports.
	 *
	 * @return the maximum bones the platform supports.
	 */
	static FORCEINLINE uint32 MaxGpuSkinBones()
	{
		return 256;
	}

	/**
	 * Gets the name of this platform
	 *
	 * Note: MUST be implemented per platform.
	 *
	 * @return Platform Name.
	 */
	static FORCEINLINE const char* PlatformName();

	/**
	 * Checks whether this platform requires cooked data.
	 *
	 * @return true if cooked data is required, false otherwise.
	 */
	static FORCEINLINE bool RequiresCookedData()
	{
		return false;
	}

	/**
	 * Checks whether this platform requires user credentials (typically server platforms).
	 *
	 * @return true if this platform requires user credentials, false otherwise.
	 */
	static FORCEINLINE bool RequiresUserCredentials()
	{
		return false;
	}

	/**
	 * Checks whether the specified build target is supported.
	 *
	 * @param BuildTarget - The build target to check.
	 *
	 * @return true if the build target is supported, false otherwise.
	 */
	static FORCEINLINE bool SupportsBuildTarget( EBuildTargets::Type BuildTarget )
	{
		return true;
	}

	/**
	 * Gets whether this platform supports gray scale sRGB texture formats.
	 *
	 * @return true if gray scale sRGB texture formats are supported.
	 */
	static FORCEINLINE bool SupportsGrayscaleSRGB()
	{
		return true;
	}

	/**
	 * Checks whether this platforms supports running multiple game instances on a single device.
	 *
	 * @return true if multiple instances are supported, false otherwise.
	 */
	static FORCEINLINE bool SupportsMultipleGameInstances()
	{
		return false;
	}

	/**
	 * Gets whether this platform supports tessellation.
	 *
	 * @return true if tessellation is supported, false otherwise.
	 */
	static FORCEINLINE bool SupportsTessellation()
	{
		return false;
	}

	/**
	 * Gets whether this platform supports windowed mode rendering.
	 *
	 * @return true if windowed mode is supported.
	 */
	static FORCEINLINE bool SupportsWindowedMode()
	{
		return false;
	}

	static FORCEINLINE bool SupportsHighQualityLightmaps()
	{
		return true;
	}

	static FORCEINLINE bool SupportsLowQualityLightmaps()
	{
		return true;
	}

	static FORCEINLINE bool SupportsDistanceFieldShadows()
	{
		return true;
	}

	static FORCEINLINE bool SupportsTextureStreaming()
	{
		return true;
	}

	static FORCEINLINE bool SupportsVertexShaderTextureSampling()
	{
		return true;
	}

	/**
	 * Gets whether user settings should override the resolution or not
	 */
	static FORCEINLINE bool HasFixedResolution()
	{
		return true;
	}

	/**
	 *	Gets whether this platform supports Fast VRAM memory 
	 *		Ie, whether TexCreate_FastVRAM flags actually mean something or not
	 *	
	 *	@return	bool		true if supported, false if not
	 */
	static FORCEINLINE bool SupportsFastVRAMMemory()
	{
		return false;
	}

	// Whether the platform allows an application to quit to the OS
	static FORCEINLINE bool SupportsQuit()
	{
		return false;
	}
};
