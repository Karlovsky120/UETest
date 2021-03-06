// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** Class that holds all profiler commands. */
class FProfilerCommands : public TCommands<FProfilerCommands>
{
public:
	/** Default constructor. */
	FProfilerCommands();
	
	/**
	 * Initialize commands.
	 */
	virtual void RegisterCommands() OVERRIDE;

public:
	/*-----------------------------------------------------------------------------
		Global and custom commands.	Need to implement following methods:

		void Map_<CommandName>_Global();
		const FUIAction <CommandName>_Custom(...) const;
	-----------------------------------------------------------------------------*/

	/** Toggles the data preview for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleDataPreview;

	/** Toggles the data capture for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleDataCapture;

	/** Toggles showing all data graphs for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > ToggleShowDataGraph;

	/** Opens event graph for all session instances. Global and custom command. */
	TSharedPtr< FUICommandInfo > OpenEventGraph;

	/*-----------------------------------------------------------------------------
		Global commands. Need to implement following methods:

		void Map_<CommandName>_Global();
	-----------------------------------------------------------------------------*/

	/** Saves all collected data to file or files. */
	TSharedPtr< FUICommandInfo > ProfilerManager_Save;

	/** Stats Profiler */
	TSharedPtr< FUICommandInfo > StatsProfiler;

	/** Memory Profiler */
	TSharedPtr< FUICommandInfo > MemoryProfiler;

	/** FPS Chart */
	TSharedPtr< FUICommandInfo > FPSChart;

	/** Open settings for the profiler manager */
	TSharedPtr< FUICommandInfo > OpenSettings;


	/** Load profiler data. Global version. */
	TSharedPtr< FUICommandInfo > ProfilerManager_Load;

	/** Toggles the real time live preview. Global version. */
	TSharedPtr< FUICommandInfo > ProfilerManager_ToggleLivePreview;


	/** Toggles the data graph view mode between time based and index based. */
	TSharedPtr< FUICommandInfo > DataGraph_ToggleViewMode;

	/** Toggles the data graph multi mode between displaying area line graph for each graph data source. */
	TSharedPtr< FUICommandInfo > DataGraph_ToggleMultiMode;


	/** Sets the data graph view mode to the time based. */
	TSharedPtr< FUICommandInfo > DataGraph_ViewMode_SetTimeBased;

	/** Sets the data graph view mode to the index based. */
	TSharedPtr< FUICommandInfo > DataGraph_ViewMode_SetIndexBased;

	/** Set the data graph multi mode to the displaying area line graph. */
	TSharedPtr< FUICommandInfo > DataGraph_MultiMode_SetCombined;

	/** Set the data graph multi mode to the displaying one line graph for each graph data source. */
	TSharedPtr< FUICommandInfo > DataGraph_MultiMode_SetOneLinePerDataSource;

	/** Select all frame in the data graph and display them in the event graph, technically switches to the begin of history. */
	TSharedPtr< FUICommandInfo > EventGraph_SelectAllFrames;
};

class FProfilerMenuBuilder
{
public:
	/**
	 * Helper method for adding a customized menu entry using the global UI command info.
	 * FUICommandInfo cannot be executed with custom parameters, so we need to create a custom FUIAction,
	 * but sometime we have global and local version for the UI command, so reuse data from the global UI command info.
	 * Ex:
	 *		SessionInstance_ToggleCapture			- Global version will toggle capture process for all active session instances
	 *		SessionInstance_ToggleCapture_OneParam	- Local version will toggle capture process only for the specified session instance
	 *
	 * @param MenuBuilder		- the menu to add items to
	 * @param FUICommandInfo	- a shared pointer to the UI command info
	 * @param UIAction			- customized version of the UI command info stored in an UI action 
	 *
	 */
	static void AddMenuEntry( FMenuBuilder& MenuBuilder, const TSharedPtr< FUICommandInfo >& FUICommandInfo, const FUIAction& UIAction );
};

/** 
 * Class that provides helper functions for the commands to avoid cluttering profiler manager with many small functions. Can't contain any variables.
 * Directly operates on the profiler manager instance.
 */
class FProfilerActionManager
{
	friend class FProfilerManager;

	/*-----------------------------------------------------------------------------
		ProfilerManager_Load
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ProfilerManager_Load with the specified UI command list. */
	void Map_ProfilerManager_Load();
		
protected:
	/** Handles FExecuteAction for ProfilerManager_Load. */
	void ProfilerManager_Load_Execute();
	/** Handles FCanExecuteAction for ProfilerManager_Load. */
	bool ProfilerManager_Load_CanExecute() const;

	/*-----------------------------------------------------------------------------
		ToggleDataPreview
		NOTE: Sends a message to the profiler service for this
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ToggleDataPreview with the specified UI command list. */
	void Map_ToggleDataPreview_Global();

	/**
	 * UI action that Toggles the data preview for the specified session instance.
	 *
	 * @param SessionInstanceID - the session instance that this action will be executed on, if not valid, all session instances will be used
	 *
	 */
	const FUIAction ToggleDataPreview_Custom( const FGuid SessionInstanceID ) const;
		
protected:
	/** Handles FExecuteAction for ToggleDataPreview. */
	void ToggleDataPreview_Execute( const FGuid SessionInstanceID );
	/** Handles FCanExecuteAction for ToggleDataPreview. */
	bool ToggleDataPreview_CanExecute( const FGuid SessionInstanceID ) const;
	/** Handles FIsActionChecked for ToggleDataPreview. */
	bool ToggleDataPreview_IsChecked( const FGuid SessionInstanceID ) const;

	/*-----------------------------------------------------------------------------
		ProfilerManager_ToggleLivePreview
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ProfilerManager_ToggleLivePreview with the specified UI command list. */
	void Map_ProfilerManager_ToggleLivePreview_Global();

protected:
	/** Handles FExecuteAction for ProfilerManager_ToggleLivePreview. */
	void ProfilerManager_ToggleLivePreview_Execute();
	/** Handles FCanExecuteAction for ProfilerManager_ToggleLivePreview. */
	bool ProfilerManager_ToggleLivePreview_CanExecute( ) const;
	/** Handles FIsActionChecked for ProfilerManager_ToggleLivePreview. */
	bool ProfilerManager_ToggleLivePreview_IsChecked() const;

	/*-----------------------------------------------------------------------------
		ToggleDataCapture
		NOTE: Sends a message to the profiler service for this
	-----------------------------------------------------------------------------*/
public:
	/** Maps UI command info ToggleDataCapture with the specified UI command list. */
	void Map_ToggleDataCapture_Global();
	
	/**
	 * UI action that toggles the data capture for the specified session instance.
	 *
	 * @param SessionInstanceID - the session instance that this action will be executed on, if not valid, all session instances will be used
	 *
	 */
	const FUIAction ToggleDataCapture_Custom( const FGuid SessionInstanceID ) const;
		
protected:
	/** Handles FExecuteAction for ToggleDataCapture. */
	void ToggleDataCapture_Execute( const FGuid SessionInstanceID );
	/** Handles FCanExecuteAction for ToggleDataCapture. */
	bool ToggleDataCapture_CanExecute( const FGuid SessionInstanceID ) const;
	/** Handles FIsActionChecked for ToggleDataCapture. */
	bool ToggleDataCapture_IsChecked( const FGuid SessionInstanceID ) const;

	/*-----------------------------------------------------------------------------
		ToggleShowDataGraph
	-----------------------------------------------------------------------------*/
public:

	/**
	 * UI action that toggles showing data graph for the specified session instance.
	 *
	 * @param SessionInstanceID - the session instance that this action will be executed on, if not valid, all session instances will be used
	 *
	 */
	const FUIAction ToggleShowDataGraph_Custom( const FGuid SessionInstanceID ) const;

protected:
	/** Handles FExecuteAction for ToggleShowDataGraph_Execute. */
	void ToggleShowDataGraph_Execute( const FGuid SessionInstanceID );
	/** Handles FCanExecuteAction for ToggleShowDataGraph_Execute. */
	bool ToggleShowDataGraph_CanExecute( const FGuid SessionInstanceID ) const;
	/** Handles FIsActionChecked for ToggleShowDataGraph_Execute. */
	bool ToggleShowDataGraph_IsChecked( const FGuid SessionInstanceID ) const;
	/** Handles IsActionButtonVisible for ToggleShowDataGraph_Execute. */
	bool ToggleShowDataGraph_IsActionButtonVisible( const FGuid SessionInstanceID ) const;

private:
	/** Private constructor. */
	FProfilerActionManager( class FProfilerManager* Instance )
		: This( Instance )
	{}

	/*-----------------------------------------------------------------------------
		OpenSettings
	-----------------------------------------------------------------------------*/
	
public:
	/** Maps UI command info OpenSettings with the specified UI command list. */
	void Map_OpenSettings_Global();
	
	/** Add comment here */
	const FUIAction OpenSettings_Custom() const;
		
protected:
	/** Handles FExecuteAction for OpenSettings. */
	void OpenSettings_Execute();
	/** Handles FCanExecuteAction for OpenSettings. */
	bool OpenSettings_CanExecute() const;
	/** Handles FIsActionChecked for OpenSettings. */

	/** Reference to the global instance of the profiler manager. */
	class FProfilerManager* This;
};