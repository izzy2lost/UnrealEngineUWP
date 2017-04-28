// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineExternalUIInterfaceLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "Online.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::UI;

const int32 PEOPLE_PICKER_MAX_SIZE = 100;
#define INVITE_UI_TEXT TEXT("Invite players")

bool FOnlineExternalUILive::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate)
{
	// Get the controller object corresponding to the desired controller Id.
	if(!FSlateApplication::IsInitialized())
	{
		return false;
	}

	// @ATG_CHANGE : BEGIN UWP LIVE support
	auto PlatformApp = FSlateApplication::Get().GetPlatformApplication();
	if(!PlatformApp.IsValid())
	{
		return false;
	}

	auto PlatformInput = static_cast<FPlatformInputInterface*>(PlatformApp->GetInputInterface());
	auto RequestedGamepad = PlatformInput->GetGamepadForUser(ControllerIndex);
	// @ATG_CHANGE : END

	AccountPickerOptions LoginOption = bAllowGuestLogin ? AccountPickerOptions::AllowGuests : AccountPickerOptions::None;

	auto asyncOp = SystemUI::ShowAccountPickerAsync(RequestedGamepad, LoginOption);
	asyncOp->Completed = ref new AsyncOperationCompletedHandler<AccountPickerResult^>(
		[=,this](IAsyncOperation<AccountPickerResult^>^ operation, AsyncStatus status)
	{
		// @ATG_CHANGE : BEGIN - avoid missing callback when async op fails
		Windows::Xbox::System::IUser^ PickedUser = nullptr;
		if(status == AsyncStatus::Completed)
		{
			auto Results = operation->GetResults();
			if (Results)
			{
				PickedUser = Results->User;
			}
		}
		else
		{
			// There was an error during the async operation.
			UE_LOG_ONLINE(Log, TEXT("Error in SystemUI::ShowAccountPickerAsync: 0x%x"), operation->ErrorCode.Value);
		}

		if(LiveSubsystem && LiveSubsystem->GetAsyncTaskManager())
		{
			FAsyncEventAccountPickerClosed* NewEvent = new FAsyncEventAccountPickerClosed(LiveSubsystem, PickedUser, ControllerIndex, Delegate);
			LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
		// @ATG_CHANGE : END
	});

	return true;
}

bool FOnlineExternalUILive::ShowFriendsUI(int32 LocalUserNum)
{
	return false;
}

bool FOnlineExternalUILive::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	const auto Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	const auto LiveUser = Identity->GetUserForControllerIndex(LocalUserNum);

	if(!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowInviteUI: Couldn't find Live user for LocalUserNum %d."), LocalUserNum);
		return false;
	}

	auto LiveContext = LiveSubsystem->GetLiveContext(LiveUser);

	Platform::Collections::Vector<Platform::String^>^ UserVector = ref new Platform::Collections::Vector<Platform::String^>;
	UserVector->Append(LiveUser->XboxUserId);

	FOnlineSessionLivePtr LiveSession = StaticCastSharedPtr<FOnlineSessionLive>(LiveSubsystem->GetSessionInterface());
	FNamedOnlineSession* Session = LiveSession->GetNamedSession(SessionName);
	if(Session == nullptr)
	{
		UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: Named session not found for %s session name. Can't send invite."), *SessionName.ToString());
		return false;
	}

	TSharedPtr<FOnlineSessionInfoLive> LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
	if (!LiveInfo->IsValid())
	{
		UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: FOnlineSessionInfoLive not valid for %s. Can't send invite."), *SessionName.ToString());
		return false;
	}

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionLiveMultiplayerSessionRef = LiveInfo->GetLiveMultiplayerSessionRef();
	if (SessionLiveMultiplayerSessionRef == nullptr)
	{
		UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: LiveMultiplayerSessionRef not valid for %s. Can't send invite."), *SessionName.ToString());
		return false;
	}

	// Need to find the user's current session to include in the invite call.
	concurrency::create_task(LiveContext->MultiplayerService->GetActivitiesForUsersAsync(
		LiveContext->AppConfig->ServiceConfigurationId,
		UserVector->GetView()
		)).then([this, LiveUser, SessionLiveMultiplayerSessionRef](concurrency::task<IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^> ^> Task)
	{
		try
		{
			IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^ ActivityDetails = Task.get();
			if(ActivityDetails->Size == 0)
			{
				UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: User is not in a session. Can't send invite."));
				throw ref new Platform::InvalidArgumentException();	// let the handler below deal with this
			}

			for(Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^ CurActivityDetail : ActivityDetails)
			{
				Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ CurrentSessionRef = CurActivityDetail->SessionReference;	

				if (CurrentSessionRef != nullptr && SessionLiveMultiplayerSessionRef->SessionName->Equals(CurrentSessionRef->SessionName))
				{
					concurrency::create_task(Windows::Xbox::UI::SystemUI::ShowSendGameInvitesAsync(
						LiveUser,
						ref new Windows::Xbox::Multiplayer::MultiplayerSessionReference(CurrentSessionRef->SessionName, CurrentSessionRef->ServiceConfigurationId, CurrentSessionRef->SessionTemplateName), 
						nullptr)
						).then([this, LiveUser](concurrency::task<void> Task)
					{
						try
						{
							Task.get();

							LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this]()
							{
								TriggerOnExternalUIChangeDelegates(true);
							});
						}
						catch(Platform::Exception^ ex)
						{
							UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: Failed to show invite UI with 0x%0.8X"), ex->HResult);
							LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this]()
							{
								TriggerOnExternalUIChangeDelegates(false);
							});
						}
					});
					break;
				}
			}
		}
		catch(Platform::Exception^ ex)
		{
			UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: Failed to get current session with 0x%0.8X"), ex->HResult);

			LiveSubsystem->GetAsyncTaskManager()->AddGenericToOutQueue([this]()
			{
				TriggerOnExternalUIChangeDelegates(false);
			});
		}
	});

	return true;
}

bool FOnlineExternalUILive::ShowAchievementsUI(int32 LocalUserNum)
{
	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	Windows::Xbox::System::User^ LiveUser = Identity->GetUserForControllerIndex(LocalUserNum);

	if (!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowAchievementsUI: Couldn't find Live user for LocalUserNum %d."), LocalUserNum);
		return false;
	}

	// @ATG_CHANGE : BEGIN - UWP LIVE Support
	uint32 TitleId = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance->TitleId;
	// @ATG_CHANGE : END - UWP LIVE Support

	auto LaunchAchievementsTask = SystemUI::LaunchAchievementsAsync(LiveUser, TitleId);

	concurrency::create_task(LaunchAchievementsTask).then([this, LiveUser](concurrency::task<void> task)
	{
		UE_LOG_ONLINE(Log, TEXT("ShowAchievementsUI: Achievement task UI displaying."));
	});

	return true;
}

bool FOnlineExternalUILive::ShowLeaderboardUI( const FString& LeaderboardName )
{
	return false;
}

bool FOnlineExternalUILive::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	WebUrlBeingOpened = Url;
	WebUrlClosedDelegate = Delegate;

	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FOnlineExternalUILive::HandleApplicationHasReactivated_WebUrl);

	auto LaunchUriTask = Windows::System::Launcher::LaunchUriAsync(ref new Uri(ref new Platform::String(*Url)));

	concurrency::create_task(LaunchUriTask).then([this, Url, Delegate](concurrency::task<bool> task)
	{
		bool Result = false;
		try
		{
			Result = task.get();
		}
		catch (Platform::Exception^ ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("ShowWebURL: URL launch result error: 0x%0.8x"), ex->HResult);
		}
		UE_LOG_ONLINE(Log, TEXT("ShowWebURL: URL launch completed, success: %d"), Result);
	});

	return true;
}

void FOnlineExternalUILive::HandleApplicationHasReactivated_WebUrl()
{
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);

	FAsyncEventWebUrlUIClosed* NewEvent = new FAsyncEventWebUrlUIClosed(LiveSubsystem, WebUrlClosedDelegate, WebUrlBeingOpened);
	LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
}

FOnlineExternalUILive::FOnlineExternalUILive(FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
	, bAllowGuestLogin(true)
	, ShouldCallUIDelegate(false)
{
	GConfig->GetBool(TEXT("OnlineSubsystemLive"), TEXT("bAllowGuestLogin"), bAllowGuestLogin, GEngineIni);
}

bool FOnlineExternalUILive::CloseWebURL()
{
	return false;
}

bool FOnlineExternalUILive::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

bool FOnlineExternalUILive::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUILive::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUILive::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	if(!LiveSubsystem || !LiveSubsystem->GetIdentityLive().IsValid())
	{
		return false;
	}
	
	Windows::Xbox::System::IUser^ RequestingUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(FUniqueNetIdLive(Requestor));

	// The string version of an FUniqueNetIdLive is the actual XUID, so we can just use ToString for the requestee here.
	auto AsyncOp = SystemUI::ShowProfileCardAsync(RequestingUser, ref new Platform::String(*Requestee.ToString()));

	concurrency::create_task(AsyncOp).then([=,this](concurrency::task<void> Task)
	{
		if(LiveSubsystem && LiveSubsystem->GetAsyncTaskManager())
		{
			FAsyncEventProfileCardClosed* NewEvent = new FAsyncEventProfileCardClosed(LiveSubsystem, Delegate);
			LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
	});

	return true;
}

FString FOnlineExternalUILive::FAsyncEventAccountPickerClosed::ToString() const
{
	return TEXT("Account picker closed.");
}

void FOnlineExternalUILive::FAsyncEventAccountPickerClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();

	if(SignedInUser)
	{
		TSharedPtr<const FUniqueNetId> UniqueId(MakeShareable(new FUniqueNetIdLive(SignedInUser->XboxUserId->Data())));
		Delegate.ExecuteIfBound(UniqueId, ControllerIndex);
	}
	else
	{
		Delegate.ExecuteIfBound(TSharedPtr<const FUniqueNetId>(), ControllerIndex);
	}
}

FString FOnlineExternalUILive::FAsyncEventProfileCardClosed::ToString() const 
{
	return TEXT("Profile card closed.");
}

void FOnlineExternalUILive::FAsyncEventProfileCardClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound();
}

/**
 * Console commands for testing and debugging
 */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void	TestProfileCard( const TArray<FString>& Args, UWorld* InWorld )
{
	if(!InWorld || Args.Num() < 2)
	{
		return;
	}

	TSharedPtr<const FUniqueNetId> Requestor;
	TSharedPtr<const FUniqueNetId> Requestee;
	for( auto Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();

		if( PlayerController )
		{
			auto Identity = Online::GetIdentityInterface();
			ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
			if( LocalPlayer && Identity.IsValid() )
			{
				const int32 ControllerId = LocalPlayer->GetControllerId();
				if(ControllerId == FCString::Atoi(*Args[0]))
				{
					Requestor = Identity->GetUniquePlayerId(ControllerId);
				}

				if(ControllerId == FCString::Atoi(*Args[1]))
				{
					Requestee = Identity->GetUniquePlayerId(ControllerId);
				}
			}
		}
	}

	auto ExternalUI = Online::GetExternalUIInterface();
	if(ExternalUI.IsValid() && Requestor.IsValid() && Requestee.IsValid())
	{
		ExternalUI->ShowProfileUI(*Requestor, *Requestee);
	}
}

FAutoConsoleCommandWithWorldAndArgs TestProfileCardCommand(
	TEXT("net.TestExternalProfileUI"), 
	TEXT( "Calls IOnlineExternalUI::ShowProfileUI. First parameter is the index of the requestor, second parameter is the index of the requestee." ), 
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(TestProfileCard)
	);
#endif
FString FOnlineExternalUILive::FAsyncEventWebUrlUIClosed::ToString() const
{
	return TEXT("WebURL closed");
}

void FOnlineExternalUILive::FAsyncEventWebUrlUIClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(WebUrl);
}
