// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "../OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineAsyncTaskLiveRegisterLocalUser.h"
#include "OnlineAsyncTaskLiveSetSessionActivity.h"

using namespace Windows::Xbox::Networking;
using namespace Microsoft::Xbox::Services::Multiplayer;

FOnlineAsyncTaskLiveRegisterLocalUser::FOnlineAsyncTaskLiveRegisterLocalUser(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	const FUniqueNetIdLive& InUserId,
	const FOnRegisterLocalPlayerCompleteDelegate& InDelegate)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, DefaultRetryCount)
	, UserId(InUserId)
	, Delegate(InDelegate)
	, Result(EOnJoinSessionCompleteResult::Success)
{
}

FOnlineAsyncTaskLiveRegisterLocalUser::FOnlineAsyncTaskLiveRegisterLocalUser(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	const FUniqueNetIdLive& InUserId,
	const FOnRegisterLocalPlayerCompleteDelegate& InDelegate,
	Windows::Foundation::Collections::IVectorView<MultiplayerSessionMember^>^ InInitializationGroup)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, DefaultRetryCount)
	, UserId(InUserId)
	, Delegate(InDelegate)
	, Result(EOnJoinSessionCompleteResult::Success)
	, InitializationGroup(InInitializationGroup)
{
}

bool FOnlineAsyncTaskLiveRegisterLocalUser::UpdateSession(MultiplayerSession^ Session)
{
	// Don't join if the session is full.
	// @ATG_CHANGE : UWP Live Support - BEGIN
	if (!Subsystem->GetSessionInterfaceLive()->CanUserJoinSession(SystemUserFromXSAPIUser(GetLiveContext()->User), Session))
	// @ATG_CHANGE : UWP Live Support - END
	{
		Result = EOnJoinSessionCompleteResult::SessionIsFull;
		return false;
	}

	// @ATG_CHANGE : BEGIN - UWP LIVE support
	Session->Join(nullptr, true, false);
	// @ATG_CHANGE : END
	Session->SetCurrentUserStatus(MultiplayerSessionMemberStatus::Active);
	Session->SetCurrentUserSecureDeviceAddressBase64(SecureDeviceAddress::GetLocal()->GetBase64String());
	
	// Indicate what events to subscribe to
	Session->SetSessionChangeSubscription(MultiplayerSessionChangeTypes::Everything);

	// Specify init groups if necessary
	if(InitializationGroup != nullptr)
	{
		Session->SetCurrentUserMembersInGroup(InitializationGroup);
	}

	return true;
}

void FOnlineAsyncTaskLiveRegisterLocalUser::TriggerDelegates()
{
	if (!WasSuccessful())
	{
		Result = EOnJoinSessionCompleteResult::UnknownError;
	}

	Delegate.ExecuteIfBound(UserId, Result);
}

void FOnlineAsyncTaskLiveRegisterLocalUser::Finalize()
{
	FOnlineAsyncTaskLiveSafeWriteSession::Finalize();
		
	// Set this as the user's activity
	Subsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveSetSessionActivity>(Subsystem, GetLiveContext(), GetSessionReference());
}
