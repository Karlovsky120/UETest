// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LiveEditorBroadcaster.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams( FLiveEditorEventMIDIMultiCast, int32, Delta, int32, MidiValue, ELiveEditControllerType::Type, ControlType );

UCLASS(DependsOn=ULiveEditorTypes,MinimalAPI)
class ULiveEditorBroadcaster : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FLiveEditorEventMIDIMultiCast OnEventMIDI;
};
