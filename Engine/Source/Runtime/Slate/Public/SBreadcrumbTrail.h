// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once


/**
 * A breadcrumb trail. Allows the user to see his currently selected path and navigate upwards.
 */
template <typename ItemType>
class SBreadcrumbTrail : public SCompoundWidget
{
private:
	/** A container for data associated with a single crumb in the trail. */
	struct FCrumbItem
	{
		int32 CrumbID;
		TSharedRef<SButton> Button;
		TSharedRef<SMenuAnchor> Delimiter;
		ItemType CrumbData;

		FCrumbItem(int32 InCrumbID, TSharedRef<SButton> InButton, TSharedRef<SMenuAnchor> InDelimiter, ItemType InCrumbData)
			: CrumbID(InCrumbID)
			, Button(InButton)
			, Delimiter(InDelimiter)
			, CrumbData(InCrumbData)
		{}
	};

public:
	/** Callback for when a crumb has been pushed on the trail */
	DECLARE_DELEGATE_OneParam( FOnCrumbPushed, const ItemType& /*CrumbData*/ );

	/** Callback for when a crumb has been popped off the trail */
	DECLARE_DELEGATE_OneParam( FOnCrumbPopped, const ItemType& /*CrumbData*/ );

	/** Callback for when a crumb in the trail has been clicked */
	DECLARE_DELEGATE_OneParam( FOnCrumbClicked, const ItemType& /*CrumbData*/ );

	/** Callback for getting the menu content to be displayed when clicking on a crumb's delimiter arrow */
	DECLARE_DELEGATE_RetVal_OneParam( TSharedPtr< SWidget >, FGetCrumbMenuContent, const ItemType& /*CrumbData*/ );


	SLATE_BEGIN_ARGS( SBreadcrumbTrail )
		: _InvertTextColorOnHover(true)
		, _ButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FButtonStyle >( "BreadcrumbButton" ) )
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText") )
		, _ButtonContentPadding(FMargin(4.0, 2.0))
		, _DelimiterImage( FCoreStyle::Get().GetBrush("BreadcrumbTrail.Delimiter") )
		, _ShowLeadingDelimiter(false)
		, _PersistentBreadcrumbs(false)
		, _GetCrumbMenuContent()
	{}

		/** When true, will invert the button text color when a crumb button is hovered */
		SLATE_ARGUMENT( bool, InvertTextColorOnHover )

		/** The name of the style to use for the crumb buttons */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
		
		/** The name of the style to use for the crumb button text */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** The padding for the content in crumb buttons */
		SLATE_ATTRIBUTE( FMargin, ButtonContentPadding )

		/** The image to use between crumb trail buttons */
		SLATE_ATTRIBUTE( const FSlateBrush*, DelimiterImage )

		/** If true, a leading delimiter will be shown */
		SLATE_ATTRIBUTE( bool, ShowLeadingDelimiter )

		/** Called when a crumb is pushed */
		SLATE_EVENT( FOnCrumbPushed, OnCrumbPushed )

		/** Called when a crumb is popped */
		SLATE_EVENT( FOnCrumbPopped, OnCrumbPopped )

		/** Called when a crumb is clicked, after the later crumbs were popped */
		SLATE_EVENT( FOnCrumbClicked, OnCrumbClicked )

		/** If true, do not remove breadcrumbs when clicking */
		SLATE_ARGUMENT( bool, PersistentBreadcrumbs )

		SLATE_EVENT( FGetCrumbMenuContent, GetCrumbMenuContent )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		bInvertTextColorOnHover = InArgs._InvertTextColorOnHover;
		ButtonStyle = InArgs._ButtonStyle;
		TextStyle = InArgs._TextStyle;
		ButtonContentPadding = InArgs._ButtonContentPadding;
		DelimiterImage = InArgs._DelimiterImage;
		ShowLeadingDelimiter = InArgs._ShowLeadingDelimiter;
		OnCrumbPushed = InArgs._OnCrumbPushed;
		OnCrumbPopped = InArgs._OnCrumbPopped;
		OnCrumbClicked = InArgs._OnCrumbClicked;
		bHasStaticBreadcrumbs = InArgs._PersistentBreadcrumbs;
		GetCrumbMenuContentCallback = InArgs._GetCrumbMenuContent;

		NextValidCrumbID = 0;

		ChildSlot
		[
			SAssignNew(CrumbBox, SHorizontalBox)
		];

		AddLeadingDelimiter();
	}

	/** Adds a crumb to the end of the trail.*/
	void PushCrumb(const TAttribute<FText>& CrumbText, const ItemType& NewCrumbData)
	{
		// Create the crumb and add it to the crumb box
		TSharedPtr<SButton> NewButton;
		TSharedPtr<SMenuAnchor> NewDelimiter;

		// Add the crumb button
		CrumbBox->AddSlot()
		.AutoWidth()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight( 1.0f )
			[
				// Crumb Button
				SAssignNew(NewButton, SButton)
				.ButtonStyle(ButtonStyle)
				.ContentPadding(ButtonContentPadding)
				.TextStyle(TextStyle)
				.Text(CrumbText)
				.OnClicked( this, &SBreadcrumbTrail::CrumbButtonClicked, NextValidCrumbID )
				.ForegroundColor( this, &SBreadcrumbTrail::GetButtonForegroundColor, NextValidCrumbID )
			]
		];

		TSharedRef< SWidget > DelimiterContent = SNullWidget::NullWidget;
		if ( GetCrumbMenuContentCallback.IsBound() )
		{
			// Crumb Arrow
			DelimiterContent = SNew(SButton)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.ButtonStyle(ButtonStyle)
				.OnClicked( this, &SBreadcrumbTrail::OnCrumbDelimiterClicked, NextValidCrumbID )
				.ContentPadding( FMargin(5, 0) )
				[
					SNew(SImage)
					.Image(DelimiterImage)
				];
		}
		else
		{
			DelimiterContent = SNew(SButton)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.ButtonStyle(ButtonStyle)
				.Visibility( this, &SBreadcrumbTrail::GetDelimiterVisibility, NextValidCrumbID )
				.ContentPadding( FMargin(5, 0) )
				[
					SNew(SImage)
					.Image(DelimiterImage)
				];
		}

		CrumbBox->AddSlot()
		.AutoWidth()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew( NewDelimiter, SMenuAnchor )
				.OnGetMenuContent( this, &SBreadcrumbTrail::GetCrumbMenuContent, NextValidCrumbID )
				[
					DelimiterContent
				]
			]
		];

		// Push the crumb data
		new (CrumbList) FCrumbItem(NextValidCrumbID, NewButton.ToSharedRef(), NewDelimiter.ToSharedRef(), NewCrumbData);

		// Increment the crumb ID for the next crumb
		NextValidCrumbID = (NextValidCrumbID + 1) % (INT_MAX - 1);

		// Trigger event
		OnCrumbPushed.ExecuteIfBound(NewCrumbData);
	}

	/** Pops a crumb off the end of the trail. Returns the crumb data. */
	ItemType PopCrumb()
	{
		check(HasCrumbs());

		// Remove from the crumb list and box
		FCrumbItem LastCrumbItem = CrumbList.Pop();
		CrumbBox->RemoveSlot(LastCrumbItem.Button);
		CrumbBox->RemoveSlot(LastCrumbItem.Delimiter);

		// Trigger event
		OnCrumbPopped.ExecuteIfBound(LastCrumbItem.CrumbData);

		// Return the popped crumb's data
		return LastCrumbItem.CrumbData;
	}

	/** Peeks at the end crumb in the trail. Returns the crumb data. */
	ItemType PeekCrumb() const
	{
		check(HasCrumbs());

		// Return the last crumb's text
		return CrumbList.Last().CrumbData;
	}

	/** Returns true if there are any crumbs in the trail. */
	bool HasCrumbs() const
	{
		return NumCrumbs() > 0;
	}

	/** Returns the number of crumbs in the trail. */
	int32 NumCrumbs() const
	{
		return CrumbList.Num();
	}

	/** Removes all crumbs from the crumb box */
	void ClearCrumbs(bool bPopAllCrumbsToClear = false)
	{
		if (bPopAllCrumbsToClear)
		{
			while ( HasCrumbs() )
			{
				PopCrumb();
			}
		}
		else
		{
			CrumbBox->ClearChildren();
			CrumbList.Empty();
			
			AddLeadingDelimiter();
		}
	}

	/** Gets all the crumb data in the trail */
	void GetAllCrumbData(TArray<ItemType>& CrumbData) const
	{
		for (int32 CrumbIdx = 0; CrumbIdx < NumCrumbs(); ++CrumbIdx)
		{
			CrumbData.Add(CrumbList[CrumbIdx].CrumbData);
		}
	}

private:

	FReply OnCrumbDelimiterClicked( int32 CrumbID )
	{
		FReply Reply = FReply::Unhandled();

		if ( GetCrumbMenuContentCallback.IsBound() )
		{
			for (int32 CrumbListIdx = 0; CrumbListIdx < NumCrumbs(); ++CrumbListIdx)
			{
				if (CrumbList[CrumbListIdx].CrumbID == CrumbID)
				{
					CrumbList[CrumbListIdx].Delimiter->SetIsOpen( true );
					Reply = FReply::Handled();
					break;
				}
			}
		}

		return Reply;
	}

	TSharedRef< SWidget > GetCrumbMenuContent( int32 CrumbId )
	{
		if ( !GetCrumbMenuContentCallback.IsBound() )
		{
			return SNullWidget::NullWidget;
		}

		TSharedPtr< SWidget > MenuContent;

		for (int32 CrumbListIdx = 0; CrumbListIdx < NumCrumbs(); ++CrumbListIdx)
		{
			if (CrumbList[CrumbListIdx].CrumbID == CrumbId)
			{
				MenuContent = GetCrumbMenuContentCallback.Execute( CrumbList[CrumbListIdx].CrumbData );
				break;
			}
		}
	
		return MenuContent.ToSharedRef();
	}

	/** Handler to determine the visibility of the arrows between crumbs */
	EVisibility GetDelimiterVisibility(int32 CrumbID) const
	{
		if ( HasCrumbs() )
		{
			if (CrumbList.Last().CrumbID == CrumbID)
			{
				// Collapse the last delimiter
				return EVisibility::Collapsed;
			}
		}

		return EVisibility::Visible;
	}

	/** Handler to determine the visibility of the arrow before all crumbs */
	EVisibility GetLeadingDelimiterVisibility() const
	{
		return ShowLeadingDelimiter.Get() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/** Handler to determine the text color of crumb buttons. Will invert the text color if allowed. */
	FSlateColor GetButtonForegroundColor(int32 CrumbID) const
	{
		if ( bInvertTextColorOnHover )
		{
			TSharedPtr<SButton> CrumbButton;

			for (int32 CrumbListIdx = 0; CrumbListIdx < NumCrumbs(); ++CrumbListIdx)
			{
				if (CrumbList[CrumbListIdx].CrumbID == CrumbID)
				{
					CrumbButton = CrumbList[CrumbListIdx].Button;
				}
			}

			if ( CrumbButton.IsValid() && CrumbButton->IsHovered() )
			{
				return FCoreStyle::Get().GetSlateColor("InvertedForeground");
			}
		}

		return FCoreStyle::Get().GetSlateColor("DefaultForeground");
	}

	/** Handler for when a crumb is clicked. Will pop crumbs down to the selected one. */
	FReply CrumbButtonClicked(int32 CrumbID)
	{
		int32 CrumbIdx = INDEX_NONE;

		if (bHasStaticBreadcrumbs)
		{
			for (int32 CrumbListIdx = 0; CrumbListIdx < NumCrumbs(); ++CrumbListIdx)
			{
				if (CrumbList[CrumbListIdx].CrumbID == CrumbID)
				{
					OnCrumbClicked.ExecuteIfBound(CrumbList[CrumbListIdx].CrumbData);
					break;
				}
			}
		}
		else
		{
			for (int32 CrumbListIdx = 0; CrumbListIdx < NumCrumbs(); ++CrumbListIdx)
			{
				if (CrumbList[CrumbListIdx].CrumbID == CrumbID)
				{
					CrumbIdx = CrumbListIdx;
				}
			}

			if ( CrumbIdx != INDEX_NONE )
			{
				while (NumCrumbs() - 1 > CrumbIdx)
				{
					PopCrumb();
				}

				OnCrumbClicked.ExecuteIfBound(CrumbList.Last().CrumbData);
			}
		}
		return FReply::Handled();
	}

	/** Adds a delimiter that is always visible */
	void AddLeadingDelimiter()
	{
		CrumbBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(DelimiterImage)
			.Visibility( this, &SBreadcrumbTrail::GetLeadingDelimiterVisibility )
		];
	}

private:

	/** The horizontal box which contains all the breadcrumbs */
	TSharedPtr<SHorizontalBox> CrumbBox;

	/** The list of crumbs and their data */
	TArray<FCrumbItem> CrumbList;

	/** The next ID to assign to a crumb when it is created */
	int32 NextValidCrumbID;

	/** When true, will invert the button text color when a crumb button is hovered */
	bool bInvertTextColorOnHover;

	/** The button style to apply to all crumbs */
	const FButtonStyle* ButtonStyle;

	/** The text style to apply to all crumbs */
	const FTextBlockStyle* TextStyle;

	/** The padding for the content in crumb buttons */
	TAttribute<FMargin> ButtonContentPadding;

	/** The image to display between crumb trail buttons */
	TAttribute< const FSlateBrush* > DelimiterImage;

	/** Delegate to invoke when a crumb is pushed. */
	FOnCrumbPushed OnCrumbPushed;

	/** Delegate to invoke when a crumb is popped. */
	FOnCrumbPopped OnCrumbPopped;

	/** Delegate to invoke when selection changes. */
	FOnCrumbClicked OnCrumbClicked;

	/** Delegate to invoke to retrieve the content for a crumb's menu */
	FGetCrumbMenuContent GetCrumbMenuContentCallback;

	/** If true, a leading delimiter will be added */
	TAttribute<bool> ShowLeadingDelimiter;

	/** If true, don't dynamically remove items when clicking */
	bool bHasStaticBreadcrumbs;
};
