// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"

#include "Widgets/SCompoundWidget.h"

enum class EDMXControlConsoleEditorViewMode : uint8;
struct FOptionalSize;
struct FSlateBrush;
struct FSlateColor;
class SDMXControlConsoleEditorExpandArrowButton;
class SHorizontalBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	class SDMXControlConsoleEditorFaderGroupToolbar;

	/** A widget which gathers a collection of Faders */
	class SDMXControlConsoleEditorFaderGroupView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupView)
			{}

		SLATE_END_ARGS()

		/** Constructor */
		SDMXControlConsoleEditorFaderGroupView();

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleFaderGroup* InFaderGroup, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Fader Group this Fader Group View is based on */
		UDMXControlConsoleFaderGroup* GetFaderGroup() const { return FaderGroup.Get(); }

		/** Gets the index of this Fader Group according to the referenced Fader Group Row */
		int32 GetIndex() const;

		/** Gets Fader Group's name */
		FString GetFaderGroupName() const;

		/** Gets current ViewMode */
		EDMXControlConsoleEditorViewMode GetViewMode() const { return ViewMode; }

		/** True if a new Fader Group can be added next to this */
		bool CanAddFaderGroup() const;

		/** True if a new Fader Group can be added on next row */
		bool CanAddFaderGroupRow() const;

		/** True if a new Fader can be added */
		bool CanAddFader() const;

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

	private:
		/** Generates ElementControllersHorizontalBox widget */
		TSharedRef<SWidget> GenerateElementControllersWidget();

		/** Gets wheter this Fader Group is selected or not */
		bool IsSelected() const;

		/** Gets a reference to the toolbar's ExpandArrow button */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> GetExpandArrowButton() const;

		/** Should be called when an Element Controller was added to the Fader Group this view displays */
		void OnElementControllerAdded();

		/** Adds an Element Controller slot widget */
		void AddElementController(UDMXControlConsoleElementController* ElementController);

		/** Should be called when an Element Controller was deleted from the Fader Group this view displays */
		void OnElementControllerRemoved();

		/** Checks if ElementControllers array contains a reference to the given Element Controller */
		bool ContainsElementController(const UDMXControlConsoleElementController* InElementController) const;

		/** Updates this widget to the last saved expansion state from the model */
		void UpdateExpansionState();

		/** Called when the Expand Arrow button is clicked */
		void OnExpandArrowClicked(bool bExpand);

		/** Adds a new Fader Group to the owner row */
		void OnAddFaderGroup() const;

		/** Adds a new Fader Group Row next to the owner row */
		void OnAddFaderGroupRow() const;

		/** Called when Fader Group Fixture Patch has changed */
		void OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* InFaderGroup, UDMXEntityFixturePatch* FixturePatch);

		/** Notifies this Fader Group's owner row to add a new Fader Group */
		FReply OnAddFaderGroupClicked() const;

		/** Notifies this Fader Group's owner row to add a new Fader Group Row */
		FReply OnAddFaderGroupRowClicked() const;

		/** Notifies this Fader Group to add a new Fader */
		FReply OnAddFaderClicked();

		/** Called when Fader Groups view mode is changed */
		void OnViewModeChanged();

		/** True if the given View Mode matches the current one */
		bool IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const;

		/** Gets the height of the FaderGroup view according to the current Faders View Mode  */
		FOptionalSize GetFaderGroupViewHeightByFadersViewMode() const;

		/** Gets fader group view border color */
		FSlateColor GetFaderGroupViewBorderColor() const;

		/** Changes brush when this widget is hovered */
		const FSlateBrush* GetFaderGroupViewBorderImage() const;

		/** Changes background brush when this widget is hovered */
		const FSlateBrush* GetFaderGroupViewBackgroundBorderImage() const;

		/** Gets visibility according to the given View Mode */
		EVisibility GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const;

		/** Gets visibility for each Element Controller widget in this view */
		EVisibility GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel) const;

		/** Manages horizontal Add Button widget's visibility */
		EVisibility GetAddButtonVisibility() const;

		/** Manages vertical Add Button widget's visibility */
		EVisibility GetAddRowButtonVisibility() const;

		/** Gets ElementControllersHorizontalBox widget visibility */
		EVisibility GetElementControllersHorizontalBoxVisibility() const;

		/** Gets add fader button visibility */
		EVisibility GetAddFaderButtonVisibility() const;

		/** Current view mode */
		EDMXControlConsoleEditorViewMode ViewMode;

		/** Reference to the toolbar widget for this view */
		TSharedPtr<SDMXControlConsoleEditorFaderGroupToolbar> FaderGroupToolbar;

		/** Horizontal Box containing the Element Controllers in this Fader Group */
		TSharedPtr<SHorizontalBox> ElementControllersHorizontalBox;

		/** Array of weak references to Element Controller widgets */
		TArray<TWeakPtr<SWidget>> ElementControllerWidgets;

		/** Weak Reference to this Fader Group */
		TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
