// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SequencerTrackFilterBase.h"
#include "Misc/TextFilterExpressionEvaluator.h"

class UMovieScene;
class UMovieSceneSequence;
class UMovieSceneTrack;

enum class SEQUENCER_API ESequencerTextFilterValueType : uint8
{
	String,
	Boolean,
	Integer
};

/** Text expression context to test the given asset data against the current text filter */
class FSequencerTextFilterExpressionContext : public ITextFilterExpressionContext
{ 
public:
	SEQUENCER_API FSequencerTextFilterExpressionContext(ISequencerTrackFilters& InFilterInterface);

	void SetFilterItem(FSequencerTrackFilterType InFilterItem, UMovieSceneTrack* const InTrackObject);

	//~ Begin FSequencerTextFilterExpressionContext
	virtual TSet<FName> GetKeys() const = 0;
	virtual ESequencerTextFilterValueType GetValueType() const = 0;
	virtual FText GetDescription() const = 0;
	//~ End FSequencerTextFilterExpressionContext

	//~ Begin ITextFilterExpressionContext

	SEQUENCER_API virtual bool TestBasicStringExpression(const FTextFilterString& InValue
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

	SEQUENCER_API virtual bool TestComplexExpression(const FName& InKey
		, const FTextFilterString& InValue
		, const ETextFilterComparisonOperation InComparisonOperation
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

	//~ End ITextFilterExpressionContext

protected:
	UMovieSceneSequence* GetFocusedMovieSceneSequence() const;
	UMovieScene* GetFocusedGetMovieScene() const;

	bool CompareFStringForExactBool(const FTextFilterString& InValue, const bool bInPassedFilter) const;

	bool CompareFStringForExactBool(const FTextFilterString& InValue
		, const ETextFilterComparisonOperation InComparisonOperation
		, const bool bInPassedFilter) const;

	ISequencerTrackFilters& FilterInterface;

	FSequencerTrackFilterType FilterItem;

	TWeakObjectPtr<UMovieSceneTrack> WeakTrackObject;
};
