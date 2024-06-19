// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorUndoClient.h"

#include "ColorGradingPanelState.h"
#include "ColorGradingListItem.h"

class FColorGradingEditorDataModel;
class SColorGradingColorWheelPanel;
class SColorGradingObjectList;
class SHorizontalBox;
class SInlineEditableTextBlock;
class UWorld;

using FColorGradingActorFilter = TFunction<bool(AActor*)>;

/** Main panel of a color grading drawer widget, which displays color wheels or selected object details */
class COLORGRADINGEDITOR_API SColorGradingPanel : public SCompoundWidget, public FEditorUndoClient
{
public:
	~SColorGradingPanel();

	SLATE_BEGIN_ARGS(SColorGradingPanel)
		: _IsInDrawer(false)
		{}

		/** Indicates whether this widget is in a drawer or docked in a tab */
		SLATE_ARGUMENT(bool, IsInDrawer)

		/** The world in which to search for actors to display for editing. If not provided, the level editor's current world will be used */
		SLATE_ATTRIBUTE(UWorld*, OverrideWorld)

		/** Event invoked when the user presses the dock button */
		SLATE_EVENT(FSimpleDelegate, OnDocked)

		/** Function which, if it returns false when passed an actor, filters it and its sub-entries out of the color grading item list */
		SLATE_ARGUMENT(FColorGradingActorFilter, ActorFilter)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget interface

	/** Refreshes the panel's UI to match the current state of the level */
	void Refresh();

	/** Gets the state of the panel UI */
	void GetPanelState(FColorGradingPanelState& OutPanelState) const;

	/** Sets the state of the panel UI */
	void SetPanelState(const FColorGradingPanelState& InPanelState);

	/** Set the list of selected objects, updating state and data model as appropriate */
	void SetSelectedObjects(const TArray<UObject*>& Objects);

private:
	/** Creates the button used to dock the drawer in the operator panel */
	TSharedRef<SWidget> CreateDockInLayoutButton();

	/** Get the world currently being edited */
	UWorld* GetWorld();

	/** Binds a callback to the BlueprintCompiled delegate of the specified class */
	void BindBlueprintCompiledDelegate(const UClass* Class);

	/** Unbinds a callback to the BlueprintCompiled delegate of the specified class */
	void UnbindBlueprintCompiledDelegate(const UClass* Class);

	/** Refreshes the object list, filling it with the current color gradable objects from the root actor and world */
	void RefreshColorGradingList();

	/** Fills the color grading group toolbar using the color grading data model */
	void FillColorGradingGroupToolBar();

	/** Gets the visibility state of the color grading group toolbar */
	EVisibility GetColorGradingGroupToolBarVisibility() const;

	/** Gets whether the color grading group at the specified index is currently selected */
	ECheckBoxState IsColorGradingGroupSelected(int32 GroupIndex) const;

	/** Raised when the user has selected the specified color grading group */
	void OnColorGradingGroupCheckedChanged(ECheckBoxState State, int32 GroupIndex);

	/** Gets the display name of the specified color grading group */
	FText GetColorGradingGroupDisplayName(int32 GroupIndex) const;

	/** Gets the font of the display name label of the specified color grading group */
	FSlateFontInfo GetColorGradingGroupDisplayNameFont(int32 GroupIndex) const;

	/** Gets the content for the right click menu for the color grading group */
	TSharedRef<SWidget> GetColorGradingGroupMenuContent(int32 GroupIndex);

	/** Raised when a color grading group has been deleted by the user */
	void OnColorGradingGroupDeleted(int32 GroupIndex);

	/** Raised when a rename has been requested on a color grading group */
	void OnColorGradingGroupRequestRename(int32 GroupIndex);

	/** Raised when a rename has been committed on a color grading group */
	void OnColorGradingGroupRenamed(const FText& InText, ETextCommit::Type TextCommitType, int32 GroupIndex);

	/** Raised when the editor replaces any UObjects with new instantiations, usually when actors have been recompiled from blueprints */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Raised when an actor is added to the current level */
	void OnLevelActorAdded(AActor* Actor);

	/** Raised when an actor has been deleted from the currnent level */
	void OnLevelActorDeleted(AActor* Actor);

	/** Raised when a world has been added */
	void OnWorldAdded(UWorld* World);

	/** Raised when a world has been destroyed */
	void OnWorldDestroyed(UWorld* World);

	/** Raised when the specified blueprint has been recompiled */
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	/** Raised when the color grading data model has been generated */
	void OnColorGradingDataModelGenerated();

	/** Raised when the "Dock in Layout" button has been clicked */
	FReply DockInLayout();

	/** Raised when the user has selected a new item in any of the drawer's list views */
	void OnListSelectionChanged(TSharedRef<SColorGradingObjectList> SourceList, FColorGradingListItemRef SelectedItem, ESelectInfo::Type SelectInfo);


private:
	/** Box containing the color grading groups */
	TSharedPtr<SHorizontalBox> ColorGradingGroupToolBarBox;

	/** List of editable text blocks containing color grading group names */
	TArray<TSharedPtr<SInlineEditableTextBlock>> ColorGradingGroupTextBlocks;

	/** Panel containing the color wheels */
	TSharedPtr<SColorGradingColorWheelPanel> ColorWheelPanel;

	/** The world from which to retrieve actors, if one was provided */
	TAttribute<UWorld*> OverrideWorld;

	/** Color grading object list widget being displayed in the drawer's list panel */
	TSharedPtr<SColorGradingObjectList> ColorGradingObjectListView;

	/** Source list for the color grading object list widget */
	TArray<FColorGradingListItemRef> ColorGradingItemList;

	/** The color grading data model for the currently selected objects */
	TSharedPtr<FColorGradingEditorDataModel> ColorGradingDataModel;

	/** Indicates whether this widget is in a drawer or docked in a tab */
	bool bIsInDrawer;

	/** Indicates that the panel should refresh itself on the next tick */
	bool bRefreshOnNextTick = false;

	/** The function to call when the user presses the dock button */
	FSimpleDelegate DockCallback;

	/** Function used to filter actors before adding them to the object list. */
	FColorGradingActorFilter ActorFilter;
};
