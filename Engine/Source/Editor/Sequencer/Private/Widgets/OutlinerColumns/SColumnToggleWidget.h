// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerOutlinerColumn.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"

namespace UE::Sequencer
{

class FSequencerEditorViewModel;

/**
 * Outliner Column Widget for toggling active state with click and drag functionality.
 * Adjusts image color and transparency based on active state and item hover.
 * Displays an alternate icon when a child is active and the item is not.
 * Column Toggle widgets have 4 States: Active, Inactive, Active Child, and Implicitly Active.
 * Column Toggle widgets will change its brush based on it's state.
 */
class SColumnToggleWidget
	: public SImage
{
public:

	SLATE_BEGIN_ARGS(SColumnToggleWidget) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		const TWeakPtr<ISequencerOutlinerColumn> InOutlinerColumn,
		const FCreateOutlinerColumnParams& InParams);

public:

	/** Called when a drag or click ends and can be used to refresh the SequencerTree if necessary. */
	virtual void OnToggleOperationComplete() {};

protected:

	/** Returns whether the widget is enabled or not. */
	virtual bool IsEnabled() const { return true; }

protected:

	/** Returns whether or not the item is directly active or not. */
	virtual bool IsActive() const = 0;

	/** Sets the active state of this item to the input value. */
	virtual void SetIsActive(const bool bInIsActive) = 0;

	/** Returns true if a child of this item is active. */
	virtual bool IsChildActive() const = 0;

	/** Returns true if this item is implicitly active, but not directly active */
	virtual bool IsImplicitlyActive() const { return false; }

	/** Returns the brush to be used to represent a widget is active. */
	virtual const FSlateBrush* GetActiveBrush() const = 0;

protected:
	
	/** Get the color and opacity of the column toggle widget. */
	virtual FSlateColor GetImageColorAndOpacity() const;

	/** Get the image this widget displays. */
	virtual const FSlateBrush* GetBrush() const;

protected:

	/** Start a new drag/drop operation for this widget. */
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** If a matching column drag drop operation has entered this widget, set its item to the specified active state. */
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/** Handles left click-type inputs and potentially begins drag and drop operation. */
	FReply HandleClick();

	/** Called when the mouse button is double clicked on this widget. */
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	/** Called when the mouse button is pressed down on this widget. */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Called when this widget had captured the mouse, but that capture has been revoked for some reason. */
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	/** Process a mouse up message. */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Called when mouse enters the column widget. */
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Called when mouse leaves the column widget. */
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;

protected:

	/** Refreshes the Sequencer Tree. Can be used after toggle operation is complete if operation affects other items. */
	void RefreshSequencerTree();

protected:
	/** Editor view-model for this view. */
	TWeakPtr<FSequencerEditorViewModel> WeakEditor;

	/** Outliner item this widget accesses. */
	TWeakViewModelPtr<IOutlinerExtension> WeakOutlinerExtension;

	/** Reference to column this widget is a member of. */
	TWeakPtr<ISequencerOutlinerColumn> WeakOutlinerColumn;

private:

	/** Whether or not the mouse is directly over the widget. */
	bool bIsMouseOverWidget;

	/** Whether or not a child of this item is active. */
	bool bIsChildActive;

	/** Brushes for the various states of active widgets. */
	const FSlateBrush* ActiveBrush;
	const FSlateBrush* ChildActiveBrush;

	/** Scoped undo transaction. */
	TUniquePtr<FScopedTransaction> UndoTransaction;
};

} // namespace UE::Sequencer