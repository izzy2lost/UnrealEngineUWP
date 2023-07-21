// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "MVVM/ViewModelPtr.h"
#include "Templates/SubclassOf.h"

class SWidget;
class UMovieSceneSequence;

namespace UE::Sequencer 
{ 
class IOutlinerExtension;
class FEditorViewModel;
}

namespace UE::Sequencer
{

/** Parameters for creating an outliner column widget. */
struct SEQUENCERCORE_API FCreateOutlinerColumnParams
{

	FCreateOutlinerColumnParams(const TViewModelPtr<IOutlinerExtension> InOutlinerExtension, const TSharedPtr<FEditorViewModel> InEditor)
		: OutlinerExtension(InOutlinerExtension)
		, Editor(InEditor)
	{}

	const TViewModelPtr<IOutlinerExtension> OutlinerExtension;
	const TSharedPtr<FEditorViewModel> Editor;
};

}

/**
* Interface for sequencer outliner columns.
*/
class SEQUENCERCORE_API ISequencerOutlinerColumn
{

public:

	/* Gets the unique name of the column to use in the SOutlinerView registry and context menus */
	virtual FName GetColumnName() const = 0;

	/* Gets the text to display in the UI for visibility */
	virtual FText GetColumnLabel() const = 0;

	/* The default visibility state of this column when loaded for the first time */
	virtual bool IsColumnVisibleByDefault() const { return true; }

	/* Gets whether or not this column is supported by a given Sequencer */
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const { return true; }

	/* Gets whether or not a widget should be generated for a given item in the outliner column. */
	virtual bool IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const = 0;

	/* Gets the widget created for each item within the SOutlinerView, column widgets must be fixed width */
	virtual TSharedRef<SWidget> CreateColumnWidget(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerOutlinerColumn() { }

};
