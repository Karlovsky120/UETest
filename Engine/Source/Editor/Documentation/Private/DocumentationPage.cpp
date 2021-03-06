// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DocumentationModulePrivatePCH.h"
#include "UDNParser.h"
#include "DocumentationPage.h"
#include "SDocumentationAnchor.h"

TSharedRef< IDocumentationPage > FDocumentationPage::Create( const FString& Link, const TSharedRef< FUDNParser >& Parser ) 
{
	return MakeShareable( new FDocumentationPage( Link, Parser ) );
}

FDocumentationPage::~FDocumentationPage() 
{

}

bool FDocumentationPage::GetExcerptContent( FExcerpt& Excerpt )
{
	for (int32 Index = 0; Index < StoredExcerpts.Num(); ++Index)
	{
		if ( Excerpt.Name == StoredExcerpts[ Index ].Name )
		{
			Parser->GetExcerptContent( Link, StoredExcerpts[ Index ] );
			Excerpt.Content = StoredExcerpts[ Index ].Content;
			return true;
		}
	}

	return false;
}

bool FDocumentationPage::HasExcerpt( const FString& ExcerptName )
{
	return StoredMetadata.ExcerptNames.Contains( ExcerptName );
}

int32 FDocumentationPage::GetNumExcerpts() const
{
	return StoredExcerpts.Num();
}

void FDocumentationPage::GetExcerpts( /*OUT*/ TArray< FExcerpt >& Excerpts )
{
	Excerpts.Empty();
	for (int32 i = 0; i < StoredExcerpts.Num(); ++i)
	{
		Excerpts.Add(StoredExcerpts[i]);
	}
}

FText FDocumentationPage::GetTitle()
{
	return StoredMetadata.Title;
}

void FDocumentationPage::Reload()
{
	StoredExcerpts.Empty();
	StoredMetadata = FUDNPageMetadata();
	Parser->Parse( Link, StoredExcerpts, StoredMetadata );
}

FDocumentationPage::FDocumentationPage( const FString& InLink, const TSharedRef< FUDNParser >& InParser ) 
	: Link( InLink )
	, Parser( InParser )
{
	Parser->Parse( Link, StoredExcerpts, StoredMetadata );
}
