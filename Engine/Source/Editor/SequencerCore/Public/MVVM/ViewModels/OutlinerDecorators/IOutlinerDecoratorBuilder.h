// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class SWidget;
class UMovieSceneSequence;

namespace UE::Sequencer
{

struct FCreateOutlinerColumnParams;
class IOutlinerColumn;
class ISequencerTreeViewRow;

/**
* Interface for building sequencer decorator outliner items.
*/
class IOutlinerDecoratorBuilder : public TSharedFromThis<IOutlinerDecoratorBuilder>
{

public:

	/** Returns the name of the Decorator outliner item. */
	virtual FName GetDecoratorName() const = 0;

	/* Gets whether or not this Decorator outliner item is supported by a given Sequencer. */
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const { return true; }

	virtual bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const = 0;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) = 0;

public:

	/** Virtual destructor. */
	virtual ~IOutlinerDecoratorBuilder() { }

};

} // namespace UE::Sequencer

