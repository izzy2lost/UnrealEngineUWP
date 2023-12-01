// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FOptionalSize;
struct FSlateColor;
class SDMXControlConsoleEditorExpandArrowButton;
class SHorizontalBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFixturePatchMatrixCell;
class UDMXControlConsoleMatrixCellController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	class SDMXControlConsoleEditorElementControllerView;

	/** Individual Matrix Cell UI class */
	class SDMXControlConsoleEditorMatrixCell
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorMatrixCell)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Element Controller this widget is based on */
		UDMXControlConsoleElementController* GetElementController() const;

		/** Gets a reference to the Matrix Cell showed by this widget */
		UDMXControlConsoleFixturePatchMatrixCell* GetMatrixCell() const;

		/** Gets a reference to this widget's ExpandArrow button */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton>& GetExpandArrowButton() { return ExpandArrowButton; }

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

	private:
		/** Should be called when a Matrix Cell Controller was added to the Matrix Cell this widget displays */
		void OnMatrixCellControllerAdded();

		/** Adds a Matrix Cell Controller slot widget */
		void AddMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController);

		/** Should be called when a Matrix Cell Controller was deleted from the Matrix Cell this widget displays */
		void OnMatrixCellControllerRemoved();

		/** Checks if the Element Controllers array contains a reference to the given Matrix Cell Controller */
		bool ContainsMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController);

		/** Returns true if any of this Matrix Cell's Matrix Cell Controllers is selected */
		bool IsAnyElementControllerSelected() const;

		/** Gets the height of the Matrix Cell according to the current Faders View Mode  */
		FOptionalSize GetMatrixCellHeightByFadersViewMode() const;

		/** Gets the Matrix Cell ID as text */
		FText GetMatrixCellLabelText() const;

		/** Gets the label background color */
		FSlateColor GetLabelBorderColor() const;

		/** Gets the visibility for each Element Controller view in the matrix */
		EVisibility GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ControllerModel) const;

		/** Gets the visibility of the ElementControllerHorizontalBox widget */
		EVisibility GetElementControllersHorizontalBoxVisibility() const;

		/** Gets the widget border brush */
		const FSlateBrush* GetBorderImage() const;

		/** Reference to the Matrix Element Controllers main widget */
		TSharedPtr<SHorizontalBox> ElementControllersHorizontalBox;

		/** Array of Matrix Cell Controllers views */
		TArray<TWeakPtr<SDMXControlConsoleEditorElementControllerView>> ElementControllerViews;

		/** Reference to the ExpandArrow button used to show/hide the Matrix Cell */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

		/** Reference to the Element Controller being displayed */
		TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
