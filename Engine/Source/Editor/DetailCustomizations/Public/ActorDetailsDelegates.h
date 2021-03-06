// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Delegate that returns a const& to an array of actors this view is inspecting */
DECLARE_DELEGATE_RetVal( const TArray< TWeakObjectPtr<AActor> >&, FGetSelectedActors );

DECLARE_MULTICAST_DELEGATE_TwoParams(FExtendActorDetails, class IDetailLayoutBuilder&, const FGetSelectedActors&);

/** Delegate that is invoked to extend the actor details view */
DETAILCUSTOMIZATIONS_API FExtendActorDetails OnExtendActorDetails;