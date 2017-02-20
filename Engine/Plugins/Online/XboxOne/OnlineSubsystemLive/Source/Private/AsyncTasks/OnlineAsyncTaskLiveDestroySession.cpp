// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveDestroySession.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "../OnlineIdentityInterfaceLive.h"
#include "../OnlineMatchmakingInterfaceLive.h"
#include "VoiceInterface.h"

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace concurrency;

//@todo: Ideally, this should be able to destroy both the XBL game session and match session associated
// with a NamedSession at the same time.

void CreateDestroyMatchmakingCompleteTask(FName SessionName,
								   MultiplayerSession^ Session,
								   FOnlineSubsystemLive* Subsystem,
								   bool bWasSuccessful)
{
	// This function should only be called on the game thread.
	check(Subsystem);

	if (Session == nullptr)
	{
		// Session timed out?
		FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
		// This case only occurs if something failed during matchmaking, so always pass false for bWasSuccessful.
		Subsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(SessionName, false);
		return;
	}

	// Find a local player in the session and make him Leave() it.
	for (auto Member : Session->Members)
	{
		FUniqueNetIdLive MemberId(Member->XboxUserId);
		Windows::Xbox::System::User^ MemberUser = Subsystem->GetIdentityLive()->GetUserForUniqueNetId(MemberId);
		if (MemberUser != nullptr)
		{
			// Found a member to Leave() the session.
			XboxLiveContext^ UserContext = Subsystem->GetLiveContext(MemberUser);
			FOnlineAsyncTaskLiveDestroySessionBase* DestroyTask = new FOnlineAsyncTaskLiveDestroyMatchmakingSession(SessionName, UserContext, Subsystem);
			Subsystem->QueueAsyncTask(DestroyTask);
			return;
		}
	}

	// If we get here, we couldn't find a local user in the session, so remove it and trigger the success delegate.
	FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
	// This case only occurs if something failed during matchmaking, so always pass false for bWasSuccessful.
	Subsystem->GetMatchmakingInterfaceLive()->TriggerOnMatchmakingCompleteDelegates(SessionName, false);
}

void CreateDestroyTask(FName SessionName,
					   MultiplayerSession^ Session,
					   FOnlineSubsystemLive* Subsystem,
					   bool bWasSuccessful,
					   const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	// This function should only be called on the game thread.
	check(Subsystem);

	if (Session == nullptr)
	{
		// Session timed out?
		FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
		CompletionDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
		Subsystem->GetSessionInterfaceLive()->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
		return;
	}

	// Find a local player in the session and make him Leave() it.
	for (auto Member : Session->Members)
	{
		FUniqueNetIdLive MemberId(Member->XboxUserId);
		Windows::Xbox::System::User^ MemberUser = Subsystem->GetIdentityLive()->GetUserForUniqueNetId(MemberId);
		if (MemberUser != nullptr)
		{
			// Found a member to Leave() the session.
			XboxLiveContext^ UserContext = Subsystem->GetLiveContext(MemberUser);			
			FOnlineAsyncTaskLiveDestroySessionBase* DestroyTask = new FOnlineAsyncTaskLiveDestroySession(SessionName, UserContext, Subsystem, CompletionDelegate);
			Subsystem->QueueAsyncTask(DestroyTask);
			return;
		}
	}

	// If we get here, we couldn't find a local user in the session, so remove it and trigger the success delegate.
	FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(Subsystem, SessionName);
	CompletionDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
	Subsystem->GetSessionInterfaceLive()->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
}

FOnlineAsyncTaskLiveDestroySessionBase::FOnlineAsyncTaskLiveDestroySessionBase(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, DefaultRetryCount)
{
}

bool FOnlineAsyncTaskLiveDestroySessionBase::UpdateSession(MultiplayerSession^ Session)
{
	Session->Leave();
	return true;
}

void FOnlineAsyncTaskLiveDestroySessionBase::RemoveAndCleanupSession(
	FOnlineSubsystemLive* Subsystem,
	FName SessionName)
{
	Subsystem->GetSessionInterfaceLive()->RemoveNamedSession(SessionName);
	Subsystem->GetMatchmakingInterface()->RemoveMatchmakingTicket(SessionName);

	if (Subsystem->GetSessionInterfaceLive()->GetNumSessions() == 0)
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
