// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BTDecorator_DoesPathExist.generated.h"

UENUM()
namespace EPathExistanceQueryType
{
	enum Type
	{
		NavmeshRaycast2D UMETA(ToolTip = "Really Fast"),
		HierarchicalQuery UMETA(ToolTip = "Fast"),
		RegularPathFinding UMETA(ToolTip = "Slow"),
	};
}

UCLASS()
class ENGINE_API UBTDecorator_DoesPathExist : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

protected:

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Condition)
	struct FBlackboardKeySelector BlackboardKeyA;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Condition)
	struct FBlackboardKeySelector BlackboardKeyB;

public:

	// deprecated, set value of blackboard key A on initialization
	UPROPERTY()
	uint32 bUseSelf:1;

	UPROPERTY(EditAnywhere, Category=Condition)
	TEnumAsByte<EPathExistanceQueryType::Type> PathQueryType;

	/** "None" will result in default filter being used */
	UPROPERTY(Category=Node, EditAnywhere)
	TSubclassOf<class UNavigationQueryFilter> FilterClass;

	virtual bool CalculateRawConditionValue(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const OVERRIDE;
	virtual FString GetStaticDescription() const OVERRIDE;
	virtual void InitializeFromAsset(class UBehaviorTree* Asset) OVERRIDE;
};
