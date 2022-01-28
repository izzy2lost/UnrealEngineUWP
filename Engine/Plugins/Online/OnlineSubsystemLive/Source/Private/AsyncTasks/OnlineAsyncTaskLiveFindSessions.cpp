// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveFindSessions.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"

#include <collection.h>

using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Social;
using namespace Windows::Foundation::Collections;

FOnlineAsyncTaskLiveFindSessions::FOnlineAsyncTaskLiveFindSessions(
	FOnlineSessionLive* InSessionInterface,
	TSharedPtr<FOnlineSessionSearch> InSearchSettings,
	std::vector<MultiplayerSession^> InSearchResults,
	IVectorView<XboxUserProfile^>^ InProfiles) 
	: SearchSettings(InSearchSettings)
	, SearchResults(std::move(InSearchResults))
	, SessionInterface(InSessionInterface)
	, Profiles(InProfiles)
{
	check(SessionInterface);
}

void FOnlineAsyncTaskLiveFindSessions::Finalize() 
{
	// Free up previous results
	SearchSettings->SearchResults.Empty();

	// Copy results from SearchResults
	for(size_t i = 0; i < SearchResults.size(); ++i)
	{
		Platform::String^ HostDisplayName = nullptr;
		if(Profiles && Profiles->GetAt(i))
		{
			HostDisplayName = Profiles->GetAt(i)->GameDisplayName;
		}
		SearchSettings->SearchResults.Add(FOnlineSessionLive::CreateSearchResultFromSession(SearchResults[i], HostDisplayName));
	}

	SearchSettings->SearchState = EOnlineAsyncTaskState::Done;

	SessionInterface->CurrentSessionSearch = nullptr;
}

void FOnlineAsyncTaskLiveFindSessions::TriggerDelegates()
{
	SessionInterface->TriggerOnFindSessionsCompleteDelegates(true); 
}
