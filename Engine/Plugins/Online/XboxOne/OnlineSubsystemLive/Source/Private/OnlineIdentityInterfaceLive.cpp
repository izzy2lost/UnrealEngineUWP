// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManagerLive.h"
// @ATG_CHANGE : BEGIN UWP LIVE support
#include "SlateBasics.h" // For SlateApplication, SlateApplication.h is not self-sufficient :(
// @ATG_CHANGE : END

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
	FPlatformInputInterface* GetInputInterface()
	{
		if(!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}

		auto PlatformApp = FSlateApplication::Get().GetPlatformApplication();
		if(!PlatformApp.IsValid())
		{
			return nullptr;
		}

		return static_cast<FPlatformInputInterface*>(PlatformApp->GetInputInterface());
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
	HookLiveEvents();
	RefreshGamepadsAndUsers();
}

FOnlineIdentityLive::~FOnlineIdentityLive()
{
	UnhookLiveEvents();
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityLive::GetUserAccount(const FUniqueNetId& UserId) const
{
	//@todo - not implemented
	return NULL;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityLive::GetAllUserAccounts() const 
{
	//@todo - not implemented
	return TArray<TSharedPtr<FUserOnlineAccount> >();
}

bool FOnlineIdentityLive::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	TSharedPtr<const FUniqueNetId> UserId = FOnlineIdentityLive::GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, TEXT(""));
		return true;
	}
	else
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdLive(FString()), 
			FString::Printf(TEXT("No user logged in for controller=%d"), LocalUserNum));
		return false;
	}
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
		return MakeShareable(new FUniqueNetIdLive(User->XboxUserId->Data()));
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
			return MakeShareable(new FUniqueNetIdLive(User->Sponsor->XboxUserId->Data()));
		}
	}

	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::CreateUniquePlayerId(uint8* Bytes, int32 Size) 
{
	if (Bytes != NULL && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShareable(new FUniqueNetIdLive(StrId));
	}
	return NULL;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityLive::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdLive(Str));
}

FString FOnlineIdentityLive::GetPlayerNickname(int32 ControllerIndex) const
{
	FString PlayerNickname;
	User^ RequestedUser = GetUserForControllerIndex(ControllerIndex);
	if( RequestedUser )
	{
		PlayerNickname = FString(RequestedUser->DisplayInfo->GameDisplayName->Data());
	}

	return PlayerNickname;
}

FString FOnlineIdentityLive::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	FString PlayerNickname;
	User^ RequestedUser = GetUserForUniqueNetId(FUniqueNetIdLive(UserId));
	if( RequestedUser )
	{
		PlayerNickname = FString(RequestedUser->DisplayInfo->GameDisplayName->Data());
	}

	return PlayerNickname;
}

FString FOnlineIdentityLive::GetAuthToken(int32 ControllerIndex) const
{
	FString AuthToken;
	User^ RequestedUser = GetUserForControllerIndex(ControllerIndex);
	if( RequestedUser )
	{
		AuthToken = FString(RequestedUser->XboxUserHash->Data());
	}

	return AuthToken;
}

User^ FOnlineIdentityLive::GetUserForControllerIndex(int32 ControllerIndex) const
{
	const auto InputInterface = GetInputInterface();
	// @ATG_CHANGE : BEGIN UWP LIVE support
	if(InputInterface == nullptr)
	// @ATG_CHANGE : END
	{
		return nullptr;
	}

	auto RequestedGamepad = InputInterface->GetGamepadForUser(ControllerIndex);

	// If there is a user associated with the controller, use it.
	if(RequestedGamepad && RequestedGamepad->User)
	{
		return SystemUserFromControllerUser(RequestedGamepad->User);
	}

	{
		// Seems like there might be a bug in LIVE that Controller binding might not properly be set; i.e. the Gamepad->User might be nullptr.
		// Forum link: https://forums.xboxlive.com/AnswerPage.aspx?qid=9e7f9bb1-74a3-4a03-a449-cbc6a8cb22de&tgt=1
		// Because of this, go through each user's Controller array to try to find the user.
		for(int CachedUserIndex = 0; CachedUserIndex < int(CachedUsers->Size); ++CachedUserIndex)
		{
			User^ CurrentUser = CachedUsers->GetAt(CachedUserIndex);
			for(int UserControllerIndex = 0; UserControllerIndex < int(CurrentUser->Controllers->Size); ++UserControllerIndex)
			{
				if(CurrentUser->Controllers->GetAt(UserControllerIndex) == RequestedGamepad)
				{
					return CurrentUser;
				}
			}
		}
	}
	return nullptr;
}

int32 FOnlineIdentityLive::GetControllerIndexForUser(Windows::Xbox::System::User^ InUser) const
{
	if (!InUser)
	{
		return -1;
	}

	const auto InputInterface = GetInputInterface();
	// @ATG_CHANGE : BEGIN UWP LIVE support
	if (InputInterface == nullptr)
		// @ATG_CHANGE : END
	{
		return -1;
	}

	// Go through the user's controllers until we find one that the input interface has bound, or until we hit the end of the list.
	int UserId = -1;
	for (int i = 0; (UserId == -1) && (i < int(InUser->Controllers->Size)); ++i)
	{
		auto CurrentController = InUser->Controllers->GetAt(i);
		UserId = InputInterface->GetUserIdForController(CurrentController);
	}

	return UserId;
}

int32 FOnlineIdentityLive::GetControllerIndexForId( const FUniqueNetId& PlayerId ) const
{
	return GetControllerIndexForUser(GetUserForUniqueNetId(FUniqueNetIdLive(PlayerId)));
}

void FOnlineIdentityLive::RefreshGamepadsAndUsers()
{
	// Cache User::Users since they can take few ms (cross VM call)
	Platform::Collections::Vector<Windows::Xbox::System::User^> ^UsersCopy = ref new Platform::Collections::Vector<Windows::Xbox::System::User^>(Windows::Xbox::System::User::Users->Size);
	for (int i = 0; i < static_cast<int>(Windows::Xbox::System::User::Users->Size); ++i)
	{
		UsersCopy->SetAt(i, Windows::Xbox::System::User::Users->GetAt(i));
	}

	CachedUsers = UsersCopy->GetView();		
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

#if !PLATFORM_UWP
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
#endif

	TaskTokenUserAdded					= User::UserAdded	+= userAddedEvent;
	TaskTokenUserRemoved				= User::UserRemoved += userRemovedEvent;
#if !PLATFORM_UWP
	TaskTokenControllerPairingChanged	= Controller::ControllerPairingChanged += controllerPairingEvent;
#endif

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FOnlineIdentityLive::HandleAppResume);
}

void FOnlineIdentityLive::HandleAppResume()
{
	UE_LOG( LogOnlineSubsystemLive, Log, TEXT( "FOnlineIdentityLive::HandleAppResume" ) );
	RefreshGamepadsAndUsers();
}

void FOnlineIdentityLive::UnhookLiveEvents()
{
#if !PLATFORM_UWP
	Controller::ControllerPairingChanged	-= TaskTokenControllerPairingChanged;
#endif
	User::UserAdded							-= TaskTokenUserAdded;
	User::UserRemoved						-= TaskTokenUserRemoved;
}

Windows::Xbox::System::User^ FOnlineIdentityLive::GetUserForUniqueNetId( const FUniqueNetIdLive& UniqueId ) const
{
	if(!CachedUsers)
	{
		return nullptr;
	}

	for(int i = 0; i < static_cast<int>(CachedUsers->Size); ++i)
	{
		User^ CurrentUser = CachedUsers->GetAt(i);
		if(CurrentUser)
		{
			if(UniqueId.ToString() == CurrentUser->XboxUserId->Data())
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
		UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineIdentityLive::GetUserPrivilege couldn't find Live user for unique id %s."), *UserId.ToString());
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
		return;
	}

	// Docs warn to not call CheckPrivilegeAsync if the user isn't signed in.
	if (!LiveUser->IsSignedIn)
	{
		UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineIdentityLive::GetUserPrivilege Live user %s is not signed in."), LiveUser->DisplayInfo->GameDisplayName->Data());
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
		return;
	}

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

	auto CheckOp = Product::CheckPrivilegeAsync(LiveUser, static_cast<uint32>(KnownPrivilege), true, nullptr);

	create_task(CheckOp).then([this, LiveId, Privilege, Delegate](task<PrivilegeCheckResult> Task)
	{
		try
		{
			auto Result = Task.get();

			LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([Delegate, LiveId, Privilege, Result]()
			{
				Delegate.ExecuteIfBound(LiveId, Privilege, Result == PrivilegeCheckResult::NoIssue ? (uint32)EPrivilegeResults::NoFailures : (uint32)EPrivilegeResults::GenericFailure);
			});
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

			LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([LiveId, Privilege, Delegate]()
			{
				Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
			});
		}
	});
#else
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
				UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

				LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([LiveId, Privilege, Delegate]()
				{
					Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
				});
			}
		});
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FOnlineIdentityLive::GetUserPrivilege failed with code %d."), Ex->HResult);

		LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([LiveId, Privilege, Delegate]()
		{
			Delegate.ExecuteIfBound(LiveId, Privilege, (uint32)EPrivilegeResults::GenericFailure);
		});
	}
#endif
}

FPlatformUserId FOnlineIdentityLive::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId)
{
	const auto InputInterface = GetInputInterface();
	// @ATG_CHANGE : BEGIN UWP LIVE support
	if (InputInterface == nullptr)
	// @ATG_CHANGE : END
	{
		return PLATFORMUSERID_NONE;
	}

	return InputInterface->GetPlatformUserIdFromXboxUserId(*UniqueNetId.ToString());
}

FString FOnlineIdentityLive::GetAuthType() const
{
	return TEXT("");
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

	UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FAsyncEventUserAdded::TriggerDelegates ControllerIndex %d"), ControllerIndex);

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

	UE_LOG(LogOnlineSubsystemLive, Log, TEXT("FAsyncEventUserRemoved::TriggerDelegates ControllerIndex %d"), ControllerIndex);

	/* HACK -- Assume the previous state could not be UsingLocalProfile */
	Subsystem->GetIdentityLive()->TriggerOnLoginStatusChangedDelegates(0, ELoginStatus::LoggedIn,
		ELoginStatus::NotLoggedIn, FUniqueNetIdLive(Args->User->XboxUserId));
}

#if !PLATFORM_UWP
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
	// @ATG_CHANGE : BEGIN UWP LIVE support
	if (InputInterface == nullptr)
	// @ATG_CHANGE : END
	{
		return;
	}

	{
		const int UserId = InputInterface->GetUserIdForController(Args->Controller);
		FUniqueNetIdLive PreviousUserId(Args->PreviousUser ? Args->PreviousUser->XboxUserId : nullptr);
		FUniqueNetIdLive NewUserId(Args->User ? Args->User->XboxUserId : nullptr);

		UE_LOG(LogOnlineSubsystemLive, Log, TEXT("Triggering OnControllerPairingChanged with UserId %d, PreviousUser '%s', NewUser '%s'"),
			UserId, *PreviousUserId.ToString(), *NewUserId.ToString());

		if(UserId != -1)
		{
			Subsystem->GetIdentityLive()->TriggerOnControllerPairingChangedDelegates(UserId, PreviousUserId, NewUserId);
		}
	}
}
#endif
