// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ExternalImagePickerPrivatePCH.h"
#include "SExternalImagePicker.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "EditorStyle.h"
#include "SEnableBox.h"

#define LOCTEXT_NAMESPACE "ExternalImagePicker"

void SExternalImagePicker::Construct(const FArguments& InArgs)
{
	TargetImagePath = InArgs._TargetImagePath;
	DefaultImagePath = InArgs._DefaultImagePath;
	Extension = FPaths::GetExtension(InArgs._TargetImagePath).ToUpper();
	OnExternalImagePicked = InArgs._OnExternalImagePicked;
	OnGetPickerPath = InArgs._OnGetPickerPath;
	MaxDisplayedImageDimensions = InArgs._MaxDisplayedImageDimensions;
	bRequiresSpecificSize = InArgs._RequiresSpecificSize;
	RequiredImageDimensions = InArgs._RequiredImageDimensions;

	TSharedPtr<SHorizontalBox> HorizontalBox = nullptr;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::Get().GetBrush("ExternalImagePicker.ThumbnailShadow"))
				.Padding(5.0f)
				.Content()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::Get().GetBrush("ExternalImagePicker.BlankImage"))
					.Padding(0.0f)
					.Content()
					[
						SNew(SBox)
						.WidthOverride(this, &SExternalImagePicker::GetImageWidth)
						.HeightOverride(this, &SExternalImagePicker::GetImageHeight)
						[
							SNew(SEnableBox)
							[
								SNew(SImage)
								.Image(this, &SExternalImagePicker::GetImage)
								.ToolTipText(this, &SExternalImagePicker::GetImageTooltip)
							]
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.ToolTipText( LOCTEXT( "FileButtonToolTipText", "Choose a file from this computer").ToString() )
				.OnClicked( FOnClicked::CreateSP(this, &SExternalImagePicker::OnPickFile) )
				.ContentPadding( 2.0f )
				.ForegroundColor( FSlateColor::UseForeground() )
				.IsFocusable( false )
				[
					SNew( SImage )
					.Image( FEditorStyle::Get().GetBrush("ExternalImagePicker.PickImageButton") )
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ErrorHintWidget, SErrorText)
		]
	];

	if(HorizontalBox.IsValid() && DefaultImagePath.Len() > 0)
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SResetToDefaultMenu)
				.OnResetToDefault(FSimpleDelegate::CreateSP(this, &SExternalImagePicker::HandleResetToDefault))
				.OnGetResetToDefaultText(FOnGetResetToDefaultText::CreateSP(this, &SExternalImagePicker::GetResetToDefaultText))
				.DiffersFromDefault(this, &SExternalImagePicker::DiffersFromDefault)
			];
	}

	ApplyImage();
}

const FSlateBrush* SExternalImagePicker::GetImage() const
{
	return ImageBrush.IsValid() ? ImageBrush.Get() : FEditorStyle::Get().GetBrush("ExternalImagePicker.BlankImage");
}

FText SExternalImagePicker::GetImageTooltip() const
{
	const FVector2D Size = ImageBrush.IsValid() ? ImageBrush->ImageSize : FVector2D::ZeroVector;
	
	FText XText = FText::AsNumber((int32)Size.X);
	FText YText = FText::AsNumber((int32)Size.Y);

	FText ImageTooltip;
	switch (TypeOfImage)
	{
	case UsingDummyPlaceholderImage:
		ImageTooltip = LOCTEXT("ImageTooltip_Missing", "Warning: No Image Available!");
		break;
	case UsingDefaultImage:
		ImageTooltip = FText::Format(LOCTEXT("ImageTooltip_Default", "Default Image\n({0})\nDimensions: {1} x {2}"), FText::FromString(DefaultImagePath), XText, YText);
		break;
	case UsingTargetImage:
		ImageTooltip = FText::Format(LOCTEXT("ImageTooltip_Target", "Target Image\n({0})\nDimensions: {1} x {2}"), FText::FromString(TargetImagePath), XText, YText);
		break;
	}
	
	return ImageTooltip;
}

FVector2D SExternalImagePicker::GetConstrainedImageSize() const
{
	const FVector2D Size = GetImage()->ImageSize;

	// make sure we have a valid size in case the image didn't get loaded
	const FVector2D ValidSize( FMath::Max(Size.X, 32.0f), FMath::Max(Size.Y, 32.0f) );

	// keep image aspect but don't display it above a reasonable size
	const float ImageAspect = ValidSize.X / ValidSize.Y;
	const FVector2D ConstrainedSize( FMath::Min(ValidSize.X, MaxDisplayedImageDimensions.X), FMath::Min(ValidSize.Y, MaxDisplayedImageDimensions.Y) );
	return FVector2D( FMath::Min(ConstrainedSize.X, ConstrainedSize.Y * ImageAspect), FMath::Min(ConstrainedSize.Y, ConstrainedSize.X / ImageAspect) );
}

FOptionalSize SExternalImagePicker::GetImageWidth() const
{
	return GetConstrainedImageSize().X;
}

FOptionalSize SExternalImagePicker::GetImageHeight() const
{
	return GetConstrainedImageSize().Y;
}

void SExternalImagePicker::ApplyImage()
{
	ErrorHintWidget->SetError(FText::GetEmpty());
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*TargetImagePath))
	{
		TypeOfImage = UsingTargetImage;
		ApplyImage(TargetImagePath);
	}
	else if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*DefaultImagePath))
	{
		TypeOfImage = UsingDefaultImage;
		ApplyImage(DefaultImagePath);
	}
	else
	{
		TypeOfImage = UsingDummyPlaceholderImage;

		if (bRequiresSpecificSize)
		{
			ErrorHintWidget->SetError(FText::Format(
				LOCTEXT("BadSizeNoImageHint", "No image at '{0}' ({1}x{2})"),
				FText::FromString(TargetImagePath),
				FText::AsNumber(RequiredImageDimensions.X),
				FText::AsNumber(RequiredImageDimensions.Y)
				));
		}
		else
		{
			ErrorHintWidget->SetError(FText::Format(LOCTEXT("NoImageErrorHint", "No image at '{0}'"), FText::FromString(TargetImagePath)));
		}

		ApplyPlaceholderImage();
	}
}

void SExternalImagePicker::ApplyImage(const FString& ImagePath)
{
	if (ImageBrush.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->ReleaseDynamicResource(*ImageBrush.Get());
		ImageBrush.Reset();
	}

	ImageBrush = LoadImageAsBrush(ImagePath);
}

void SExternalImagePicker::ApplyPlaceholderImage()
{
	if (ImageBrush.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->ReleaseDynamicResource(*ImageBrush.Get());
	}

	ImageBrush.Reset();
}

TSharedPtr< FSlateDynamicImageBrush > SExternalImagePicker::LoadImageAsBrush( const FString& ImagePath )
{
	TSharedPtr< FSlateDynamicImageBrush > Brush;
	bool bSucceeded = false;
	
	TArray<uint8> RawFileData;
	if( FFileHelper::LoadFileToArray( RawFileData, *ImagePath ) )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		IImageWrapperPtr ImageWrappers[3] =
		{ 
			ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG ),
			ImageWrapperModule.CreateImageWrapper( EImageFormat::BMP ),
			ImageWrapperModule.CreateImageWrapper( EImageFormat::ICO ),
		};

		for( auto ImageWrapper : ImageWrappers )
		{
			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num() ) )
			{			
				const TArray<uint8>* RawData = NULL;
				if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, RawData))
				{
					if ( FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource( *ImagePath, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), *RawData ) )
					{
						Brush = MakeShareable(new FSlateDynamicImageBrush( *ImagePath, FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight()) ) );
						bSucceeded = true;
						break;
					}
				}
			}
		}

		if(!bSucceeded)
		{
			UE_LOG(LogSlate, Log, TEXT("Only BGRA pngs, bmps or icos are supported in by External Image Picker"));
			ErrorHintWidget->SetError(LOCTEXT("BadFormatHint", "Unsupported image format"));
		}
		else
		{
			if (bRequiresSpecificSize)
			{
				if (Brush->ImageSize != RequiredImageDimensions)
				{
					ErrorHintWidget->SetError(FText::Format(
						LOCTEXT("BadSizeHint", "Incorrect size ({0}x{1} but should be {2}x{3})"),
						FText::AsNumber((int32)Brush->ImageSize.X),
						FText::AsNumber((int32)Brush->ImageSize.Y),
						FText::AsNumber(RequiredImageDimensions.X),
						FText::AsNumber(RequiredImageDimensions.Y)
						));
				}
			}
		}
	}
	else
	{
		UE_LOG(LogSlate, Log, TEXT("Could not find file for image: %s"), *ImagePath );
	}

	return Brush;
}


FReply SExternalImagePicker::OnPickFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( DesktopPlatform )
	{
		TArray<FString> OutFiles;
		const FString Filter = FString::Printf(TEXT("%s files (*.%s)|*.%s"), *Extension, *Extension, *Extension);
		const FString DefaultPath = OnGetPickerPath.IsBound() ? OnGetPickerPath.Execute() : FPaths::GetPath(FPaths::GetProjectFilePath());

		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		if (DesktopPlatform->OpenFileDialog(ParentWindowHandle, FText::Format(LOCTEXT("ImagePickerDialogTitle", "Choose a {0} file"), FText::FromString(Extension)).ToString(), DefaultPath, TEXT(""), Filter, EFileDialogFlags::None, OutFiles))
		{
			check(OutFiles.Num() == 1);

			if(OnExternalImagePicked.Execute(FPaths::ConvertRelativePathToFull(OutFiles[0]), TargetImagePath))
			{
				ApplyImage();
			}
		}
	}

	return FReply::Handled();
}

void SExternalImagePicker::HandleResetToDefault()
{
	if(OnExternalImagePicked.Execute(DefaultImagePath, TargetImagePath))
	{
		ApplyImage();
	}
}

FText SExternalImagePicker::GetResetToDefaultText() const
{
	return FText::FromString(DefaultImagePath);
}

bool SExternalImagePicker::DiffersFromDefault() const
{
	return TargetImagePath != DefaultImagePath;
}

#undef LOCTEXT_NAMESPACE
