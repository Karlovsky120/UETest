// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerPrivatePCH.h"

#include "MessageLog.h"
#include "SSocketChooser.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerTreeItems.h"

#define LOCTEXT_NAMESPACE "SSceneOutliner"

namespace SceneOutliner
{
	/* Struct used for validation of a drag/drop operation in the scene outliner */
	struct FDragValidationInfo
	{
		/* The tooltip type to display on the operation */
		FActorDragDropGraphEdOp::ToolTipTextType TooltipType;

		/* The tooltip text to display on the operation */
		FText ValidationText;

		/* Construct this validation information out of a tootip type and some text */
		FDragValidationInfo(const FActorDragDropGraphEdOp::ToolTipTextType InTooltipType, const FText InValidationText)
			: TooltipType(InTooltipType)
			, ValidationText(InValidationText)
		{}

		/* Return a generic invalid result */
		static FDragValidationInfo Invalid()
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText());
		}
		
		/* @return true if this operation is valid, false otheriwse */ 
		bool IsValid() const
		{
			switch(TooltipType)
			{
			case FActorDragDropGraphEdOp::ToolTip_Compatible:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleAttach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleDetach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach:
				return true;
			default:
				return false;
			}
		}
	};

	void ExtractDraggedActorsAndFolders(TSharedPtr<FDragDropOperation> Operation, const TArray<TWeakObjectPtr<AActor>>*& DraggedActors, const TArray<TWeakPtr<TOutlinerFolderTreeItem>>*& DraggedFolders)
	{
		if (!Operation.IsValid())
		{
			return;
		}

		if (DragDrop::IsTypeMatch<FFolderActorDragDropOp>(Operation))
		{
			DraggedFolders = &StaticCastSharedPtr<FFolderActorDragDropOp>(Operation)->Folders;
		}
		else if (DragDrop::IsTypeMatch<FFolderDragDropOp>(Operation))
		{
			DraggedFolders = &StaticCastSharedPtr<FFolderDragDropOp>(Operation)->Folders;
		}

		if (DragDrop::IsTypeMatch<FActorDragDropGraphEdOp>(Operation))
		{
			DraggedActors = &StaticCastSharedPtr<FActorDragDropGraphEdOp>(Operation)->Actors;
		}
	}

	/** Compute validation information for dropping folder(s) onto another folder */
	static FDragValidationInfo ValidateDropOnFolder(TSharedRef<SSceneOutliner> SceneOutlinerRef, const TArray<TWeakPtr<TOutlinerFolderTreeItem>>& DragFolders, FName DropTargetPath)
	{
		// Iterate over all the folders that have been dragged
		for (auto FolderIt = DragFolders.CreateConstIterator(); FolderIt; ++FolderIt)
		{
			auto DragFolderPtr = FolderIt->Pin();
			const FString DragFolderPath = DragFolderPtr->Path.ToString();

			if (DragFolderPtr.IsValid())
			{
				const FString LeafName = DragFolderPtr->LeafName.ToString();
				const FString DstFolderPath = DropTargetPath.IsNone() ? FString() : DropTargetPath.ToString();
				const FString NewPath = DstFolderPath / LeafName;

				if (SceneOutlinerRef->FindFolderByPath(FName(*NewPath)).IsValid())
				{
					// This operation will cause a duplicate folder name so we must merge the folders together
					FFormatNamedArguments Args;
					Args.Add(TEXT("DragPath"), FText::FromString(DragFolderPath));
					Args.Add(TEXT("DropPath"), FText::FromString(NewPath));
					return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric,
						FText::Format(LOCTEXT("FolderMerge", "Merge \"{DragPath}\" into \"{DropPath}\""), Args));
				}
				else if (DragFolderPath == DstFolderPath || DstFolderPath.StartsWith(DragFolderPath + "/"))
				{
					// Cannot drag as a child of itself
					FFormatNamedArguments Args;
					Args.Add(TEXT("FolderPath"), FText::FromName(DragFolderPtr->Path));
					return FDragValidationInfo(
						FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric,
						FText::Format(LOCTEXT("ChildOfItself", "Cannot move \"{FolderPath}\" to be a child of itself"), Args));
				}
			}
		}

		// Everything else is a valid operation
		if (DropTargetPath.IsNone())
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, LOCTEXT("MoveToRoot", "Move to root"));
		}
		else
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, FText::Format(LOCTEXT("MoveInto", "Move into \"{DropPath}\""), FText::FromName(DropTargetPath)));
		}
	}

	/** Compute validation information for dropping actor(s) onto another actor */
	static FDragValidationInfo ValidateDropOnActor(const TArray<TWeakObjectPtr<AActor>>& DragActors, const AActor& DropTarget)
	{
		FText AttachErrorMsg;
		bool CanAttach = true;
		bool DraggedOntoAttachmentParent = true;

		for (auto ActorIt = DragActors.CreateConstIterator(); ActorIt; ++ActorIt)
		{
			AActor* DragActor = ActorIt->Get();
			if (DragActor)
			{
				if (!GEditor->CanParentActors(&DropTarget, DragActor, &AttachErrorMsg))
				{
					CanAttach = false;
				}
				if (DragActor->GetAttachParentActor() != &DropTarget)
				{
					DraggedOntoAttachmentParent = false;
				}
			}
		}

		if (DraggedOntoAttachmentParent)
		{
			if (DragActors.Num() == 1)
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleDetach, FText::FromString(DropTarget.GetActorLabel()));
			}
			else
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach, FText::FromString(DropTarget.GetActorLabel()));
			}
		}
		else if (CanAttach)
		{
			if (DragActors.Num() == 1)
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleAttach, FText::FromString(DropTarget.GetActorLabel()));
			}
			else
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach, FText::FromString(DropTarget.GetActorLabel()));
			}
		}
		else
		{
			if (DragActors.Num() == 1)
			{
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, AttachErrorMsg);
			}
			else
			{
				const FText ReasonText = FText::Format(LOCTEXT("DropOntoText", "{DropTarget}. {AttachMessage}"), FText::FromString(DropTarget.GetActorLabel()), AttachErrorMsg);
				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleMultipleAttach, ReasonText);
			}
		}
	}

	/** Validate a drop event that wants to drop at the root of the tree */
	static FDragValidationInfo ValidateRootDragDropEvent(TSharedRef<SSceneOutliner> SceneOutlinerRef, const FDragDropEvent& DragDropEvent)
	{
		const TArray<TWeakPtr<TOutlinerFolderTreeItem>>* DraggedFolders = nullptr;
		const TArray<TWeakObjectPtr<AActor>>* DraggedActors = nullptr;

		ExtractDraggedActorsAndFolders(DragDropEvent.GetOperation(), DraggedActors, DraggedFolders);

		if (DraggedFolders && DraggedFolders->Num())
		{
			// Dropping folders at the root
			const FDragValidationInfo ValidationInfo = ValidateDropOnFolder(SceneOutlinerRef, *DraggedFolders, FName());

			// If the operation is invalid, or we don't have any actors to check for, return immediately
			// If it is valid then we need to drop down and validate the actors below.
			if (!ValidationInfo.IsValid() || !DraggedActors || DraggedActors->Num() == 0)
			{
				return ValidationInfo;
			}
		}

		if (DraggedActors)
		{
			// Actors are always valid
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, LOCTEXT("MoveToRoot", "Move to root"));
		}

		return FDragValidationInfo::Invalid();
	}

	/** Validate a drop event that wants to drop onto the specified destination row */
	static FDragValidationInfo ValidateDragDropEvent(TSharedRef<SSceneOutliner> SceneOutlinerRef, const FDragDropEvent& DragDropEvent, TSharedRef<TOutlinerTreeItem> Destination)
	{
		// Only allow dropping objects when in actor browsing mode
		if (SceneOutlinerRef->GetInitOptions().Mode != ESceneOutlinerMode::ActorBrowsing)
		{
			return FDragValidationInfo::Invalid();
		}

		//	--------------------------------------------------------------------------------
		//	Validate dragging a mixture of actors and folders inside the scene outliner
		const TArray<TWeakPtr<TOutlinerFolderTreeItem>>* DraggedFolders = nullptr;
		const TArray<TWeakObjectPtr<AActor>>* DraggedActors = nullptr;

		ExtractDraggedActorsAndFolders(DragDropEvent.GetOperation(), DraggedActors, DraggedFolders);

		if (DraggedFolders)
		{
			if (DraggedFolders->Num())
			{
				if (Destination->Type == TOutlinerTreeItem::Actor)
				{
					return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, LOCTEXT("FoldersOnActorError", "Cannot attach folders to actors"));
				}
				else
				{
					// Dropping folders onto a folder
					auto DragFolder = StaticCastSharedRef<TOutlinerFolderTreeItem>(Destination);

					const FDragValidationInfo FolderValidation = ValidateDropOnFolder(SceneOutlinerRef, *DraggedFolders, DragFolder->Path);

					// If the operation is invalid, or we don't have any actors to check for, return immediately
					// If it is valid then we need to drop down and validate the actors below.
					if (!FolderValidation.IsValid() || !DraggedActors || DraggedActors->Num() == 0)
					{
						return FolderValidation;
					}
				}
			}
		}

		//	--------------------------------------------------------------------------------
		//	Validate dragging actors on the scene outliner
		if (DraggedActors)
		{
			if (Destination->Type == TOutlinerTreeItem::Actor)
			{
				// Dragged into an actor
				auto ActorTreeItem = StaticCastSharedRef<TOutlinerActorTreeItem>(Destination);
				AActor* DropActor = ActorTreeItem->Actor.Get();
				if (DropActor)
				{
					return ValidateDropOnActor(*DraggedActors, *DropActor);
				}
			}
			else
			{
				// Entered a folder - it's always valid to drop things into a folder
				FFormatNamedArguments Args;
				Args.Add(TEXT("DestName"), FText::FromName(StaticCastSharedRef<TOutlinerFolderTreeItem>(Destination)->LeafName));

				FText Text;
				if (DraggedActors->Num() == 1)
				{
					AActor* DragActor = DraggedActors->GetTypedData()[0].Get();
					if (DragActor)
					{
						Args.Add(TEXT("SourceName"), FText::FromString(DragActor->GetActorLabel()));
						Text = FText::Format(LOCTEXT("MoveToFolderSingle", "Move {SourceName} into to {DestName}"), Args);
					}
				}
				
				if (Text.IsEmpty())
				{
					Text = FText::Format(LOCTEXT("MoveToFolderMulti", "Move items into to {DestName}"), Args);
				}

				return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, Text);
			}
		}

		return FDragValidationInfo::Invalid();
	}

	void SOutlinerTreeView::Construct(const SOutlinerTreeView::FArguments& InArgs, TSharedRef<SSceneOutliner> Owner)
	{
		SceneOutlinerWeak = Owner;
		STreeView::Construct(InArgs);
	}

	void SOutlinerTreeView::FlashHighlightOnItem( FOutlinerTreeItemPtr FlashHighlightOnItem )
	{
		TSharedPtr< SSceneOutlinerTreeRow > RowWidget = StaticCastSharedPtr< SSceneOutlinerTreeRow >( WidgetGenerator.GetWidgetForItem( FlashHighlightOnItem ) );
		if( RowWidget.IsValid() )
		{
			TSharedPtr< SSceneOutlinerItemWidget > ItemWidget = RowWidget->GetItemWidget();
			if( ItemWidget.IsValid() )
			{
				ItemWidget->FlashHighlight();
			}
		}
	}

	FReply SOutlinerTreeView::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
		if (!SceneOutlinerPtr.IsValid())
		{
			return FReply::Handled();
		}


		if (DragDrop::IsTypeMatch<FDecoratedDragDropOp>(DragDropEvent.GetOperation()))
		{
			const FDragValidationInfo ValidationInfo = ValidateRootDragDropEvent(SceneOutlinerPtr.ToSharedRef(), DragDropEvent);

			if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric || ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric)
			{
				auto DecoratedDragOp = StaticCastSharedPtr<FDecoratedDragDropOp>(DragDropEvent.GetOperation());
				DecoratedDragOp->CurrentHoverText = ValidationInfo.ValidationText.ToString();
				if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric)
				{
					DecoratedDragOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
				}
				else
				{
					DecoratedDragOp->CurrentIconBrush = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
				}
			}
			else if (DragDrop::IsTypeMatch<FActorDragDropGraphEdOp>(DragDropEvent.GetOperation()))
			{
				auto ActorDragOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(DragDropEvent.GetOperation());
				ActorDragOp->SetToolTip(ValidationInfo.TooltipType, ValidationInfo.ValidationText.ToString());
			}
		}

		return FReply::Handled();
	}
	
	void SOutlinerTreeView::OnDragLeave(const FDragDropEvent& DragDropEvent)
	{
		if( !SceneOutlinerWeak.Pin()->GetInitOptions().bShowParentTree )
		{
			// We only allowing dropping to perform attachments when displaying the actors
			// in their parent/child hierarchy
			return;
		}

		if(DragDrop::IsTypeMatch<FDecoratedDragDropOp>( DragDropEvent.GetOperation()))
		{
			StaticCastSharedPtr<FDecoratedDragDropOp>(DragDropEvent.GetOperation())->ResetToDefaultToolTip();
		}
	}

	FReply SOutlinerTreeView::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
		if (!SceneOutlinerPtr.IsValid() ||  !SceneOutlinerPtr->GetInitOptions().bShowParentTree)
		{
			// We only allowing dropping to perform attachments when displaying the actors in their parent/child hierarchy
			return FReply::Unhandled();
		}

		// Only allow dropping objects when in actor browsing mode
		if (SceneOutlinerPtr->GetInitOptions().Mode != ESceneOutlinerMode::ActorBrowsing )
		{
			return FReply::Handled();
		}

		const FScopedTransaction Transaction(LOCTEXT("UndoAction_DropOnSceneOutliner", "Drop on Scene Outliner"));
		
		const TArray<TWeakPtr<TOutlinerFolderTreeItem>>* DraggedFolders = nullptr;
		const TArray<TWeakObjectPtr<AActor>>* DraggedActors = nullptr;
		ExtractDraggedActorsAndFolders(DragDropEvent.GetOperation(), DraggedActors, DraggedFolders);

		if (DraggedFolders)
		{
			for (auto& Folder : *DraggedFolders)
			{
				auto FolderPtr = Folder.Pin();
				if (FolderPtr.IsValid())
				{
					// Put it at the root
					FolderPtr->MoveTo(FName());
				}
			}
		}

		if (DraggedActors)
		{
			for (auto& Actor : *DraggedActors)
			{
				auto* ActorPtr = Actor.Get();
				if (ActorPtr)
				{
					ActorPtr->SetFolderPath(FName());
				}
			}
		}

		return FReply::Handled();
	}

	FReply SSceneOutlinerTreeRow::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
	{
		auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
		if (!SceneOutlinerPtr.IsValid() ||  !SceneOutlinerPtr->GetInitOptions().bShowParentTree)
		{
			// We only allowing dropping to perform attachments when displaying the actors
			// in their parent/child hierarchy
			return FReply::Unhandled();
		}

		// Validate now to make sure we don't doing anything we shouldn't
		const FDragValidationInfo ValidationInfo = ValidateDragDropEvent(SceneOutlinerPtr.ToSharedRef(), DragDropEvent, Item.ToSharedRef());
		if (!ValidationInfo.IsValid())
		{
			return FReply::Handled();
		}

		const FScopedTransaction Transaction(LOCTEXT("UndoAction_DropOnSceneOutliner", "Drop on Scene Outliner"));

		const TArray<TWeakPtr<TOutlinerFolderTreeItem>>* DraggedFolders = nullptr;
		const TArray<TWeakObjectPtr<AActor>>* DraggedActors = nullptr;

		ExtractDraggedActorsAndFolders(DragDropEvent.GetOperation(), DraggedActors, DraggedFolders);

		// Drop on a folder?
		if (Item->Type == TOutlinerTreeItem::Folder && DraggedFolders)
		{
			auto DropFolder = StaticCastSharedPtr<TOutlinerFolderTreeItem>(Item);

			for (auto& FolderIt : *DraggedFolders)
			{
				auto FolderItemPtr = FolderIt.Pin();
				if (FolderItemPtr.IsValid())
				{
					FolderItemPtr->MoveTo(DropFolder->Path);
				}
			}
		}

		if (DraggedActors)
		{
			// Dragged some actors, deal with those now
			if (Item->Type == TOutlinerTreeItem::Actor)
			{
				// Dropped on an actor
				auto ActorTreeItem = StaticCastSharedPtr<TOutlinerActorTreeItem>(Item);
				AActor* DropActor = ActorTreeItem->Actor.Get();
				if (!DropActor)
				{
					return FReply::Handled();
				}

				FMessageLog EditorErrors("EditorErrors");
				EditorErrors.NewPage(LOCTEXT("ActorAttachmentsPageLabel", "Actor attachment"));

				if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach || ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleDetach)
				{
					PerformDetachment(ActorTreeItem->Actor);
				}
				else if (ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach || ValidationInfo.TooltipType == FActorDragDropGraphEdOp::ToolTip_CompatibleAttach)
				{
					// Show socket chooser if we have sockets to select

					//@TODO: Should create a menu for each component that contains sockets, or have some form of disambiguation within the menu (like a fully qualified path)
					// Instead, we currently only display the sockets on the root component
					USceneComponent* Component = DropActor->GetRootComponent();
					if ((Component != NULL) && (Component->HasAnySockets()))
					{
						TWeakObjectPtr<AActor> Actor = ActorTreeItem->Actor;
						// Create the popup
						FSlateApplication::Get().PushMenu(
							SceneOutlinerPtr.ToSharedRef(),
							SNew(SSocketChooserPopup)
							.SceneComponent( Component )
							.OnSocketChosen(this, &SSceneOutlinerTreeRow::PerformAttachment, Actor, *DraggedActors ),
							FSlateApplication::Get().GetCursorPos(),
							FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
							);
					}
					else
					{
						PerformAttachment(NAME_None, ActorTreeItem->Actor, *DraggedActors);
					}
				}

				// Report errors
				EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));
			}
			else
			{
				// Dropped on a folder
				auto FolderTreeItem = StaticCastSharedPtr<TOutlinerFolderTreeItem>(Item);
				for (auto& ActorIt : *DraggedActors)
				{
					auto* ActorPtr = ActorIt.Get();
					if (ActorPtr)
					{
						ActorPtr->SetFolderPath(FolderTreeItem->Path);
					}
				}
			}
		}

		return FReply::Handled();
	}

	void SSceneOutlinerTreeRow::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
	{
		auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
		if( !SceneOutlinerPtr.IsValid() || !SceneOutlinerPtr->GetInitOptions().bShowParentTree )
		{
			// We only allowing dropping to perform attachments when displaying the actors
			// in their parent/child hierarchy
			return;
		}

		// Perform the validation and update the tooltip if necessary
		const FDragValidationInfo ValidationInfo = ValidateDragDropEvent(SceneOutlinerPtr.ToSharedRef(), DragDropEvent, Item.ToSharedRef());

		if (DragDrop::IsTypeMatch<FActorDragDropGraphEdOp>(DragDropEvent.GetOperation()))
		{
			auto ActorDragOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(DragDropEvent.GetOperation());
			ActorDragOp->SetToolTip(ValidationInfo.TooltipType, ValidationInfo.ValidationText.ToString());
		}
		else if (DragDrop::IsTypeMatch<FDecoratedDragDropOp>(DragDropEvent.GetOperation()))
		{
			auto DecoratedDragOp = StaticCastSharedPtr<FDecoratedDragDropOp>(DragDropEvent.GetOperation());
			DecoratedDragOp->CurrentHoverText = ValidationInfo.ValidationText.ToString();
			DecoratedDragOp->CurrentIconBrush = ValidationInfo.IsValid() ? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		}
	}

	void SSceneOutlinerTreeRow::PerformAttachment( FName SocketName, TWeakObjectPtr<AActor> ParentActorPtr, const TArray< TWeakObjectPtr<AActor> > ChildrenPtrs )
	{
		AActor* ParentActor = ParentActorPtr.Get();
		if(ParentActor != NULL)
		{
			// modify parent and child
			const FScopedTransaction Transaction( LOCTEXT("UndoAction_PerformAttachment", "Attach actors") );

			// Attach each child
			bool bAttached = false;
			for(int32 i=0; i<ChildrenPtrs.Num(); i++)
			{
				AActor* ChildActor = ChildrenPtrs[i].Get();
				if (GEditor->CanParentActors(ParentActor, ChildActor))
				{
					GEditor->ParentActors(ParentActor, ChildActor, SocketName);
					bAttached = true;
				}

			}

			// refresh the tree, and ensure parent is expanded so we can still see the child if we attached something
			if (bAttached)
			{
				SceneOutlinerWeak.Pin()->ExpandActor(ParentActor);
			}
		}
	}

	void SSceneOutlinerTreeRow::PerformDetachment( TWeakObjectPtr<AActor> ParentActorPtr )
	{
		AActor* ParentActor = ParentActorPtr.Get();
		if(ParentActor != NULL)
		{
			bool bAttached = GEditor->DetachSelectedActors();
			// refresh the tree, and ensure parent is expanded so we can still see the child if we attached something
			if (bAttached)
			{
				SceneOutlinerWeak.Pin()->ExpandActor(ParentActor);
			}
		}
	}

	FReply SSceneOutlinerTreeRow::HandleDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
		{
			auto SceneOutlinerPtr = SceneOutlinerWeak.Pin();
			if (!SceneOutlinerPtr.IsValid())
			{
				return FReply::Unhandled();
			}

			// We only support drag and drop when in actor browsing mode
			if( SceneOutlinerPtr->GetInitOptions().Mode == ESceneOutlinerMode::ActorBrowsing )
			{
				auto SelectedItems = SceneOutlinerPtr->GetSelectedItems();
				return FReply::Handled().BeginDragDrop(SceneOutliner::CreateDragDropOperation(SelectedItems));
			}
		}

		return FReply::Unhandled();
	}

	FReply SSceneOutlinerTreeRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FReply Reply = SMultiColumnTableRow<FOutlinerTreeItemPtr>::OnMouseButtonDown( MyGeometry, MouseEvent );

			// We only support drag and drop when in actor browsing mode
			if( SceneOutlinerWeak.Pin()->GetInitOptions().Mode == ESceneOutlinerMode::ActorBrowsing )
			{
				return Reply.DetectDrag( SharedThis(this) , EKeys::LeftMouseButton );
			}

			return Reply.PreventThrottling();
		}

		return FReply::Unhandled();
	}

	TSharedPtr< SSceneOutlinerItemWidget > SSceneOutlinerTreeRow::GetItemWidget()
	{
		return ItemWidget;
	}

	TSharedRef<SWidget> SSceneOutlinerTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
	{
		// Create the widget for this item
		TSharedRef< SWidget > NewItemWidget = SceneOutlinerWeak.Pin()->GenerateWidgetForItemAndColumn( Item, ColumnName, FIsSelected::CreateSP(this, &SSceneOutlinerTreeRow::IsSelectedExclusively) );
		if( ColumnName == ColumnID_ActorLabel )
		{
			ItemWidget = StaticCastSharedPtr<SSceneOutlinerItemWidget>( TSharedPtr< SWidget >(NewItemWidget) );
		}

		if( ColumnName == ColumnID_ActorLabel )
		{
			// The first column gets the tree expansion arrow for this row
			return 
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew( SExpanderArrow, SharedThis(this) ).IndentAmount(12)
				]

			+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					NewItemWidget
				];
		}
		else
		{
			// Other columns just get widget content -- no expansion arrow needed
			return NewItemWidget;
		}
	}

	void SSceneOutlinerTreeRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		SceneOutlinerWeak = InArgs._SceneOutliner;
		Item = InArgs._Item;

		SMultiColumnTableRow< FOutlinerTreeItemPtr >::Construct(
			FSuperRowType::FArguments()
			.OnDragDetected(this, &SSceneOutlinerTreeRow::HandleDragDetected)
			.Style(&FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
		,InOwnerTableView );
	}
}

#undef LOCTEXT_NAMESPACE
