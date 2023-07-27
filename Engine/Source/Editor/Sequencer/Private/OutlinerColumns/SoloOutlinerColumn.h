// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerOutlinerColumn.h"

/**
 * A column for soloing/unsoloing tracks.
 */
class FSoloOutlinerColumn
	: public ISequencerOutlinerColumn
{
public:

	FSoloOutlinerColumn()
	{ }

	/** Returns the name of the column. Used for determining column type when dragging or saving settings. */
	virtual FName GetColumnName() const override;

	/** Returns the label of the column to display in visibility settings. */
	virtual FText GetColumnLabel() const override;

	/* Gets whether or not an outliner column widget should be generated for a given item in the outliner column. */
	virtual bool IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const override;

	/**
	 * Creates an instance of this class.
	 *
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerOutlinerColumn> CreateOutlinerColumn();
	

	/**
	 * Creates a widget that solos an item in the outliner view.
	 */
	virtual TSharedRef<SWidget> CreateColumnWidget(const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const override;
};
