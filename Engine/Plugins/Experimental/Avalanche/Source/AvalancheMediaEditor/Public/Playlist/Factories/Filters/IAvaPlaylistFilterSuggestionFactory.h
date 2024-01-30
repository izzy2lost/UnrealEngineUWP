// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTypeTraits.h"

class UAvalanchePlaylist;
struct FAvalanchePage;
struct FAssetSearchBoxSuggestion;
enum class EAvaPlaylistSearchListType : uint8;

/**
 * Holds the arguments required for the suggestion factory to get the suggestion.
 */
struct FAvaPlaylistFilterSuggestionPayload
{
	/** Holds all the suggestion that will be shown */
	TArray<FAssetSearchBoxSuggestion>& PossibleSuggestions;

	/** The current string value written in the searchbox */
	const FString FilterValue;

	/** Current item to check */
	int32 ItemPageId = UE::AvalanchePlaylist::InvalidPageId;

	/** Current item playlist */
	const UAvalanchePlaylist* Playlist = nullptr;

	/** Used to speed up the check on whether a suggestion is already added */
	TSet<FString>& FilterCache;
};

class IAvaPlaylistFilterSuggestionFactory : public TSharedFromThis<IAvaPlaylistFilterSuggestionFactory>
{
public:
	virtual ~IAvaPlaylistFilterSuggestionFactory() {}

	/**
	  * Create and return an Instance of the AvaPlaylistFilterSuggestionFactory Requested
	  * @tparam InPlaylistSuggestionFactoryType The type of the factory to instantiate
	  * @param InArgs Additional Args for constructor of the AvaPlaylistFilterSuggestionFactory class if needed
	  * @return The new suggestion factory
	  */
	template <
		typename InPlaylistSuggestionFactoryType,
		typename... InArgsType
		UE_REQUIRES(TIsDerivedFrom<InPlaylistSuggestionFactoryType, IAvaPlaylistFilterSuggestionFactory>::Value)
	>
	static TSharedRef<InPlaylistSuggestionFactoryType> MakeInstance(InArgsType&&... InArgs)
	{
		return MakeShared<InPlaylistSuggestionFactoryType>(Forward<InArgsType>(InArgs)...);
	}

	/** Get the suggestion identifier for this factory */
	virtual FName GetSuggestionIdentifier() const = 0;

	/** True if the suggestion doesn't need to use the item to add its suggestions */
	virtual bool IsSimpleSuggestion() const = 0;
	/**
	 * Add suggestions entry to be shown
	 * @param InPayload Arguments to be passed to the factory to get the suggestion to show
	 */
	virtual void AddSuggestion(const TSharedRef<FAvaPlaylistFilterSuggestionPayload>& InPayload) = 0;

	/**
	 * Whether the factory support a given type of suggestion
	 * @param InSuggestionType Suggestion type to check
	 * @return True if Factory support passed Suggestion Type, False otherwise
	 */
	virtual bool SupportSuggestionType(EAvaPlaylistSearchListType InSuggestionType) const = 0;
};
