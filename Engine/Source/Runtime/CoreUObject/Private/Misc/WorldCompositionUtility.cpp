// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WorldCompositionUtility.cpp : Support structures for world composition
=============================================================================*/
#include "CoreUObjectPrivate.h"

FArchive& operator<<( FArchive& Ar, FWorldTileLayer& D )
{
	// Serialized with FPackageFileSummary
	Ar << D.Name << D.Reserved0 << D.Reserved1;
		
	if (Ar.UE4Ver() >= VER_UE4_WORLD_LEVEL_INFO_UPDATED)
	{
		Ar << D.StreamingDistance;
	}

	if (Ar.UE4Ver() >= VER_UE4_WORLD_LAYER_ENABLE_DISTANCE_STREAMING)
	{
		Ar << D.DistanceStreamingEnabled;
	}
	
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FWorldTileLODInfo& D )
{
	// Serialized with FPackageFileSummary
	Ar << D.RelativeStreamingDistance 
		<< D.GenDetailsPercentage
		<< D.GenMaxDeviation
		<< D.Reserved0
		<< D.Reserved1;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FWorldTileInfo& D )
{
	// Serialized with FPackageFileSummary
	Ar << D.Position << D.Bounds  << D.Layer;
	
	if (Ar.UE4Ver() >= VER_UE4_WORLD_LEVEL_INFO_UPDATED)
	{
		Ar << D.Reserved0 << D.ParentTilePackageName;
	}

	if (Ar.UE4Ver() >= VER_UE4_WORLD_LEVEL_INFO_LOD_LIST)
	{
		Ar << D.LODList;
	}
	
	if (Ar.UE4Ver() >= VER_UE4_WORLD_LEVEL_INFO_ZORDER)
	{
		Ar << D.ZOrder;
	}
		
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		Ar << D.AbsolutePosition;
	}

	return Ar;
}

bool FWorldTileInfo::Read(const FString& InPackageFileName, FWorldTileInfo& OutInfo)
{
	// Fill with default information
	OutInfo = FWorldTileInfo();
	
	// Create a file reader to load the file
	TScopedPointer<FArchive> FileReader(IFileManager::Get().CreateFileReader(*InPackageFileName));
	if (FileReader == NULL)
	{
		// Couldn't open the file
		return false;
	}

	// Read package file summary from the file
	FPackageFileSummary FileSummary;
	(*FileReader) << FileSummary;

	// Make sure this is indeed a package
	if (FileSummary.Tag != PACKAGE_FILE_TAG)
	{
		// Unrecognized or malformed package file
		return false;
	}

	// Does the package contains a level info?
	if(FileSummary.WorldTileInfoDataOffset != 0)
	{
		// Seek the the part of the file where the structure lives
		FileReader->Seek(FileSummary.WorldTileInfoDataOffset);

		//make sure the filereader gets the correct version number (it defaults to latest version)
		FileReader->SetUE3Ver(FileSummary.GetFileVersionUE3());
		FileReader->SetUE4Ver(FileSummary.GetFileVersionUE4());

		// Load the structure
		*FileReader << OutInfo;
	}
	
	return true;
}