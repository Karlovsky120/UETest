// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CodeViewPrivatePCH.h"
#include "ModuleManager.h"

class FCodeViewPlugin : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() OVERRIDE
	{
		OnExtendActorDetails.AddRaw(this, &FCodeViewPlugin::AddCodeViewCategory);
	}

	virtual void ShutdownModule() OVERRIDE
	{
		OnExtendActorDetails.RemoveAll(this);
	}
	// End of IModuleInterface implementation

	void AddCodeViewCategory(IDetailLayoutBuilder& DetailBuilder, const FGetSelectedActors& GetSelectedActors)
	{
		if (FModuleManager::Get().IsSolutionFilePresent())
		{
			TSharedRef< CodeView::SCodeView > CodeViewWidget =
				SNew( CodeView::SCodeView )
				.GetSelectedActors( GetSelectedActors );

			// Only start out expanded if we're already in "ready to populate" mode.  This is because we don't want
			// to immediately start digesting symbols as soon as the widget is visible.  Instead, when the user
			// expands the section, we'll start loading symbols.  However, this state is remembered even after
			// the widget is destroyed.
			const bool bShouldInitiallyExpand = CodeViewWidget->IsReadyToPopulate();

			DetailBuilder.EditCategory( "CodeView", NSLOCTEXT("ActorDetails", "CodeViewSection", "Code View").ToString(), ECategoryPriority::Uncommon )
				.InitiallyCollapsed( !bShouldInitiallyExpand )
				// The expansion state should not be restored
				.RestoreExpansionState( false )
				.OnExpansionChanged( FOnBooleanValueChanged::CreateSP( CodeViewWidget, &CodeView::SCodeView::OnDetailSectionExpansionChanged ) )
				.AddCustomRow( NSLOCTEXT("ActorDetails", "CodeViewSection", "Code View").ToString() )
				[
					// @todo editcode1: Width of item is too big for detail view?!
					CodeViewWidget
				];
		}
	}
};


IMPLEMENT_MODULE(FCodeViewPlugin, CodeView)


