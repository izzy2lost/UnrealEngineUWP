// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManagerLive.h"
// @ATG_CHANGE : UWP Live Support - platform headers moved to pch
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "AsyncTasks/OnlineAsyncTaskLiveGetXSTSToken.h"
#include "Misc/ConfigCacheIni.h"
// @ATG_CHANGE : BEGIN - Needs UWP implementation.  XDK-based implementation should come from standard OSSLive
#if PLATFORM_XBOXONE
#include "AsyncTasks/OnlineAsyncTaskLiveCheckForPackageUpdate.h"
#endif
// @ATG_CHANGE : END
#include "Misc/ScopeLock.h"

using namespace Microsoft;
using namespace Windows::Foundation;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Input;

using namespace concurrency;

namespace
{
	// @ATG_CHANGE : BEGIN UWP LIVE support
	/**
	 * Helper to get a pointer to the platform InputInterface. It could be null if Slate isn't initialized, so check it!
	 *
	 * @return A pointer to the input interface, if it could be found. Null otherwise.
	 */
	TSharedPtr<FPlatformInputInterface> GetInputInterface()
	{
		if(!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}

#if PLATFORM_XBOXONE
		FXboxOneApplication* PlatformApp = FXboxOneApplication::GetXboxOneApplication();
#elif PLATFORM_UWP
		FUWPApplication* PlatformApp = FUWPApplication::GetUWPApplication();
#endif
		if(PlatformApp == nullptr)
		{
			return nullptr;
		}
#if PLATFORM_XBOXONE
		return PlatformApp->GetXboxInputInterface();
#elif PLATFORM_UWP
		return PlatformApp->GetUWPInputInterface();
#endif
	}
	// @ATG_CHANGE : END

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
	RefreshGamepadsAndUsers();
}

FOnlineIdentityLive::~FOnlineIdentityLive()
{
	UnhookLiveEvents();
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityLive::GetUserAccount(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdLive& LiveUserId = static_cast<const FUniqueNetIdLive&>(UserId);

	const TSharedPtr<FUserOnlineAccount>* FoundUserPtr = OnlineUsers.Find(LiveUserId);
	if (FoundUserPtr != nullptr)
	{
		return *FoundUserPtr;
	}

	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityLive::GetAllUserAccounts() const 
{
	TArray<TSharedPtr<FUserOnlineAccount> > UserAccounts;
	OnlineUsers.GenerateValueArray(UserAccounts);

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

	FOnlineAsyncTaskManagerLive* MyTaskManager = LiveSubsystem->GetAsyncTaskManager();
	if (MyTaskManager == nullptr)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, FString(TEXT("Could not queue Login Token fetch task")));
		return false;
	}

	const auto OnLoginCompleteDelegate = FOnXSTSTokenCompleteDelegate::CreateLambda(
	[this](FOnlineError Result, int32 LocalUserNum, const FUniqueNetId& UserId, const FString& ResultSignature, const FString& ResultToken)
	{
		// At this point, if we were successful, our auth token is saved on this user for using in listening delegates
		TriggerOnLoginCompleteDelegates(LocalUserNum, Result.bSucceeded, UserId, Result.ErrorMessage.ToString());
	});

	// @ATG_CHANGE : BEGIN - Support passing endpoint requiring XSTS token via AccountCredentials
	FOnlineAsyncTaskLiveGetXSTSToken* const GetXSTSTokenTask = new FOnlineAsyncTaskLiveGetXSTSToken(LiveSubsystem, XboxUser, LocalUserNum, TargetEndpoint, MoveTemp(OnLoginCompleteDelegate));
	// @ATG_CHANGE : END
	MyTaskManager->AddToParallelTasks(GetXSTSTokenTask);

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
	User^ RequestedUser = GetUserForControllerIndex(ControllerIndex);
	return GetLoginStatusForUser(RequestedUser);
}

ELoginStatus::Type FOnlineIdentityLive::GetLoginStatus( const FUniqueNetId& UserId ) const 
{
	User^ RequestedUser = GetUserForUniqueNetId(FUniqueNetIdLive(UserId));
	return GetLoginStatusForUser(RequestedUser);
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::GetUniquePlayerId(int32 ControllerIndex) const
{
	User^ User = GetUserForControllerIndex(ControllerIndex);
	if( User != nullptr )
	{
		return MakeShared<FUniqueNetIdLive>(User->XboxUserId->Data());
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::GetSponsorUniquePlayerId(int32 ControllerIndex) const
{
	User^ User = GetUserForControllerIndex(ControllerIndex);
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

	User^ RequestedUser = GetUserForControllerIndex(ControllerIndex);
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

User^ FOnlineIdentityLive::GetUserForControllerIndex(int32 ControllerIndex) const
{
	const auto InputInterface = GetInputInterface();
	if (!InputInterface.IsValid())
	{
		return nullptr;
	}

	// @ATG_CHANGE : BEGIN - UWP LIVE support
	auto RequestedGamepad = InputInterface->GetGamepadForUser(ControllerIndex);

	// If there is a user associated with the controller, use it.
	if (RequestedGamepad && RequestedGamepad->User)
	{
		return SystemUserFromControllerUser(RequestedGamepad->User);
	}
	// @ATG_CHANGE : END

	{
		// Lock CachedUsers while we access it
		const FScopeLock CachedUsersScopeLock(&CachedUsersLock);

		// Seems like there might be a bug in LIVE that Controller binding might not properly be set; i.e. the Gamepad->User might be nullptr.
		// Forum link: https://forums.xboxlive.com/AnswerPage.aspx?qid=9e7f9bb1-74a3-4a03-a449-cbc6a8cb22de&tgt=1
		// Because of this, go through each user's Controller array to try to find the user.
		const int32 CachedUsersSize = static_cast<int32>(CachedUsers->Size);
		for(int32 CachedUserIndex = 0; CachedUserIndex < CachedUsersSize; ++CachedUserIndex)
		{
			User^ CurrentUser = CachedUsers->GetAt(CachedUserIndex);
			if (CurrentUser != nullptr)
			{
				const int32 CurrentUserControllerSize = static_cast<int32>(CurrentUser->Controllers->Size);
				for (int32 UserControllerIndex = 0; UserControllerIndex < CurrentUserControllerSize; ++UserControllerIndex)
				{
					if (CurrentUser->Controllers->GetAt(UserControllerIndex) == RequestedGamepad)
					{
						return CurrentUser;
					}
				}
			}
		}
	}

	return nullptr;
}

int32 FOnlineIdentityLive::GetControllerIndexForUser( Windows::Xbox::System::User^ InUser ) const
{
	if (!InUser)
	{
		return -1;
	}

	const auto InputInterface = GetInputInterface();
	if (!InputInterface.IsValid())
	{
		return -1;
	}

	// Go through the user's controllers until we find one that the input interface has bound, or until we hit the end of the list.
	int32 UserId = -1;
	for (int32 i = 0; (UserId == -1) && (i < static_cast<int32>(InUser->Controllers->Size)); ++i)
	{
		// @ATG_CHANGE : BEGIN - UWP LIVE support
		auto CurrentController = InUser->Controllers->GetAt(i);
		// @ATG_CHANGE : END - UWP LIVE support
		UserId = InputInterface->GetUserIdForController(CurrentController);
	}

	return UserId;
}

int32 FOnlineIdentityLive::GetControllerIndexForId( const FUniqueNetId& PlayerId ) const
{
	return GetControllerIndexForUser(GetUserForUniqueNetId(FUniqueNetIdLive(PlayerId)));
}

Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ FOnlineIdentityLive::GetCachedUsers() const
{
	return CachedUsers;
}

void FOnlineIdentityLive::RefreshGamepadsAndUsers()
{
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

	// cache the online user account info
	const int32 VectorSize = static_cast<int32>(CachedUsers->Size);
	for (int32 Index = 0; Index < VectorSize; ++Index)
	{
		Windows::Xbox::System::User^ User = CachedUsers->GetAt(Index);
		if (User != nullptr)
		{
			FUniqueNetIdLive UserXboxId(User->XboxUserId);
			if (!OnlineUsers.Contains(UserXboxId))
			{
				TSharedPtr<FUserOnlineAccountLive> OnlineUser(new FUserOnlineAccountLive(User));
				OnlineUsers.Add(MoveTemp(UserXboxId), MoveTemp(OnlineUser));
			}
		}
	}
}

// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
void FOnlineIdentityLive::SetUserXSTSToken(Windows::Xbox::System::User^ User, const FString& EndPointURL, const FString& AuthToken)
// @ATG_CHANGE : END
{
	check(User);

	FUniqueNetIdLive UserId(User->XboxUserId);

	const TSharedPtr<FUserOnlineAccount>* const FoundUser = OnlineUsers.Find(UserId);
	if (FoundUser == nullptr || !FoundUser->IsValid())
	{
		TSharedPtr<FUserOnlineAccountLive> OnlineUser(new FUserOnlineAccountLive(User));

		// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
		// Record the token keyed by the URL that it is associated with.  Can be retrieved via GetAuthAttribute
		OnlineUser->SetUserAttribute(EndPointURL, AuthToken);

		// If this is the primary login endpoint, also use the token as the 'AccessToken' (original behavior)
		if (EndPointURL == LoginXSTSEndpoint)
		{
			OnlineUser->SetAccessToken(AuthToken);
		}
		// @ATG_CHANGE : END

		OnlineUsers.Add(MoveTemp(UserId), MoveTemp(OnlineUser));
	}
	else
	{
		TSharedPtr<FUserOnlineAccountLive> OnlineUserLive = StaticCastSharedPtr<FUserOnlineAccountLive>(*FoundUser);

		// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
		// Record the token keyed by the URL that it is associated with.  Can be retrieved via GetAuthAttribute
		OnlineUserLive->SetUserAttribute(EndPointURL, AuthToken);

		// If this is the primary login endpoint, also use the token as the 'AccessToken' (original behavior)
		if (EndPointURL == LoginXSTSEndpoint)
		{
			OnlineUserLive->SetAccessToken(AuthToken);
		}
		// @ATG_CHANGE : END
	}
}

void FOnlineIdentityLive::HookLiveEvents()
{
	// Listen to User Added events
	EventHandler<UserAddedEventArgs^>^ userAddedEvent = ref new EventHandler<UserAddedEventArgs^>(
		[this] (Platform::Object^, UserAddedEventArgs^ Args)
	{
		// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
		if(LiveSubsystem->GetAsyncTaskManager())
		{
			auto NewEvent = new FAsyncEventUserAdded(LiveSubsystem, Args);
			LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
	});

	// Listen to User Removed events
	EventHandler<UserRemovedEventArgs^>^ userRemovedEvent = ref new EventHandler<UserRemovedEventArgs^>(
		[this] (Platform::Object^, UserRemovedEventArgs^ Args)
	{
		// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
		if(LiveSubsystem->GetAsyncTaskManager())
		{
			auto NewEvent = new FAsyncEventUserRemoved(LiveSubsystem, Args);
			LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
	});

	// Listen to Controller Pairing events
	EventHandler<ControllerPairingChangedEventArgs^>^ controllerPairingEvent = ref new EventHandler<ControllerPairingChangedEventArgs^>(
		[this] (Platform::Object^, ControllerPairingChangedEventArgs^ Args)
	{
		// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
		if(LiveSubsystem->GetAsyncTaskManager())
		{
			auto NewEvent = new FAsyncEventControllerPairingChanged(LiveSubsystem, Args);
			LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
	});

	TaskTokenUserAdded					= User::UserAdded	+= userAddedEvent;
	TaskTokenUserRemoved				= User::UserRemoved += userRemovedEvent;
	TaskTokenControllerPairingChanged	= Controller::ControllerPairingChanged += controllerPairingEvent;

	ControllerConnectionChanged = FCoreDelegates::OnControllerConnectionChange.AddRaw(this, &FOnlineIdentityLive::OnControllerConnectionChange);

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FOnlineIdentityLive::HandleAppResume);
}

void FOnlineIdentityLive::HandleAppResume()
{
	UE_LOG_ONLINE(Log, TEXT( "FOnlineIdentityLive::HandleAppResume" ) );
	RefreshGamepadsAndUsers();
}

void FOnlineIdentityLive::UnhookLiveEvents()
{
	Controller::ControllerPairingChanged	-= TaskTokenControllerPairingChanged;
	User::UserAdded							-= TaskTokenUserAdded;
	User::UserRemoved						-= TaskTokenUserRemoved;

	FCoreDelegates::OnControllerConnectionChange.Remove(ControllerConnectionChanged);
}

Windows::Xbox::System::User^ FOnlineIdentityLive::GetUserForUniqueNetId(const FUniqueNetIdLive& UniqueId) const
{
	// Lock CachedUsers while we access it
	const FScopeLock CachedUsersScopeLock(&CachedUsersLock);

	if (!CachedUsers)
	{
		return nullptr;
	}

	const int32 CachedUserSize = CachedUsers->Size;
	for (int32 i = 0; i < CachedUserSize; ++i)
	{
		User^ CurrentUser = CachedUsers->GetAt(i);
		if (CurrentUser)
		{
			if (UniqueId.UniqueNetIdStr == CurrentUser->XboxUserId->Data())
			{
				return CurrentUser;
			}
		}
	}

	return nullptr;
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

	auto CheckOp = Product::CheckPrivilegeAsync(LiveUser, static_cast<uint32>(KnownPrivilege), false, nullptr);

	create_task(CheckOp).then([this, LiveId, Privilege, Delegate, LiveUser](task<PrivilegeCheckResult> Task)
	{
		try
		{
			auto Result = Task.get();

			LiveSubsystem->CreateAndDispatchAsyncEvent<FOnlineAsyncTaskLiveCheckForPackageUpdate>(LiveSubsystem, LiveUser,
				FOnCheckForPackageUpdateCompleteDelegate::CreateLambda([this, Delegate, LiveId, Privilege, Result](bool IsUpdateAvailable, bool IsUpdateRequired)
			{
				if (IsUpdateAvailable && IsUpdateRequired)
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::RequiredPatchAvailable);
				}
				else
				{
					// Default to GenericFailure
					EPrivilegeResults PrivilegeResult = EPrivilegeResults::GenericFailure;
					if (Result == PrivilegeCheckResult::NoIssue)
					{
						PrivilegeResult = EPrivilegeResults::NoFailures;
					}
					else if (Result == PrivilegeCheckResult::Restricted)
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
				}
			}));
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG_ONLINE(Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

			LiveSubsystem->ExecuteNextTick([LiveId, Privilege, Delegate]()
			{
				Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
			});
		}
	});
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
}

FPlatformUserId FOnlineIdentityLive::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId)
{
	const auto InputInterface = GetInputInterface();
	if(!InputInterface.IsValid())
	{
		return PLATFORMUSERID_NONE;
	}

	return InputInterface->GetPlatformUserIdFromXboxUserId(*UniqueNetId.ToString());
}

FString FOnlineIdentityLive::GetAuthType() const
{
	return TEXT("xbl");
}

FOnlineIdentityLive::FAsyncEventUserAdded::FAsyncEventUserAdded( FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::UserAddedEventArgs^ InArgs ) :
	FOnlineAsyncEvent(InLiveSubsystem),
	Args(InArgs)
{

}

void FOnlineIdentityLive::FAsyncEventUserAdded::Finalize()
{
	Subsystem->GetIdentityLive()->RefreshGamepadsAndUsers();
}

FString FOnlineIdentityLive::FAsyncEventUserAdded::ToString() const 
{
	return FString("Xbox user added");
}

void FOnlineIdentityLive::FAsyncEventUserAdded::TriggerDelegates()
{
	const int ControllerIndex = Subsystem->GetIdentityLive()->GetControllerIndexForUser(Args->User);

	UE_LOG_ONLINE(Log, TEXT("FAsyncEventUserAdded::TriggerDelegates ControllerIndex %d"), ControllerIndex);

	ELoginStatus::Type LoginStatus = GetLoginStatusForUser(Args->User);

	/* HACK -- Assume the previous state could not be UsingLocalProfile */
	Subsystem->GetIdentityLive()->TriggerOnLoginStatusChangedDelegates(0, LoginStatus != ELoginStatus::LoggedIn ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn,
		LoginStatus, FUniqueNetIdLive(Args->User->XboxUserId));
}

FOnlineIdentityLive::FAsyncEventUserRemoved::FAsyncEventUserRemoved( FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::UserRemovedEventArgs^ InArgs ) :
	FOnlineAsyncEvent(InLiveSubsystem),
	Args(InArgs)
{

}

void FOnlineIdentityLive::FAsyncEventUserRemoved::Finalize()
{
	Subsystem->GetIdentityLive()->RefreshGamepadsAndUsers();
}

FString FOnlineIdentityLive::FAsyncEventUserRemoved::ToString() const 
{
	return FString("Xbox user removed");
}

void FOnlineIdentityLive::FAsyncEventUserRemoved::TriggerDelegates()
{
	const int ControllerIndex = Subsystem->GetIdentityLive()->GetControllerIndexForUser(Args->User);

	UE_LOG_ONLINE(Log, TEXT("FAsyncEventUserRemoved::TriggerDelegates ControllerIndex %d"), ControllerIndex);

	/* HACK -- Assume the previous state could not be UsingLocalProfile */
	Subsystem->GetIdentityLive()->TriggerOnLoginStatusChangedDelegates(0, ELoginStatus::LoggedIn,
		ELoginStatus::NotLoggedIn, FUniqueNetIdLive(Args->User->XboxUserId));
}

FOnlineIdentityLive::FAsyncEventControllerPairingChanged::FAsyncEventControllerPairingChanged( FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::Input::ControllerPairingChangedEventArgs^ InArgs ) :
	FOnlineAsyncEvent(InLiveSubsystem),
	Args(InArgs)
{

}

void FOnlineIdentityLive::FAsyncEventControllerPairingChanged::Finalize()
{
	Subsystem->GetIdentityLive()->RefreshGamepadsAndUsers();
}

FString FOnlineIdentityLive::FAsyncEventControllerPairingChanged::ToString() const 
{
	return FString("Xbox controller pairing changed");
}

void FOnlineIdentityLive::FAsyncEventControllerPairingChanged::TriggerDelegates()
{
	const auto InputInterface = GetInputInterface();
	if(!InputInterface.IsValid())
	{
		return;
	}

	{
		const int UserId = InputInterface->GetUserIdForController(Args->Controller);
		FUniqueNetIdLive PreviousUserId(Args->PreviousUser ? Args->PreviousUser->XboxUserId : nullptr);
		FUniqueNetIdLive NewUserId(Args->User ? Args->User->XboxUserId : nullptr);

		UE_LOG_ONLINE(Log, TEXT("Triggering OnControllerPairingChanged with UserId %d, PreviousUser '%s', NewUser '%s'"),
			UserId, *PreviousUserId.ToString(), *NewUserId.ToString());

		if(UserId != -1)
		{
			Subsystem->GetIdentityLive()->TriggerOnControllerPairingChangedDelegates(UserId, PreviousUserId, NewUserId);
		}
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

void FOnlineIdentityLive::OnControllerConnectionChange(bool Connected, int32 UserId, int32 ControllerId)
{
	// only act if this is a disconnect event (Connected == false)
	if (!Connected)
	{
		RefreshGamepadsAndUsers();
	}
}
