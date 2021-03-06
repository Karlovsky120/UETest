// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "SBehaviorTreeDiff.h"
#include "GraphDiffControl.h"
#include "EdGraphUtilities.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

#define LOCTEXT_NAMESPACE "SBehaviorTreeDiff"

//////////////////////////////////////////////////////////////////////////
// FDiffResultItem

struct FDiffResultItem: public TSharedFromThis<FDiffResultItem>
{
	/**
	 * Constructor
	 * @param InResult A difference result 
	 */
	FDiffResultItem(const FDiffSingleResult& InResult): Result(InResult){}

	/**
	 * GenerateWidget for the diff item
	 * @return The Widget
	 */
	TSharedRef<SWidget>	GenerateWidget() const
	{
		FString ToolTip = Result.ToolTip;
		FLinearColor Color = Result.DisplayColor;
		FString Text = Result.DisplayString;
		if(Text.Len() == 0)
		{
			Text = LOCTEXT("DIF_UnknownDiff", "Unknown Diff").ToString();
			ToolTip = LOCTEXT("DIF_Confused", "There is an unspecified difference").ToString();
		}
		return SNew(STextBlock)
			.ToolTipText(ToolTip)
			.ColorAndOpacity(Color)
			.Text(Text);
	}

	// A result of a diff
	const FDiffSingleResult Result;
};


//////////////////////////////////////////////////////////////////////////
// FDiffListCommands

class FDiffListCommands : public TCommands<FDiffListCommands>
{
public:
	/** Constructor */
	FDiffListCommands() 
		: TCommands<FDiffListCommands>("DiffList", LOCTEXT("Diff", "Behavior Tree Diff"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** Initialize commands */
	virtual void RegisterCommands() OVERRIDE
	{
		UI_COMMAND(Previous, "Prev", "Go to previous difference", EUserInterfaceActionType::Button, FInputGesture(EKeys::F7, EModifierKey::Control));
		UI_COMMAND(Next, "Next", "Go to next difference", EUserInterfaceActionType::Button, FInputGesture(EKeys::F7));
	}

	/** Go to previous difference */
	TSharedPtr<FUICommandInfo> Previous;

	/** Go to next difference */
	TSharedPtr<FUICommandInfo> Next;
};

//////////////////////////////////////////////////////////////////////////
// SBehaviorTreeDiff

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SBehaviorTreeDiff::Construct( const FArguments& InArgs )
{
	LastPinTarget = NULL;
	LastOtherPinTarget = NULL;

	FDiffListCommands::Register();

	PanelOld.BehaviorTree = InArgs._BehaviorTreeOld;
	PanelNew.BehaviorTree = InArgs._BehaviorTreeNew;

	PanelOld.RevisionInfo = InArgs._OldRevision;
	PanelNew.RevisionInfo = InArgs._NewRevision;

	PanelOld.bShowAssetName = InArgs._ShowAssetNames;
	PanelNew.bShowAssetName = InArgs._ShowAssetNames;

	OpenInDefaults = InArgs._OpenInDefaults;

	TSharedRef<SHorizontalBox> DefaultEmptyPanel = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BehaviorTreeDifGraphsToolTip", "Select Graph to Diff").ToString())
		];

	this->ChildSlot
	[	
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Content()
		[
			SNew(SSplitter)
			+SSplitter::Slot()
			.Value(0.2f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						//open in p4dif tool
						SNew(SButton)
						.OnClicked(this, &SBehaviorTreeDiff::OnOpenInDefaults)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DifBehaviorTreeDefaults", "Default Diff").ToString())
						]
					]
					+SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						GenerateDiffListWidget()
					]
				]
			]
			+SSplitter::Slot()
			.Value(0.8f)
			[
				// Diff Window
				SNew(SSplitter)
				+SSplitter::Slot()
				.Value(0.5f)
				[
					// Left Diff
					SAssignNew(PanelOld.GraphEditorBorder, SBorder)
					.VAlign(VAlign_Fill)
					[
						DefaultEmptyPanel
					]
				]
				+SSplitter::Slot()
				.Value(0.5f)
				[
					// Right Diff
					SAssignNew(PanelNew.GraphEditorBorder, SBorder)
					.VAlign(VAlign_Fill)
					[
						DefaultEmptyPanel
					]
				]
			]
		]
	];

	PanelOld.GeneratePanel(PanelOld.BehaviorTree->BTGraph, PanelNew.BehaviorTree->BTGraph);
	PanelNew.GeneratePanel(PanelNew.BehaviorTree->BTGraph, PanelOld.BehaviorTree->BTGraph);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SBehaviorTreeDiff::OnOpenInDefaults()
{
	OpenInDefaults.ExecuteIfBound(PanelOld.BehaviorTree, PanelNew.BehaviorTree);
	return FReply::Handled();
}

TSharedRef<SWidget> SBehaviorTreeDiff::GenerateDiffListWidget()
{
	BuildDiffSourceArray();
	if(DiffListSource.Num() > 0)
	{
		struct FSortDiff
		{
			bool operator () (const FSharedDiffOnGraph& A, const FSharedDiffOnGraph& B) const
			{
				return A->Result.Diff < B->Result.Diff;
			}
		};
		Sort(DiffListSource.GetTypedData(),DiffListSource.Num(), FSortDiff());

		// Map commands through UI
		const FDiffListCommands& Commands = FDiffListCommands::Get();
		KeyCommands = MakeShareable(new FUICommandList );

		KeyCommands->MapAction(Commands.Previous, FExecuteAction::CreateSP(this, &SBehaviorTreeDiff::PrevDiff));
		KeyCommands->MapAction(Commands.Next, FExecuteAction::CreateSP(this, &SBehaviorTreeDiff::NextDiff));

		FToolBarBuilder ToolbarBuilder(KeyCommands.ToSharedRef(), FMultiBoxCustomization::None);
		ToolbarBuilder.AddToolBarButton(Commands.Previous, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintDif.PrevDiff"));
		ToolbarBuilder.AddToolBarButton(Commands.Next, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintDif.NextDiff"));

		TSharedRef<SHorizontalBox> Result =	SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.MaxWidth(350.f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0.f)
			.AutoHeight()
			[
				ToolbarBuilder.MakeWidget()
			]
			+SVerticalBox::Slot()
			.Padding(0.f)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("PropertyWindow.CategoryBackground"))
				.Padding(FMargin(2.0f))
				.ForegroundColor(FEditorStyle::GetColor("PropertyWindow.CategoryForeground"))
				.ToolTipText(LOCTEXT("BehvaiorTreeDifDifferencesToolTip", "List of differences found between revisions, click to select"))
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RevisionDifferences", "Revision Differences"))
				]
			]
			+SVerticalBox::Slot()
			.Padding(1.f)
			.FillHeight(1.f)
			[
				SAssignNew(DiffList, SListViewType)
				.ItemHeight(24)
				.ListItemsSource(&DiffListSource)
				.OnGenerateRow(this, &SBehaviorTreeDiff::OnGenerateRow)
				.SelectionMode(ESelectionMode::Single)
				.OnSelectionChanged(this, &SBehaviorTreeDiff::OnSelectionChanged)
			]
		];
		return Result;
	}
	else
	{
		return SNew(SBorder).Visibility(EVisibility::Hidden);
	}
}

void SBehaviorTreeDiff::BuildDiffSourceArray()
{
	TArray<FDiffSingleResult> FoundDiffs;
	FGraphDiffControl::DiffGraphs(PanelOld.BehaviorTree->BTGraph, PanelNew.BehaviorTree->BTGraph, FoundDiffs);

	DiffListSource.Empty();
	for (auto DiffIt(FoundDiffs.CreateConstIterator()); DiffIt; ++DiffIt)
	{
		DiffListSource.Add(FSharedDiffOnGraph(new FDiffResultItem(*DiffIt)));
	}
}

void SBehaviorTreeDiff::NextDiff()
{
	int32 Index = (GetCurrentDiffIndex() + 1) % DiffListSource.Num();
	DiffList->SetSelection(DiffListSource[Index]);
}

void SBehaviorTreeDiff::PrevDiff()
{
	int32 Index = GetCurrentDiffIndex();
	if(Index == 0)
	{
		Index = DiffListSource.Num() - 1;
	}
	else
	{
		Index = (Index - 1) % DiffListSource.Num();
	}
	DiffList->SetSelection(DiffListSource[Index]);
}

int32 SBehaviorTreeDiff::GetCurrentDiffIndex() 
{
	auto Selected = DiffList->GetSelectedItems();
	if(Selected.Num() == 1)
	{	
		int32 Index = 0;
		for(auto It(DiffListSource.CreateIterator());It;++It,Index++)
		{
			if(*It == Selected[0])
			{
				return Index;
			}
		}
	}
	return 0;
}

TSharedRef<ITableRow> SBehaviorTreeDiff::OnGenerateRow(FSharedDiffOnGraph Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return	SNew(STableRow< FSharedDiffOnGraph >, OwnerTable)
			.Content()
			[
				Item->GenerateWidget()
			];
}

void SBehaviorTreeDiff::OnSelectionChanged(FSharedDiffOnGraph Item, ESelectInfo::Type SelectionType)
{
	DisablePinDiffFocus();

	if(!Item.IsValid())
	{
		return;
	}

	//focus the graph onto the diff that was clicked on
	FDiffSingleResult Result = Item->Result;
	if(Result.Pin1)
	{
		PanelNew.GraphEditor.Pin()->ClearSelectionSet();
		PanelOld.GraphEditor.Pin()->ClearSelectionSet();

		if(Result.Pin1)
		{
			LastPinTarget = Result.Pin1;
			Result.Pin1->bIsDiffing = true;
			GetGraphEditorForGraph(Result.Pin1->GetOwningNode()->GetGraph())->JumpToPin(Result.Pin1);
		}

		if(Result.Pin2)
		{
			LastOtherPinTarget = Result.Pin2;
			Result.Pin2->bIsDiffing = true;
			GetGraphEditorForGraph(Result.Pin2->GetOwningNode()->GetGraph())->JumpToPin(Result.Pin2);
		}
	}
	else if(Result.Node1)
	{
		PanelNew.GraphEditor.Pin()->ClearSelectionSet();
		PanelOld.GraphEditor.Pin()->ClearSelectionSet();

		if(Result.Node2)
		{
			GetGraphEditorForGraph(Result.Node2->GetGraph())->JumpToNode(Result.Node2, false);
		}
		GetGraphEditorForGraph(Result.Node1->GetGraph())->JumpToNode(Result.Node1, false);
	}
}

SGraphEditor* SBehaviorTreeDiff::GetGraphEditorForGraph(UEdGraph* Graph) const
{
	if(PanelOld.GraphEditor.Pin()->GetCurrentGraph() == Graph)
	{
		return PanelOld.GraphEditor.Pin().Get();
	}
	else if(PanelNew.GraphEditor.Pin()->GetCurrentGraph() == Graph)
	{
		return PanelNew.GraphEditor.Pin().Get();
	}
	checkNoEntry();
	return NULL;
}

void SBehaviorTreeDiff::DisablePinDiffFocus()
{
	if(LastPinTarget)
	{
		LastPinTarget->bIsDiffing = false;
	}
	if(LastOtherPinTarget)
	{
		LastOtherPinTarget->bIsDiffing = false;
	}
}


//////////////////////////////////////////////////////////////////////////
// FBehaviorTreeDiffPanel
//////////////////////////////////////////////////////////////////////////

SBehaviorTreeDiff::FBehaviorTreeDiffPanel::FBehaviorTreeDiffPanel()
{
	BehaviorTree = NULL;
}

void SBehaviorTreeDiff::FBehaviorTreeDiffPanel::GeneratePanel(UEdGraph* Graph, UEdGraph* GraphToDiff)
{
	TSharedPtr<SWidget> Widget = SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text( LOCTEXT("BTDifPanelNoGraphTip", "Graph does not exist in this revision").ToString())
		];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( false, false, true, true, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	DetailsView->SetObject( NULL );
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateRaw(this, &SBehaviorTreeDiff::FBehaviorTreeDiffPanel::IsPropertyVisible));
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateRaw(this, &SBehaviorTreeDiff::FBehaviorTreeDiffPanel::IsPropertyEditable));
	
	if(Graph)
	{
		SGraphEditor::FGraphEditorEvents InEvents;
		InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateRaw(this, &SBehaviorTreeDiff::FBehaviorTreeDiffPanel::OnSelectionChanged);

		FGraphAppearanceInfo AppearanceInfo;
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_BehaviorDif", "DIFF").ToString();

		if (!GraphEditorCommands.IsValid())
		{
			GraphEditorCommands = MakeShareable(new FUICommandList());

			GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
				FExecuteAction::CreateRaw(this, &FBehaviorTreeDiffPanel::CopySelectedNodes),
				FCanExecuteAction::CreateRaw(this, &FBehaviorTreeDiffPanel::CanCopyNodes));
		}

		auto Editor = SNew(SGraphEditor)
			.AdditionalCommands(GraphEditorCommands)
			.GraphToEdit(Graph)
			.GraphToDiff(GraphToDiff)
			.IsEditable(false)
			.TitleBarEnabledOnly(false)
			.TitleBar(SNew(SBorder).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(GetTitle())
			])
			.Appearance(AppearanceInfo)
			.GraphEvents(InEvents);

		const FSlateBrush* ContentAreaBrush = FEditorStyle::GetBrush( "Docking.Tab", ".ContentAreaBrush" );

		auto NewWidget = SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+SSplitter::Slot()
			.Value(0.8f)
			[
				Editor
			]
			+SSplitter::Slot()
			.Value(0.2f)
			[
				SNew( SBorder )
				.Visibility( EVisibility::Visible )
				.BorderImage( ContentAreaBrush )
				[
					DetailsView.ToSharedRef()
				]
			];

		GraphEditor = Editor;
		Widget = NewWidget;
	}

	GraphEditorBorder->SetContent(Widget.ToSharedRef());
}

FString SBehaviorTreeDiff::FBehaviorTreeDiffPanel::GetTitle() const
{
	FString Title = LOCTEXT("CurrentRevision", "Current Revision").ToString();

	// if this isn't the current working version being displayed
	if (RevisionInfo.Revision >= 0)
	{
		FString DateString = RevisionInfo.Date.ToString(TEXT("%m/%d/%Y"));
		if (bShowAssetName)
		{
			FString AssetName = BehaviorTree->GetName();
			FText LocalizedFormat = LOCTEXT("NamedRevisionDiff", "%s - Revision %i, CL %i, %s");
			Title = FString::Printf(*LocalizedFormat.ToString(), *AssetName, RevisionInfo.Revision, RevisionInfo.Changelist, *DateString);
		}
		else
		{
			FText LocalizedFormat = LOCTEXT("PreviousRevisionDif", "Revision %i, CL %i, %s");
			Title = FString::Printf(*LocalizedFormat.ToString(), RevisionInfo.Revision, RevisionInfo.Changelist, *DateString);
		}
	}
	else if (bShowAssetName)
	{
		FString AssetName = BehaviorTree->GetName();
		FText LocalizedFormat = LOCTEXT("NamedCurrentRevision", "%s - Current Revision");
		Title = FString::Printf(*LocalizedFormat.ToString(), *AssetName);
	}

	return Title;
}

FGraphPanelSelectionSet SBehaviorTreeDiff::FBehaviorTreeDiffPanel::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = GraphEditor.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void SBehaviorTreeDiff::FBehaviorTreeDiffPanel::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformMisc::ClipboardCopy(*ExportedText);
}

bool SBehaviorTreeDiff::FBehaviorTreeDiffPanel::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void SBehaviorTreeDiff::FBehaviorTreeDiffPanel::OnSelectionChanged( const FGraphPanelSelectionSet& NewSelection )
{
	TArray<UObject*> Selection;
	if(NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			UBehaviorTreeGraphNode_Composite* GraphNode_Composite = Cast<UBehaviorTreeGraphNode_Composite>(*SetIt);
			if (GraphNode_Composite)
			{
				Selection.Add(GraphNode_Composite->NodeInstance);
				continue;
			}

			UBehaviorTreeGraphNode_Task* GraphNode_Task = Cast<UBehaviorTreeGraphNode_Task>(*SetIt);
			if (GraphNode_Task)
			{
				Selection.Add(GraphNode_Task->NodeInstance);
				continue;
			}
			Selection.Add(*SetIt);
		}
	}

	if (Selection.Num() == 1)
	{
		if (DetailsView.IsValid())
		{
			DetailsView->SetObjects(Selection);
		}
	}
	else if (DetailsView.IsValid())
	{
		DetailsView->SetObject(NULL);
	}
}

bool SBehaviorTreeDiff::FBehaviorTreeDiffPanel::IsPropertyVisible(UProperty const * const InProperty) const
{
	return !InProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
}

bool SBehaviorTreeDiff::FBehaviorTreeDiffPanel::IsPropertyEditable()
{
	return false;
}

#undef LOCTEXT_NAMESPACE