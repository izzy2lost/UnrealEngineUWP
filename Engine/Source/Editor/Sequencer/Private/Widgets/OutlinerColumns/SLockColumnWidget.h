// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/OutlinerColumns/SColumnToggleWidget.h"

namespace UE::Sequencer
{

/**
* A widget that shows and controls the locked state of outliner items.
*/
class SLockColumnWidget
	: public SColumnToggleWidget
{
public:
	SLATE_BEGIN_ARGS(SLockColumnWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const FCreateOutlinerColumnParams& InParams);

public:
	/** Returns whether or not this outliner item is locked. */
	virtual bool IsActive() const override;

	/** Sets this item and it's sections as locked or unlocked. If selected, applies to all selected items. */
	virtual void SetIsActive(const bool bInIsActive) override;

	/** Returns whether or not a child of this item is locked. */
	virtual bool IsChildActive() const override;

	/** Returns the brush to display when this item is locked. */
	virtual const FSlateBrush* GetActiveBrush() const override;
};

} // namespace UE::Sequencer