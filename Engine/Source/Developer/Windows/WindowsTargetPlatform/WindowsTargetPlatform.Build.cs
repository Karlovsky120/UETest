// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WindowsTargetPlatform : ModuleRules
{
	public WindowsTargetPlatform(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Settings",
				"TargetPlatform"
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine"
				}
			);

			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/WindowsTargetPlatform/Classes"
			}
		);
	}
}
