// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "SessionMessageRouter.h"

#include "OnlineAsyncTaskmanagerLive.h"
#include "OnlineSubsystemLive.h"
#include "AsyncTasks/OnlineAsyncTaskLiveSessionChanged.h"
#include "Misc/ScopeLock.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::RealTimeActivity;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Xbox::System;
using namespace Windows::Foundation;
using namespace concurrency;

bool FSessionMessageRouter::FSessionChangedDelegatePair::Equals(const FSessionMessageRouter::FSessionChangedDelegatePair& Other) const
{
	if (FOnlineSubsystemLive::AreSessionReferencesEqual(SessionReference, Other.SessionReference) == false)
	{
		return false;
	}

	//Compare delegate target
	if (Delegate.GetUObject() != Other.Delegate.GetUObject())
	{
		return false;
	}

	return &Delegate == &Other.Delegate || (!Delegate.IsBound() && !Other.Delegate.IsBound());
}

bool FSessionMessageRouter::FSessionChangedDelegatePair::BoundTo(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference) const
{
	return FOnlineSubsystemLive::AreSessionReferencesEqual(InSessionReference, SessionReference);
}

void FSessionMessageRouter::TriggerOnSessionChangedDelegates(
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference,
	FName SessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes Diff) const
{
	FScopeLock Lock(&DelegateLock);
	TArray<FOnSessionChangedDelegate> DelegatesToRun;

	for (auto Pair : RegisteredDelegates)
	{
		if (Pair.BoundTo(SessionReference))
		{
			DelegatesToRun.Add(Pair.Delegate);
		}
	}

	// These need to be invoked after traversing RegisteredDelegates, since they may modify it
	for (auto DelegateToRun : DelegatesToRun)
	{
		DelegateToRun.ExecuteIfBound(SessionName, Diff);
	}
}

void FSessionMessageRouter::AddOnSessionChangedDelegate(const FOnSessionChangedDelegate& Delegate, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference)
{
	FScopeLock Lock(&DelegateLock);
	FSessionChangedDelegatePair DelegatePair(Delegate, SessionReference);
	if (RegisteredDelegates.FindNode(DelegatePair) == nullptr)
	{
		RegisteredDelegates.AddTail(DelegatePair);
	}
}

void FSessionMessageRouter::ClearOnSessionChangedDelegate(const FOnSessionChangedDelegate& Delegate, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference)
{
	FScopeLock Lock(&DelegateLock);
	if (auto Node = RegisteredDelegates.FindNode(FSessionChangedDelegatePair(Delegate, SessionReference)))
	{
		RegisteredDelegates.RemoveNode(Node);
	}
}

FSessionMessageRouter::FSessionMessageRouter(FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
{
	EventHandler<SignInCompletedEventArgs^>^ SignInCompletedEvent = ref new EventHandler<SignInCompletedEventArgs^>(
		[this] (Platform::Object^, SignInCompletedEventArgs^ EventArgs)
	{
		// Since this event is called from non-game threads, marshal the call to the game thread
		// to ensure that data access is safe.
		LiveSubsystem->ExecuteNextTick([this, EventArgs]()
		{
			SubscribeToMultiplayerEvents(EventArgs->User);
		});
	});
	SignInCompletedToken = Windows::Xbox::System::User::SignInCompleted += SignInCompletedEvent;

	EventHandler<SignOutStartedEventArgs^>^ SignOutStartedEvent = ref new EventHandler<SignOutStartedEventArgs^>(
		[this] (Platform::Object^, SignOutStartedEventArgs^ EventArgs)
	{
		// Since this event is called from non-game threads, marshal the call to the game thread
		// to ensure that data access is safe.
		LiveSubsystem->ExecuteNextTick([this, EventArgs]()
		{
			// @ATG_CHANGE : UWP Live Support - BEGIN
			UnsubscribeFromMultiplayerEvents(FUniqueNetIdLive(EventArgs->User->XboxUserId->Data()));
			// @ATG_CHANGE : UWP Live Support - END
		});
	});
	SignOutStartedToken = Windows::Xbox::System::User::SignOutStarted += SignOutStartedEvent;

	SubscribeAllUsersToMultiplayerEvents();
}

FSessionMessageRouter::~FSessionMessageRouter()
{
	try
	{
		Windows::Xbox::System::User::SignInCompleted -= SignInCompletedToken;
		Windows::Xbox::System::User::SignOutStarted -= SignOutStartedToken;
	}
	catch (Platform::Exception^ )
	{
		UE_LOG_ONLINE(Warning, TEXT("User Exception during shutdown"));
	}

	UnsubscribeAllUsersFromMultiplayerEvents();
}

void FSessionMessageRouter::SubscribeAllUsersToMultiplayerEvents()
{
	for (User^ CurrentUser : Windows::Xbox::System::User::Users)
	{
		if (CurrentUser->IsSignedIn)
		{
			SubscribeToMultiplayerEvents(CurrentUser);
		}
	}
}

void FSessionMessageRouter::UnsubscribeAllUsersFromMultiplayerEvents()
{
	ensure(IsInGameThread());

	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ CachedUsers = Windows::Xbox::System::User::Users;
	const int32 CachedUsersSize = static_cast<int32>(CachedUsers->Size);

	// @ATG_CHANGE : BEGIN uwp support
	TArray<FString> SubscribedUserIds;
	MultiplayerSubscriptionTokens.GenerateKeyArray(SubscribedUserIds);

	for (FString UserId : SubscribedUserIds)
	{
		// This call seems to crash on app resume when the UserId is valid but the User^ has signed out during suspend
		//Windows::Xbox::System::User^ SubscribedUser = Windows::Xbox::System::User::GetUserById(UserId);

		Windows::Xbox::System::User^ SubscribedUser = nullptr;
		for (int32 CachedUserIndex = 0; CachedUserIndex < CachedUsersSize; ++CachedUserIndex)
		{
			Windows::Xbox::System::User^ CurrentUser = CachedUsers->GetAt(CachedUserIndex);
			if ((CurrentUser != nullptr) && (CurrentUser)->XboxUserId->Data() == UserId)
			{
				SubscribedUser = CurrentUser;
				break;
			}
		}

		if (SubscribedUser)
		{
			UnsubscribeFromMultiplayerEvents(FUniqueNetIdLive(SubscribedUser->XboxUserId->Data()));
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Couldn't find user with ID %u to unsubscribe from multiplayer events."), *UserId);
			MultiplayerSubscriptionTokens.Remove(UserId);
		}
	}
	// @ATG_CHANGE : END
}

void FSessionMessageRouter::SubscribeToMultiplayerEvents(Windows::Xbox::System::User^ SubscribingUser)
{
	ensure(IsInGameThread());

	XboxLiveContext^ UserContext = LiveSubsystem->GetLiveContext(SubscribingUser);
	if (UserContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FSessionMessageRouter::SubscribeToMultiplayerEvents failed - couldn't get XboxLiveContext for user %s."), SubscribingUser->XboxUserId->Data());
		return;
	}

	if (!UserContext->MultiplayerService->MultiplayerSubscriptionsEnabled)
	{
		Windows::Foundation::EventRegistrationToken SessionChangedToken = UserContext->MultiplayerService->MultiplayerSessionChanged +=
			ref new Windows::Foundation::EventHandler<MultiplayerSessionChangeEventArgs^>(
			[this](Platform::Object^, MultiplayerSessionChangeEventArgs^ EventArgs)
		{
			OnMultiplayerSessionChanged(EventArgs);
		});

		Windows::Foundation::EventRegistrationToken SubscriptionsLostToken = UserContext->MultiplayerService->MultiplayerSubscriptionLost +=
			ref new Windows::Foundation::EventHandler<MultiplayerSubscriptionLostEventArgs^>(
			[this, SubscribingUser](Platform::Object^, MultiplayerSubscriptionLostEventArgs^ EventArgs)
		{
			UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter: MultiplayerSubscriptionsLost - event thread, adding async task"));

			LiveSubsystem->ExecuteNextTick([this, SubscribingUser]()
			{
				OnMultiplayerSubscriptionsLost(SubscribingUser);
			});
		});

		MultiplayerSubscriptionTokenPair EventTokenPair;
		EventTokenPair.SessionChangedToken = SessionChangedToken;
		EventTokenPair.SubscriptionsLostToken = SubscriptionsLostToken;

		// @ATG_CHANGE : BEGIN - UWP LIVE support
		MultiplayerSubscriptionTokens.Add(SubscribingUser->XboxUserId->Data(), EventTokenPair);
		// @ATG_CHANGE : END

		UserContext->MultiplayerService->EnableMultiplayerSubscriptions();

		UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter::SubscribeToMultiplayerEvents created subscription for user %s."), SubscribingUser->XboxUserId->Data());
	}
}

// @ATG_CHANGE : BEGIN - UWP LIVE support
void FSessionMessageRouter::UnsubscribeFromMultiplayerEvents(const FUniqueNetId& SubscribedUser)
// @ATG_CHANGE : END
{
	ensure(IsInGameThread());

	auto UserContext = LiveSubsystem->GetLiveContext(SubscribedUser);

	if (UserContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FSessionMessageRouter::UnsubscribeFromMultiplayerEvents failed - couldn't get XboxLiveContext for user %s."), *SubscribedUser.ToString());
		return;
	}

	MultiplayerSubscriptionTokenPair EventTokenPair;
	// @ATG_CHANGE : BEGIN - UWP LIVE support
	if (MultiplayerSubscriptionTokens.RemoveAndCopyValue(SubscribedUser.ToString(), EventTokenPair))
	// @ATG_CHANGE : END
	{
		UserContext->MultiplayerService->MultiplayerSessionChanged -= EventTokenPair.SessionChangedToken;
		UserContext->MultiplayerService->MultiplayerSubscriptionLost -= EventTokenPair.SubscriptionsLostToken;
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FSessionMessageRouter::UnsubscribeFromMultiplayerEvents - couldn't find event tokens for user %s."), *SubscribedUser.ToString());
	}

	if (UserContext->MultiplayerService->MultiplayerSubscriptionsEnabled)
	{
		UserContext->MultiplayerService->DisableMultiplayerSubscriptions();
	}

	UE_LOG_ONLINE(Log, TEXT("FOnlineSessionLive::UnsubscribeFromMultiplayerEvents succeeded for user %s."), *SubscribedUser.ToString());
}

/** Detect a loss of connection to the subscription service and exit multiplayer. */
void FSessionMessageRouter::OnMultiplayerSubscriptionsLost(Windows::Xbox::System::User^ User)
{
	check(IsInGameThread());

	UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter::OnMultiplayerSubscriptionsLost - game thread"));
	UE_LOG_ONLINE(Log, TEXT("  Connection to multiplayer service lost. Destroying session objects."));

	// Dispatch event so individual systems can figure out what to do.
	TriggerOnSubscriptionLostDelegates();

	// Re-enable subscriptions so game can attempt to recover
	XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(User);
	if (LiveContext == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FSessionMessageRouter::OnMultiplayerSubscriptionsLost - couldn't get XboxLiveContext for user %s."), User->XboxUserId->Data());
		return;
	}

	LiveContext->MultiplayerService->EnableMultiplayerSubscriptions();
}

void FSessionMessageRouter::OnMultiplayerSessionChanged(MultiplayerSessionChangeEventArgs^ EventArgs)
{
	UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter::OnMultiplayerSessionChanged - Branch: %s, ChangeNumber: %u"), EventArgs->Branch->Data(), EventArgs->ChangeNumber);

	// If there are multiple local users, we'll get multiple events for each session change. Only process the
	// event if we haven't seen this change yet.
	FScopeLock Lock(&LastSeenChangeNumberMapLock);

	FString BranchString(EventArgs->Branch->Data());
	uint64* lastSeenChangeNumber = LastSeenChangeNumberMap.Find(BranchString);

	if (lastSeenChangeNumber == nullptr || *lastSeenChangeNumber < EventArgs->ChangeNumber)
	{
		LastSeenChangeNumberMap.Add(BranchString, EventArgs->ChangeNumber);
		UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter::OnMultiplayerSessionChanged - adding async task"));

		//Task needs to be constructed from game thread
		LiveSubsystem->GetAsyncTaskManager()->AddGenericToInQueue([this, EventArgs]()
		{
			FOnlineAsyncTaskLiveSessionChanged* SessionChangedTask = new FOnlineAsyncTaskLiveSessionChanged(
				LiveSubsystem,
				EventArgs->SessionReference,
				EventArgs->Branch->Data(),
				EventArgs->ChangeNumber);

			LiveSubsystem->QueueAsyncTask(SessionChangedTask);
		});
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("FSessionMessageRouter: MultiplayerSessionChanged - duplicate event, ignoring"));
	}
}

void FSessionMessageRouter::SyncInitialSessionState(FName SessionName, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session)
{
	check(IsInGameThread());

	TriggerOnSessionNeedsInitialStateDelegates(SessionName);

	// Set baseline for future change notifications. Session should be the most recent MultiplayerSession object for the new
	// game or match session.
	LiveSubsystem->SetLastDiffedSession(SessionName, Session);
	SetLastProcessedChangeNumber(Session->Branch->Data(), Session->ChangeNumber);
}

uint64 FSessionMessageRouter::GetLastProcessedChangeNumber(const FString& Branch)
{
	FScopeLock Lock(&LastProcessedChangeNumberMapLock);
	uint64* lastProcessedChangeNumber = LastProcessedChangeNumberMap.Find(Branch);

	return (lastProcessedChangeNumber == nullptr) ? 0 : *lastProcessedChangeNumber;
}

void FSessionMessageRouter::SetLastProcessedChangeNumber(const FString& Branch, uint64 ChangeNumber)
{
	FScopeLock Lock(&LastProcessedChangeNumberMapLock);

	if(GetLastProcessedChangeNumber(Branch) < ChangeNumber)
	{
		LastProcessedChangeNumberMap.Add(Branch, ChangeNumber);
	}
}
