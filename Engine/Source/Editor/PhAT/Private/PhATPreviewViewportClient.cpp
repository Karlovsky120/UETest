// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "PhATEdSkeletalMeshComponent.h"
#include "PhAT.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "PhATHitProxies.h"
#include "PhATActions.h"
#include "PhATSharedData.h"
#include "PhATPreviewViewportClient.h"
#include "SPhATPreviewViewport.h"


FPhATEdPreviewViewportClient::FPhATEdPreviewViewportClient(TWeakPtr<FPhAT> InPhAT, TSharedPtr<FPhATSharedData> Data)
	: FEditorViewportClient(&Data->PreviewScene)
	, PhATPtr(InPhAT)
	, SharedData(Data)
	, MinPrimSize(0.5f)
	, PhAT_TranslateSpeed(0.25f)
	, PhAT_RotateSpeed(1.0 * (PI / 180.0))
	, PhAT_LightRotSpeed(0.22f)
	, SimGrabCheckDistance(5000.0f)
	, SimHoldDistanceChangeDelta(20.0f)
	, SimMinHoldDistance(10.0f)
	, SimGrabMoveSpeed(1.0f)
{
	check(PhATPtr.IsValid());

	bAllowedToMoveCamera = true;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = 32767;

	PhATFont = GEngine->GetSmallFont();
	check(PhATFont);

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.CompositeEditorPrimitives = true;

	// Get actors asset collision bounding box, and move actor so its not intersection the floor plane at Z = 0.
	FBox CollBox = SharedData->PhysicsAsset->CalcAABB(SharedData->EditorSkelComp);	
	FVector SkelCompLocation = FVector(0, 0, -CollBox.Min.Z + SharedData->EditorSimOptions->FloorGap);

	SharedData->EditorSkelComp->SetAbsolute(true, true, true);
	SharedData->EditorSkelComp->SetRelativeLocation(SkelCompLocation);
	SharedData->ResetTM = SharedData->EditorSkelComp->GetComponentToWorld();

	// Get new bounding box and set view based on that.
	CollBox = SharedData->PhysicsAsset->CalcAABB(SharedData->EditorSkelComp);	
	FVector CollBoxExtent = CollBox.GetExtent();

	// Take into account internal mesh translation/rotation/scaling etc.
	FTransform LocalToWorld = SharedData->EditorSkelComp->ComponentToWorld;
	FSphere WorldSphere = SharedData->EditorSkelMesh->Bounds.GetSphere().TransformBy(LocalToWorld);

	CollBoxExtent = CollBox.GetExtent();
	if (CollBoxExtent.X > CollBoxExtent.Y)
	{
		SetViewLocation( FVector(WorldSphere.Center.X, WorldSphere.Center.Y - 1.5*WorldSphere.W, WorldSphere.Center.Z) );
		SetViewRotation( FRotator(0,90.f,0) );	
	}
	else
	{
		SetViewLocation( FVector(WorldSphere.Center.X - 1.5*WorldSphere.W, WorldSphere.Center.Y, WorldSphere.Center.Z) );
		SetViewRotation( FRotator::ZeroRotator );	
	}
	
	SetViewLocationForOrbiting(FVector::ZeroVector);

	SetViewMode(VMI_Lit);

	bUsingOrbitCamera = true;

	if (!FPhAT::IsPIERunning())
	{
		SetRealtime(true);
	}
}

FPhATEdPreviewViewportClient::~FPhATEdPreviewViewportClient()
{
}

void FPhATEdPreviewViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
	if (!PhATPtr.IsValid())
	{
		return;
	}

	// Turn on/off the ground box
	SharedData->EditorFloorComp->SetVisibility(SharedData->bDrawGround);

	float W, H;
	PhATFont->GetCharSize(TEXT('L'), W, H);

	const float XOffset = 150.0f;

	FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::GetEmpty(), PhATFont, FLinearColor::White );

	// Write body/constraint count at top.
	FString StatusString = FText::Format(
		NSLOCTEXT("UnrealEd", "BodiesConstraints_F", "{0} BODIES  {1} CONSIDERED_FOR_BOUNDS  {2} Ratio  {3} CONSTRAINTS"),
		FText::AsNumber(SharedData->PhysicsAsset->BodySetup.Num()),
		FText::AsNumber(SharedData->PhysicsAsset->BoundsBodies.Num()),
		FText::AsNumber(static_cast<float>(SharedData->PhysicsAsset->BoundsBodies.Num())/static_cast<float>(SharedData->PhysicsAsset->BodySetup.Num())),
		FText::AsNumber(SharedData->PhysicsAsset->ConstraintSetup.Num()) ).ToString();

	TextItem.Text = FText::FromString( StatusString );
	Canvas.DrawItem( TextItem, XOffset, 3 );
	
	TextItem.Text = FText::GetEmpty();
	if (SharedData->bRunningSimulation)
	{
		TextItem.Text = NSLOCTEXT("UnrealEd", "Sim", "SIM: Ctrl+RightMouse to interact with bodies");
	}
	else if (SharedData->bSelectionLock)
	{
		TextItem.Text = NSLOCTEXT("UnrealEd", "Lock", "LOCK");
	}else if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
	{
		if(SharedData->WidgetMode == FWidget::WM_Translate)
		{
			TextItem.Text = NSLOCTEXT("UnrealEd", "SingleMove", "hold ALT to move a single reference frame");
		}else if(SharedData->WidgetMode == FWidget::WM_Rotate)
		{
			TextItem.Text = NSLOCTEXT("UnrealEd", "DoubleRotate", "hold ALT to rotate both reference frames");
		}
	}

	Canvas.DrawItem( TextItem,  XOffset, Viewport->GetSizeXY().Y - (3 + H) );

	// Draw current physics weight
	if (SharedData->bRunningSimulation)
	{
		FString PhysWeightString = FString::Printf(TEXT("Phys Blend: %3.0f pct"), SharedData->EditorSimOptions->PhysicsBlend * 100.f);
		int32 PWLW, PWLH;
		StringSize(PhATFont, PWLW, PWLH, *PhysWeightString);
		TextItem.Text = FText::FromString(PhysWeightString);
		Canvas.DrawItem( TextItem, Viewport->GetSizeXY().X - (3 + PWLW + 2*W), Viewport->GetSizeXY().Y - (3 + H) );
	}

	int32 HalfX = (Viewport->GetSizeXY().X-XOffset)/2;
	int32 HalfY = Viewport->GetSizeXY().Y/2;

	if ((SharedData->bShowHierarchy && SharedData->EditorSimOptions->bShowNamesInHierarchy))
	{
		// Iterate over each graphics bone.
		for (int32 i = 0; i <SharedData->EditorSkelComp->SpaceBases.Num(); ++i)
		{
			FVector BonePos = SharedData->EditorSkelComp->ComponentToWorld.TransformPosition(SharedData->EditorSkelComp->SpaceBases[i].GetLocation());

			FPlane proj = View.Project(BonePos);
			if (proj.W > 0.f) // This avoids drawing bone names that are behind us.
			{
				int32 XPos = HalfX + (HalfX * proj.X);
				int32 YPos = HalfY + (HalfY * (proj.Y * -1));

				FName BoneName = SharedData->EditorSkelMesh->RefSkeleton.GetBoneName(i);

				FColor BoneNameColor = FColor(255,255,255);
				//iterate through selected bones and see if any match
				for(int32 j=0; j< SharedData->SelectedBodies.Num(); ++j)
				{
					int32 SelectedBodyIndex = SharedData->SelectedBodies[j].Index;
					FName SelectedBoneName = SharedData->PhysicsAsset->BodySetup[SelectedBodyIndex]->BoneName;
					if(SelectedBoneName == BoneName)
					{
						BoneNameColor = FColor(0,255,0);
						break;
					}

				}
				
				if (Canvas.IsHitTesting()) 
				{
					Canvas.SetHitProxy(new HPhATEdBoneNameProxy(i));
				}
				
				TextItem.Text = FText::FromString(BoneName.ToString());
				TextItem.SetColor(BoneNameColor);
				Canvas.DrawItem( TextItem,  XPos, YPos );
				
				if (Canvas.IsHitTesting())
				{
					Canvas.SetHitProxy(NULL);
				}
			}
		}
	}

	// If showing center-of-mass, and physics is started up..
	if (SharedData->bShowCOM)
	{
		// iterate over each bone
		for (int32 i = 0; i <SharedData->EditorSkelComp->Bodies.Num(); ++i)
		{
			FBodyInstance* BodyInst = SharedData->EditorSkelComp->Bodies[i];
			check(BodyInst);

			FVector BodyCOMPos = BodyInst->GetCOMPosition();
			float BodyMass = BodyInst->GetBodyMass();

			FPlane Projection = View.Project(BodyCOMPos);
			if (Projection.W > 0.f) // This avoids drawing bone names that are behind us.
			{
				int32 XPos = HalfX + (HalfX * Projection.X);
				int32 YPos = HalfY + (HalfY * (Projection.Y * -1));

				FString COMString = FString::Printf(TEXT("%3.3f"), BodyMass);
				TextItem.Text = FText::FromString(COMString);
				TextItem.SetColor(SharedData->COMRenderColor);
				Canvas.DrawItem( TextItem,  XPos, YPos );				
			}
		}
	}
}

void FPhATEdPreviewViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View,PDI);

	FPhATSharedData::EPhATRenderMode MeshViewMode = SharedData->GetCurrentMeshViewMode();

	if (MeshViewMode != FPhATSharedData::PRM_None)
	{
		SharedData->EditorSkelComp->SetVisibility(true);

		if (MeshViewMode == FPhATSharedData::PRM_Wireframe)
		{
			SharedData->EditorSkelComp->SetForceWireframe(true);
		}
		else
		{
			SharedData->EditorSkelComp->SetForceWireframe(false);
		}
	}
	else
	{
		SharedData->EditorSkelComp->SetVisibility(false);
	}

	// Draw phat skeletal component.
	if (PDI->IsHitTesting())
	{
		SharedData->EditorSkelComp->RenderHitTest(View, PDI);
	}
	else
	{
		SharedData->EditorSkelComp->Render(View, PDI);
	}
}

bool FPhATEdPreviewViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool Gamepad)
{
	int32 HitX = Viewport->GetMouseX();
	int32 HitY = Viewport->GetMouseY();
	bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

	bool bHandled = false;
	if (SharedData->bRunningSimulation)
	{
		if (Key == EKeys::RightMouseButton || Key == EKeys::LeftMouseButton)
		{
			if (Event == IE_Pressed)
			{
				if (bShiftDown)
				{
					bHandled = true;
					SimMousePress(Viewport, false, Key);
					bAllowedToMoveCamera = false;
				}
				else if (bCtrlDown)
				{
					bHandled = true;
					SimMousePress(Viewport, true, Key);
					bAllowedToMoveCamera = false;
				}
			}
			else if (Event == IE_Released)
			{
				bHandled = true;
				SimMouseRelease();
				bAllowedToMoveCamera = true;
			}
			UpdateAndApplyCursorVisibility();
		}
		else if (Key == EKeys::MouseScrollUp)
		{
			bHandled = true;
			SimMouseWheelUp();
		}
		else if (Key == EKeys::MouseScrollDown)
		{
			bHandled = true;
			SimMouseWheelDown();
		}
		else if(IsFlightCameraActive())
		{
			// If the flight camera is active (user is looking or moving around the scene)
			// consume the event so hotkeys don't fire.
			bHandled = true;
		}
	}

	if( !bHandled )
	{
		bHandled = FEditorViewportClient::InputKey( Viewport, ControllerId, Key, Event, AmountDepressed, Gamepad );
	}

	if( bHandled )
	{
		Invalidate();
	}

	return bHandled;
}

bool FPhATEdPreviewViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	bool bHandled = false;
	// If we are 'manipulating' don't move the camera but do something else with mouse input.
	if (SharedData->bManipulating)
	{
		bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);

		if (SharedData->bRunningSimulation)
		{
			if (Key == EKeys::MouseX)
			{
				SimMouseMove(Delta, 0.0f);
			}
			else if (Key == EKeys::MouseY)
			{
				SimMouseMove(0.0f, Delta);
			}
			bHandled = true;
		}
	}

	if( !bHandled )
	{
		bHandled = FEditorViewportClient::InputAxis(Viewport,ControllerId,Key,Delta,DeltaTime,NumSamples,bGamepad);
	}

	Viewport->Invalidate();

	return bHandled;
}

void FPhATEdPreviewViewportClient::ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	if( Key == EKeys::LeftMouseButton )
	{
		if (HitProxy && HitProxy->IsA(HPhATEdBoneProxy::StaticGetType()))
		{
			HPhATEdBoneProxy* BoneProxy = (HPhATEdBoneProxy*)HitProxy;

			SharedData->HitBone(BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex, IsCtrlPressed());
		}
		else if (HitProxy && HitProxy->IsA(HPhATEdConstraintProxy::StaticGetType()))
		{
			HPhATEdConstraintProxy* ConstraintProxy = (HPhATEdConstraintProxy*)HitProxy;

			SharedData->HitConstraint(ConstraintProxy->ConstraintIndex, IsCtrlPressed());
		}
		else
		{	
			HitNothing();
		}
	}
	else if( Key == EKeys::RightMouseButton )
	{
		if (HitProxy)
		{
			if (HitProxy->IsA(HPhATEdBoneProxy::StaticGetType()) && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
			{
				HPhATEdBoneProxy* BoneProxy = (HPhATEdBoneProxy*)HitProxy;

				bool bAlreadySelected = false;
				
				//find out if body is already selected
				for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
				{
					if(SharedData->SelectedBodies[i].Index == BoneProxy->BodyIndex && SharedData->SelectedBodies[i].PrimitiveType == BoneProxy->PrimType && SharedData->SelectedBodies[i].PrimitiveIndex == BoneProxy->PrimIndex)
					{
						bAlreadySelected = true;
						break;
					}
				}

				// Select body under cursor if not already selected	(if ctrl is held down we only add, not remove)
				if (!bAlreadySelected)
				{
					FPhATSharedData::FSelection Selection(BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex);
					SharedData->SetSelectedBody(&Selection, IsCtrlPressed());
				}

				// Pop up menu, if we have a body selected.
				if (SharedData->GetSelectedBody())
				{
					OpenBodyMenu();
				}
			}
			else if (HitProxy->IsA(HPhATEdConstraintProxy::StaticGetType()) && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
			{
				HPhATEdConstraintProxy* ConstraintProxy = (HPhATEdConstraintProxy*)HitProxy;

				bool bAlreadySelected = false;

				//find out if constraint is already selected
				for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
				{
					if(SharedData->SelectedConstraints[i].Index == ConstraintProxy->ConstraintIndex)
					{
						bAlreadySelected = true;
						break;
					}
				}

				// Select constraint under cursor if not already selected (if ctrl is held down we only add, not remove)
				if (!bAlreadySelected)
				{
					SharedData->SetSelectedConstraint(ConstraintProxy->ConstraintIndex, IsCtrlPressed());
				}

				// Pop up menu, if we have a constraint selected.
				if (SharedData->GetSelectedConstraint())
				{
					OpenConstraintMenu();
				}
			}
		}
	}
}

bool FPhATEdPreviewViewportClient::InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale )
{
	bool bHandled = false;
	TArray<FPhATSharedData::FSelection> & SelectedObjects = SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit ? SharedData->SelectedBodies : SharedData->SelectedConstraints;

	for(int32 i=0; i<SelectedObjects.Num(); ++i)
	{
		FPhATSharedData::FSelection & SelectedObject = SelectedObjects[i];
		if( SharedData->bManipulating )
		{
			if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit) /// BODY EDITING ///
			{
				int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[SelectedObject.Index]->BoneName);

				FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
				BoneTM.RemoveScaling();

				SelectedObject.WidgetTM = SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SelectedObject.Index, SelectedObject.PrimitiveType, SelectedObject.PrimitiveIndex, 1.f );
			}
			else  /// CONSTRAINT EDITING ///
			{
				SelectedObject.WidgetTM = SharedData->GetConstraintMatrix(SelectedObject.Index, EConstraintFrame::Frame2, 1.f);
			}

			if ( SharedData->WidgetMode == FWidget::WM_Translate )
			{
				FVector Dir = SelectedObject.WidgetTM.InverseTransformVector( Drag.SafeNormal() );
				FVector DragVec = Dir * Drag.Size();
				SelectedObject.ManipulateTM.AddToTranslation( DragVec );
			}
			else if ( SharedData->WidgetMode == FWidget::WM_Rotate )
			{
				FVector Axis; 
				float Angle;
				Rot.Quaternion().ToAxisAndAngle(Axis, Angle);
		
				Axis = SelectedObject.WidgetTM.InverseTransformVector( Axis );
		
				const FQuat Start = SelectedObject.ManipulateTM.GetRotation();
				const FQuat Delta = FQuat( Axis, Angle );
				const FQuat Result = Delta * Start;

				SelectedObject.ManipulateTM = FTransform( Result );
			}
			else if ( SharedData->WidgetMode == FWidget::WM_Scale && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit) // Scaling only valid for bodies.
			{
				ModifyPrimitiveSize(SelectedObject.Index, SelectedObject.PrimitiveType, SelectedObject.PrimitiveIndex, Scale );
			}

			if (SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
			{
				UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SelectedObject.Index];

				ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame2, SelectedObject.ManipulateTM * StartManParentConTM);

				//Rotation by default only rotates one frame, but translation by default moves both
				bool bMultiFrame = (IsAltPressed() && SharedData->WidgetMode == FWidget::WM_Rotate) || (!IsAltPressed() && SharedData->WidgetMode == FWidget::WM_Translate);
				
				if (bMultiFrame)
				{
					SharedData->SetSelectedConstraintRelTM(StartManRelConTM);
				}
				else
				{
					ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame1, FTransform(StartManChildConTM));
				}
			}

			bHandled = true;
		}
	}

	return bHandled;
}

void FPhATEdPreviewViewportClient::TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	// If releasing the mouse button, check we are done manipulating
	if ( bIsDraggingWidget  )
	{
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
		FSceneView* View = CalcSceneView(&ViewFamily);
		
		const int32	HitX = InInputState.GetViewport()->GetMouseX();
		const int32	HitY = InInputState.GetViewport()->GetMouseY();
		
		StartManipulating(Widget->GetCurrentAxis(), FViewportClick(View, this, InInputState.GetKey(), InInputState.GetInputEvent(), HitX, HitY), View->ViewMatrices.ViewMatrix);

		// If we are manipulating, don't move the camera as we drag now.
		if (SharedData->bManipulating)
		{
			bAllowedToMoveCamera = false;
		}
	}
}

void FPhATEdPreviewViewportClient::TrackingStopped()
{
	if (SharedData->bManipulating)
	{
		EndManipulating();
		bAllowedToMoveCamera = true;
	}
}

FWidget::EWidgetMode FPhATEdPreviewViewportClient::GetWidgetMode() const
{
	FWidget::EWidgetMode WidgetMode = FWidget::WM_None;

	if( SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit && SharedData->GetSelectedBody()) 
	{
		WidgetMode = SharedData->WidgetMode;
	}
	else if( 
		SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit 
		&& SharedData->GetSelectedConstraint()
		&& SharedData->WidgetMode != FWidget::WM_Scale )
	{
		WidgetMode = SharedData->WidgetMode;
	}

	return WidgetMode;
}

FVector FPhATEdPreviewViewportClient::GetWidgetLocation() const
{
	if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit) /// BODY EDITING ///
	{
		// Don't draw widget if nothing selected.
		if (!SharedData->GetSelectedBody())
		{
			return FVector::ZeroVector;
		}

		int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[SharedData->GetSelectedBody()->Index]->BoneName);

		FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
		BoneTM.RemoveScaling();

		return SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SharedData->GetSelectedBody()->Index, SharedData->GetSelectedBody()->PrimitiveType, SharedData->GetSelectedBody()->PrimitiveIndex, 1.f).GetTranslation();
	}
	else  /// CONSTRAINT EDITING ///
	{
		if (!SharedData->GetSelectedConstraint())
		{
			return FVector::ZeroVector;
		}

		return SharedData->GetConstraintMatrix(SharedData->GetSelectedConstraint()->Index, EConstraintFrame::Frame2, 1.f).GetTranslation();
	}
}

FMatrix FPhATEdPreviewViewportClient::GetWidgetCoordSystem() const
{
	if( GetWidgetCoordSystemSpace() == COORD_Local )
	{
		if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit) /// BODY EDITING ///
		{
			// Don't draw widget if nothing selected.
			if (!SharedData->GetSelectedBody())
			{
				return FMatrix::Identity;
			}

			int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[SharedData->GetSelectedBody()->Index]->BoneName);

			FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
			BoneTM.RemoveScaling();

			return SharedData->EditorSkelComp->GetPrimitiveTransform(BoneTM, SharedData->GetSelectedBody()->Index, SharedData->GetSelectedBody()->PrimitiveType, SharedData->GetSelectedBody()->PrimitiveIndex, 1.f).ToMatrixNoScale().RemoveTranslation();
		}
		else  /// CONSTRAINT EDITING ///
		{
			if (!SharedData->GetSelectedConstraint())
			{
				return FMatrix::Identity;
			}

			return SharedData->GetConstraintMatrix(SharedData->GetSelectedConstraint()->Index, EConstraintFrame::Frame2, 1.f).ToMatrixNoScale().RemoveTranslation();
		}
	}
	else
	{
		return FMatrix::Identity;
	}
}


ECoordSystem FPhATEdPreviewViewportClient::GetWidgetCoordSystemSpace() const
{
	return SharedData->WidgetMode == FWidget::WM_Scale ? COORD_Local : SharedData->MovementSpace;
}

void FPhATEdPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	if (SharedData->bRunningSimulation)
	{
		// check if PIE disabled the realtime viewport and quit sim if so
		if (!bIsRealtime)
		{
			SharedData->ToggleSimulation();
			
			Invalidate();
		}

		UWorld* World = SharedData->PreviewScene.GetWorld();
		AWorldSettings* Setting = World->GetWorldSettings();
		Setting->WorldGravityZ = SharedData->bNoGravitySimulation ? 0.0f : UPhysicsSettings::Get()->DefaultGravityZ*SharedData->EditorSimOptions->GravScale;
		Setting->bWorldGravitySet = true;

		// We back up the transforms array now
		SharedData->EditorSkelComp->AnimationSpaceBases = SharedData->EditorSkelComp->SpaceBases;
		SharedData->EditorSkelComp->SetPhysicsBlendWeight(SharedData->EditorSimOptions->PhysicsBlend);
	}
}

FSceneInterface* FPhATEdPreviewViewportClient::GetScene() const
{
	return SharedData->PreviewScene.GetScene();
}

FLinearColor FPhATEdPreviewViewportClient::GetBackgroundColor() const
{
	return FColor(64,64,64);
}

void FPhATEdPreviewViewportClient::OpenBodyMenu()
{
	TSharedPtr<SWidget> MenuWidget = PhATPtr.Pin()->BuildMenuWidgetBody();
	TSharedPtr< SPhATPreviewViewport > ParentWidget = PhATPtr.Pin()->GetPreviewViewportWidget();

	if ( MenuWidget.IsValid() && ParentWidget.IsValid() )
	{
		const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();

		FSlateApplication::Get().PushMenu(
			ParentWidget.ToSharedRef(),
			MenuWidget.ToSharedRef(),
			MouseCursorLocation,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

void FPhATEdPreviewViewportClient::OpenConstraintMenu()
{
	TSharedPtr<SWidget> MenuWidget = PhATPtr.Pin()->BuildMenuWidgetConstraint();
	TSharedPtr< SPhATPreviewViewport > ParentWidget = PhATPtr.Pin()->GetPreviewViewportWidget();

	if ( MenuWidget.IsValid() && ParentWidget.IsValid() )
	{
		const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();

		FSlateApplication::Get().PushMenu(
			ParentWidget.ToSharedRef(),
			MenuWidget.ToSharedRef(),
			MouseCursorLocation,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

void FPhATEdPreviewViewportClient::StartManipulating(EAxisList::Type Axis, const FViewportClick& Click, const FMatrix& WorldToCamera)
{
	check(!SharedData->bManipulating);
	if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit && SharedData->SelectedBodies.Num())
	{
		GEditor->BeginTransaction( NSLOCTEXT("UnrealEd", "MoveElement", "Move Element") );
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index]->Modify();
			SharedData->SelectedBodies[i].ManipulateTM = FTransform::Identity;
		}

		SharedData->bManipulating = true;
	}
	else if( SharedData->GetSelectedConstraint())
	{
		GEditor->BeginTransaction( NSLOCTEXT("UnrealEd", "MoveConstraint", "Move Constraint") );
		for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
		{
			SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[i].Index]->Modify();
			SharedData->SelectedConstraints[i].ManipulateTM = FTransform::Identity;
		}

		const FTransform WParentFrame = SharedData->GetConstraintWorldTM(SharedData->GetSelectedConstraint(), EConstraintFrame::Frame2);
		const FTransform WChildFrame = SharedData->GetConstraintWorldTM(SharedData->GetSelectedConstraint(), EConstraintFrame::Frame1);
		StartManRelConTM = WChildFrame * WParentFrame.InverseSafe();

		UPhysicsConstraintTemplate* Setup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->GetSelectedConstraint()->Index];

		StartManParentConTM = Setup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2);
		StartManChildConTM = Setup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1);

		SharedData->bManipulating = true;
	}

}

void FPhATEdPreviewViewportClient::EndManipulating()
{
	if (SharedData->bManipulating)
	{
		SharedData->bManipulating = false;

			if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
			{
				
				for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
				{
					FPhATSharedData::FSelection & SelectedObject = SharedData->SelectedBodies[i];
					UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[SelectedObject.Index];

					FKAggregateGeom* AggGeom = &BodySetup->AggGeom;

					if (SelectedObject.PrimitiveType == KPT_Sphere)
					{
						AggGeom->SphereElems[SelectedObject.PrimitiveIndex].Center = (SelectedObject.ManipulateTM * AggGeom->SphereElems[SelectedObject.PrimitiveIndex].GetTransform() ).GetLocation();
					}
					else if (SelectedObject.PrimitiveType == KPT_Box)
					{
						AggGeom->BoxElems[SelectedObject.PrimitiveIndex].SetTransform(SelectedObject.ManipulateTM * AggGeom->BoxElems[SelectedObject.PrimitiveIndex].GetTransform() );
					}
					else if (SelectedObject.PrimitiveType == KPT_Sphyl)
					{
						AggGeom->SphylElems[SelectedObject.PrimitiveIndex].SetTransform(SelectedObject.ManipulateTM * AggGeom->SphylElems[SelectedObject.PrimitiveIndex].GetTransform() );
					}
					else if (SelectedObject.PrimitiveType == KPT_Convex)
					{
						FKConvexElem& Convex = AggGeom->ConvexElems[SelectedObject.PrimitiveIndex];
						Convex.SetTransform(SelectedObject.ManipulateTM * Convex.GetTransform() );
					}
			}
		}

		GEditor->EndTransaction();

		Viewport->Invalidate();
	}
}

void FPhATEdPreviewViewportClient::SimMousePress(FViewport* Viewport, bool bConstrainRotation, FKey Key)
{
	bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
	FSceneView* View = CalcSceneView(&ViewFamily);

	const FViewportClick Click(View, this, EKeys::Invalid, IE_Released, Viewport->GetMouseX(), Viewport->GetMouseY());
#if DEBUG_CLICK_VIEWPORT	
	SharedData->LastClickOrigin = Click.GetOrigin();
	SharedData->LastClickDirection = Click.GetDirection();
#endif
	SharedData->LastClickPos = Click.GetClickPos();
	FHitResult Result(1.f);
	bool bHit = SharedData->EditorSkelComp->LineTraceComponent(Result, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * SimGrabCheckDistance, FCollisionQueryParams(true));

	if (bHit)
	{
		check(Result.Item != INDEX_NONE);
		FName BoneName = SharedData->PhysicsAsset->BodySetup[Result.Item]->BoneName;

		//UE_LOG(LogPhysics, Warning, TEXT("Hit Bone Name (%s)"), *BoneName.ToString());

		// Right mouse is for dragging things around
		if (Key == EKeys::RightMouseButton)
		{
			SharedData->bManipulating = true;
			DragX = 0.0f;
			DragY = 0.0f;
			SimGrabPush = 0.0f;

			// Update mouse force properties from sim options.
			SharedData->MouseHandle->LinearDamping = SharedData->EditorSimOptions->HandleLinearDamping;
			SharedData->MouseHandle->LinearStiffness = SharedData->EditorSimOptions->HandleLinearStiffness;
			SharedData->MouseHandle->AngularDamping = SharedData->EditorSimOptions->HandleAngularDamping;
			SharedData->MouseHandle->AngularStiffness = SharedData->EditorSimOptions->HandleAngularStiffness;
			SharedData->MouseHandle->InterpolationSpeed = SharedData->EditorSimOptions->InterpolationSpeed;

			// Create handle to object.
			SharedData->MouseHandle->GrabComponent(SharedData->EditorSkelComp, BoneName, Result.Location, bConstrainRotation);

			FMatrix	InvViewMatrix = View->ViewMatrices.ViewMatrix.Inverse();

			SimGrabMinPush = SimMinHoldDistance - (Result.Time * SimGrabCheckDistance);

			SimGrabLocation = Result.Location;
			SimGrabX = InvViewMatrix.GetUnitAxis( EAxis::X );
			SimGrabY = InvViewMatrix.GetUnitAxis( EAxis::Y );
			SimGrabZ = InvViewMatrix.GetUnitAxis( EAxis::Z );
		}
		// Left mouse is for poking things
		else if (Key == EKeys::LeftMouseButton)
		{
			SharedData->EditorSkelComp->AddImpulseAtLocation(Click.GetDirection() * SharedData->EditorSimOptions->PokeStrength, Result.Location, BoneName);
		}
	}
}

void FPhATEdPreviewViewportClient::SimMouseMove(float DeltaX, float DeltaY)
{

	DragX = Viewport->GetMouseX() - SharedData->LastClickPos.X;
	DragY = Viewport->GetMouseY() - SharedData->LastClickPos.Y;

	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return;
	}

	//We need to convert Pixel Delta into Screen position (deal with different viewport sizes)
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
	FSceneView* View = CalcSceneView(&ViewFamily);
	FVector4 ScreenOldPos = View->PixelToScreen(SharedData->LastClickPos.X, SharedData->LastClickPos.Y, 1.f);
	FVector4 ScreenNewPos = View->PixelToScreen(DragX + SharedData->LastClickPos.X, DragY + SharedData->LastClickPos.Y, 1.f);
	FVector4 ScreenDelta = ScreenNewPos - ScreenOldPos;
	FVector4 ProjectedDelta = View->ScreenToWorld(ScreenDelta);
	FVector4 WorldDelta;

	//Now we project new ScreenPos to xy-plane of SimGrabLocation
	FVector LocalOffset = View->ViewMatrices.ViewMatrix.TransformPosition(SimGrabLocation + SimGrabZ * SimGrabPush);
	float ZDistance = fabs(LocalOffset.Z);
	WorldDelta = ProjectedDelta * ZDistance;
	
	//Now we convert back into WorldPos
	FVector WorldPos = SimGrabLocation + WorldDelta + SimGrabZ * SimGrabPush;
	FVector NewLocation = WorldPos;
	float QuickRadius = 5 - SimGrabPush / SimHoldDistanceChangeDelta;
	QuickRadius = QuickRadius < 2 ? 2 : QuickRadius;

	DrawDebugPoint(GetWorld(), NewLocation, QuickRadius, FColor(255,0,0), false, 0.3);

	SharedData->MouseHandle->SetTargetLocation(NewLocation);
	SharedData->MouseHandle->GrabbedComponent->WakeRigidBody(SharedData->MouseHandle->GrabbedBoneName);
}

void FPhATEdPreviewViewportClient::SimMouseRelease()
{
	SharedData->bManipulating = false;

	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return;
	}

	SharedData->MouseHandle->GrabbedComponent->WakeRigidBody(SharedData->MouseHandle->GrabbedBoneName);
	SharedData->MouseHandle->ReleaseComponent();
}

void FPhATEdPreviewViewportClient::SimMouseWheelUp()
{
	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return;
	}

	SimGrabPush += SimHoldDistanceChangeDelta;

	SimMouseMove(0.0f, 0.0f);
}

void FPhATEdPreviewViewportClient::SimMouseWheelDown()
{
	if (!SharedData->MouseHandle->GrabbedComponent)
	{
		return;
	}

	SimGrabPush -= SimHoldDistanceChangeDelta;
	SimGrabPush = FMath::Max(SimGrabMinPush, SimGrabPush); 

	SimMouseMove(0.0f, 0.0f);
}

void FPhATEdPreviewViewportClient::ModifyPrimitiveSize(int32 BodyIndex, EKCollisionPrimitiveType PrimType, int32 PrimIndex, FVector DeltaSize)
{
	check(SharedData->GetSelectedBody());

	FKAggregateGeom* AggGeom = &SharedData->PhysicsAsset->BodySetup[BodyIndex]->AggGeom;

	if (PrimType == KPT_Sphere)
	{
		// Find element with largest magnitude, btu preserve sign.
		float DeltaRadius = DeltaSize.X;
		if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
			DeltaRadius = DeltaSize.Y;
		else if (FMath::Abs(DeltaSize.Z) > FMath::Abs(DeltaRadius))
			DeltaRadius = DeltaSize.Z;

		AggGeom->SphereElems[PrimIndex].Radius = FMath::Max(AggGeom->SphereElems[PrimIndex].Radius + DeltaRadius, MinPrimSize);
	}
	else if (PrimType == KPT_Box)
	{
		// Sizes are lengths, so we double the delta to get similar increase in size.
		AggGeom->BoxElems[PrimIndex].X = FMath::Max(AggGeom->BoxElems[PrimIndex].X + 2*DeltaSize.X, MinPrimSize);
		AggGeom->BoxElems[PrimIndex].Y = FMath::Max(AggGeom->BoxElems[PrimIndex].Y + 2*DeltaSize.Y, MinPrimSize);
		AggGeom->BoxElems[PrimIndex].Z = FMath::Max(AggGeom->BoxElems[PrimIndex].Z + 2*DeltaSize.Z, MinPrimSize);

	}
	else if (PrimType == KPT_Sphyl)
	{
		float DeltaRadius = DeltaSize.X;
		if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
		{
			DeltaRadius = DeltaSize.Y;
		}

		float DeltaHeight = DeltaSize.Z;

		AggGeom->SphylElems[PrimIndex].Radius = FMath::Max(AggGeom->SphylElems[PrimIndex].Radius + DeltaRadius, MinPrimSize);
		AggGeom->SphylElems[PrimIndex].Length = FMath::Max(AggGeom->SphylElems[PrimIndex].Length + 2*DeltaHeight, MinPrimSize);
	}
	else if (PrimType == KPT_Convex)
	{
		FVector ModifiedScale = DeltaSize;
		if( GEditor->UsePercentageBasedScaling() )
		{
			ModifiedScale = DeltaSize * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
		}

		FTransform Transform = AggGeom->ConvexElems[PrimIndex].GetTransform();
		Transform.SetScale3D(Transform.GetScale3D() + ModifiedScale);
		AggGeom->ConvexElems[PrimIndex].SetTransform(Transform);
	}
}

void FPhATEdPreviewViewportClient::HitNothing()
{
	if (!SharedData->bSelectionLock)
	{
		if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
		{
			if(IsCtrlPressed() == false)	//we only want to deselect if Ctrl is not used
			{
				SharedData->SetSelectedBody(0);	
			}
		}
		else
		{
			if(IsCtrlPressed() == false)	//we only want to deselect if Ctrl is not used
			{
				SharedData->SetSelectedConstraint(INDEX_NONE);
			}
		}
	}

	if (!SharedData->bSelectionLock)
	{
		Viewport->Invalidate();
		PhATPtr.Pin()->RefreshHierachyTree();
	}
}
