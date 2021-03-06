// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "EngineClasses.h"
#include "Net/UnrealNetwork.h"
#include "AI/BehaviorTreeDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameplayDebugging, Log, All);

#define BUGIT_VIEWS (1<<EAIDebugDrawDataView::Basic) | (1 << EAIDebugDrawDataView::OverHead)
#define BREAK_LINE_TEXT TEXT("________________________________________________________________")

UGameplayDebuggingControllerComponent::UGameplayDebuggingControllerComponent(const class FPostConstructInitializeProperties& PCIP) 
	: Super(PCIP)
	, DebugComponentKey(EKeys::Quote)
	, CurrentQuickBugNum(0)
	, bJustBugIt(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	bAutoActivate = false;
	bTickInEditor=true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	DebugComponentHUDClass = NULL;
	DebugAITargetActor = NULL;
	DebugComponent_LastActiveViews = 1 << EAIDebugDrawDataView::Basic;
}

void UGameplayDebuggingControllerComponent::BeginDestroy()
{
#if !UE_BUILD_SHIPPING && WITH_EDITOR
	// Register for debug drawing
	UDebugDrawService::Unregister(FDebugDrawDelegate::CreateUObject(this, &UGameplayDebuggingControllerComponent::OnDebugAI));
#endif

	APlayerController* PC = Cast<APlayerController>(GetOuter());
	if (PC && PC->InputComponent)
	{
		for (int32 Index = PC->InputComponent->KeyBindings.Num() - 1; Index >= 0; --Index)
		{
			const FInputKeyBinding& KeyBind = PC->InputComponent->KeyBindings[Index];
			if (KeyBind.KeyDelegate.IsBoundToObject(this))
			{
				PC->InputComponent->KeyBindings.RemoveAtSwap(Index);
			}
		}
	}

	if(PC)
	{
		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			PC->ServerReplicateMessageToAIDebugView(Pawn, EDebugComponentMessage::DeactivateReplilcation, 0);
		}
	}
	Super::BeginDestroy();
}

void UGameplayDebuggingControllerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	APlayerController* PC = Cast<APlayerController>(GetOuter());

	if (PC && PC->InputComponent)
	{
		PC->InputComponent->BindKey(FInputChord(DebugComponentKey, false, false, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::OnControlPressed);
		PC->InputComponent->BindKey(FInputChord(DebugComponentKey, false, false, false), IE_Released, this, &UGameplayDebuggingControllerComponent::OnControlReleased);
	}

	if ( DebugComponentHUDClass == NULL )
	{
		DebugComponentHUDClass = StaticLoadClass( AHUD::StaticClass(), NULL, *DebugComponentHUDClassName, NULL, LOAD_None, NULL );
	}

#if !UE_BUILD_SHIPPING && WITH_EDITOR
	// Register for debug drawing
	UDebugDrawService::Register(TEXT("DebugAI"), FDebugDrawDelegate::CreateUObject(this, &UGameplayDebuggingControllerComponent::OnDebugAI));
#endif
}

void UGameplayDebuggingControllerComponent::OnControlPressed()
{
	ControlKeyPressedTime = GetWorld()->GetTimeSeconds();
	bWasDisplayingInfo = bDisplayAIDebugInfo;
	StartAIDebugView();
}

void UGameplayDebuggingControllerComponent::OnControlReleased()
{
	const float DownTime = GetWorld()->GetTimeSeconds() - ControlKeyPressedTime;
	if (DownTime < 0.5f && bWasDisplayingInfo)
	{
		EndAIDebugView();
	}
	else
	{
		LockAIDebugView();
	}
}

void UGameplayDebuggingControllerComponent::OnDebugAI(class UCanvas* Canvas, class APlayerController* PC)
{
#if !UE_BUILD_SHIPPING && WITH_EDITOR
	if (World == NULL || IsPendingKill() || Canvas == NULL || Canvas->IsPendingKill())
	{
		return;
	}

	if (Canvas->SceneView != NULL && !Canvas->SceneView->bIsGameView)
	{
		return;
	}

	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	if (MyPC == NULL || MyPC->IsPendingKill() )
	{
		return;
	}

	for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator )
	{
		APawn *Pawn = *Iterator;
		if (Pawn == NULL || Pawn->IsPendingKill())
		{
			continue;
		}

		if (UGameplayDebuggingComponent* DebuggingComponent = Pawn->GetDebugComponent(true))
		{
			if (DebuggingComponent->IsPendingKill() == false)
			{
				DebuggingComponent->OnDebugAI(Canvas, MyPC);
			}
		}
	}

	if (OnDebugAIHUD == NULL && MyPC != NULL && DebugComponentHUDClass != NULL)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = MyPC;
		SpawnInfo.Instigator = MyPC->GetPawnOrSpectator();
		SpawnInfo.bNoCollisionFail = true;

		OnDebugAIHUD = GetWorld()->SpawnActor<AHUD>( DebugComponentHUDClass, SpawnInfo );
		OnDebugAIHUD->SetCanvas(Canvas, Canvas);

	}
	else if (OnDebugAIHUD != NULL && OnDebugAIHUD->PlayerOwner)
	{
		OnDebugAIHUD->SetCanvas(Canvas, Canvas);
		OnDebugAIHUD->PostRender();
	}
#endif
}

void UGameplayDebuggingControllerComponent::StartAIDebugView()
{
	BeginAIDebugView();
}

void UGameplayDebuggingControllerComponent::BeginAIDebugView(bool bInJustBugIt)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* PC = Cast<APlayerController>(GetOuter());

	SetGameplayDebugFlag(true, PC);

	APawn* Pawn = PC ? PC->GetPawn() : NULL;

	if (!World || !PC || !Pawn)
	{
		return;
	}

	Activate();
	SetComponentTickEnabled(true);
	
	if (!bDisplayAIDebugInfo && !bWaitingForOwnersComponent)
	{
		PC->ServerReplicateMessageToAIDebugView(Pawn, EDebugComponentMessage::ActivateReplication, 0); //create component for my player's pawn, to replicate non pawn's related data

		if (!bInJustBugIt)
		{
			for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator )
			{
				APawn* NewTarget = *Iterator;
				if ( NewTarget == PC->GetPawn() || (NewTarget->PlayerState && !NewTarget->PlayerState->bIsABot))
				{
					continue;
				}

				PC->ServerReplicateMessageToAIDebugView(NewTarget, EDebugComponentMessage::ActivateReplication, 0);
			}
		}
	}

	bWaitingForOwnersComponent = false;
	if (UGameplayDebuggingComponent* OwnerComp = Pawn->GetDebugComponent(false))
	{
		OwnerComp->ServerEnableTargetSelection(true);
	}
	else
	{
		bWaitingForOwnersComponent = true;
		return;
	}

	if (DebugAITargetActor != NULL && DebugAITargetActor->GetDebugComponent() != NULL )
	{
		DebugComponent_LastActiveViews = DebugAITargetActor->GetDebugComponent()->GetActiveViews();
	}
	else
	{
		DebugAITargetActor = NULL;
	}

	GEngine->bEnableOnScreenDebugMessages = false;

	if (!bDisplayAIDebugInfo)
	{
		bDisplayAIDebugInfo = true;

		OryginalHUD = PC->GetHUD();
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = PC;
		SpawnInfo.Instigator = PC->Instigator;
		SpawnInfo.bNoCollisionFail = true;
		PC->MyHUD = PC->GetWorld()->SpawnActor<AHUD>( DebugComponentHUDClass, SpawnInfo );
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::LockAIDebugView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	APawn* Pawn = MyPC ? MyPC->GetPawn() : NULL;
	if (Pawn)
	{
		if (UGameplayDebuggingComponent* OwnerComp = Pawn->GetDebugComponent(false))
		{
			OwnerComp->ServerEnableTargetSelection(false);
		}
	}

	BindAIDebugViewKeys();

	FBehaviorTreeDelegates::OnDebugLocked.Broadcast(DebugAITargetActor);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::EndAIDebugView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// disable debug flag

	APlayerController* MyPC = Cast<APlayerController>(GetOuter());

	SetGameplayDebugFlag(false, MyPC);

	if (AIDebugViewInputComponent && MyPC)
	{
		AIDebugViewInputComponent->UnregisterComponent();
		MyPC->PopInputComponent(AIDebugViewInputComponent);
	}
	AIDebugViewInputComponent = NULL;

	APawn* Pawn = MyPC ? MyPC->GetPawn() : NULL;
	if (Pawn)
	{
		if (UGameplayDebuggingComponent* OwnerComp = Pawn->GetDebugComponent(false))
		{
			OwnerComp->ServerEnableTargetSelection(false);
			if (OwnerComp->IsViewActive(EAIDebugDrawDataView::NavMesh))
			{
				ToggleAIDebugView_SetView0();
			}
		}
	}

	if (DebugAITargetActor != NULL)
	{
		if (DebugAITargetActor->GetDebugComponent(false))
		{
			DebugComponent_LastActiveViews = DebugAITargetActor->GetDebugComponent(false)->GetActiveViews();
			DebugAITargetActor->GetDebugComponent(false)->EnableDebugDraw(false);
		}
		if (MyPC)
		{
			MyPC->ServerReplicateMessageToAIDebugView(DebugAITargetActor, EDebugComponentMessage::DisableExtendedView);
		}
	}

	if (MyPC)
	{
		UWorld* World = MyPC->GetWorld();
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator )
		{
			APawn* NewTarget = *Iterator;
			if (NewTarget && NewTarget != MyPC->GetPawn())
			{
				MyPC->ServerReplicateMessageToAIDebugView(NewTarget, EDebugComponentMessage::DeactivateReplilcation, 1 << EAIDebugDrawDataView::Empty);
			}
		}
	}

	SetComponentTickEnabled(false);
	Deactivate();

	if (MyPC)
	{
		if (MyPC->MyHUD)
		{
			MyPC->MyHUD->Destroy();
		}

		MyPC->MyHUD = OryginalHUD;
	}
	OryginalHUD = NULL;

	DebugAITargetActor = NULL;
	bDisplayAIDebugInfo = false;
	bJustBugIt = false;

	GEngine->bEnableOnScreenDebugMessages = !bDisplayAIDebugInfo;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::SetGameplayDebugFlag(bool bNewValue, APlayerController* PlayerController)
{
	if (PlayerController == NULL)
	{
		PlayerController = Cast<APlayerController>(GetOwner());
	}

	if (PlayerController && Cast<ULocalPlayer>(PlayerController->Player) && ((ULocalPlayer*)(PlayerController->Player))->ViewportClient && UGameplayDebuggingComponent::ShowFlagIndex != INDEX_NONE)
	{
		((ULocalPlayer*)(PlayerController->Player))->ViewportClient->EngineShowFlags.SetSingleFlag(UGameplayDebuggingComponent::ShowFlagIndex, bNewValue);
	}
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView()
{
	if (!bDisplayAIDebugInfo)
	{
		StartAIDebugView();
		LockAIDebugView();
	}
	else
	{
		EndAIDebugView();
	}
}

void UGameplayDebuggingControllerComponent::BugIt()
{
	BeginAIDebugView(true);
	LockAIDebugView();
	bJustBugIt = true;
}

void UGameplayDebuggingControllerComponent::BindAIDebugViewKeys()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!AIDebugViewInputComponent)
	{
		AIDebugViewInputComponent = ConstructObject<UInputComponent>(UInputComponent::StaticClass(), GetOwner(), TEXT("AIDebugViewInputComponent0"));
		AIDebugViewInputComponent->RegisterComponent();

		AIDebugViewInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_CycleView);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView1);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView2);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView3);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView4);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView5);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView6);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadSeven, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView7);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadEight, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView8);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadNine, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView9);
	}
	APlayerController* PC = Cast<APlayerController>(GetOuter());
	PC->PushInputComponent(AIDebugViewInputComponent);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UGameplayDebuggingControllerComponent::CanToggleView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	APawn* Pawn = MyPC->GetPawn();
	return  (MyPC && bDisplayAIDebugInfo && DebugAITargetActor);
#else
	return false;
#endif
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_CycleView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		APlayerController* MyPC = Cast<APlayerController>(GetOuter());
		MyPC->ServerReplicateMessageToAIDebugView(DebugAITargetActor, EDebugComponentMessage::CycleReplicationView);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		APlayerController* MyPC = Cast<APlayerController>(GetOuter());
		APawn* Pawn = MyPC->GetPawn();
		if (UGameplayDebuggingComponent* OwnerComp = Pawn->GetDebugComponent(false))
		{
			OwnerComp->ToggleActiveView(EAIDebugDrawDataView::NavMesh);
			if (OwnerComp->IsViewActive(EAIDebugDrawDataView::NavMesh))
			{
				GetWorld()->GetTimerManager().SetTimer(this, &UGameplayDebuggingControllerComponent::UpdateNavMeshTimer, 5.0f, true);
				UpdateNavMeshTimer();

				MyPC->ServerReplicateMessageToAIDebugView(Pawn, EDebugComponentMessage::ActivateReplication, 1 << EAIDebugDrawDataView::Empty);
				OwnerComp->SetVisibility(true, true);
				OwnerComp->SetHiddenInGame(false, true);
				OwnerComp->MarkRenderStateDirty();
			}
			else
			{
				GetWorld()->GetTimerManager().ClearTimer(this, &UGameplayDebuggingControllerComponent::UpdateNavMeshTimer);

				MyPC->ServerReplicateMessageToAIDebugView(Pawn, EDebugComponentMessage::DeactivateReplilcation, 1 << EAIDebugDrawDataView::Empty);
				OwnerComp->ServerDiscardNavmeshData();
				OwnerComp->SetVisibility(false, true);
				OwnerComp->SetHiddenInGame(true, true);
				OwnerComp->MarkRenderStateDirty();
			}
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView1()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::Basic);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView2()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::BehaviorTree);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView3()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::EQS);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView4()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::Perception);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView5()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::GameView1);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView6()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::GameView2);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView7()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::GameView3);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView8()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::GameView4);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView9()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CanToggleView())
	{
		DebugAITargetActor->GetDebugComponent(false)->ToggleActiveView(EAIDebugDrawDataView::GameView5);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

APawn* UGameplayDebuggingControllerComponent::GetCurrentDebugTarget() const
{
	return DebugAITargetActor;
}


void UGameplayDebuggingControllerComponent::OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ToggleAIDebugView();
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::TakeScreenShot()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	
	SetGameplayDebugFlag(true, MyPC);
	
	const FString ScreenShotDescription = FString::Printf(TEXT("QuickBug%d"), CurrentQuickBugNum++);

	MyPC->ConsoleCommand( FString::Printf(TEXT("BUGSCREENSHOTWITHHUDINFO %s"), *ScreenShotDescription ));

	FVector ViewLocation;
	FRotator ViewRotation;
	MyPC->GetPlayerViewPoint( ViewLocation, ViewRotation );

	if( MyPC->GetPawn() != NULL )
	{
		ViewLocation = MyPC->GetPawn()->GetActorLocation();
	}

	FString GoString, LocString;
	GoString = FString::Printf(TEXT("BugItGo %f %f %f %f %f %f"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll);
	UE_LOG(LogGameplayDebugging, Log, TEXT("%s"), *GoString);

	LocString = FString::Printf(TEXT("?BugLoc=%s?BugRot=%s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogGameplayDebugging, Log, TEXT("%s"), *LocString);

	MyPC->GetWorldTimerManager().SetTimer(this, &UGameplayDebuggingControllerComponent::EndAIDebugView, 0.1, false);

	LogOutBugItGoToLogFile( ScreenShotDescription, GoString, LocString );
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	APawn* MyPawn = MyPC ? MyPC->GetPawn() : NULL;

	UGameplayDebuggingComponent* OwnerComp = MyPawn ? MyPawn->GetDebugComponent(false) : NULL;
	if (OwnerComp)
	{
		if(bWaitingForOwnersComponent)
		{
			BeginAIDebugView( bJustBugIt );
			LockAIDebugView();
		}

		if (DebugAITargetActor != OwnerComp->TargetActor)
		{
			FBehaviorTreeDelegates::OnDebugSelected.Broadcast(OwnerComp->TargetActor);
			if (DebugAITargetActor != NULL)
			{

				if (DebugAITargetActor->GetDebugComponent(false))
				{
					DebugAITargetActor->GetDebugComponent(false)->EnableDebugDraw(true, false);
				}
				MyPC->ServerReplicateMessageToAIDebugView(DebugAITargetActor, EDebugComponentMessage::DisableExtendedView);
			}

			if (OwnerComp->TargetActor != NULL && OwnerComp->TargetActor->GetDebugComponent(false))
			{
				DebugAITargetActor = OwnerComp->TargetActor;
				DebugAITargetActor->GetDebugComponent(false)->SetActiveViews(DebugComponent_LastActiveViews | (1 << EAIDebugDrawDataView::OverHead));
				DebugAITargetActor->GetDebugComponent(false)->EnableDebugDraw(true, true);
				MyPC->ServerReplicateMessageToAIDebugView(DebugAITargetActor, EDebugComponentMessage::EnableExtendedView);
			}
		}
	}

	if (bJustBugIt && MyPC && MyPawn && OwnerComp)
	{
		if (DebugAITargetActor && DebugAITargetActor->GetDebugComponent(false))
		{
			DebugAITargetActor->GetDebugComponent(false)->SetActiveViews( BUGIT_VIEWS );
		}

		if (OwnerComp->IsViewActive(EAIDebugDrawDataView::NavMesh))
		{
			ToggleAIDebugView_SetView0();
		}
		ToggleAIDebugView_SetView1();
		MyPC->GetWorldTimerManager().SetTimer(this, &UGameplayDebuggingControllerComponent::TakeScreenShot, 1.0, false);
		bJustBugIt = false;
	}


	if (GetWorld()->bPlayersOnly && GetOwnerRole() == ROLE_Authority)
	{
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator )
		{
			APawn* NewTarget = *Iterator;
			if (UGameplayDebuggingComponent* DebuggingComponent = NewTarget->GetDebugComponent(false))
			{
				DebuggingComponent->CollectDataToReplicate();
			}
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::LogOutBugItGoToLogFile( const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString )
{
#if ALLOW_DEBUG_FILES
	// Create folder if not already there

	const FString OutputDir = FPaths::BugItDir() + InScreenShotDesc + TEXT("/");

	IFileManager::Get().MakeDirectory( *OutputDir );
	// Create archive for log data.
	// we have to +1 on the GScreenshotBitmapIndex as it will be incremented by the bugitscreenshot which is processed next tick

	const FString DescPlusExtension = FString::Printf( TEXT("%s%i.txt"), *InScreenShotDesc, GScreenshotBitmapIndex );
	const FString TxtFileName = CreateProfileFilename( DescPlusExtension, false );

	//FString::Printf( TEXT("BugIt%s-%s%05i"), *GEngineVersion.ToString(), *InScreenShotDesc, GScreenshotBitmapIndex+1 ) + TEXT( ".txt" );
	const FString FullFileName = OutputDir + TxtFileName;

	FOutputDeviceFile OutputFile(*FullFileName);
	//FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *(FullFileName), FILEWRITE_Append );


	OutputFile.Logf( TEXT("Dumping BugIt data chart at %s using build %s built from changelist %i"), *FDateTime::Now().ToString(), *GEngineVersion.ToString(), GetChangeListNumberForPerfTesting() );

	const FString MapNameStr = GetWorld()->GetMapName();

	OutputFile.Logf( TEXT("MapName: %s"), *MapNameStr );

	OutputFile.Logf( TEXT("Description: %s"), *InScreenShotDesc );
	OutputFile.Logf( TEXT("%s"), *InGoString );
	OutputFile.Logf( TEXT("%s"), *InLocString );

	OutputFile.Logf( TEXT(" ---=== GameSpecificData ===--- ") );
	
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	AGameplayDebuggingHUDComponent* HUD = Cast<AGameplayDebuggingHUDComponent>(MyPC->MyHUD);
	if(HUD)
	{
		FString TargetString = HUD->GenerateAllData();
		FString Line;

		if(!TargetString.IsEmpty())
		{
			bool bDrewDivider = false;
			while(TargetString.Split(TEXT("\n"), &Line, &TargetString))
			{
				OutputFile.Log(*Line);

				if(!bDrewDivider)
				{
					OutputFile.Logf(BREAK_LINE_TEXT);
					bDrewDivider = true;
				}
			}

			OutputFile.Logf(BREAK_LINE_TEXT);
		}
	}

	APawn* MyPawn = MyPC->GetPawn();
	if (UGameplayDebuggingComponent* OwnerComp = MyPawn->GetDebugComponent(false))
	{
		OwnerComp->LogGameSpecificBugIt(OutputFile);
	}


	// Flush, close and delete.
	//delete OutputFile;
	OutputFile.TearDown();

	// so here we want to send this bad boy back to the PC
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), *(FullFileName) );
#endif // ALLOW_DEBUG_FILES
}

void UGameplayDebuggingControllerComponent::UpdateNavMeshTimer()
{
	APlayerController* MyPC = Cast<APlayerController>(GetOuter());
	APawn* Pawn = MyPC ? MyPC->GetPawn() : NULL;
	UGameplayDebuggingComponent* OwnerComp = Pawn ? Pawn->GetDebugComponent(false) : NULL;
	if (OwnerComp)
	{
		const FVector AdditionalTargetLoc =
			DebugAITargetActor ? DebugAITargetActor->GetActorLocation() :
			Pawn ? Pawn->GetActorLocation() : 
			FVector::ZeroVector;

		OwnerComp->ServerCollectNavmeshData(AdditionalTargetLoc);
	}
}
