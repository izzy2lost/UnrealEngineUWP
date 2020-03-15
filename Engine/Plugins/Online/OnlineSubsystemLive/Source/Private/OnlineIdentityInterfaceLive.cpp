// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManagerLive.h"
// @ATG_CHANGE : BEGIN UWP Live Support
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "AsyncTasks/OnlineAsyncTaskLiveGetXSTSToken.h"
#if PLATFORM_XBOXONE
#include "AsyncTasks/OnlineAsyncTaskLiveCheckForPackageUpdate.h"
#endif
// @ATG_CHANGE : END
#include "AsyncTasks/OnlineAsyncTaskLiveGetOverallReputation.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// @ATG_CHANGE : UWP Live Support - platform headers moved to pch

using namespace Microsoft;
using namespace Windows::Foundation;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Input;

using namespace concurrency;

namespace
{
	/** Helper function to get an Unreal login status from a User^. */
	ELoginStatus::Type GetLoginStatusForUser(User^ InUser)
	{
		if (InUser == nullptr)
		{
			return ELoginStatus::NotLoggedIn;
		}
		
		if (InUser->IsSignedIn)
		{
			return ELoginStatus::LoggedIn;
		}
		
		return ELoginStatus::UsingLocalProfile;
	}
}

FOnlineIdentityLive::FOnlineIdentityLive(class FOnlineSubsystemLive* InSubsystem) :
	LiveSubsystem(InSubsystem)
{
	check(LiveSubsystem);

	// Store the Login XSTS endpoint for later XSTS generation
	GConfig->GetString(TEXT("OnlineSubsystemLive"), TEXT("LoginXSTSEndpoint"), LoginXSTSEndpoint, GEngineIni);

	HookLiveEvents();
}

FOnlineIdentityLive::~FOnlineIdentityLive()
{
	UnhookLiveEvents();
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityLive::GetUserAccount(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	const TSharedRef<FUserOnlineAccountLive>* FoundUserPtr = OnlineUsers.Find(LiveUserId);
	if (FoundUserPtr != nullptr)
	{
		return *FoundUserPtr;
	}

	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityLive::GetAllUserAccounts() const 
{
	TArray<TSharedPtr<FUserOnlineAccount> > UserAccounts;
	UserAccounts.Empty(OnlineUsers.Num());

	for (const TPair<FUniqueNetIdLive, TSharedRef<FUserOnlineAccountLive> >& Pair : OnlineUsers)
	{
		UserAccounts.Emplace(Pair.Value);
	}

	return UserAccounts;
}

bool FOnlineIdentityLive::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	if (LocalUserNum < 0 || LocalUserNum > MAX_LOCAL_PLAYERS)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdLive(), FString::Printf(TEXT("Invalid controller index %d"), LocalUserNum));
		return false;
	}

	TSharedPtr<const FUniqueNetId> UserId = FOnlineIdentityLive::GetUniquePlayerId(LocalUserNum);
	if (!UserId.IsValid())
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdLive(), FString::Printf(TEXT("No user logged in for controller=%d"), LocalUserNum));
		return false;
	}

	// @ATG_CHANGE : BEGIN - Support passing endpoint requiring XSTS token via AccountCredentials
	FString TargetEndpoint = AccountCredentials.Type;
	if (TargetEndpoint.IsEmpty())
	{
		// Default endpoint
		TargetEndpoint = LoginXSTSEndpoint;
	}
	// If there is no configured Endpoint, we do not need to fetch a XSTS token
	if (TargetEndpoint.IsEmpty())
	// @ATG_CHANGE : END
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, FString());
		return true;
	}

	Windows::Xbox::System::User^ XboxUser = GetUserForUniqueNetId(static_cast<const FUniqueNetIdLive&>(*UserId));
	if (!XboxUser)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, FString::Printf(TEXT("No user logged in for controller=%d"), LocalUserNum));
		return false;
	}

	const auto OnLoginCompleteDelegate = FOnXSTSTokenCompleteDelegate::CreateLambda(
	[this](FOnlineError Result, int32 LocalUserNum, const FUniqueNetId& UserId, const FString& ResultSignature, const FString& ResultToken)
	{
		// At this point, if we were successful, our auth token is saved on this user for using in listening delegates
		TriggerOnLoginCompleteDelegates(LocalUserNum, Result.bSucceeded, UserId, Result.ErrorMessage.ToString());

		// Check if we need to query this user's bad reputation state, and do so if needed
		const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);
		if (TSharedRef<FUserOnlineAccountLive>* OnlineUserLive = OnlineUsers.Find(LiveUserId))
		{
			// Check if our bad reputation is set
			TOptional<bool> BadReputationState((*OnlineUserLive)->GetIsBadReputation());
			if (!BadReputationState.IsSet())
			{
				// Find or create a live context for this user
				Microsoft::Xbox::Services::XboxLiveContext^ LocalUserLiveContext = LiveSubsystem->GetLiveContext(LiveUserId);
				if (LocalUserLiveContext)
				{
					// Have this user request their bad reputation state
					LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveGetOverallReputation>(
						LiveSubsystem,
						LocalUserLiveContext,
						MakeShared<FUniqueNetIdLive>(LiveUserId),
						FOnGetOverallReputationCompleteDelegate::CreateThreadSafeSP(this, &FOnlineIdentityLive::OnReputationQueryComplete));
				}
			}
		}
	});

	// @ATG_CHANGE : BEGIN - Support passing endpoint requiring XSTS token via AccountCredentials
	LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveGetXSTSToken>(LiveSubsystem, XboxUser, LocalUserNum, TargetEndpoint, OnLoginCompleteDelegate);
	// @ATG_CHANGE : END

	return true;
}

bool FOnlineIdentityLive::Logout(int32 LocalUserNum)
{
	TriggerOnLogoutCompleteDelegates(LocalUserNum,false);
	return false;
}

bool FOnlineIdentityLive::AutoLogin(int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}

ELoginStatus::Type FOnlineIdentityLive::GetLoginStatus(int32 ControllerIndex) const
{
	User^ RequestedUser = GetUserForPlatformUserId(ControllerIndex);
	return GetLoginStatusForUser(RequestedUser);
}

ELoginStatus::Type FOnlineIdentityLive::GetLoginStatus( const FUniqueNetId& UserId ) const 
{
	User^ RequestedUser = GetUserForUniqueNetId(FUniqueNetIdLive(UserId));
	return GetLoginStatusForUser(RequestedUser);
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::GetUniquePlayerId(int32 ControllerIndex) const
{
	User^ User = GetUserForPlatformUserId(ControllerIndex);
	if( User != nullptr )
	{
		return MakeShared<FUniqueNetIdLive>(User->XboxUserId->Data());
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::GetSponsorUniquePlayerId(int32 ControllerIndex) const
{
	User^ User = GetUserForPlatformUserId(ControllerIndex);
	if( User != nullptr )
	{
		if( User->Sponsor != nullptr )
		{
			return MakeShared<FUniqueNetIdLive>(User->Sponsor->XboxUserId->Data());
		}
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::CreateUniquePlayerId(uint8* Bytes, int32 Size) 
{
	if (Bytes != NULL && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShared<FUniqueNetIdLive>(StrId);
	}
	return NULL;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::CreateUniquePlayerId(const FString& Str)
{
	return MakeShared<FUniqueNetIdLive>(Str);
}

FString FOnlineIdentityLive::GetPlayerNickname(int32 ControllerIndex) const
{
	FString PlayerNickname;

	User^ RequestedUser = GetUserForPlatformUserId(ControllerIndex);
	if( RequestedUser )
	{
		PlayerNickname = FOnlineUserInfoLive::FilterPlayerName(RequestedUser->DisplayInfo->GameDisplayName);
		UE_LOG_ONLINE(VeryVerbose, TEXT("[FOnlineIdentityLive LocalPlayer Info] XUID: [%s] Nickname: [%s]"), RequestedUser->XboxUserId->Data(), *PlayerNickname);
	}

	return PlayerNickname;
}

FString FOnlineIdentityLive::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	FString PlayerNickname;

	User^ RequestedUser = GetUserForUniqueNetId(FUniqueNetIdLive(UserId));
	if( RequestedUser )
	{
		PlayerNickname = FOnlineUserInfoLive::FilterPlayerName(RequestedUser->DisplayInfo->GameDisplayName);
		UE_LOG_ONLINE(VeryVerbose, TEXT("[FOnlineIdentityLive LocalPlayer Info] XUID: [%s] Nickname: [%s]"), RequestedUser->XboxUserId->Data(), *PlayerNickname);
	}

	return PlayerNickname;
}

FString FOnlineIdentityLive::GetAuthToken(int32 ControllerIndex) const
{
	FString AuthToken;

	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(ControllerIndex);
	if (UserId.IsValid())
	{
		TSharedPtr<FUserOnlineAccountLive> UserAccount = StaticCastSharedPtr<FUserOnlineAccountLive>(GetUserAccount(*UserId));
		if (UserAccount.IsValid())
		{
			AuthToken = UserAccount->GetAccessToken();
		}
	}

	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("There was no AuthToken stored for ControllerId %d"), ControllerIndex);
	}

	return AuthToken;
}

void FOnlineIdentityLive::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineIdentityLive::RevokeAuthToken not implemented"));
	TSharedRef<const FUniqueNetId> UserIdRef(UserId.AsShared());
	LiveSubsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

Windows::Xbox::System::User^ FOnlineIdentityLive::GetUserForUniqueNetId(const FUniqueNetIdLive& UniqueId) const
{
	FPlatformUserId PlatformUserId = GetPlatformUserIdFromUniqueNetId(UniqueId);
	User^ UserPtr = GetUserForPlatformUserId(PlatformUserId);
	return UserPtr;
}

User^ FOnlineIdentityLive::GetUserForPlatformUserId(int32 PlatformUserId) const
{
#if PLATFORM_XBOXONE
	// This isn't thread safe
	check(IsInGameThread());

	const auto InputInterface = GetInputInterface();
	if (!InputInterface.IsValid())
	{
		return nullptr;
	}

	return InputInterface->GetXboxUserFromPlatformUserId(PlatformUserId);
#else
	// Note: assume SUA
	const FScopeLock CachedUsersScopeLock(&CachedUsersLock);
	return PlatformUserId == 0 && CachedUsers->Size > 0 ? CachedUsers->GetAt(0) : nullptr;
#endif
}

FPlatformUserId FOnlineIdentityLive::GetPlatformUserIdFromXboxUser( Windows::Xbox::System::User^ InUser ) const
{
// @ATG_CHANGE : BEGIN - UWP LIVE support, workaround for platform user no having a tie in to Live
#if PLATFORM_XBOXONE
	// This isn't thread safe
	check(IsInGameThread());

	if (!InUser)
	{
		return PLATFORMUSERID_NONE;
	}

	const auto InputInterface = GetInputInterface();
	if (!InputInterface.IsValid())
	{
		return PLATFORMUSERID_NONE;
	}

	return InputInterface->GetPlatformUserIdFromXboxUser(InUser);
#elif PLATFORM_UWP
	// Note: assume SUA for now - if we have a valid user then that's the one-and-only user
	return InUser ? 0 : -1;
#else
	return -1;
#endif
// @ATG_CHANGE : END
}

Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ FOnlineIdentityLive::GetCachedUsers() const
{
	return CachedUsers;
}

bool FOnlineIdentityLive::AddUserAccount(Windows::Xbox::System::User^ InUser)
{
	if (InUser)
	{
		FUniqueNetIdLive UserXboxId(InUser->XboxUserId);
		// @ATG_CHANGE : BEGIN - UWP LIVE support
		uint32 cachedUserIndex = 0;
		if (!CachedUsers->IndexOf(InUser, &cachedUserIndex))
		{
			RefreshGamepadsAndUsers();
		}
		// @ATG_CHANGE : END

		if (!OnlineUsers.Contains(UserXboxId))
		{
			UE_LOG_ONLINE(Log, TEXT("User %s added to OnlineUsers"), *UserXboxId.ToString());
			TSharedRef<FUserOnlineAccountLive> OnlineUser(MakeShared<FUserOnlineAccountLive>(InUser));
			OnlineUsers.Add(MoveTemp(UserXboxId), MoveTemp(OnlineUser));
			return true;
		}
		else
		{
			UE_LOG_ONLINE(Log, TEXT("User %s already exists in OnlineUsers"), *UserXboxId.ToString());
		}
	}

	return false;
}

bool FOnlineIdentityLive::RemoveUserAccount(Windows::Xbox::System::User^ InUser)
{
	if (InUser)
	{
		FUniqueNetIdLive UserXboxId(InUser->XboxUserId);
		if (OnlineUsers.Contains(UserXboxId))
		{
			return OnlineUsers.Remove(UserXboxId) > 0;
		}
		else
		{
			UE_LOG_ONLINE(Log, TEXT("User %s not found in OnlineUsers"), *UserXboxId.ToString());
		}
	}

	return false;
}

void FOnlineIdentityLive::RefreshGamepadsAndUsers()
{
	UE_LOG_ONLINE(Log, TEXT("RefreshGamepadsAndUsers"));
	// Lock CachedUsers while we access it
	const FScopeLock CachedUsersScopeLock(&CachedUsersLock);

	// Cache User::Users since they can take few ms (cross VM call)
	// @ATG_CHANGE : BEGIN - UWP LIVE support
	Platform::Collections::Vector<Windows::Xbox::System::User^> ^UsersCopy = ref new Platform::Collections::Vector<Windows::Xbox::System::User^>(Windows::Xbox::System::User::Users->Size);
	for (int i = 0; i < static_cast<int>(Windows::Xbox::System::User::Users->Size); ++i)
	{
		UsersCopy->SetAt(i, Windows::Xbox::System::User::Users->GetAt(i));
	}

	CachedUsers = UsersCopy->GetView();
	// @ATG_CHANGE : END - UWP LIVE support
	TArray<TSharedRef<const FUniqueNetIdLive> > UserReputationsToQuery;

	// cache the online user account info
	const int32 NumCachedUsers = static_cast<int32>(CachedUsers->Size);
	for (int32 Index = 0; Index < NumCachedUsers; ++Index)
	{
		Windows::Xbox::System::User^ User = CachedUsers->GetAt(Index);
		if (User != nullptr)
		{
			const bool bIsNewUser = AddUserAccount(User);
			if (bIsNewUser)
			{
				UserReputationsToQuery.Add(MakeShared<FUniqueNetIdLive>(User->XboxUserId));
			}
		}
	}

	for (LiveUserAccountMap::TIterator UserIter(OnlineUsers); UserIter; ++UserIter)
	{
		bool bFound = false;
		for (int32 Index = 0; Index < NumCachedUsers; ++Index)
		{
			Windows::Xbox::System::User^ User = CachedUsers->GetAt(Index);
			if (User != nullptr)
			{
				FUniqueNetIdLive UserXboxId(User->XboxUserId);
				if (UserXboxId == UserIter.Key())
				{
					bFound = true;
					break;
				}
			}
		}

		if (!bFound)
		{
			UserIter.RemoveCurrent();
		}
	}

	// Query new user's reputation state for clients to read
	if (UserReputationsToQuery.Num() > 0)
	{
		TSharedPtr<const FUniqueNetId> LocalSignedInUser = GetFirstSignedInUser(AsShared());
		if (LocalSignedInUser.IsValid())
		{
			Microsoft::Xbox::Services::XboxLiveContext^ LocalUserLiveContext = LiveSubsystem->GetLiveContext(*LocalSignedInUser);
			if (LocalUserLiveContext)
			{
				LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveGetOverallReputation>(
					LiveSubsystem,
					LocalUserLiveContext,
					MoveTemp(UserReputationsToQuery),
					FOnGetOverallReputationCompleteDelegate::CreateThreadSafeSP(this, &FOnlineIdentityLive::OnReputationQueryComplete));
			}
		}
	}
}

void FOnlineIdentityLive::OnReputationQueryComplete(const TMap<const FUniqueNetIdLive, bool>& UserIsBadMap)
{
	for (const TPair<const FUniqueNetIdLive, bool>& Pair : UserIsBadMap)
	{
		if (TSharedRef<FUserOnlineAccountLive>* LiveUserPtr = OnlineUsers.Find(Pair.Key))
		{
			(*LiveUserPtr)->SetBadReputation(Pair.Value);
			UE_LOG_ONLINE(Verbose, TEXT("Setting User %s's BadReputation to %s"), *Pair.Key.ToString(), Pair.Value ? TEXT("1") : TEXT("0"));
		}
		else
		{
			UE_LOG_ONLINE(Verbose, TEXT("User %s no longer present, but their BadReputation was %s"), *Pair.Key.ToString(), Pair.Value ? TEXT("1") : TEXT("0"));
		}
	}
}

// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
void FOnlineIdentityLive::SetUserXSTSToken(Windows::Xbox::System::User^ User, const FString& EndPointURL, const FString& AuthToken)
// @ATG_CHANGE : END
{
	check(User);

	FUniqueNetIdLive UserId(User->XboxUserId);

	const TSharedRef<FUserOnlineAccountLive>* const FoundUser = OnlineUsers.Find(UserId);
	if (FoundUser == nullptr)
	{
		TSharedRef<FUserOnlineAccountLive> OnlineUser(MakeShared<FUserOnlineAccountLive>(User));

		// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
		// Record the token keyed by the URL that it is associated with.  Can be retrieved via GetAuthAttribute
		OnlineUser->SetUserAttribute(EndPointURL, AuthToken);

		// If this is the primary login endpoint, also use the token as the 'AccessToken' (original behavior)
		if (EndPointURL == LoginXSTSEndpoint)
		{
			OnlineUser->SetAccessToken(AuthToken);
		}
		// @ATG_CHANGE : END

		OnlineUsers.Add(MoveTemp(UserId), OnlineUser);
	}
	else
	{
		// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
		// Record the token keyed by the URL that it is associated with.  Can be retrieved via GetAuthAttribute
		(*FoundUser)->SetUserAttribute(EndPointURL, AuthToken);

		// If this is the primary login endpoint, also use the token as the 'AccessToken' (original behavior)
		if (EndPointURL == LoginXSTSEndpoint)
		{
			(*FoundUser)->SetAccessToken(AuthToken);
		}
		// @ATG_CHANGE : END
	}
}

void FOnlineIdentityLive::HandleAppResume()
{
	UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::HandleAppResume"));
	RefreshGamepadsAndUsers();
}

void FOnlineIdentityLive::OnEngineInitComplete()
{
// @ATG_CHANGE : BEGIN - UWP LIVE support
#if PLATFORM_XBOXONE
	const auto InputInterface = GetInputInterface();
	if (ensure(InputInterface.IsValid()))
	{
		UserAdded = InputInterface->OnUserAddedDelegates.AddRaw(this, &FOnlineIdentityLive::OnUserAdded);
		UserRemoved = InputInterface->OnUserRemovedDelegates.AddRaw(this, &FOnlineIdentityLive::OnUserRemoved);
	}
#elif PLATFORM_UWP
	EventHandler<UserAddedEventArgs^>^ userAddedEvent = ref new EventHandler<UserAddedEventArgs^>(
		[this](Platform::Object^, UserAddedEventArgs^ Args)
	{
		OnUserAdded(Args->User);
	});

	// Listen to User Removed events
	EventHandler<UserRemovedEventArgs^>^ userRemovedEvent = ref new EventHandler<UserRemovedEventArgs^>(
		[this](Platform::Object^, UserRemovedEventArgs^ Args)
	{
		OnUserRemoved(Args->User);
	});
	TaskTokenUserAdded	= User::UserAdded	+= userAddedEvent;
	TaskTokenUserRemoved	= User::UserRemoved	+= userRemovedEvent;
#endif
// @ATG_CHANGE : END
}


void FOnlineIdentityLive::HookLiveEvents()
{
	AppInitComplete = FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FOnlineIdentityLive::OnEngineInitComplete);
	ControllerConnectionChanged = FCoreDelegates::OnControllerConnectionChange.AddRaw(this, &FOnlineIdentityLive::OnControllerConnectionChange);
	ControllerPairingChanged = FCoreDelegates::OnControllerPairingChange.AddRaw(this, &FOnlineIdentityLive::OnControllerPairingChange);
	
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FOnlineIdentityLive::HandleAppResume);
}

void FOnlineIdentityLive::UnhookLiveEvents()
{
#if PLATFORM_XBOXONE
	const TSharedPtr<FXboxOneInputInterface> InputInterface = GetInputInterface();
	if (InputInterface.IsValid())
	{
		InputInterface->OnUserAddedDelegates.Remove(UserAdded);
		InputInterface->OnUserRemovedDelegates.Remove(UserRemoved);
	}
#elif PLATFORM_UWP
	User::UserAdded  -= TaskTokenUserAdded;
	User::UserRemoved  -= TaskTokenUserRemoved;
#endif

	FCoreDelegates::OnFEngineLoopInitComplete.Remove(AppInitComplete);
	FCoreDelegates::OnControllerConnectionChange.Remove(ControllerConnectionChanged);
	FCoreDelegates::OnControllerPairingChange.Remove(ControllerPairingChanged);
}

void FOnlineIdentityLive::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	FUniqueNetIdLive LiveId(UserId);
	auto LiveUser = GetUserForUniqueNetId(LiveId);

	if (LiveUser == nullptr)
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::GetUserPrivilege couldn't find Live user for unique id %s."), *UserId.ToString());
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::UserNotFound);
		return;
	}

	// Docs warn to not call CheckPrivilegeAsync if the user isn't signed in.
	if (!LiveUser->IsSignedIn)
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::GetUserPrivilege Live user %s is not signed in."), LiveUser->DisplayInfo->GameDisplayName->Data());
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::UserNotLoggedIn);
		return;
	}

// @ATG_CHANGE : BEGIN - UWP LIVE support
#if PLATFORM_XBOXONE
	using namespace Windows::Xbox::ApplicationModel::Store;

	KnownPrivileges KnownPrivilege = KnownPrivileges::XPRIVILEGE_MULTIPLAYER_SESSIONS;

	switch (Privilege)
	{
		case EUserPrivileges::CanPlayOnline:
		{
			KnownPrivilege = KnownPrivileges::XPRIVILEGE_MULTIPLAYER_SESSIONS;
			break;
		}

		case EUserPrivileges::CanCommunicateOnline:
		{
			KnownPrivilege = KnownPrivileges::XPRIVILEGE_COMMUNICATIONS;
			break;
		}

		case EUserPrivileges::CanUseUserGeneratedContent:
		{
			KnownPrivilege = KnownPrivileges::XPRIVILEGE_USER_CREATED_CONTENT;
			break;
		}

		default:
		{
			// @todo: Add other privilege types
			Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
			return;
		}
	}

	const bool bShowUpsellUIIfPossible = true;
	try
	{
		IAsyncOperation<PrivilegeCheckResult>^ CheckOp = Product::CheckPrivilegeAsync(LiveUser, static_cast<uint32>(KnownPrivilege), bShowUpsellUIIfPossible, nullptr);
		Concurrency::create_task(CheckOp).then([this, LiveId, Privilege, Delegate, LiveUser](Concurrency::task<PrivilegeCheckResult> Task)
		{
			try
			{
				PrivilegeCheckResult Result = Task.get();

				LiveSubsystem->CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskLiveCheckForPackageUpdate>(LiveSubsystem, LiveUser,
					FOnCheckForPackageUpdateCompleteDelegate::CreateLambda([this, Delegate, LiveId, Privilege, Result](const FOnlineError& ErrorResult, const TOptional<ECheckForPackageUpdateResult> OptionalPatchCheckResult)
				{
					if (!ErrorResult.bSucceeded)
					{
						// Ensure our patch check succeeds in non-development environments.  In development, unpackaged builds won't have a package
						if (LiveSubsystem->GetOnlineEnvironment() != EOnlineEnvironment::Development)
						{
							Delegate.ExecuteIfBound(LiveId, Privilege, static_cast<uint32>(EPrivilegeResults::GenericFailure));
							return;
						}
					}

					if (OptionalPatchCheckResult.IsSet())
					{
						const ECheckForPackageUpdateResult PatchCheckResult = OptionalPatchCheckResult.GetValue();
						if (PatchCheckResult == ECheckForPackageUpdateResult::MandatoryUpdateAvailable)
						{
							Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::RequiredPatchAvailable);
							return;
						}
					}

					const uint32 ResultInt = static_cast<uint32>(Result);

					// Default to GenericFailure
					EPrivilegeResults PrivilegeResult = EPrivilegeResults::GenericFailure;
					if (Result == PrivilegeCheckResult::NoIssue)
					{
						PrivilegeResult = EPrivilegeResults::NoFailures;
					}
					else if (ResultInt & static_cast<uint32>(PrivilegeCheckResult::PurchaseRequired))
					{
						PrivilegeResult = EPrivilegeResults::AccountTypeFailure;
					}
					else if ((ResultInt & static_cast<uint32>(PrivilegeCheckResult::Restricted))
						|| (ResultInt & static_cast<uint32>(PrivilegeCheckResult::Banned)))
					{
						if (Privilege == EUserPrivileges::CanPlayOnline)
						{
							PrivilegeResult = EPrivilegeResults::OnlinePlayRestricted;
						}
						else if (Privilege == EUserPrivileges::CanCommunicateOnline)
						{
							PrivilegeResult = EPrivilegeResults::ChatRestriction;
						}
						else if (Privilege == EUserPrivileges::CanUseUserGeneratedContent)
						{
							PrivilegeResult = EPrivilegeResults::UGCRestriction;
						}
					}
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)PrivilegeResult);
				}));
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code 0x%0.8X."), Ex->HResult);

				LiveSubsystem->ExecuteNextTick([LiveId, Privilege, Delegate]()
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
				});
			}
			catch (const std::exception& Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityLive::GetUserPrivilege starting  failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
				LiveSubsystem->ExecuteNextTick([LiveId, Privilege, Delegate]()
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
				});
			}
		});
		return;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityLive::GetUserPrivilege starting failed with code 0x%0.8X."), Ex->HResult);
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineIdentityLive::GetUserPrivilege starting  failed with reason '%s'."), ANSI_TO_TCHAR(Ex.what()));
	}

#elif PLATFORM_UWP
	Microsoft::Xbox::Services::System::GamingPrivilege KnownPrivilege = Microsoft::Xbox::Services::System::GamingPrivilege::MultiplayerSessions;

	switch (Privilege)
	{
		case EUserPrivileges::CanPlayOnline:
		{
			KnownPrivilege = Microsoft::Xbox::Services::System::GamingPrivilege::MultiplayerSessions;
			break;
		}

		case EUserPrivileges::CanCommunicateOnline:
		{
			KnownPrivilege = Microsoft::Xbox::Services::System::GamingPrivilege::Communications;
			break;
		}

		case EUserPrivileges::CanUseUserGeneratedContent:
		{
			KnownPrivilege = Microsoft::Xbox::Services::System::GamingPrivilege::UserCreatedContent;
			break;
		}

		default:
		{
			// @todo: Add other privilege types
			Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
			return;
		}
	}

	try
	{
		auto CheckOp = Microsoft::Xbox::Services::System::TitleCallableUI::CheckGamingPrivilegeWithUI(KnownPrivilege, nullptr);
		create_task(CheckOp).then([this, LiveId, Privilege, Delegate](task<bool> Task)
		{
			try
			{
				auto Result = Task.get();

				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([Delegate, LiveId, Privilege, Result]()
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, Result ? (uint32)EPrivilegeResults::NoFailures : (uint32)EPrivilegeResults::GenericFailure);
				});
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([LiveId, Privilege, Delegate]()
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
				});
			}
		});
		return;
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

		LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([LiveId, Privilege, Delegate]()
		{
			Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
		});
	}
#else // not XboxOne or UWP
#error "Unsupported platform"
#endif // PLATFORM_*
// @ATG_CHANGE : END - UWP LIVE support

	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
}

FPlatformUserId FOnlineIdentityLive::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
// @ATG_CHANGE : BEGIN - UWP LIVE support
#if PLATFORM_XBOXONE
	const auto InputInterface = GetInputInterface();
	if(!InputInterface.IsValid())
	{
		return PLATFORMUSERID_NONE;
	}

	return InputInterface->GetPlatformUserIdFromXboxUserId(*UniqueNetId.ToString());
#else
	// Note: assume SUA for now - either this netid matches the only user, or it doesn't
	const FScopeLock CachedUsersScopeLock(&CachedUsersLock);
	FPlatformUserId UserId = -1;
	if (CachedUsers->Size > 0)
	{
		check(CachedUsers->Size == 1);
		if (UniqueNetId.ToString() == CachedUsers->GetAt(0)->XboxUserId->Data())
		{
			UserId = 0;
		}
	}

	return UserId;
#endif
// @ATG_CHANGE : END
}

FString FOnlineIdentityLive::GetAuthType() const
{
	return FString(TEXT("xbl"));
}

void FOnlineIdentityLive::OnUserAdded(Windows::Xbox::System::User^ InUserAdded)
{
	AddUserAccount(InUserAdded);

	ELoginStatus::Type LoginStatus = GetLoginStatusForUser(InUserAdded);
	const FPlatformUserId PlatformUserId = GetPlatformUserIdFromXboxUser(InUserAdded);
	TSharedRef<const FUniqueNetIdLive> UserAdded = MakeShared<const FUniqueNetIdLive>(InUserAdded ? InUserAdded->XboxUserId : nullptr);

	UE_LOG_ONLINE(Log, TEXT("UserAdded PlatformUserId %d"), PlatformUserId);

	/* HACK -- Assume the previous state could not be UsingLocalProfile */
	TriggerOnLoginStatusChangedDelegates(PlatformUserId, (LoginStatus != ELoginStatus::LoggedIn) ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn,
		LoginStatus, *UserAdded);
}

void FOnlineIdentityLive::OnUserRemoved(Windows::Xbox::System::User^ InUserRemoved)
{
	RemoveUserAccount(InUserRemoved);

	const FPlatformUserId PlatformUserId = GetPlatformUserIdFromXboxUser(InUserRemoved);
	TSharedRef<const FUniqueNetIdLive> UserRemoved = MakeShared<const FUniqueNetIdLive>(InUserRemoved ? InUserRemoved->XboxUserId : nullptr);

	UE_LOG_ONLINE(Log, TEXT("UserRemoved PlatformUserId %d"), PlatformUserId);

	/* HACK -- Assume the previous state could not be UsingLocalProfile */
	TriggerOnLoginStatusChangedDelegates(PlatformUserId, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserRemoved);
}

void FOnlineIdentityLive::OnControllerConnectionChange(bool Connected, int32 UserId, int32 ControllerId)
{
	// only act if this is a disconnect event (Connected == false)
	if (!Connected)
	{
		RefreshGamepadsAndUsers();
	}
}

void FOnlineIdentityLive::OnControllerPairingChange(int32 InControllerIndex, FPlatformUserId InNewUserId, FPlatformUserId InOldUserId)
{
	RefreshGamepadsAndUsers();

	User^ NewUser = GetUserForPlatformUserId(InNewUserId);
	User^ OldUser = GetUserForPlatformUserId(InOldUserId);

	TSharedRef<const FUniqueNetIdLive> PreviousUserId = MakeShared<const FUniqueNetIdLive>(OldUser ? OldUser->XboxUserId : nullptr);
	TSharedRef<const FUniqueNetIdLive> NewUserId = MakeShared<const FUniqueNetIdLive>(NewUser ? NewUser->XboxUserId : nullptr);

	UE_LOG_ONLINE(Log, TEXT("Triggering OnControllerPairingChanged with ControllerIndex %d, PreviousUser '%s', NewUser '%s'"),
		InControllerIndex, *PreviousUserId->ToString(), *NewUserId->ToString());

	if (InControllerIndex != -1)
	{
		TriggerOnControllerPairingChangedDelegates(InControllerIndex, *PreviousUserId, *NewUserId);
	}
}

/** FUserOnlineAccountLive */

FString FUserOnlineAccountLive::GetAccessToken() const
{
	return UserXSTSToken;
}

void FUserOnlineAccountLive::SetAccessToken(const FString& AuthToken)
{
	UserXSTSToken = AuthToken;
}

void FUserOnlineAccountLive::SetBadReputation(const bool bIsBadReputation)
{
	UserAttributes.Add(BAD_REPUTATION_ATTRIBUTE, bIsBadReputation ? TEXT("1") : TEXT("0"));
}

TOptional<bool> FUserOnlineAccountLive::GetIsBadReputation() const
{
	TOptional<bool> ReturnValue;

	if (const FString* BadReputationStringPtr = UserAttributes.Find(BAD_REPUTATION_ATTRIBUTE))
	{
		ReturnValue = TOptional<bool>(BadReputationStringPtr->ToBool());
	}

	return ReturnValue;
}

bool FUserOnlineAccountLive::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return GetUserAttribute(AttrName, OutAttrValue);
}

bool FUserOnlineAccountLive::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	// Check if value is already set to this value
	const FString* const ExistingAttrPtr = UserAttributes.Find(AttrName);
	if (ExistingAttrPtr != nullptr)
	{
		if (ExistingAttrPtr->Equals(AttrValue))
		{
			return false;
		}
	}

	// Add or Set value
	UserAttributes.Add(AttrName, AttrValue);
	return true;
}

TSharedRef<const FUniqueNetId> FUserOnlineAccountLive::GetUserId() const
{
	return UserId;
}

FString FUserOnlineAccountLive::GetRealName() const
{
	return FOnlineUserInfoLive::FilterPlayerName(UserData->DisplayInfo->GameDisplayName);
}

FString FUserOnlineAccountLive::GetDisplayName(const FString& Platform /*= FString()*/) const
{
	return FOnlineUserInfoLive::FilterPlayerName(UserData->DisplayInfo->GameDisplayName);
}

bool FUserOnlineAccountLive::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* const ExistingAttrPtr = UserAttributes.Find(AttrName);
	if (ExistingAttrPtr != nullptr)
	{
		OutAttrValue = *ExistingAttrPtr;
		return true;
	}

	OutAttrValue.Empty();
	return false;
}

// Debugging commands to get or set the first logged in user's reputation.
#if !UE_BUILD_SHIPPING

static void SetReputationFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (HttpResponse.IsValid())
	{
		UE_LOG_ONLINE(Log, TEXT("SetReputation HTTP request complete. bSucceeded = %d. Response code = %d"),
			bSucceeded ? 1 : 0, HttpResponse->GetResponseCode());
	}
	else
	{
		UE_LOG_ONLINE(Log, TEXT("Xbox One SetReputation HTTP request complete. Response is null."));
	}
}

static void DebugSetReputation(const FString& ReputationJson)
{
	const IOnlineSubsystem* const Subsystem = IOnlineSubsystem::Get(LIVE_SUBSYSTEM);

	if (Subsystem == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugSetReputation: couldn't get an online subsystem"));
		return;
	}

	const IOnlineIdentityPtr IdentityInterface = Subsystem->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugSetReputation: couldn't get the identity interface"));
		return;
	}
	
	const TSharedPtr<const FUniqueNetId> UserId = GetFirstSignedInUser(IdentityInterface);
	if(!UserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugSetReputation: invalid UserId"));
		return;
	}
	
	FString UserHash;
	TSharedPtr<FOnlineIdentityLive, ESPMode::ThreadSafe> IdLivePtr = StaticCastSharedPtr<FOnlineIdentityLive>(IdentityInterface);
	if (IdLivePtr.IsValid())
	{
		User^ LiveUser = IdLivePtr->GetUserForUniqueNetId(*StaticCastSharedPtr<const FUniqueNetIdLive>(UserId));
		if (LiveUser)
		{
			UserHash = LiveUser->XboxUserHash->Data();
		}
	}

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	HttpRequest->OnProcessRequestComplete().BindStatic(&SetReputationFinished);
	HttpRequest->SetURL(FString::Printf(TEXT("https://reputation.xboxlive.com/users/me/resetreputation")));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("x-xbl-contract-version"), TEXT("101"));
	HttpRequest->SetHeader(TEXT("xbl-authz-actor-10"), UserHash);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetContentAsString(ReputationJson);

	HttpRequest->ProcessRequest();
}

static void DebugSetBadReputation()
{
	DebugSetReputation(TEXT("{ \"fairplayReputation\": 1, \"commsReputation\": 1, \"userContentReputation\": 1 }"));
}

static void DebugSetGoodReputation()
{
	DebugSetReputation(TEXT("{ \"fairplayReputation\": 75, \"commsReputation\": 75, \"userContentReputation\": 75 }"));
}

static FAutoConsoleCommand ConsoleCommandLiveSetBadReputation(
	TEXT("online.LiveSetBadReputation"),
	TEXT("Set a bad reputation for the first logged in user."),
	FConsoleCommandDelegate::CreateStatic(DebugSetBadReputation)
);

static FAutoConsoleCommand ConsoleCommandLiveSetGoodReputation(
	TEXT("online.LiveSetGoodReputation"),
	TEXT("Set a good reputation for the first logged in user."),
	FConsoleCommandDelegate::CreateStatic(DebugSetGoodReputation)
);

static void DebugLogReputation()
{
	FOnlineSubsystemLive* const LiveSubsystem = static_cast<FOnlineSubsystemLive*>(IOnlineSubsystem::Get(LIVE_SUBSYSTEM));

	if (LiveSubsystem == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugLogReputation: couldn't get an online subsystem"));
		return;
	}

	const IOnlineIdentityPtr IdentityInterface = LiveSubsystem->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugLogReputation: couldn't get the identity interface"));
		return;
	}
	
	const TSharedPtr<const FUniqueNetId> UserId = GetFirstSignedInUser(IdentityInterface);
	if(!UserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("DebugLogReputation: invalid UserId"));
		return;
	}

	TSharedRef<const FUniqueNetIdLive> UserIdLiveRef = StaticCastSharedRef<const FUniqueNetIdLive>(UserId.ToSharedRef());

	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(*UserId);
	TArray<TSharedRef<const FUniqueNetIdLive>> UserIdArray;
	UserIdArray.Add(UserIdLiveRef);
	
	LiveSubsystem->CreateAndDispatchAsyncTaskSerial<FOnlineAsyncTaskLiveGetOverallReputation>(LiveSubsystem, LiveContext, MoveTemp(UserIdArray),
		FOnGetOverallReputationCompleteDelegate::CreateLambda([IdentityInterface](const UserReputationMap& UsersWithBadReputation)
		{
			for (const UserReputationMap::ElementType& UserPair : UsersWithBadReputation)
			{
				UE_LOG_ONLINE(Log, TEXT("DebugLogReputation: user %s, OverallReputationIsBad: %s"), 
					*IdentityInterface->GetPlayerNickname(UserPair.Key),
					UserPair.Value ? TEXT("true") : TEXT("false"));
			}
		}));
}

static FAutoConsoleCommand ConsoleCommandLiveDebugLogReputation(
	TEXT("online.LiveDebugLogReputation"),
	TEXT("Prints the reputation of the first logged in user to the log."),
	FConsoleCommandDelegate::CreateStatic(DebugLogReputation)
);

#endif
