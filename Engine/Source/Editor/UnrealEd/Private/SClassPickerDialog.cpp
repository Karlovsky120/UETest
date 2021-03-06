// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "Editor/ClassViewer/Private/SClassViewer.h"
#include "SClassPickerDialog.h"

#define LOCTEXT_NAMESPACE "SClassPicker"

void SClassPickerDialog::Construct(const FArguments& InArgs)
{
	WeakParentWindow = InArgs._ParentWindow;

	bPressedOk = false;
	ChosenClass = NULL;

	ClassViewer = StaticCastSharedRef<SClassViewer>(FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(InArgs._Options, FOnClassPicked::CreateSP(this,&SClassPickerDialog::OnClassPicked)));

	// Load in default settings
	for(int ClassIdx = 0; ClassIdx < GUnrealEd->GetUnrealEdOptions()->NewAssetDefaultClasses.Num(); ++ClassIdx)
	{
		FClassPickerDefaults& DefaultObj = GUnrealEd->GetUnrealEdOptions()->NewAssetDefaultClasses[ClassIdx];
		UClass* AssetType = LoadClass<UObject>(NULL, *DefaultObj.AssetClass, NULL, LOAD_None, NULL);
		if (InArgs._AssetType->IsChildOf(AssetType))
		{
			AssetDefaultClasses.Add(MakeShareable(new FClassPickerDefaults(DefaultObj)));
		}
	}

	bool bHasDefaultClasses = AssetDefaultClasses.Num() > 0;
	bool bExpandDefaultClassPicker = true;
	bool bExpandCustomClassPicker = !bHasDefaultClasses;

	if(bHasDefaultClasses)
	{
		GConfig->GetBool(TEXT("/Script/UnrealEd.UnrealEdOptions"), TEXT("bExpandClassPickerDefaultClassList"), bExpandDefaultClassPicker, GEditorIni);
		GConfig->GetBool(TEXT("/Script/UnrealEd.UnrealEdOptions"), TEXT("bExpandCustomClassPickerClassList"), bExpandCustomClassPicker, GEditorIni);
	}

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.WidthOverride(500.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(!bExpandDefaultClassPicker)
					.AreaTitle(NSLOCTEXT("SClassPickerDialog", "StandardClassesAreaTitle", "Standard Classes"))
					.OnAreaExpansionChanged(this, &SClassPickerDialog::OnDefaultAreaExpansionChanged)
					.BodyContent()
					[
						SNew(SListView < TSharedPtr<FClassPickerDefaults>  >)
						.ItemHeight(24)
						.SelectionMode(ESelectionMode::None)
						.ListItemsSource(&AssetDefaultClasses)
						.OnGenerateRow(this, &SClassPickerDialog::GenerateListRow)
						.Visibility(bHasDefaultClasses? EVisibility::Visible: EVisibility::Collapsed)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SExpandableArea)
					.MaxHeight(385.f)
					.InitiallyCollapsed(!bExpandCustomClassPicker)
					.AreaTitle(NSLOCTEXT("SClassPickerDialog", "CustomClassesAreaTitle", "Custom Classes"))
					.OnAreaExpansionChanged(this, &SClassPickerDialog::OnCustomAreaExpansionChanged)
					.BodyContent()
					[
						ClassViewer.ToSharedRef()
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(8)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton)
						.Text(NSLOCTEXT("SClassPickerDialog", "ClassPickerSelectButton", "Select").ToString())
						.HAlign(HAlign_Center)
						.Visibility( this, &SClassPickerDialog::GetSelectButtonVisibility )
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SClassPickerDialog::OnClassPickerConfirmed)
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton)
						.Text(NSLOCTEXT("SClassPickerDialog", "ClassPickerCancelButton", "Cancel").ToString())
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SClassPickerDialog::OnClassPickerCanceled)
					]
				]
			]
		]
	];
}

bool SClassPickerDialog::PickClass(const FText& TitleText, const FClassViewerInitializationOptions& ClassViewerOptions, UClass*& OutChosenClass, UClass* AssetType)
{
	// Create the window to pick the class
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule( ESizingRule::Autosized )
		.ClientSize( FVector2D( 0.f, 300.f ));

	TSharedRef<SClassPickerDialog> ClassPickerDialog = SNew(SClassPickerDialog)
		.ParentWindow(PickerWindow)
		.Options(ClassViewerOptions)
		.AssetType(AssetType);

	PickerWindow->SetContent(ClassPickerDialog);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (ClassPickerDialog->bPressedOk)
	{
		OutChosenClass = ClassPickerDialog->ChosenClass;
		return true;
	}
	else
	{
		// Ok was not selected, NULL the class
		OutChosenClass = NULL;
		return false;
	}
}

void SClassPickerDialog::OnClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
}

TSharedRef<ITableRow> SClassPickerDialog::GenerateListRow(TSharedPtr<FClassPickerDefaults> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FClassPickerDefaults* Obj = InItem.Get();
	UClass* ItemClass = LoadClass<UObject>(NULL, *Obj->ClassName, NULL, LOAD_None, NULL);

	return 
	SNew(STableRow< TSharedPtr<FClassPickerDefaults> >, OwnerTable)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.MaxHeight(30.0f)
		.Padding(0.0f, 6.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.45f)
			[
				SNew(SButton)
				.OnClicked(this, &SClassPickerDialog::OnDefaultClassPicked, ItemClass)
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.FillWidth(0.12f)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush(Obj->Image))
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					.FillWidth(0.8f)
					[
						SNew(STextBlock)
						.Text(Obj->GetName())
					]
				]
			]
			+SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f)
			.MaxWidth(500.0f)
			[
				SNew(STextBlock)
				.Text(FString::Printf(*NSLOCTEXT("EditorFactories", "FClassPickerDefaults", "%s").ToString(), *Obj->GetDescription()))
				.AutoWrapText(true)
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 4.0f)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]
	];
}

FReply SClassPickerDialog::OnDefaultClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
	bPressedOk = true;
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SClassPickerDialog::OnClassPickerConfirmed()
{
	if (ChosenClass == NULL)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("EditorFactories", "MustChooseClassWarning", "You must choose a class."));
	}
	else
	{
		bPressedOk = true;

		if (WeakParentWindow.IsValid())
		{
			WeakParentWindow.Pin()->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}

FReply SClassPickerDialog::OnClassPickerCanceled()
{
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

void SClassPickerDialog::OnDefaultAreaExpansionChanged(bool bExpanded)
{
	if (bExpanded && WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin().Get()->SetWidgetToFocusOnActivate(ClassViewer);
	}

	if (AssetDefaultClasses.Num() > 0)
	{
		GConfig->SetBool(TEXT("/Script/UnrealEd.UnrealEdOptions"), TEXT("bExpandClassPickerDefaultClassList"), bExpanded, GEditorIni);
	}
}

void SClassPickerDialog::OnCustomAreaExpansionChanged(bool bExpanded)
{
	if (bExpanded && WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin().Get()->SetWidgetToFocusOnActivate(ClassViewer);
	}

	if (AssetDefaultClasses.Num() > 0)
	{
		GConfig->SetBool(TEXT("/Script/UnrealEd.UnrealEdOptions"), TEXT("bExpandCustomClassPickerClassList"), bExpanded, GEditorIni);
	}
}

EVisibility SClassPickerDialog::GetSelectButtonVisibility() const
{
	EVisibility Visibility = EVisibility::Hidden;
	if( ChosenClass != NULL )
	{
		Visibility = EVisibility::Visible;
	}
	return Visibility;
}

/** Overridden from SWidget: Called when a key is pressed down - capturing copy */
FReply SClassPickerDialog::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	WeakParentWindow.Pin().Get()->SetWidgetToFocusOnActivate(ClassViewer);

	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		return OnClassPickerCanceled();
	}
	else
	{
		return ClassViewer->OnKeyDown(MyGeometry, InKeyboardEvent);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
