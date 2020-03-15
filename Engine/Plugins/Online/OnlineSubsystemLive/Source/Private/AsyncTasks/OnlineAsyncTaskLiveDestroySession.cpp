// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveDestroySession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "Interfaces/VoiceInterface.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;

using Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember;

//@todo: Ideally, this should be able to destroy both the XBL game session and match session associated
// with a NamedSession at the same time.

void CreateDestroyMatchmakingCompleteTask(FName SessionName,
								   MultiplayerSession^ Session,
								   FOnlineSubsystemLive* Subsystem,
								   bool bWasSuccessful)
{
	// This function should only be called on the game thread.
	check(IsInGameThread());
	check(Subsystem);

	FOnlineMatchmakingInterfaceLivePtr MatchmakingInt = Subsystem->GetMatchmakingInterfaceLive();
	check(MatchmakingInt.IsValid());

	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Unable to leave session %s, session gone. bWasSuccessful: 0"), *SessionName.ToString());

		// Session timed out?
		FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
		// This case only occurs if something failed during matchmaking, so always pass false for bWasSuccessful.
		MatchmakingInt->TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		return;
	}

	const FOnlineIdentityLivePtr IdentityInt = Subsystem->GetIdentityLive();
	check(IdentityInt.IsValid());

	// Find the local player in the session and make them Leave() it.
	for (MultiplayerSessionMember^ Member : Session->Members)
	{
		FUniqueNetIdLive MemberId(Member->XboxUserId);
		Windows::Xbox::System::User^ MemberUser = IdentityInt->GetUserForUniqueNetId(MemberId);
		if (MemberUser != nullptr)
		{
			// Found a member to Leave() the session.
			XboxLiveContext^ UserContext = Subsystem->GetLiveContext(MemberUser);
			check(UserContext);

			UE_LOG_ONLINE(Log, TEXT("Leaving session %s for user %ls"), *SessionName.ToString(), Member->XboxUserId->Data());
			Subsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveDestroyMatchmakingSession>(SessionName, UserContext, Subsystem);
			return;
		}
	}
	
	UE_LOG_ONLINE(Log, TEXT("Finished leaving Session %s for all local-members bWasSuccessful: 0"), *SessionName.ToString());
	// If we get here, we couldn't find a local user in the session, so remove it and trigger the success delegate.
	FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
	// This case only occurs if something failed during matchmaking, so always pass false for bWasSuccessful.
	MatchmakingInt->TriggerOnMatchmakingCompleteDelegates(SessionName, false);
}

void CreateDestroyTask(FName SessionName,
					   MultiplayerSession^ Session,
					   FOnlineSubsystemLive* Subsystem,
					   bool bWasSuccessful,
					   const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	// This function should only be called on the game thread.
	check(IsInGameThread());
	check(Subsystem);

	FOnlineSessionLivePtr SessionInt = Subsystem->GetSessionInterfaceLive();
	check(SessionInt.IsValid());

	if (Session == nullptr)
	{
		// Session timed out?
		UE_LOG_ONLINE(Warning, TEXT("Unable to leave session %s, session gone? bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
		FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
		CompletionDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
		SessionInt->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
		return;
	}

	const FOnlineIdentityLivePtr IdentityInt = Subsystem->GetIdentityLive();
	check(IdentityInt.IsValid());

	// Find the local player in the session and make them Leave() it.
	for (MultiplayerSessionMember^ Member : Session->Members)
	{
		FUniqueNetIdLive MemberId(Member->XboxUserId);
		Windows::Xbox::System::User^ MemberUser = IdentityInt->GetUserForUniqueNetId(MemberId);
		if (MemberUser != nullptr)
		{
			// Found a member to Leave() the session.
			XboxLiveContext^ UserContext = Subsystem->GetLiveContext(MemberUser);
			check(UserContext);

			UE_LOG_ONLINE(Log, TEXT("Leaving session %s for user %ls"), *SessionName.ToString(), Member->XboxUserId->Data());
			Subsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveDestroySession>(SessionName, UserContext, Subsystem, CompletionDelegate);
			return;
		}
	}

	UE_LOG_ONLINE(Log, TEXT("Finished leaving Session %s for all local-members bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
	// If we get here, we couldn't find a local user in the session, so remove it and trigger the success delegate.
	FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
	CompletionDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
	SessionInt->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
}

FOnlineAsyncTaskLiveDestroySessionBase::FOnlineAsyncTaskLiveDestroySessionBase(FName InSessionName, Microsoft::Xbox::Services::XboxLiveContext^ InContext, FOnlineSubsystemLive* InSubsystem)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, DefaultRetryCount)
{
}

bool FOnlineAsyncTaskLiveDestroySessionBase::UpdateSession(MultiplayerSession^ Session)
{
	Session->Leave();
	return true;
}

void FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(FOnlineSubsystemLive* Subsystem, FName SessionName)
{
	FOnlineSessionLivePtr SessionInt = Subsystem->GetSessionInterfaceLive();
	FOnlineMatchmakingInterfaceLivePtr MatchmakingInt = Subsystem->GetMatchmakingInterface();

	SessionInt->RemoveNamedSession(SessionName);
	MatchmakingInt->RemoveMatchmakingTicket(SessionName);

	// TODO: remove this block? The game should be deciding when to talk or not talk?
	if (SessionInt->GetNumSessions() == 0)
	{
		IOnlineVoicePtr VoiceInt = Subsystem->GetVoiceInterface();
		if (VoiceInt.IsValid())
		{
			if (!Subsystem->IsDedicated())
			{
				// Stop local talkers
				VoiceInt->UnregisterLocalTalkers();
			}

			// Stop remote voice 
			VoiceInt->RemoveAllRemoteTalkers();
		}
	}
}

void FOnlineAsyncTaskLiveDestroyMatchmakingSession::Finalize()
{
	FOnlineAsyncTaskLiveSafeWriteSession::Finalize();

	// Attempt to find another local user to Leave() the session.
	CreateDestroyMatchmakingCompleteTask(GetSessionName(), GetLatestLiveSession(), Subsystem, bWasSuccessful);
}

void FOnlineAsyncTaskLiveDestroySession::Finalize()
{
	FOnlineAsyncTaskLiveSafeWriteSession::Finalize();

	// Attempt to find another local user to Leave() the session.
	CreateDestroyTask(GetSessionName(), GetLatestLiveSession(), Subsystem, bWasSuccessful, CompletionDelegate);
}
