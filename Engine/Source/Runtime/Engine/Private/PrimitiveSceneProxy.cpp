// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneProxy.cpp: Primitive scene proxy implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "PrimitiveSceneProxy.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName InResourceName)
:	DrawInGame(InComponent->bVisible && !InComponent->bHiddenInGame)
,	DrawInEditor(InComponent->bVisible)
,	bReceivesDecals(InComponent->bReceivesDecals)
,	bOnlyOwnerSee(InComponent->bOnlyOwnerSee)
,	bOwnerNoSee(InComponent->bOwnerNoSee)
,	bStatic(false)
,	bOftenMoving(false)
,	bSelected(InComponent->ShouldRenderSelected())
,	bHovered(false)
,	bUseViewOwnerDepthPriorityGroup(InComponent->bUseViewOwnerDepthPriorityGroup)
,	bHasMotionBlurVelocityMeshes(InComponent->bHasMotionBlurVelocityMeshes)
,	StaticDepthPriorityGroup(InComponent->GetStaticDepthPriorityGroup())
,	ViewOwnerDepthPriorityGroup(InComponent->ViewOwnerDepthPriorityGroup)
,	bStaticLighting(InComponent->HasStaticLighting())
,	bRenderCustomDepth(InComponent->bRenderCustomDepth)
,	bRenderInMainPass(InComponent->bRenderInMainPass)
,	bRequiresVisibleLevelToRender(false)
,	bIsComponentLevelVisible(false)
,	bCollisionEnabled(InComponent->IsCollisionEnabled())
,	bTreatAsBackgroundForOcclusion(InComponent->bTreatAsBackgroundForOcclusion)
,	bNeedsUnbuiltPreviewLighting(InComponent->HasStaticLighting() && !InComponent->bHasCachedStaticLighting)
,	bHasValidSettingsForStaticLighting(InComponent->HasValidSettingsForStaticLighting())
,	bWillEverBeLit(true)
	// Disable dynamic shadow casting if the primitive only casts indirect shadows, since dynamic shadows are always shadowing direct lighting
,	bCastDynamicShadow(InComponent->bCastDynamicShadow && InComponent->CastShadow && !InComponent->GetShadowIndirectOnly())
,   bAffectDynamicIndirectLighting(InComponent->bAffectDynamicIndirectLighting)
,	bCastStaticShadow(InComponent->CastShadow && InComponent->bCastStaticShadow)
,	bCastVolumetricTranslucentShadow(InComponent->bCastDynamicShadow && InComponent->CastShadow && InComponent->bCastVolumetricTranslucentShadow)
,	bCastHiddenShadow(InComponent->bCastHiddenShadow)
,	bCastShadowAsTwoSided(InComponent->bCastShadowAsTwoSided)
,	bCastInsetShadow(InComponent->bCastInsetShadow)
,	bLightAttachmentsAsGroup(InComponent->bLightAttachmentsAsGroup)
,	bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer(false)
,	bAlwaysHasVelocity(false)
,	bUseAsOccluder(InComponent->bUseAsOccluder)
,	bAllowApproximateOcclusion(InComponent->Mobility != EComponentMobility::Movable)
,	bSelectable(InComponent->bSelectable)
,	bAllowShadowFade(InComponent->bAllowShadowFade)
,	bHasPerInstanceHitProxies(InComponent->bHasPerInstanceHitProxies)
,	bUseEditorCompositing(InComponent->bUseEditorCompositing)
,	LpvBiasMultiplier(InComponent->LpvBiasMultiplier)
,	Scene(InComponent->GetScene())
,	PrimitiveComponentId(InComponent->ComponentId)
,	PrimitiveSceneInfo(NULL)
,	OwnerName(InComponent->GetOwner() ? InComponent->GetOwner()->GetFName() : NAME_None)
,	ResourceName(InResourceName)
,	LevelName(InComponent->GetOutermost()->GetFName())
#if WITH_EDITOR
// by default we are always drawn
,	HiddenEditorViews(0)
#endif
,	TranslucencySortPriority(FMath::Clamp(InComponent->TranslucencySortPriority, SHRT_MIN, SHRT_MAX))
,	VisibilityId(InComponent->VisibilityId)
,	StatId()
,	MaxDrawDistance(InComponent->CachedMaxDrawDistance > 0 ? InComponent->CachedMaxDrawDistance : FLT_MAX)
,	MinDrawDistance(InComponent->MinDrawDistance)
,	ComponentForDebuggingOnly(InComponent)
#if WITH_EDITOR
,	NumUncachedStaticLightingInteractions(0)
#endif
{
#if STATS
	{
		UObject const* StatObject = InComponent->AdditionalStatObject(); // prefer the additional object, this is usually the thing related to the component
		if (!StatObject)
		{
			StatObject = InComponent;
		}
		StatId = StatObject->GetStatID(true);
	}
#endif

	// Initialize the uniform buffer resource.
	BeginInitResource(&UniformBuffer);

	if (bNeedsUnbuiltPreviewLighting && !bHasValidSettingsForStaticLighting)
	{
		// Don't use unbuilt preview lighting for static components that have an invalid lightmap UV setup
		// Otherwise they would light differently in editor and in game, even after a lighting rebuild
		bNeedsUnbuiltPreviewLighting = false;
	}

	bStatic = InComponent->Mobility == EComponentMobility::Static;
	bOftenMoving = InComponent->Mobility == EComponentMobility::Movable;

	if(InComponent->GetOwner())
	{
		DrawInGame &= !(InComponent->GetOwner()->bHidden);
		#if WITH_EDITOR
			DrawInEditor &= !InComponent->GetOwner()->IsHiddenEd();
		#endif

		if(bOnlyOwnerSee || bOwnerNoSee || bUseViewOwnerDepthPriorityGroup)
		{
			// Make a list of the actors which directly or indirectly own the component.
			for(const AActor* Owner = InComponent->GetOwner();Owner;Owner = Owner->GetOwner())
			{
				Owners.Add(Owner);
			}
		}

#if WITH_EDITOR
		// cache the actor's group membership
		HiddenEditorViews = InComponent->GetOwner()->HiddenEditorViews;
#endif
	}

#if WITH_EDITOR
	if (GIsEditor && InComponent)
	{
		LightMapResolutionScale = FVector2D(0.0f, 0.0f);
		LightMapType = LMIT_None;
		bLightMapResolutionPadded = false;
	}
#endif
	
	// 
	// Flag components to render only after level will be fully added to the world
	//
	ULevel* ComponentLevel = InComponent->GetComponentLevel();
	if (ComponentLevel && ComponentLevel->bRequireFullVisibilityToRender)
	{
		bRequiresVisibleLevelToRender = true;
		bIsComponentLevelVisible = ComponentLevel->bIsVisible;
	}
	else
	{
		bRequiresVisibleLevelToRender = false;
	}
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
	check(IsInRenderingThread());
	UniformBuffer.ReleaseResource();
}

HHitProxy* FPrimitiveSceneProxy::CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	if(Component->GetOwner())
	{
		HHitProxy* ActorHitProxy;

		if (Component->GetOwner()->IsA(ABrush::StaticClass()) && Component->IsA(UBrushComponent::StaticClass()))
		{
			ActorHitProxy = new HActor(Component->GetOwner(), Component, HPP_Wireframe);
		}
		else
		{
			ActorHitProxy = new HActor(Component->GetOwner(), Component);
		}
		OutHitProxies.Add(ActorHitProxy);
		return ActorHitProxy;
	}
	else
	{
		return NULL;
	}
}

FPrimitiveViewRelevance FPrimitiveSceneProxy::GetViewRelevance(const FSceneView* View)
{
	return FPrimitiveViewRelevance();
}

void FPrimitiveSceneProxy::UpdateActorPosition(FVector InActorPosition)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateActorPosition,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		FVector,InActorPosition,InActorPosition,
	{
		if (PrimitiveSceneProxy->ActorPosition != InActorPosition)
		{
			PrimitiveSceneProxy->ActorPosition = InActorPosition;
			// Update the uniform shader parameters.
			const FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters =
			GetPrimitiveUniformShaderParameters(PrimitiveSceneProxy->LocalToWorld, InActorPosition, PrimitiveSceneProxy->Bounds,PrimitiveSceneProxy->LocalBounds, PrimitiveSceneProxy->bReceivesDecals, PrimitiveSceneProxy->GetLpvBiasMultiplier() );

			PrimitiveSceneProxy->UniformBuffer.SetContents(PrimitiveUniformShaderParameters);
			PrimitiveSceneProxy->OnActorPositionChanged();
		}
	});
}

void FPrimitiveSceneProxy::SetTransform(const FMatrix& InLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, FVector InActorPosition)
{
	check(IsInRenderingThread());

	// Update the cached transforms.
	LocalToWorld = InLocalToWorld;
	bIsLocalToWorldDeterminantNegative = LocalToWorld.Determinant() < 0.0f;

	// Update the cached bounds.
	Bounds = InBounds;
	LocalBounds = InLocalBounds;
	ActorPosition = InActorPosition;
	
	// Update the uniform shader parameters.
	const FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(LocalToWorld, ActorPosition, Bounds, LocalBounds, bReceivesDecals, LpvBiasMultiplier );
	UniformBuffer.SetContents(PrimitiveUniformShaderParameters);
	
	// Notify the proxy's implementation of the change.
	OnTransformChanged();
}

void FPrimitiveSceneProxy::ApplyWorldOffset(FVector InOffset)
{
	FBoxSphereBounds NewBounds = FBoxSphereBounds(Bounds.Origin + InOffset, Bounds.BoxExtent, Bounds.SphereRadius);
	FBoxSphereBounds NewLocalBounds = LocalBounds;
	FVector NewActorPosition = ActorPosition + InOffset;
	FMatrix NewLocalToWorld = LocalToWorld.ConcatTranslation(InOffset);
	
	SetTransform(NewLocalToWorld, NewBounds, NewLocalBounds, NewActorPosition);
}

/**
 * Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread.
 * @param bInSelected - true if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_RenderThread(const bool bInSelected)
{
	check(IsInRenderingThread());
	bSelected = bInSelected;
}

/**
 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
 * This is called in the game thread as selection is toggled.
 * @param bInSelected - true if the parent actor is selected in the editor
 */
void FPrimitiveSceneProxy::SetSelection_GameThread(const bool bInSelected)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetNewSelection,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const bool,bNewSelection,bInSelected,
	{
		PrimitiveSceneProxy->SetSelection_RenderThread(bNewSelection);
	});
}


/**
 * Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread.
 * @param bInHovered - true if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_RenderThread(const bool bInHovered)
{
	check(IsInRenderingThread());
	bHovered = bInHovered;
}

/**
 * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
 * This is called in the game thread as hover state changes
 * @param bInHovered - true if the parent actor is hovered
 */
void FPrimitiveSceneProxy::SetHovered_GameThread(const bool bInHovered)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread containing the interaction to add.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetNewHovered,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const bool,bNewHovered,bInHovered,
	{
		PrimitiveSceneProxy->SetHovered_RenderThread(bNewHovered);
	});
}

/**
 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_GameThread( uint64 InHiddenEditorViews )
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetEditorVisibility,
		FPrimitiveSceneProxy*,PrimitiveSceneProxy,this,
		const uint64,NewHiddenEditorViews,InHiddenEditorViews,
	{
		PrimitiveSceneProxy->SetHiddenEdViews_RenderThread(NewHiddenEditorViews);
	});
}

/**
 * Updates the hidden editor view visibility map on the render thread 
 */
void FPrimitiveSceneProxy::SetHiddenEdViews_RenderThread( uint64 InHiddenEditorViews )
{
#if WITH_EDITOR
	check(IsInRenderingThread());
	HiddenEditorViews = InHiddenEditorViews;
#endif
}

void FPrimitiveSceneProxy::SetCollisionEnabled_GameThread(const bool bNewEnabled)
{
	check(IsInGameThread());

	// Enqueue a message to the rendering thread to change draw state
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetCollisionEnabled,
		FPrimitiveSceneProxy*,PrimSceneProxy,this,
		const bool,bEnabled,bNewEnabled,
	{
		PrimSceneProxy->SetCollisionEnabled_RenderThread(bEnabled);
	});
}

void FPrimitiveSceneProxy::SetCollisionEnabled_RenderThread(const bool bNewEnabled)
{
	check(IsInRenderingThread());
	bCollisionEnabled = bNewEnabled;
}

/** @return True if the primitive is visible in the given View. */
bool FPrimitiveSceneProxy::IsShown(const FSceneView* View) const
{
#if WITH_EDITOR
	if(View->Family->EngineShowFlags.Editor)
	{
		if(!DrawInEditor)
		{
			return false;
		}

		// if all of it's groups are hidden in this view, don't draw
		if ((HiddenEditorViews & View->EditorViewBitflag) != 0)
		{
			return false;
		}

		// If we are in a collision view, hide anything which doesn't have collision enabled
		const bool bCollisionView = (View->Family->EngineShowFlags.CollisionVisibility || View->Family->EngineShowFlags.CollisionPawn);
		if(bCollisionView && !IsCollisionEnabled())
		{
			return false;
		}
	}
	else
#endif
	{

		if(!DrawInGame
#if WITH_EDITOR
			|| (!View->bIsGameView && View->Family->EngineShowFlags.Game && !DrawInEditor)	// ..."G" mode in editor viewport. covers the case when the primitive must be rendered for the voxelization pass, but the user has chosen to hide the primitive from view.
#endif
			)
		{
			return false;
		}

		// if primitive requires component level to be visible
		if (bRequiresVisibleLevelToRender && !bIsComponentLevelVisible)
		{
			return false;
		}

		if(bOnlyOwnerSee && !Owners.Contains(View->ViewActor))
		{
			return false;
		}

		if(bOwnerNoSee && Owners.Contains(View->ViewActor))
		{
			return false;
		}
	}

	return true;
}

/** @return True if the primitive is casting a shadow. */
bool FPrimitiveSceneProxy::IsShadowCast(const FSceneView* View) const
{
	check(PrimitiveSceneInfo);

	if (!CastsStaticShadow() && !CastsDynamicShadow())
	{
		return false;
	}

	if(!CastsHiddenShadow())
	{
		// Primitives that are hidden in the game don't cast a shadow.
		if (!DrawInGame)
		{
			return false;
		}
		
		if (View->HiddenPrimitives.Contains(PrimitiveComponentId))
		{
			return false;
		}

#if WITH_EDITOR
		// For editor views, we use a show flag to determine whether shadows from editor-hidden actors are desired.
		if(View->Family->EngineShowFlags.Editor && !View->Family->EngineShowFlags.ShadowsFromEditorHiddenActors)
		{
			if(!DrawInEditor)
			{
				return false;
			}
		
			// if all of it's groups are hidden in this view, don't draw
			if ((HiddenEditorViews & View->EditorViewBitflag) != 0)
			{
				return false;
			}
		}
#endif	//#if WITH_EDITOR

		// In the OwnerSee cases, we still want to respect hidden shadows...
		// This assumes that bCastHiddenShadow trumps the owner see flags.
		if(bOnlyOwnerSee && !Owners.Contains(View->ViewActor))
		{
			return false;
		}

		if(bOwnerNoSee && Owners.Contains(View->ViewActor))
		{
			return false;
		}
	}

	return true;
}

void FPrimitiveSceneProxy::RenderBounds(
	FPrimitiveDrawInterface* PDI, 
	const FEngineShowFlags& EngineShowFlags, 
	const FBoxSphereBounds& Bounds, 
	bool bRenderInEditor) const
{
	const ESceneDepthPriorityGroup DrawBoundsDPG = EngineShowFlags.Game ? SDPG_World : SDPG_Foreground;
	if (EngineShowFlags.Bounds && (EngineShowFlags.Game || bRenderInEditor))
	{
		// Draw the static mesh's bounding box and sphere.
		DrawWireBox(PDI,Bounds.GetBox(), FColor(72,72,255),DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(1,0,0),FVector(0,1,0),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(1,0,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
		DrawCircle(PDI,Bounds.Origin,FVector(0,1,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32,DrawBoundsDPG);
	}
}

void FPrimitiveSceneProxy::DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color, uint8 DepthPriorityGroup, const float Thickness, const bool bScreenSpace)
{
	if (Segments == 0)
	{
		return;
	}

	const float ARC_PTS_SCALE = 1.0f / (float)Segments;

	const float X0 = Start.X;
	const float Y0 = Start.Y;
	const float Z0 = Start.Z;
	const float Dx = End.X - X0;
	const float Dy = End.Y - Y0;
	const float Dz = End.Z - Z0;
	const float Length = FMath::Sqrt(Dx*Dx + Dy*Dy + Dz*Dz);
	float Px = X0, Py = Y0, Pz = Z0;
	for (uint32 i = 1; i <= Segments; ++i)
	{
		const float U = i * ARC_PTS_SCALE;
		const float X = X0 + Dx * U;
		const float Y = Y0 + Dy * U;
		const float Z = Z0 + Dz * U + (Length*Height) * (1-(U*2-1)*(U*2-1));

		PDI->DrawLine( FVector(Px, Py, Pz), FVector(X, Y, Z), Color, SDPG_World, Thickness, bScreenSpace);

		Px = X; Py = Y; Pz = Z;
	}
}

void FPrimitiveSceneProxy::DrawArrowHead(FPrimitiveDrawInterface* PDI, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color, uint8 DepthPriorityGroup, const float Thickness, const bool bScreenSpace)
{
	//float ax[3], ay[3] = {0,1,0}, az[3];
	FVector Ax, Ay, Az(0,1,0);
	// dtVsub(az, q, p);
	Ay = Origin - Tip;
	// dtVnormalize(az);
	Ay.Normalize();
	// dtVcross(ax, ay, az);
	Ax = FVector::CrossProduct(Az, Ay);
	// dtVcross(ay, az, ax);
	//Az = FVector::CrossProduct(Ay, Ax);
	////dtVnormalize(ay);
	//Az.Normalize();

	PDI->DrawLine( Tip
		//, FVector(p[0]+az[0]*s+ax[0]*s/3, p[1]+az[1]*s+ax[1]*s/3, p[2]+az[2]*s+ax[2]*s/3)
		, FVector(Tip.X + Ay.X*Size + Ax.X*Size/3, Tip.Y + Ay.Y*Size + Ax.Y*Size/3, Tip.Z + Ay.Z*Size + Ax.Z*Size/3)
		, Color, SDPG_World, Thickness, bScreenSpace);

	PDI->DrawLine( Tip
		//, FVector(p[0]+az[0]*s-ax[0]*s/3, p[1]+az[1]*s-ax[1]*s/3, p[2]+az[2]*s-ax[2]*s/3)
		, FVector(Tip.X + Ay.X*Size - Ax.X*Size/3, Tip.Y + Ay.Y*Size - Ax.Y*Size/3, Tip.Z + Ay.Z*Size - Ax.Z*Size/3)
		, Color, SDPG_World, Thickness, bScreenSpace);
}
