// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyHelper.h"

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/


void UArrayProperty::LinkInternal(FArchive& Ar)
{
	ULinkerLoad* MyLinker = GetLinker();
	if( MyLinker )
	{
		MyLinker->Preload(this);
	}
	Ar.Preload(Inner);
	Inner->Link(Ar);
	Super::LinkInternal(Ar);
}
bool UArrayProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	checkSlow(Inner);

	FScriptArrayHelper ArrayHelperA(this, A);
	FScriptArrayHelper ArrayHelperB(this, B);

	const int32 ArrayNum = ArrayHelperA.Num();
	if ( ArrayNum != (B ? ArrayHelperB.Num() : 0) )
	{
		return false;
	}

	if ( B != NULL )
	{
		for ( int32 ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
		{
			if ( !Inner->Identical( ArrayHelperA.GetRawPtr(ArrayIndex), ArrayHelperB.GetRawPtr(ArrayIndex), PortFlags) )
			{
				return false;
			}
		}
	}
	else
	{
		for ( int32 ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
		{
			if ( !Inner->Identical( ArrayHelperA.GetRawPtr(ArrayIndex), 0, PortFlags) )
			{
				return false;
			}
		}
	}
	return true;
}
void UArrayProperty::SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const
{
	checkSlow(Inner);

	// Ensure that the Inner itself has been loaded before calling SerializeItem() on it
	Ar.Preload(Inner);

	FScriptArrayHelper ArrayHelper(this, Value);
	int32		n		= ArrayHelper.Num();
	Ar << n;
	if( Ar.IsLoading() )
	{
		ArrayHelper.EmptyAndAddValues(n);
	}
	ArrayHelper.CountBytes( Ar );

	const int32 ArrayMaxReadBytes = MaxReadBytes > 0 ? ( MaxReadBytes - sizeof( n ) ) : 0;
	for( int32 i=0; i<n; i++ )
	{
		const int32 ItemMaxReadBytes = ArrayMaxReadBytes > 0 ? ArrayMaxReadBytes / n: 0;
		Inner->SerializeItem( Ar, ArrayHelper.GetRawPtr(i), ItemMaxReadBytes );
	}
}

bool UArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );
	return 1;
}

void UArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Inner;
	checkSlow(Inner||HasAnyFlags(RF_ClassDefaultObject));
}
void UArrayProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UArrayProperty* This = CastChecked<UArrayProperty>(InThis);
	Collector.AddReferencedObject( This->Inner, This );
	Super::AddReferencedObjects( This, Collector );
}
FString UArrayProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	checkSlow(Inner);

	if ( ExtendedTypeText != NULL )
	{
		FString InnerExtendedTypeText;
		FString InnerTypeText = Inner->GetCPPType(&InnerExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider array inners to be "arguments or return values"
		if ( InnerExtendedTypeText.Len() && InnerExtendedTypeText.Right(1) == TEXT(">") )
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		else if ( !InnerExtendedTypeText.Len() && InnerTypeText.Len() && InnerTypeText.Right(1) == TEXT(">") )
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *InnerTypeText, *InnerExtendedTypeText);
	}
	return TEXT("TArray");
}
FString UArrayProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(Inner);
	ExtendedTypeText = Inner->GetCPPType();
	return TEXT("TARRAY");
}
void UArrayProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	checkSlow(Inner);

	FScriptArrayHelper ArrayHelper(this, PropertyValue);
	FScriptArrayHelper DefaultArrayHelper(this, DefaultValue);

	uint8* StructDefaults = NULL;
	UStructProperty* StructProperty = Cast<UStructProperty>(Inner);
	if ( StructProperty != NULL )
	{
		checkSlow(StructProperty->Struct);
		StructDefaults = (uint8*)FMemory::Malloc(StructProperty->Struct->GetStructureSize());
		StructProperty->InitializeValue(StructDefaults);
	}

	const bool bReadableForm = (0 != (PPF_BlueprintDebugView & PortFlags));

	int32 Count = 0;
	for( int32 i=0; i<ArrayHelper.Num(); i++ )
	{
		++Count;
		if(!bReadableForm)
		{
			if ( Count == 1 )
			{
				ValueStr += TCHAR('(');
			}
			else
			{
				ValueStr += TCHAR(',');
			}
		}
		else
		{
			if(Count > 1)
			{
				ValueStr += TCHAR('\n');
			}
			ValueStr += FString::Printf(TEXT("[%i] "), i);
		}

		uint8* PropData = ArrayHelper.GetRawPtr(i);
		uint8* PropDefault = (DefaultValue && DefaultArrayHelper.Num() > i)
			? DefaultArrayHelper.GetRawPtr(i)
			: StructDefaults;

		// Do not re-export duplicate data from superclass when exporting to .int file
		if ( (PortFlags & PPF_LocalizedOnly) != 0 && Inner->Identical(PropData, PropDefault) )
		{
			continue;
		}

		Inner->ExportTextItem( ValueStr, PropData, PropDefault, Parent, PortFlags|PPF_Delimited, ExportRootScope );
	}

	if ((Count > 0) && !bReadableForm)
	{
		ValueStr += TEXT(")");
	}
	if (StructDefaults)
	{
		StructProperty->DestroyValue(StructDefaults);
		FMemory::Free(StructDefaults);
	}
}

const TCHAR* UArrayProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	checkSlow(Inner);

	if ( *Buffer++ != TCHAR('(') )
	{
		return NULL;
	}

	FScriptArrayHelper ArrayHelper(this, Data);

	// only clear the array if we're not importing localized text
	if ( (PortFlags&PPF_LocalizedOnly) == 0 )
	{
		ArrayHelper.EmptyValues();
	}

	SkipWhitespace(Buffer);

	int32 Index = 0;

	ArrayHelper.ExpandForIndex(0);
	while ((Buffer != NULL) && (*Buffer != TCHAR(')')))
	{
		SkipWhitespace(Buffer);

		if (*Buffer != TCHAR(','))
		{
			// Parse the item
			Buffer = Inner->ImportText(Buffer, ArrayHelper.GetRawPtr(Index), PortFlags | PPF_Delimited, Parent, ErrorText);

			if(!Buffer)
			{
				return NULL;
			}

			SkipWhitespace(Buffer);
		}


		if (*Buffer == TCHAR(','))
		{
			Buffer++;
			Index++;
			ArrayHelper.ExpandForIndex(Index);
		}
		else
		{
			break;
		}
	}

	// Make sure we ended on a )
	if (*Buffer++ != TCHAR(')'))
	{
		return NULL;
	}

	return Buffer;
}

void UArrayProperty::AddCppProperty( UProperty* Property )
{
	check(!Inner);
	check(Property);

	Inner = Property;
}

void UArrayProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(Count==1); // this was never supported, apparently
	FScriptArrayHelper SrcArrayHelper(this, Src);
	FScriptArrayHelper DestArrayHelper(this, Dest);

	int32 Num = SrcArrayHelper.Num();
	if ( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
	{
	DestArrayHelper.EmptyAndAddValues(Num);
	}
	else
	{
		DestArrayHelper.EmptyAndAddUninitializedValues(Num);
	}
	if (Num)
	{
		int32 Size = Inner->ElementSize;
		uint8* SrcData = (uint8*)SrcArrayHelper.GetRawPtr();
		uint8* DestData = (uint8*)DestArrayHelper.GetRawPtr();
		if( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
		{
			for( int32 i=0; i<Num; i++ )
			{
				Inner->CopyCompleteValue( DestData + i * Size, SrcData + i * Size );
			}
		}
		else
		{
			FMemory::Memcpy( DestData, SrcData, Num*Size );
		}
	}
}
void UArrayProperty::ClearValueInternal( void* Data ) const
{
	FScriptArrayHelper ArrayHelper(this, Data);
	ArrayHelper.EmptyValues();
}
void UArrayProperty::DestroyValueInternal( void* Dest ) const
{
	FScriptArrayHelper ArrayHelper(this, Dest);
	ArrayHelper.EmptyValues();

	//@todo UE4 potential double destroy later from this...would be ok for a script array, but still
	((FScriptArray*)Dest)->~FScriptArray();
}
bool UArrayProperty::IsLocalized() const
{
	return Inner->IsLocalized() ? true : Super::IsLocalized();
}
bool UArrayProperty::PassCPPArgsByRef() const
{
	return true;
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UArrayProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	if( Data && Inner->ContainsInstancedObjectProperty())
	{
		FScriptArrayHelper ArrayHelper(this, Data);
		FScriptArrayHelper DefaultArrayHelper(this, DefaultData);

		for( int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ElementIndex++ )
		{
			uint8* DefaultValue = (DefaultData && ElementIndex < DefaultArrayHelper.Num()) ? DefaultArrayHelper.GetRawPtr(ElementIndex) : NULL;
			Inner->InstanceSubobjects( ArrayHelper.GetRawPtr(ElementIndex), DefaultValue, Owner, InstanceGraph );
		}
	}
}

bool UArrayProperty::SameType(const UProperty* Other) const
{
	return Super::SameType(Other) && Inner && Inner->SameType(((UArrayProperty*)Other)->Inner);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UArrayProperty, UProperty,
	{
		Class->EmitObjectReference( STRUCT_OFFSET( UArrayProperty, Inner ) );

		// Ensure that TArray and FScriptArray are interchangeable, as FScriptArray will be used to access a native array property
		// from script that is declared as a TArray in C++.
		checkAtCompileTime(sizeof(FScriptArray) == sizeof(TArray<uint8>),FScriptArrayAndTArrayMustBeInterchangable);
	}
);
