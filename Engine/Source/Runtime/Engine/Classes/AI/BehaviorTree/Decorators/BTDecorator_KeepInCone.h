// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BTDecorator_KeepInCone.generated.h"

struct FBTKeepInConeDecoratorMemory
{
	FVector InitialDirection;
};

UCLASS(HideCategories=(Condition))
class ENGINE_API UBTDecorator_KeepInCone : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	typedef FBTKeepInConeDecoratorMemory TNodeInstanceMemory;

	/** max allowed time for execution of underlying node */
	UPROPERTY(Category=Decorator, EditAnywhere)
	float ConeHalfAngle;
	
	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	struct FBlackboardKeySelector ConeOrigin;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	struct FBlackboardKeySelector Observed;

	// deprecated, set value of ConeOrigin on initialization
	UPROPERTY()
	uint32 bUseSelfAsOrigin:1;

	// deprecated, set value of Observed on initialization
	UPROPERTY()
	uint32 bUseSelfAsObserved:1;
	
	float ConeHalfAngleDot;

	virtual void InitializeFromAsset(class UBehaviorTree* Asset) OVERRIDE;
	virtual uint16 GetInstanceMemorySize() const OVERRIDE;
	virtual void DescribeRuntimeValues(const class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const OVERRIDE;
	virtual FString GetStaticDescription() const OVERRIDE;

protected:

	virtual void OnBecomeRelevant(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) OVERRIDE;
	virtual void TickNode(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, float DeltaSeconds) OVERRIDE;

	bool CalculateCurrentDirection(const UBehaviorTreeComponent* OwnerComp, FVector& Direction) const;
};
