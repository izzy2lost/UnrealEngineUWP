// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/List.h"
#include <collection.h>

DECLARE_MULTICAST_DELEGATE(FOnSubscriptionLost);
typedef FOnSubscriptionLost::FDelegate FOnSubscriptionLostDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSessionNeedsInitialState, FName);
typedef FOnSessionNeedsInitialState::FDelegate FOnSessionNeedsInitialStateDelegate;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSessionChanged, FName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes);
typedef FOnSessionChanged::FDelegate FOnSessionChangedDelegate;

class FSessionMessageRouter
{
	struct FSessionChangedDelegatePair
	{
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
		const FOnSessionChangedDelegate& Delegate;

		FSessionChangedDelegatePair(const FOnSessionChangedDelegate& InDelegate,
									Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference)
			: SessionReference(InSessionReference)
			, Delegate(InDelegate)
		{
		}

		bool BoundTo(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference) const;

		bool Equals(const FSessionChangedDelegatePair& Other) const;

		bool operator== (const FSessionChangedDelegatePair& Other) const
		{
			return Equals(Other);
		}
	};

	typedef TDoubleLinkedList<FSessionChangedDelegatePair> DelegateList;
	DelegateList RegisteredDelegates;
	mutable FCriticalSection DelegateLock;

public:
	FSessionMessageRouter(class FOnlineSubsystemLive* InSubsystem);
	~FSessionMessageRouter();

	void TriggerOnSessionChangedDelegates(
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference,
		FName SessionName,
		Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff) const;

	void AddOnSessionChangedDelegate(const FOnSessionChangedDelegate& Delegate, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference);
	void ClearOnSessionChangedDelegate(const FOnSessionChangedDelegate& Delegate, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference);


	//Delegates
	DEFINE_ONLINE_DELEGATE(OnSubscriptionLost);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnSessionNeedsInitialState, FName);

	void SyncInitialSessionState(FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session);

	void SubscribeAllUsersToMultiplayerEvents();
	void UnsubscribeAllUsersFromMultiplayerEvents();

PACKAGE_SCOPE:
	uint64 GetLastProcessedChangeNumber(const FString& Branch);
	void SetLastProcessedChangeNumber(const FString& Branch, uint64 ChangeNumber);

private:
	//Prevent copies
	FSessionMessageRouter(const FSessionMessageRouter&) = delete;
	FSessionMessageRouter& operator=(const FSessionMessageRouter&) = delete;

	/** Start listening to XBL session change events as the specified user. */
	void SubscribeToMultiplayerEvents(Windows::Xbox::System::User^ SubscribingUser);

	/** Stop listening to XBL session change events as the specified user. */
	// @ATG_CHANGE : BEGIN - UWP LIVE support
	void UnsubscribeFromMultiplayerEvents(const FUniqueNetId& SubscribedUser);
	// @ATG_CHANGE : END

	/** Detect a loss of connection to the subscription service and exit multiplayer. */
	void OnMultiplayerSubscriptionsLost(Windows::Xbox::System::User^ User);

	/** Handle changes to the game session */
	void OnMultiplayerSessionChanged(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs^ EventArgs);

private:
	class FOnlineSubsystemLive* LiveSubsystem;

	TMap<FString, uint64> LastSeenChangeNumberMap, LastProcessedChangeNumberMap;
	FCriticalSection LastSeenChangeNumberMapLock, LastProcessedChangeNumberMapLock;

	/** Tokens to track session subscriptions */
	struct MultiplayerSubscriptionTokenPair
	{
		Windows::Foundation::EventRegistrationToken SessionChangedToken;
		Windows::Foundation::EventRegistrationToken SubscriptionsLostToken;
	};
	// @ATG_CHANGE : BEGIN - UWP LIVE support
	TMap<FString, MultiplayerSubscriptionTokenPair> MultiplayerSubscriptionTokens;
	// @ATG_CHANGE : END

	/** Token for un-registering the signin callback */
	Windows::Foundation::EventRegistrationToken SignInCompletedToken;

	/** Token for un-registering the signout callback */
	Windows::Foundation::EventRegistrationToken SignOutStartedToken;
};