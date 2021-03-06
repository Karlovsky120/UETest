// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node_CallArrayFunction.generated.h"

struct FArrayPropertyPinCombo
{
	UEdGraphPin* ArrayPin;
	UEdGraphPin* ArrayPropPin;

	FArrayPropertyPinCombo()
		: ArrayPin(NULL)
		, ArrayPropPin(NULL)
	{}
};

UCLASS(MinimalAPI)
class UK2Node_CallArrayFunction : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() OVERRIDE;
	virtual void PostReconstructNode() OVERRIDE;
	// End UEdGraphNode interface

	// Begin UK2Node interface
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) OVERRIDE;
	// End UK2Node interface

	/** Retrieves the target pin for the function */
	BLUEPRINTGRAPH_API UEdGraphPin* GetTargetArrayPin() const;

	/**
	 * Retrieves the array pins and their property pins as a combo-struct
	 *
	 * @param OutArrayPinInfo		The pins and their property pins will be added to this array
	 */
	BLUEPRINTGRAPH_API void GetArrayPins(TArray< FArrayPropertyPinCombo >& OutArrayPinInfo ) const;

	/**
	 * Checks if the passed in property is a wildcard property
	 *
	 * @param InArrayFunction		The array function to check
	 * @param InProperty			Property to examine to see if it is marked in metadata as a wildcard
	 *
	 * @return						TRUE if the property is a wildcard.
	 */
	BLUEPRINTGRAPH_API static bool IsWildcardProperty(UFunction* InArrayFunction, const UProperty* InProperty);

	void GetArrayTypeDependentPins(TArray<UEdGraphPin*>& OutPins) const;
	void PropagateArrayTypeInfo();
};

