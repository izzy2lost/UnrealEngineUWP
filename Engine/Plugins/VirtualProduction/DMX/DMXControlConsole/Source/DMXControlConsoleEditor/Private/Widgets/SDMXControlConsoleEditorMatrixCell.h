// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FOptionalSize;
struct FSlateColor;
class SDMXControlConsoleEditorExpandArrowButton;
class SDMXControlConsoleEditorFader;
class SHorizontalBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFixturePatchMatrixCell;


/** Individual Matrix Cell UI class */
class SDMXControlConsoleEditorMatrixCell
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorMatrixCell)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UDMXControlConsoleFixturePatchMatrixCell* InMatrixCell, UDMXControlConsoleEditorModel* InEditorModel);

	/** Gets a reference to the Matrix Cell showed by this widget */
	UDMXControlConsoleFixturePatchMatrixCell* GetMatrixCell() { return MatrixCell.Get(); }

	/** Gets a reference to this widget's ExpandArrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton>& GetExpandArrowButton() { return ExpandArrowButton; }

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

private:
	/** Should be called when a Cell Attribute Fader was added to the Matrix Cell Fader this widget displays */
	void OnCellAttributeFaderAdded();

	/** Adds a Cell Attribute Fader slot widget */
	void AddCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader);

	/** Should be called when a Cell Attribute Fader was deleted from the Matrix Cell Fader this widget displays */
	void OnCellAttributeFaderRemoved();

	/** Checks if the CellAttributeFaders array contains a reference to the given Cell Attribute Fader */
	bool ContainsCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader);

	/** Gets wheter this Matrix Cell Fader is selected or not */
	bool IsSelected() const;

	/** Returns true if any of this Matrix Cell's Cell Attribute Fader is selected */
	bool IsAnyCellAttributeFaderSelected() const;

	/** Gets the height of the Matrix Cell according to the current Faders View Mode  */
	FOptionalSize GetMatrixCellHeightByFadersViewMode() const;

	/** Gets the Matrix Cell ID as text */
	FText GetMatrixCellLabelText() const;

	/** Gets the label background color */
	FSlateColor GetLabelBorderColor() const;

	/** Gets the visibility for each Fader widget in this view */
	EVisibility GetFaderWidgetVisibility(const UDMXControlConsoleFaderBase* Fader) const;

	/** Gets the visibility of the CellAttributeFadersHorizontalBox widget */
	EVisibility GetCellAttributeFadersHorizontalBoxVisibility() const;

	/** Gets the widget border brush */
	const FSlateBrush* GetBorderImage() const;

	/** Reference to the Cell Attribute Faders main widget */
	TSharedPtr<SHorizontalBox> CellAttributeFadersHorizontalBox;

	/** Array of Cell Attribute Fader widgets */
	TArray<TWeakPtr<SDMXControlConsoleEditorFader>> CellAttributeFaderWidgets;

	/** Reference to the ExpandArrow button used to show/hide Matrix Cell */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

	/** Reference to the Matrix Cell being displayed */
	TWeakObjectPtr<UDMXControlConsoleFixturePatchMatrixCell> MatrixCell;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
