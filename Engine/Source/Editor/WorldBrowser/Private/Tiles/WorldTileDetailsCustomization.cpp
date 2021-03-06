// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

#include "SWorldDetailsView.h"
#include "SPropertyEditorLevelPackage.h"
#include "WorldTileDetails.h"
#include "WorldTileCollectionModel.h"
#include "WorldTileDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

// Width for level package selection combo box
static const float LevelPackageWidgetMinDesiredWidth = 1000.f;

static FString GetWorldRoot(TWeakPtr<FWorldTileCollectionModel> WorldModel)
{
	auto PinnedWorldData = WorldModel.Pin();
	if (PinnedWorldData.IsValid())
	{
		return PinnedWorldData->GetWorld()->WorldComposition->GetWorldRoot();
	}
	else
	{
		return FPackageName::FilenameToLongPackageName(FPaths::GameContentDir());
	}
}

static bool HasLODSuffix(const FString& InPackageName)
{
	return InPackageName.Contains(WORLDTILE_LOD_PACKAGE_SUFFIX, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
}

static bool IsPersistentLevel(const FString& InPackageName, const TSharedPtr<FWorldTileCollectionModel>& InWorldData)
{
	if (InWorldData.IsValid() && InWorldData->GetWorld())
	{
		if (InWorldData->GetWorld()->GetOutermost()->GetName() == InPackageName)
		{
			return true;
		}
	}
	
	return false;
}

/////////////////////////////////////////////////////
// FWorldTileDetails 
TSharedRef<IDetailCustomization> FWorldTileDetailsCustomization::MakeInstance(TSharedRef<FWorldTileCollectionModel> InWorldModel)
{
	TSharedRef<FWorldTileDetailsCustomization> Instance = MakeShareable(new FWorldTileDetailsCustomization());
	Instance->WorldModel = InWorldModel;
	return Instance;
}

void FWorldTileDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	// Property editable state
	TAttribute<bool> IsPropertyEnabled = TAttribute<bool>::Create(
		TAttribute<bool>::FGetter::CreateSP(this, &FWorldTileDetailsCustomization::IsPropertyEditable)
		);
	
	IDetailCategoryBuilder& TileCategory = DetailLayoutBuilder.EditCategory("Tile");

	// Set properties state
	{
		// Package Name
		TileCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, PackageName));
	
		// Parent Package Name
		{
			auto ParentPackagePropertyHandle = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, ParentPackageName));
			TileCategory.AddProperty(ParentPackagePropertyHandle)
				.IsEnabled(IsPropertyEnabled)
				.CustomWidget()
					.NameContent()
					[
						ParentPackagePropertyHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
						.MinDesiredWidth(LevelPackageWidgetMinDesiredWidth)
					[
						SNew(SPropertyEditorLevelPackage, ParentPackagePropertyHandle)
							.RootPath(GetWorldRoot(WorldModel))
							.SortAlphabetically(true)
							.OnShouldFilterPackage(this, &FWorldTileDetailsCustomization::OnShouldFilterParentPackage)
					];
		}

		// Position
		TileCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, Position))
			.IsEnabled(IsPropertyEnabled);

		// Absolute Position
		TileCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, AbsolutePosition))
			.IsEnabled(false);

		// Z Order
		TileCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, ZOrder))
			.IsEnabled(IsPropertyEnabled);
		
		// Streaming levels
		TileCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, StreamingLevels))
			.IsEnabled(IsPropertyEnabled);

		// bTileEditable (invisible property to control other properties editable state)
		TileEditableHandle = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, bTileEditable));
		TileCategory.AddProperty(TileEditableHandle)
			.Visibility(EVisibility::Hidden);
	}
	
	// LOD
	IDetailCategoryBuilder& LODSettingsCategory = DetailLayoutBuilder.EditCategory("LODSettings");
	{
		NumLODHandle = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, NumLOD));
		LODSettingsCategory.AddProperty(NumLODHandle)
			.IsEnabled(IsPropertyEnabled);
		
		LODSettingsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, LOD1))
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FWorldTileDetailsCustomization::GetLODPropertyVisibility, 1)));

		LODSettingsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, LOD2))
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FWorldTileDetailsCustomization::GetLODPropertyVisibility, 2)));

		LODSettingsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, LOD3))
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FWorldTileDetailsCustomization::GetLODPropertyVisibility, 3)));

		LODSettingsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UWorldTileDetails, LOD4))
			.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FWorldTileDetailsCustomization::GetLODPropertyVisibility, 4)));
	}
}

bool FWorldTileDetailsCustomization::IsPropertyEditable() const
{
	// Properties are editable if at least one tile is editable
	bool bTileEditable = true;
	if (TileEditableHandle->GetValue(bTileEditable) != FPropertyAccess::Fail)
	{
		return bTileEditable;
	}
	
	return false;
}

EVisibility FWorldTileDetailsCustomization::GetLODPropertyVisibility(int LODIndex) const
{
	int32 NumLOD = MAX_int32;
	if (NumLODHandle->GetValue(NumLOD) != FPropertyAccess::Fail)
	{
		return (NumLOD >= LODIndex ? EVisibility::Visible : EVisibility::Hidden);
	}
	
	return EVisibility::Hidden;
}

bool FWorldTileDetailsCustomization::OnShouldFilterParentPackage(const FString& InPackageName)
{
	// Filter out LOD levels and persistent level
	if (HasLODSuffix(InPackageName) || IsPersistentLevel(InPackageName, WorldModel.Pin()))
	{
		return true;
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FStreamingLevelDetailsCustomization
TSharedRef<IStructCustomization> FStreamingLevelDetailsCustomization::MakeInstance(TSharedRef<FWorldTileCollectionModel> InWorldData) 
{
	TSharedRef<FStreamingLevelDetailsCustomization> Instance = MakeShareable(new FStreamingLevelDetailsCustomization());
	Instance->WorldModel = InWorldData;
	return Instance;
}

void FStreamingLevelDetailsCustomization::CustomizeStructHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, 
																class FDetailWidgetRow& HeaderRow, 
																IStructCustomizationUtils& StructCustomizationUtils )
{
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FStreamingLevelDetailsCustomization::CustomizeStructChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, 
																	class IDetailChildrenBuilder& ChildBuilder, 
																	IStructCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> StreamingModeProperty = StructPropertyHandle->GetChildHandle(TEXT("StreamingMode"));
	TSharedPtr<IPropertyHandle> PackageNameProperty = StructPropertyHandle->GetChildHandle(TEXT("PackageName"));

	ChildBuilder.AddChildProperty(StreamingModeProperty.ToSharedRef());
	
	ChildBuilder.AddChildProperty(PackageNameProperty.ToSharedRef())
		.CustomWidget()
				.NameContent()
				[
					PackageNameProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
					.MinDesiredWidth(LevelPackageWidgetMinDesiredWidth)
				[
					SNew(SPropertyEditorLevelPackage, PackageNameProperty)
						.RootPath(GetWorldRoot(WorldModel))
						.SortAlphabetically(true)
						.OnShouldFilterPackage(this, &FStreamingLevelDetailsCustomization::OnShouldFilterStreamingPackage)
				];

}

bool FStreamingLevelDetailsCustomization::OnShouldFilterStreamingPackage(const FString& InPackageName) const
{
	// Filter out LOD levels and persistent level
	if (HasLODSuffix(InPackageName) || IsPersistentLevel(InPackageName, WorldModel.Pin()))
	{
		return true;
	}
	
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FTileLODEntryDetailsCustomization
TSharedRef<IStructCustomization> FTileLODEntryDetailsCustomization::MakeInstance(TSharedRef<FWorldTileCollectionModel> InWorldData) 
{
	TSharedRef<FTileLODEntryDetailsCustomization> Instance = MakeShareable(new FTileLODEntryDetailsCustomization());
	Instance->WorldModel = InWorldData;
	return Instance;
}

void FTileLODEntryDetailsCustomization::CustomizeStructHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, 
																class FDetailWidgetRow& HeaderRow, 
																IStructCustomizationUtils& StructCustomizationUtils )
{
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew( SBox )
			.HAlign( HAlign_Center )
			[
				SNew(SButton)
				.Text(LOCTEXT("Generate", "Generate"))
				.OnClicked(this, &FTileLODEntryDetailsCustomization::OnGenerateTile)
				.IsEnabled(this, &FTileLODEntryDetailsCustomization::IsGenerateTileEnabled)
			]
		];
}

void FTileLODEntryDetailsCustomization::CustomizeStructChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, 
																	class IDetailChildrenBuilder& ChildBuilder, 
																	IStructCustomizationUtils& StructCustomizationUtils )
{
	LODIndexHandle = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FTileLODEntryDetails, LODIndex)
		);
	
	TSharedPtr<IPropertyHandle> DistanceProperty = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FTileLODEntryDetails, Distance)
		);
	
	TSharedPtr<IPropertyHandle> DetailsPercentage = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FTileLODEntryDetails, DetailsPercentage)
		);
	
	TSharedPtr<IPropertyHandle> MaxDeviation = StructPropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FTileLODEntryDetails, MaxDeviation)
		);

	ChildBuilder.AddChildProperty(LODIndexHandle.ToSharedRef())
		.Visibility(EVisibility::Hidden);

	ChildBuilder.AddChildProperty(DistanceProperty.ToSharedRef());
	// Reduction settings available if editor supports LOD levels generation
	ChildBuilder.AddChildProperty(DetailsPercentage.ToSharedRef())
		.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FTileLODEntryDetailsCustomization::IsGenerateTileEnabled)));
	// Useless now
	ChildBuilder.AddChildProperty(MaxDeviation.ToSharedRef())
		.Visibility(EVisibility::Hidden);
}

FReply FTileLODEntryDetailsCustomization::OnGenerateTile()
{
	int32 LODIndex;
	if (LODIndexHandle->GetValue(LODIndex) == FPropertyAccess::Success)
	{
		TSharedPtr<FWorldTileCollectionModel> PinnedWorldModel = WorldModel.Pin();
		if (PinnedWorldModel.IsValid())
		{
			FLevelModelList LevelsList = PinnedWorldModel->GetSelectedLevels();
			for (auto Level : LevelsList)
			{
				PinnedWorldModel->GenerateLODLevel(Level, LODIndex);
			}
		}
	}
	
	return FReply::Handled();
}

bool FTileLODEntryDetailsCustomization::IsGenerateTileEnabled() const
{
	TSharedPtr<FWorldTileCollectionModel> PinnedWorldModel = WorldModel.Pin();
	if (PinnedWorldModel.IsValid())
	{
		return PinnedWorldModel->HasGenerateLODLevelSupport();
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE