// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IExternalImagePickerModule.h"

/**
 * Delegate fired before an image has been copied
 * @param	InChosenImage	The path to the image that has been picked by the user.
 * @return true if the operation was successful
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnPreExternalImageCopy, const FString& /** InChosenImage */);

/**
 * Delegate fired after an image has been copied
 * @param	InChosenImage	The path to the image that has been picked by the user.
 * @return true if the operation was successful
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnPostExternalImageCopy, const FString& /** InChosenImage */);

/////////////////////////////////////////////////////
// SExternalImageReference

// This widget shows an external image preview of a per-project configurable image
// (one where the engine provides a default, but each project may have its own override)

class SHAREDSETTINGSWIDGETS_API SExternalImageReference : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SExternalImageReference)
		: _FileDescription(NSLOCTEXT("SExternalImageReference", "FileDescription", "External Image"))
		, _MaxDisplaySize(400.0f, 400.0f)
		, _RequiredSize(-1, -1)
		{}

		/** The description of the file, used in error messages/notifications */
		SLATE_ARGUMENT(FText, FileDescription)

		/** How big should we display the image? */
		SLATE_ARGUMENT(FVector2D, MaxDisplaySize)

		/** How big does the image need to be (any size is allowed if this is omitted) */
		SLATE_ARGUMENT(FIntPoint, RequiredSize)

		/** Delegate fired before an image has been copied */
		SLATE_ARGUMENT(FOnPreExternalImageCopy, OnPreExternalImageCopy)

		/** Delegate fired after an image has been copied */
		SLATE_ARGUMENT(FOnPostExternalImageCopy, OnPostExternalImageCopy)

		/** Delegate fired to get the path to start picking from. */
		SLATE_ARGUMENT(FOnGetPickerPath, OnGetPickerPath)

		/** A property handle to use if required */
		SLATE_ARGUMENT(TSharedPtr<class IPropertyHandle>, PropertyHandle)
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, const FString& InBaseFilename, const FString& InOverrideFilename);

protected:
	/** Delegate handler for when an image is picked */
	bool HandleExternalImagePicked(const FString& InChosenImage, const FString& InTargetImage);

protected:

	/** The image on disk that we will use if the override does not exist. */
	FString BaseFilename;

	/** The image on disk that the override image is stored as. */
	FString OverrideFilename;

	/** The description of the file in question, e.g. 'image' or 'icon', used for error reporting */
	FText FileDescription;

	/** Delegate fired before an image has been copied */
	FOnPreExternalImageCopy OnPreExternalImageCopy;

	/**  Delegate fired after an image has been copied */
	FOnPostExternalImageCopy OnPostExternalImageCopy;

	/** A property handle to use if required */
	TSharedPtr<class IPropertyHandle> PropertyHandle;
};
