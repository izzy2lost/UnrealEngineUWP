// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"

#include <vector>

class FOnlineSessionSearch;
class FOnlineSessionLive;

/** 
 * Async item used to marshal search results from the system callback thread to the game thread.
 */
class FOnlineAsyncTaskLiveFindSessions : public FOnlineAsyncItem
{
private:
	/** Pointer to the settings used for this search. */
	TSharedPtr<FOnlineSessionSearch> SearchSettings;

	/** All the results returned from the MPSD. */
	std::vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> SearchResults;

	/** Parallel vector of profiles of the host of each session, used to get the GameDisplayName */
	Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ Profiles;

	/** The session that created this task */
	FOnlineSessionLive* SessionInterface;

public:
	/**
	 * Constructor
	 *
	 * @param InSessionInterface The session that created this task
	 * @param InSearchSettings The settings used for this search
	 * @param InSearchResults The results as returned by the Live service
	 */
	FOnlineAsyncTaskLiveFindSessions(
		class FOnlineSessionLive* InSessionInterface,
		TSharedPtr<FOnlineSessionSearch> InSearchSettings,
		std::vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^> InSearchResults,
		Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::XboxUserProfile^>^ InProfiles);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("FindSessions"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;
};
