// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Slate.h"


void SExpanderArrow::Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow  )
{
	OwnerRowPtr = TableRow;
	IndentAmount = InArgs._IndentAmount;

	this->ChildSlot
	.Padding( TAttribute<FMargin>( this, &SExpanderArrow::GetExpanderPadding ) )
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Visibility( this, &SExpanderArrow::GetExpanderVisibility )
		.ClickMethod( EButtonClickMethod::MouseDown )
		.OnClicked( this, &SExpanderArrow::OnArrowClicked )
		.ContentPadding(0.f)
		.ForegroundColor( FSlateColor::UseForeground() )
		.IsFocusable( false )
		[
			SNew(SImage)
			.Image( this, &SExpanderArrow::GetExpanderImage )
			.ColorAndOpacity( FSlateColor::UseForeground() )
		]
	];
}

/** Invoked when the expanded button is clicked (toggle item expansion) */
FReply SExpanderArrow::OnArrowClicked()
{
	// Recurse the expansion if "shift" is being pressed
	const FModifierKeysState ModKeyState = FSlateApplication::Get().GetModifierKeys();
	if(ModKeyState.IsShiftDown())
	{
		OwnerRowPtr.Pin()->Private_OnExpanderArrowShiftClicked();
	}
	else
	{
		OwnerRowPtr.Pin()->ToggleExpansion();
	}

	return FReply::Handled();
}

/** @return Visible when has children; invisible otherwise */
EVisibility SExpanderArrow::GetExpanderVisibility() const
{
	return OwnerRowPtr.Pin()->DoesItemHaveChildren() ? EVisibility::Visible : EVisibility::Hidden;
}

/** @return the margin corresponding to how far this item is indented */
FMargin SExpanderArrow::GetExpanderPadding() const
{
	const int32 NestingDepth = OwnerRowPtr.Pin()->GetIndentLevel();
	const float Indent = IndentAmount.Get(10.f);
	return FMargin( NestingDepth*Indent, 0,0,0 );
}

/** @return the name of an image that should be shown as the expander arrow */
const FSlateBrush* SExpanderArrow::GetExpanderImage() const
{
	const bool bIsItemExpanded = OwnerRowPtr.Pin()->IsItemExpanded();

	FName ResourceName;
	if (bIsItemExpanded)
	{
		if ( ExpanderArrow->IsHovered() )
		{
			static FName ExpandedHoveredName = "TreeArrow_Expanded_Hovered";
			ResourceName = ExpandedHoveredName;
		}
		else
		{
			static FName ExpandedName = "TreeArrow_Expanded";
			ResourceName = ExpandedName;
		}
	}
	else
	{
		if ( ExpanderArrow->IsHovered() )
		{
			static FName CollapsedHoveredName = "TreeArrow_Collapsed_Hovered";
			ResourceName = CollapsedHoveredName;
		}
		else
		{
			static FName CollapsedName = "TreeArrow_Collapsed";
			ResourceName = CollapsedName;
		}
	}

	return FCoreStyle::Get().GetBrush( ResourceName );
}

