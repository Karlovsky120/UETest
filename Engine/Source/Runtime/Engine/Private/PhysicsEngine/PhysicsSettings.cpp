// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UPhysicsSettings::UPhysicsSettings(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP),
DefaultGravityZ(-980.f),
bEnableAsyncScene(false),
MaxPhysicsDeltaTime(1.f / 30.f),
bSubstepping(false),
MaxSubstepDeltaTime(1.f / 60.f),
MaxSubsteps(6),
SyncSceneSmoothingFactor(0.0f),
AsyncSceneSmoothingFactor(0.99f),
InitialAverageFrameRate(1.f / 60.f)
{
}

void UPhysicsSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	LoadSurfaceType();
#endif
}

#if WITH_EDITOR
bool UPhysicsSettings::CanEditChange(const UProperty* Property) const
{
	bool bIsEditable = Super::CanEditChange(Property);
	if(bIsEditable && Property != NULL)
	{
		const FName Name = Property->GetFName();
		if(Name == TEXT("MaxPhysicsDeltaTime") || Name == TEXT("SyncSceneSmoothingFactor") || Name == TEXT("AsyncSceneSmoothingFactor") || Name == TEXT("InitialAverageFrameRate"))
		{
			bIsEditable = !bSubstepping;
		}
	}

	return bIsEditable;
}

void UPhysicsSettings::LoadSurfaceType()
{
	// read "SurfaceType" defines and set meta data for the enum
	// find the enum
	UEnum * Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPhysicalSurface"), true);
	// we need this Enum
	check(Enum);

	const FString KeyName = TEXT("DisplayName");
	const FString HiddenMeta = TEXT("Hidden");
	const FString UnusedDisplayName = TEXT("Unused");

	// remainders, set to be unused
	for(int32 EnumIndex=1; EnumIndex<Enum->NumEnums(); ++EnumIndex)
	{
		// if meta data isn't set yet, set name to "Unused" until we fix property window to handle this
		// make sure all hide and set unused first
		// if not hidden yet
		if(!Enum->HasMetaData(*HiddenMeta, EnumIndex))
		{
			Enum->SetMetaData(*HiddenMeta, TEXT(""), EnumIndex);
			Enum->SetMetaData(*KeyName, *UnusedDisplayName, EnumIndex);
		}
	}

	for(auto Iter=PhysicalSurfaces.CreateConstIterator(); Iter; ++Iter)
	{
		// @todo only for editor
		Enum->SetMetaData(*KeyName, *Iter->Name.ToString(), Iter->Type);
		// also need to remove "Hidden"
		Enum->RemoveMetaData(*HiddenMeta, Iter->Type);
	}
}
#endif	// WITH_EDITOR