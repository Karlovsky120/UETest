// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FGraphActionNode;
struct FCreateWidgetForActionData;

/** Delegate for hooking up an inline editable text block to be notified that a rename is requested. */
DECLARE_DELEGATE( FOnRenameRequestActionNode );
/** Delegate executed when the mouse button goes down */
DECLARE_DELEGATE_RetVal_OneParam( bool, FCreateWidgetMouseButtonDown, TWeakPtr<FEdGraphSchemaAction> );

/** Default widget for GraphActionMenu */
class GRAPHEDITOR_API SDefaultGraphActionWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SDefaultGraphActionWidget ) {}
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FCreateWidgetForActionData* InCreateData);
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/** The item that we want to display with this widget */
	TWeakPtr<FEdGraphSchemaAction> ActionPtr;
	/** Delegate executed when mouse button goes down */
	FCreateWidgetMouseButtonDown MouseButtonDownDelegate;
};

struct FCreateWidgetForActionData
{
	/** True if we want to use the mouse delegate */
	bool bHandleMouseButtonDown;
	/** Delegate for mouse button going down */
	FCreateWidgetMouseButtonDown MouseButtonDownDelegate;
	/** The action being used for the widget */
	TSharedPtr< FEdGraphSchemaAction > Action;
	/** The delegate to determine if the current action is selected in the row */
	FIsSelected IsRowSelectedDelegate;
	/** This will be returned, hooked up to request a rename */
	FOnRenameRequestActionNode* const OnRenameRequest;
	/** The text to highlight */
	TAttribute<FText> HighlightText;
	/** True if the widget should be read only - no renaming allowed */
	bool bIsReadOnly;

	FCreateWidgetForActionData(FOnRenameRequestActionNode* const InOnRenameRequest)
		: OnRenameRequest(InOnRenameRequest)
		, bIsReadOnly(false)
	{

	}
};

struct FCustomExpanderData
{
	/** The menu row associated with the widget being customized */
	TSharedPtr<ITableRow> TableRow;

	/** The action associated with the menu row being customized */
	TSharedPtr<FEdGraphSchemaAction> RowAction;

	/** The widget container that the custom expander will belong to */
	TSharedPtr<SPanel> WidgetContainer;
};

/** Class that displays a list of graph actions and them to be searched and selected */
class GRAPHEDITOR_API SGraphActionMenu : public SCompoundWidget, public FGCObject
{
public:
	/** Delegate that can be used to create a widget for a particular action */
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<SWidget>, FOnCreateWidgetForAction, FCreateWidgetForActionData* const );
	/** Delegate that can be used to create a custom "expander" widget for a particular row */
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<SExpanderArrow>, FOnCreateCustomRowExpander, FCustomExpanderData const& );
	/** Delegate executed when an action is selected */
	DECLARE_DELEGATE_OneParam( FOnActionSelected, const TArray< TSharedPtr<FEdGraphSchemaAction> >& );
	/** Delegate executed when an action is double clicked */
	DECLARE_DELEGATE_OneParam( FOnActionDoubleClicked, const TArray< TSharedPtr<FEdGraphSchemaAction> >& );
	/** Delegate executed when an action is dragged */
	DECLARE_DELEGATE_RetVal_TwoParams( FReply, FOnActionDragged, const TArray< TSharedPtr<FEdGraphSchemaAction> >&, const FPointerEvent& );
	/** Delegate executed when a category is dragged */
	DECLARE_DELEGATE_RetVal_TwoParams( FReply, FOnCategoryDragged, const FString&, const FPointerEvent& );
	/** Delegate executed when the list of all actions needs to be refreshed */
	DECLARE_DELEGATE_OneParam( FOnCollectAllActions, FGraphActionListBuilderBase& );
	/** Delegate executed when a category is being renamed so any post-rename actions can be handled */
	DECLARE_DELEGATE_ThreeParams( FOnCategoryTextCommitted, const FText&, ETextCommit::Type, TWeakPtr< FGraphActionNode >);
	/** Delegate executed to check if the selected action is valid for renaming */
	DECLARE_DELEGATE_RetVal_OneParam( bool, FCanRenameSelectedAction, TWeakPtr< FGraphActionNode > );
	/** Delegate to get the name of a section if the widget is a section separator. */
	DECLARE_DELEGATE_RetVal_OneParam( FText, FGetSectionTitle, int32 );
	/** Delegate to get the filter text */
	DECLARE_DELEGATE_RetVal( FText, FGetFilterText);

	SLATE_BEGIN_ARGS( SGraphActionMenu ) 
		{
			_AutoExpandActionMenu = false;
			_ShowFilterTextBox = true;
			_AlphaSortItems = true;
		}
		SLATE_EVENT( FOnActionSelected, OnActionSelected )
		SLATE_EVENT( FOnActionDoubleClicked, OnActionDoubleClicked )
		SLATE_EVENT( FOnActionDragged, OnActionDragged )
		SLATE_EVENT( FOnCategoryDragged, OnCategoryDragged )
		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )
		SLATE_EVENT( FOnCreateWidgetForAction, OnCreateWidgetForAction )
		SLATE_EVENT( FOnCreateCustomRowExpander, OnCreateCustomRowExpander )
		SLATE_EVENT( FOnCollectAllActions, OnCollectAllActions )
		SLATE_EVENT( FOnCategoryTextCommitted, OnCategoryTextCommitted )
		SLATE_EVENT( FCanRenameSelectedAction, OnCanRenameSelectedAction )
		SLATE_EVENT( FGetSectionTitle, OnGetSectionTitle )		
		SLATE_EVENT( FGetFilterText, OnGetFilterText )
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
		SLATE_ARGUMENT( bool, AlphaSortItems )
		SLATE_ARGUMENT( bool, ShowFilterTextBox )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, bool bIsReadOnly = true );

	// FGCObject override
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE;

	/**
	 * Refreshes the actions that this widget should display
	 *
	 * @param bPreserveExpansion		TRUE if the expansion state of the tree should be preserved
	 * @param bHandleOnSelectionEvent		TRUE if the item should be selected and any actions that occur with selection will be handled. FALSE and only selection will occur */
	void RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent = true);

protected:
	/** Tree view for showing actions */
	TSharedPtr< STreeView< TSharedPtr<FGraphActionNode> > > TreeView;
	/** Text box used for searching for actions */
	TSharedPtr<SSearchBox> FilterTextBox;

	/** List of all actions we can browser */
	FGraphActionListBuilderBase AllActions;

	/** Flattened list of all actions passing the filter */
	TArray< TSharedPtr<FGraphActionNode> > FilteredActionNodes; 

	/** Root of filtered actions tree */
	TSharedPtr<FGraphActionNode> FilteredRootAction;

	/** Used to track selected action for keyboard interaction */
	int32 SelectedSuggestion;
	/** Allows us to set selection (via keyboard) without triggering action */
	bool bIgnoreUIUpdate;
	/** Should we auto-expand categories */
	bool bAutoExpandActionMenu;
	/** Should we display the filter text box */
	bool bShowFilterTextBox;
	/** Don't sort items alphabetically */
	bool bAlphaSortItems;

	/** Delegate to call when action is selected */
	FOnActionSelected OnActionSelected;
	/** Delegate to call when action is double clicked */
	FOnActionDoubleClicked OnActionDoubleClicked;
	/** Delegate to call when an action is dragged. */
	FOnActionDragged OnActionDragged;
	/** Delegate to call when a category is dragged. */
	FOnCategoryDragged OnCategoryDragged;
	/** Delegate to call to create widget for an action */
	FOnCreateWidgetForAction OnCreateWidgetForAction;
	/** Delegate to call for creating a custom "expander" widget for indenting a menu row with */
	FOnCreateCustomRowExpander OnCreateCustomRowExpander;
	/** Delegate to call to collect all actions */
	FOnCollectAllActions OnCollectAllActions;
	/** Delegate to call to handle any post-category rename events */
	FOnCategoryTextCommitted OnCategoryTextCommitted;
	/** Delegate to call to check if a selected action is valid for renaming */
	FCanRenameSelectedAction OnCanRenameSelectedAction;
	/** Delegate to get the name of a section seperator. */
	FGetSectionTitle OnGetSectionTitle;
	/** Delegate to get the filter text if supplied from an external source */
	FGetFilterText OnGetFilterText;
public:
	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent) OVERRIDE;
	// End of SWidget interface

	/** Get filter text box widget */
	TSharedRef<SEditableTextBox> GetFilterTextBox();

	/** Get action that is currently selected */
	void GetSelectedActions(TArray< TSharedPtr<FEdGraphSchemaAction> >& OutSelectedActions) const;

	/** Initiates a rename on the selected action node, if possible */
	void OnRequestRenameOnActionNode();

	/** Queries if a rename on the selected action node is possible */
	bool CanRequestRenameOnActionNode() const;

	/** Get category that is currently selected */
	FString GetSelectedCategoryName() const;
	
	/** Get category child actions that is currently selected */
	void GetSelectedCategorySubActions(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const;

	/** Get category child actions for the passed in action */
	void GetCategorySubActions(TWeakPtr<FGraphActionNode> InAction, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const;

	/*
	 * Selects an non-creation item in the list, searching by FName, deselects if name is none 
	 *
	 * @param	ItemName		The name of the item to select
	 * @param	SelectInfo		The selection type
	 * @return					TRUE if the item was successfully selected or the tree cleared, FALSE if unsuccessful
	 */
	bool SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo = ESelectInfo::Direct );

	/** Expands any category with the associated name */
	void ExpandCategory(const FString& CategoryName);

	/* Handler for mouse button going down */
	bool OnMouseButtonDownEvent( TWeakPtr<FEdGraphSchemaAction> InAction );

	/** Regenerated filtered results (FilteredRootAction and FilteredActionNodes) based on filter text  */ 
	void GenerateFilteredItems(bool bPreserveExpansion);

protected:
	/** Get current filter text */
	FText GetFilterText() const;

	/** Change the selection to reflect the active suggestion */
	void MarkActiveSuggestion();

	/** Try to spawn the node reflected by the active suggestion */
	bool TryToSpawnActiveSuggestion();

	/** Returns true if the tree should be autoexpanded */
	bool ShouldExpandNodes() const;

	/** Checks if the passed in node is safe for renaming */
	bool CanRenameNode(TWeakPtr<FGraphActionNode> InNode) const;

	/** Get the 'weight' of the given action relevance to a given filter list */
	int32 GetActionFilteredWeight( const FGraphActionListBuilderBase::ActionGroup& InCurrentAction, const TArray<FString>& InFilterTerms, const TArray<FString>& InSanitizedFilterTerms );

	// Delegates

	/** Called when filter text changes */
	void OnFilterTextChanged( const FText& InFilterText );
	/** Called when enter is hit in search box */
	void OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	/** Get children  */
	void OnGetChildrenForCategory( TSharedPtr<FGraphActionNode> InItem, TArray< TSharedPtr<FGraphActionNode> >& OutChildren );
	/** Create widget for the supplied node */
	TSharedRef<ITableRow> MakeWidget( TSharedPtr<FGraphActionNode> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bIsReadOnly );

	/*
	 * Called when tree item is selected 
	 *
	 * @param	InSelectedItem	The action node that is being selected
	 * @param	SelectInfo		Selection type - Only OnMouseClick and OnKeyPress will trigger a call to HandleSelection
	 */
	void OnItemSelected( TSharedPtr< FGraphActionNode > InSelectedItem, ESelectInfo::Type SelectInfo );

	/*
	 * Executes the selection delegate providing it has been bound, and the provided action node given is valid and is an action node
	 *
	 * @param InselectedItem	The graph action node selected
	 *
	 * @return true if item selection delegate was executed
	 */
	bool HandleSelection( TSharedPtr< FGraphActionNode > &InSelectedItem );

	/** Called when tree item is double clicked */
	void OnItemDoubleClicked( TSharedPtr< FGraphActionNode > InClickedItem );
	/** Called when tree item dragged */
	FReply OnItemDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );
	/** Callback when rename text is committed */
	void OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, TWeakPtr< FGraphActionNode > InAction);
	/** Handler for when an item has scrolled into view after having been requested to do so */
	void OnItemScrolledIntoView( TSharedPtr<FGraphActionNode> InActionNode, const TSharedPtr<ITableRow>& InWidget );	
};

