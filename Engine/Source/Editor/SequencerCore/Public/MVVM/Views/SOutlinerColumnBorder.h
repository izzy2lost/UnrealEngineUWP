// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerOutlinerColumn.h"
#include "MVVM/ViewModelPtr.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::Sequencer
{

class FEditorViewModel;
class IOutlinerExtension;

/**
* The widget for the inner, darker border of a column widget that handles coloring / sizing
*/
class SOutlinerColumnInnerBorder
	: public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SOutlinerColumnInnerBorder) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		const FCreateOutlinerColumnParams& InParams
	);

private:

	/** Determine whether or not the mouse is directly over this widget for background color */
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/**
	* @return The border image to show in the tree node.
	*/
	const FSlateBrush* GetBorderImage() const;

	/**
	* @return The inner background border color and opacity, darker when mouse is directly over inner widget.
	*/
	FSlateColor GetBackgroundTint() const;

	/** Used to determine color if outliner item is directly selected */
	TWeakViewModelPtr<IOutlinerExtension> WeakOutlinerExtension;

	/** Used to determine color if outliner item is being hovered */
	TWeakPtr<FEditorViewModel> WeakEditor;

	/** Brush used for inner border */
	const FSlateBrush* BackgroundBrush;

	/** Is the mouse directly over this widget */
	bool bIsMouseOverInnerBorder = false;
};

/**
* A widget for containing a column widget with the correct style / border
*/
class SOutlinerColumnBorder
	: public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SOutlinerColumnBorder) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs, 
		const FCreateOutlinerColumnParams& InParams
	);

private:

	/** Notifies the outliner item that the mouse is within the row for hovered state */
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/**
	* @return The border image to show in the tree node.
	*/
	const FSlateBrush* GetBorderImage() const;

	/**
	* @return The tint to apply to the border image
	*/
	FSlateColor GetBackgroundTint() const;

	/** Used to determine color if outliner item is directly selected */
	TWeakViewModelPtr<IOutlinerExtension> WeakOutlinerExtension;

	/** Used to determine color if outliner item is being hovered */
	TWeakPtr<FEditorViewModel> WeakEditor;

	/** Brush used for border */
	const FSlateBrush* BackgroundBrush;
};

} // namespace UE::Sequencer

