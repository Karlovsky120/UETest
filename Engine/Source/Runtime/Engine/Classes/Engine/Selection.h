// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * Manages selections of objects.  Used in the editor for selecting
 * objects in the various browser windows.
 */

#pragma once
#include "Selection.generated.h"

UCLASS(customConstructor, transient)
class ENGINE_API USelection : public UObject
{
	GENERATED_UCLASS_BODY()
private:
	typedef TArray<TWeakObjectPtr<UObject> >	ObjectArray;
	typedef TMRUArray<UClass*>	ClassArray;

	friend class FSelectionIterator;

public:
	/** Params: UObject* NewSelection */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, UObject*);

	/** Called when selection in editor has changed */
	static FOnSelectionChanged SelectionChangedEvent;
	/** Called when an object has been selected (generally an actor) */
	static FOnSelectionChanged SelectObjectEvent;
	/** Called to deselect everything */
	static FSimpleMulticastDelegate SelectNoneEvent;

	USelection(const class FPostConstructInitializeProperties& PCIP);

	typedef ClassArray::TIterator TClassIterator;
	typedef ClassArray::TConstIterator TClassConstIterator;

	TClassIterator			ClassItor()				{ return TClassIterator( SelectedClasses ); }
	TClassConstIterator		ClassConstItor() const	{ return TClassConstIterator( SelectedClasses ); }

	/**
	 * Returns the number of objects in the selection set.  This function is used by clients in
	 * conjunction with op::() to iterate over selected objects.  Note that some of these objects
	 * may be NULL, and so clients should use CountSelections() to get the true number of
	 * non-NULL selected objects.
	 * 
	 * @return		Number of objects in the selection set.
	 */
	int32 Num() const
	{
		return SelectedObjects.Num();
	}

	/**
	 * @return	The Index'th selected objects.  May be NULL.
	 */
	UObject* GetSelectedObject(const int32 InIndex)
	{
		return (SelectedObjects.IsValidIndex(InIndex) && SelectedObjects[InIndex].IsValid() ? SelectedObjects[InIndex].Get() : NULL);
	}

	/**
	 * @return	The Index'th selected objects.  May be NULL.
	 */
	const UObject* GetSelectedObject(const int32 InIndex) const
	{
		return (SelectedObjects.IsValidIndex(InIndex) && SelectedObjects[InIndex].IsValid() ? SelectedObjects[InIndex].Get() : NULL);
	}

	/**
	 * Call before beginning selection operations
	 */
	void BeginBatchSelectOperation()
	{
		SelectionMutex++;
	}

	/**
	 * Should be called when selection operations are complete.  If all selection operations are complete, notifies all listeners
	 * that the selection has been changed.
	 */
	void EndBatchSelectOperation(bool bNotify = true)
	{
		if ( --SelectionMutex == 0 )
		{
			const bool bSelectionChanged = bIsBatchDirty;
			bIsBatchDirty = false;

			if ( bSelectionChanged && bNotify )
			{
				// new version - includes which selection set was modified
				USelection::SelectionChangedEvent.Broadcast(this);
			}
		}
	}

	/**
	 * @return	Returns whether or not the selection object is currently in the middle of a batch select block.
	 */
	bool IsBatchSelecting() const
	{
		return SelectionMutex != 0;
	}

	/**
	 * Selects the specified object.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 */
	void Select(UObject* InObject);

	/**
	 * Deselects the specified object.
	 *
	 * @param	InObject	The object to deselect.  Must be non-NULL.
	 */
	void Deselect(UObject* InObject);

	/**
	 * Selects or deselects the specified object, depending on the value of the bSelect flag.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 * @param	bSelect		true selects the object, false deselects.
	 */
	void Select(UObject* InObject, bool bSelect);

	/**
	 * Toggles the selection state of the specified object.
	 *
	 * @param	InObject	The object to select/deselect.  Must be non-NULL.
	 */
	void ToggleSelect(UObject* InObject);

	/**
	 * Deselects all objects of the specified class, if no class is specified it deselects all objects.
	 *
	 * @param	InClass		The type of object to deselect.  Can be NULL.
	 */
	void DeselectAll( UClass* InClass = NULL );

	/**
	 * If batch selection is active, sets flag indicating something actually changed.
	 */
	void MarkBatchDirty();

	/**
	 * Returns the first selected object of the specified class.
	 *
	 * @param	InClass				The class of object to return.  Must be non-NULL.
	 * @param	RequiredInterface	[opt] Interface this class must implement to be returned.  May be NULL.
	 * @param	bArchetypesOnly		[opt] true to only return archetype objects, false otherwise
	 * @return						The first selected object of the specified class.
	 */
	UObject* GetTop(UClass* InClass, UClass* RequiredInterface=NULL, bool bArchetypesOnly=false)
	{
		check( InClass );
		for( int32 i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = GetSelectedObject(i);
			if (SelectedObject)
			{
				// maybe filter out non-archetypes
				if ( bArchetypesOnly && !SelectedObject->HasAnyFlags(RF_ArchetypeObject) )
				{
					continue;
				}

				if ( InClass->HasAnyClassFlags(CLASS_Interface) )
				{
					// InClass is an interface, and we want the top object that implements it
					if ( SelectedObject->GetClass()->ImplementsInterface(InClass) )
					{
						return SelectedObject;
					}
				}
				else if ( SelectedObject->IsA(InClass) )
				{
					// InClass is a class, so we want the top object of that class that implements the required interface, if specified
					if ( !RequiredInterface || SelectedObject->GetClass()->ImplementsInterface(RequiredInterface) )
					{
						return SelectedObject;
					}
				}
			}
		}
		return NULL;
	}

	/**
	* Returns the last selected object of the specified class.
	*
	* @param	InClass		The class of object to return.  Must be non-NULL.
	* @return				The last selected object of the specified class.
	*/
	UObject* GetBottom(UClass* InClass)
	{
		check( InClass );
		for( int32 i = SelectedObjects.Num()-1 ; i > -1 ; --i )
		{
			UObject* SelectedObject = GetSelectedObject(i);
			if( SelectedObject && SelectedObject->IsA(InClass) )
			{
				return SelectedObject;
			}
		}
		return NULL;
	}

	/**
	 * Returns the first selected object.
	 *
	 * @return				The first selected object.
	 */
	template< class T > T* GetTop()
	{
		UObject* Selected = GetTop(T::StaticClass());
		return Selected ? CastChecked<T>(Selected) : NULL;
	}

	/**
	* Returns the last selected object.
	*
	* @return				The last selected object.
	*/
	template< class T > T* GetBottom()
	{
		UObject* Selected = GetBottom(T::StaticClass());
		return Selected ? CastChecked<T>(Selected) : NULL;
	}

	/**
	 * Returns true if the specified object is non-NULL and selected.
	 *
	 * @param	InObject	The object to query.  Can be NULL.
	 * @return				true if the object is selected, or false if InObject is unselected or NULL.
	 */
	bool IsSelected(const UObject* InObject) const;

	/**
	 * Returns the number of selected objects of the specified type.
	 *
	 * @param	bIgnorePendingKill	specify true to count only those objects which are not pending kill (marked for garbage collection)
	 * @return						The number of objects of the specified type.
	 */
	template< class T >
	int32 CountSelections( bool bIgnorePendingKill=false )
	{
		int32 Count = 0;
		for( int32 i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = GetSelectedObject(i);
			if( SelectedObject && SelectedObject->IsA(T::StaticClass()) && !(bIgnorePendingKill && SelectedObject->IsPendingKill()) )
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * Untemplated version of CountSelections.
	 */
	int32 CountSelections(UClass *ClassToCount, bool bIgnorePendingKill=false)
	{
		int32 Count = 0;
		for( int32 i=0; i<SelectedObjects.Num(); ++i )
		{
			UObject* SelectedObject = GetSelectedObject(i);
			if( SelectedObject && SelectedObject->IsA(ClassToCount) && !(bIgnorePendingKill && SelectedObject->IsPendingKill()) )
			{
				++Count;
			}
		}
		return Count;
	}

	/**
	 * Gets selected class by index.
	 *
	 * @return			The selected class at the specified index.
	 */
	UClass* GetSelectedClass(int32 InIndex) const
	{
		return SelectedClasses[ InIndex ];
	}

	/**
	 * Sync's all objects' RF_EdSelected flag based on the current selection list.
	 */
	void RefreshObjectFlags();


	// Begin UObject Interface
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	// End UObject Interface


	/**
	 * Fills in the specified array with all selected objects of the desired type.
	 * 
	 * @param	OutSelectedObjects		[out] Array to fill with selected objects of type T
	 * @return							The number of selected objects of the specified type.
	 */
	template< class T > 
	int32 GetSelectedObjects(TArray<T*> &OutSelectedObjects)
	{
		OutSelectedObjects.Empty();
		for ( int32 Idx = 0 ; Idx < SelectedObjects.Num() ; ++Idx )
		{
			UObject* SelectedObject = GetSelectedObject(Idx);
			if ( SelectedObject && SelectedObject->IsA(T::StaticClass()) )
			{
				OutSelectedObjects.Add( (T*)SelectedObject );
			}
		}
		return OutSelectedObjects.Num();
	}
	int32 GetSelectedObjects( ObjectArray& out_SelectedObjects )
	{
		out_SelectedObjects = SelectedObjects;
		return SelectedObjects.Num();
	}

	int32 GetSelectedObjects(UClass *FilterClass, TArray<UObject*> &OutSelectedObjects)
	{
		OutSelectedObjects.Empty();
		for ( int32 Idx = 0 ; Idx < SelectedObjects.Num() ; ++Idx )
		{
			UObject* SelectedObject = GetSelectedObject(Idx);
			if ( SelectedObject && SelectedObject->IsA(FilterClass) )
			{
				OutSelectedObjects.Add( SelectedObject );
			}
		}
		return OutSelectedObjects.Num();
	}

protected:
	/** List of selected objects, ordered as they were selected. */
	ObjectArray	SelectedObjects;

	/** Tracks the most recently selected actor classes.  Used for UnrealEd menus. */
	ClassArray	SelectedClasses;

	/** Tracks the number of active selection operations.  Allows batched selection operations to only send one notification at the end of the batch */
	int32			SelectionMutex;

	/** Tracks whether the selection set changed during a batch selection operation */
	bool		bIsBatchDirty;
	
private:
	// Hide IsSelected(), as calling IsSelected() on a selection set almost always indicates
	// an error where the caller should use IsSelected(UObject* InObject).
	bool IsSelected() const
	{
		return UObject::IsSelected();
	}
};

/**
 * Manages selections of objects.  Used in the editor for selecting
 * objects in the various browser windows.
 */
class FSelectionIterator
{
public:
	FSelectionIterator(USelection& InSelection)
		: Selection( InSelection )
	{
		Reset();
	}

	/** Advances iterator to the next valid element in the container. */
	void operator++()
	{
		while ( true )
		{
			++Index;

			// Halt if the end of the selection set has been reached.
			if ( !IsIndexValid() )
			{
				return;
			}

			// Halt if at a valid object.
			if ( IsObjectValid() )
			{
				return;
			}
		}
	}

	/** Element access. */
	UObject* operator*() const
	{
		return GetCurrentObject();
	}

	/** Element access. */
	UObject* operator->() const
	{
		return GetCurrentObject();
	}

	/** Returns true if the iterator has not yet reached the end of the selection set. */
	operator bool() const
	{
		return IsIndexValid();
	}

	/** Resets the iterator to the beginning of the selection set. */
	void Reset()
	{
		Index = -1;
		++( *this );
	}

	/** Returns an index to the current element. */
	int32 GetIndex() const
	{
		return Index;
	}

private:
	UObject* GetCurrentObject() const
	{
		return Selection.GetSelectedObject(Index);
	}

	bool IsObjectValid() const
	{
		return GetCurrentObject() != NULL;
	}

	bool IsIndexValid() const
	{
		return Selection.SelectedObjects.IsValidIndex( Index );
	}

	USelection&	Selection;
	int32			Index;
};


/**
 * Ensures that only selected objects are marked with what was the RF_EdSelected flag.
 */
void RefreshSelectionSets();
