// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TextFilterUtils.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/NameTypes.h"

class UAvalanchePlaylist;
struct FAvalanchePage;
enum class EAvaPlaylistSearchListType : uint8;

/**
 * Holds the arguments required for the expression evaluation.
 */
struct FAvaPlaylistTextFilterArgs
{
	/** Item Playlist */
	const UAvalanchePlaylist* ItemPlaylist;

	/** Second Value */
	FTextFilterString ValueToCheck;

	/** Comparison Operation requested (Equal/Greater/etc...) still needed for certain Filter */
	ETextFilterComparisonOperation ComparisonOperation;

	/** Comparison Mode requested see ETextFilterTextComparisonMode for more information */
	ETextFilterTextComparisonMode ComparisonMode;
};

class IAvaPlaylistFilterExpressionFactory : public TSharedFromThis<IAvaPlaylistFilterExpressionFactory>
{
public:
	virtual ~IAvaPlaylistFilterExpressionFactory() {}

	/** Get the filter identifier for this factory */
	virtual FName GetFilterIdentifier() const = 0;

	/**
 	 * Create and return an Instance of the AvaPlaylistFilterExpressionFactory Requested
 	 * @tparam InPlaylistFilterExpressionFactoryType The type of the factory to instantiate
 	 * @param InArgs Additional Args for constructor of the AvaPlaylistFilterExpressionFactory class if needed
 	 * @return The new playlist filter factory
 	 */
	template <
		typename InPlaylistFilterExpressionFactoryType,
		typename... InArgsType
		UE_REQUIRES(TIsDerivedFrom<InPlaylistFilterExpressionFactoryType, IAvaPlaylistFilterExpressionFactory>::Value)
	>
	static TSharedRef<InPlaylistFilterExpressionFactoryType> MakeInstance(InArgsType&&... InArgs)
	{
		return MakeShared<InPlaylistFilterExpressionFactoryType>(Forward<InArgsType>(InArgs)...);
	}

	/**
	 * Evaluate the expression with the Value given
	 * @param InItem Current Item being checked
	 * @param InArgs The argument containing the data to evaluate the expression see FAvaTextFilterArgs for more information
	 * @return True if the expression evaluate to True, False otherwise
	 */
	virtual bool FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const = 0;

	/**
	 * Whether the factory support a given Comparison Operation
	 * @param InComparisonOperation Comparison Operation to check
	 * @param InPlaylistSearchListType Type of the Search List either Template or Instanced
	 * @return True if Factory support passed Comparison Operation, False otherwise
	 */
	virtual bool SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const = 0;
};
