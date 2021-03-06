// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTreeTypes.h"
#include "BTAuxiliaryNode.generated.h"

struct FBTAuxiliaryMemory : public FBTInstancedNodeMemory
{
	float NextTickRemainingTime;
};

/** 
 * Auxiliary nodes are supporting nodes, that receive notification about execution flow and can be ticked
 *
 * Because some of them can be instanced for specific AI, following virtual functions are not marked as const:
 *  - OnBecomeRelevant
 *  - OnCeaseRelevant
 *  - TickNode
 *
 * If your node is not being instanced (default behavior), DO NOT change any properties of object within those functions!
 * Template nodes are shared across all behavior tree components using the same tree asset and must store
 * their runtime properties in provided NodeMemory block (allocation size determined by GetInstanceMemorySize() )
 *
 */

UCLASS(Abstract)
class ENGINE_API UBTAuxiliaryNode : public UBTNode
{
	GENERATED_UCLASS_BODY()

	/** wrapper for node instancing: OnBecomeRelevant */
	void WrappedOnBecomeRelevant(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const;

	/** wrapper for node instancing: OnCeaseRelevant */
	void WrappedOnCeaseRelevant(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const;

	/** wrapper for node instancing: TickNode */
	void WrappedTickNode(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, float DeltaSeconds) const;

	virtual void DescribeRuntimeValues(const class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const OVERRIDE;
	virtual uint16 GetSpecialMemorySize() const OVERRIDE;

protected:

	/** if set, OnBecomeRelevant will be used */
	uint8 bNotifyBecomeRelevant:1;

	/** if set, OnCeaseRelevant will be used */
	uint8 bNotifyCeaseRelevant:1;

	/** if set, OnTick will be used */
	uint8 bNotifyTick : 1;

	/** if set, conditional tick will use remaining time form node's memory */
	uint8 bTickIntervals : 1;

	/** called when auxiliary node becomes active
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnBecomeRelevant(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory);

	/** called when auxiliary node becomes inactive
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnCeaseRelevant(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory);

	/** tick function
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void TickNode(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, float DeltaSeconds);

	/** sets next tick time */
	void SetNextTickTime(uint8* NodeMemory, float RemainingTime) const;
};
