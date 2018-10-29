// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineExternalUIInterfaceLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "OnlineSubsystemLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch
#include "Online.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "GenericPlatform/GenericPlatformHttp.h"

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::UI;
// @ATG_CHANGE : BEGIN - UWP LIVE Support - store compat wrapper needed
#if PLATFORM_XBOXONE
using namespace Windows::Xbox::ApplicationModel;

using Windows::Xbox::ApplicationModel::Store::ProductItemTypes;
#endif
// @ATG_CHANGE : END - UWP LIVE Support

const int32 PEOPLE_PICKER_MAX_SIZE = 100;
#define INVITE_UI_TEXT TEXT("Invite players")

bool FOnlineExternalUILive::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	// Get the controller object corresponding to the desired controller Id.
	const auto InputInterface = GetInputInterface();
	if (!InputInterface.IsValid())
	{
		LiveSubsystem->ExecuteNextTick([ControllerIndex, Delegate]()
		{
			Delegate.ExecuteIfBound(MakeShared<FUniqueNetIdLive>(), ControllerIndex, FOnlineError(false));
		});
		return false;
	}

	// @ATG_CHANGE : BEGIN - UWP LIVE Support 
	auto RequestedGamepad = InputInterface->GetGamepadForControllerId(ControllerIndex);
	// @ATG_CHANGE : END

	AccountPickerOptions LoginOption = bAllowGuestLogin ? AccountPickerOptions::AllowGuests : AccountPickerOptions::None;

	auto asyncOp = SystemUI::ShowAccountPickerAsync(RequestedGamepad, LoginOption);
	asyncOp->Completed = ref new AsyncOperationCompletedHandler<AccountPickerResult^>(
		[=,this](IAsyncOperation<AccountPickerResult^>^ operation, AsyncStatus status)
	{
		Windows::Xbox::System::IUser^ User = nullptr;
		if (status == AsyncStatus::Completed)
		{
			auto Results = operation->GetResults();
			if (Results)
			{
				User = Results->User;
			}
		}
		else if (status == AsyncStatus::Canceled)
		{
			UE_LOG_ONLINE(Log, TEXT("SystemUI::ShowAccountPickerAsync: Canceled"));
		}
		else
		{
			// There was an error during the async operation.
			UE_LOG_ONLINE(Log, TEXT("Error in SystemUI::ShowAccountPickerAsync: 0x%0.8X"), operation->ErrorCode.Value);
		}

		LiveSubsystem->ExecuteNextTick([this, User, Delegate]()
		{
			// We want to wait 1 extra tick so that the input system has definitely be called before we tell the game to check
			// the user's state.  Without the ExecuteNextTick, this can happen before the input system registers the user/controllers
			LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventAccountPickerClosed>(LiveSubsystem, User, Delegate);
		});
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

	const auto LiveUser = Identity->GetUserForPlatformUserId(LocalUserNum);

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

	FOnlineSessionInfoLivePtr LiveInfo = StaticCastSharedPtr<FOnlineSessionInfoLive>(Session->SessionInfo);
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
		// @ATG_CHANGE : BEGIN - UWP LIVE Support
		LiveContext->AppConfig->ServiceConfigurationId,
		// @ATG_CHANGE : END - UWP LIVE Support
		UserVector->GetView()
		)).then([this, LiveUser, SessionLiveMultiplayerSessionRef](concurrency::task<IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^> Task)
	{
		try
		{
			IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^>^ ActivityDetails = Task.get();
			if(ActivityDetails->Size == 0)
			{
				UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: User is not in a session. Can't send invite."));
				throw ref new Platform::InvalidArgumentException();	// let the handler below deal with this
			}

			for (Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails^ CurActivityDetail : ActivityDetails)
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

							LiveSubsystem->ExecuteNextTick([this]()
							{
								TriggerOnExternalUIChangeDelegates(true);
							});
						}
						catch (Platform::Exception^ Ex)
						{
							UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: Failed to show invite UI with 0x%0.8X"), Ex->HResult);
							LiveSubsystem->ExecuteNextTick([this]()
							{
								TriggerOnExternalUIChangeDelegates(false);
							});
						}
					});
					break;
				}
			}
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogOnline, Warning, TEXT("ShowInviteUI: Failed to get current session with 0x%0.8X"), Ex->HResult);

			LiveSubsystem->ExecuteNextTick([this]()
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

	Windows::Xbox::System::User^ LiveUser = Identity->GetUserForPlatformUserId(LocalUserNum);

	if (!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowAchievementsUI: Couldn't find Live user for LocalUserNum %d."), LocalUserNum);
		return false;
	}

	auto LaunchAchievementsTask = SystemUI::LaunchAchievementsAsync(LiveUser, LiveSubsystem->TitleId);

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
	if (ShowWebUrlRequest.WaitForProtocolActivationTimeRemaining > 0.0f)
	{
		// If we had one outstanding, consider it cancelled
		UE_LOG_ONLINE(Log, TEXT("FOnlineExternalUILive::ShowWebURL: Abandoning wait for previous ShowWebUrl protocol activation"));
		FinishShowWebUrl(MoveTemp(ShowWebUrlRequest.WebUrlBeingOpened));
	}
	ShowWebUrlRequest.WebUrlBeingOpened = Url;
	ShowWebUrlRequest.WebUrlClosedDelegate = Delegate;

	FCoreDelegates::ApplicationHasReactivatedDelegate.AddThreadSafeSP(this, &FOnlineExternalUILive::HandleApplicationHasReactivated_WebUrl);
#ifdef XBOXONE_HASONACTIVATEDBYPROTOCOL
	FXboxOneApplication* XboxOneApp = FXboxOneApplication::GetXboxOneApplication();
	XboxOneApp->OnActivatedByProtocol().AddThreadSafeSP(this, &FOnlineExternalUILive::HandleApplicationReactivatedByProtocol_WebUrl);
#endif

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
	if (!ShowWebUrlRequest.WebUrlBeingOpened.IsEmpty())
	{
		// Wait for a protocol activation before triggering the complete delegate
		// Not guaranteed to come, will only come if the web browser reactivates the application with a specific uri
		ShowWebUrlRequest.WaitForProtocolActivationTimeRemaining = 3.0f;
	}
}

void FOnlineExternalUILive::HandleApplicationReactivatedByProtocol_WebUrl(FString ActivationUri)
{
	// Does this match the format we expect?
	const FString ActivationUriDomain = FGenericPlatformHttp::GetUrlDomain(ActivationUri);
	if (ActivationUriDomain.Equals(TEXT("showWebUrlComplete"), ESearchCase::IgnoreCase))
	{
#ifdef XBOXONE_HASONACTIVATEDBYPROTOCOL
		FXboxOneApplication* XboxOneApp = FXboxOneApplication::GetXboxOneApplication();
		XboxOneApp->OnActivatedByProtocol().RemoveAll(this);
#endif
		// We have handled this protocol activation, clear out the global one
		// @ATG_CHANGE : BEGIN - UWP LIVE support
		FPlatformMisc::SetProtocolActivationUri(FString());
		// @ATG_CHANGE : END
		if (!ShowWebUrlRequest.WebUrlBeingOpened.IsEmpty())
		{
			// Parse out the final url
			int32 UrlStart = ActivationUri.Find(TEXT("url="));
			if (UrlStart != INDEX_NONE)
			{
				const int32 UrlParamLen = 4;
				FinishShowWebUrl(ActivationUri.Mid(UrlStart + UrlParamLen));
			}
			else
			{
				FinishShowWebUrl(MoveTemp(ShowWebUrlRequest.WebUrlBeingOpened));
			}
		}
	}
}

FOnlineExternalUILive::FOnlineExternalUILive(FOnlineSubsystemLive* InSubsystem)
	: LiveSubsystem(InSubsystem)
	, bAllowGuestLogin(true)
	, bShouldCallUIDelegate(false)
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
// @ATG_CHANGE : BEGIN - UWP LIVE Support - wrapper needed
#if PLATFORM_XBOXONE
	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	Windows::Xbox::System::User^ LiveUser = Identity->GetUserForPlatformUserId(LocalUserNum);
	if (!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowStoreUI: Couldn't find Live user for LocalUserNum %d."), LocalUserNum);
		LiveSubsystem->ExecuteNextTick([this, Delegate]()
		{
			Delegate.ExecuteIfBound(false);
		});
		return false;
	}

	StoreUIClosedDelegate = Delegate;
	bShouldCallUIDelegate = true;

	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw(this, &FOnlineExternalUILive::HandleApplicationHasReactivated_Store);
	ProductPurchasedToken = Store::Product::ProductPurchased += ref new Store::ProductPurchasedEventHandler([this](Store::ProductPurchasedEventArgs^ Args)
	{
		LiveSubsystem->ExecuteNextTick([this, Args]()
		{
			ProductPurchased_Store(Args);
		});
	});

	try
	{
		IAsyncAction^ ShowStoreTask = nullptr;
		if (!ShowParams.ProductId.IsEmpty())
		{
			ShowStoreTask = Store::Product::ShowDetailsAsync(LiveUser,
				ref new Platform::String(*ShowParams.ProductId));
		}
		else
		{
			const FString& TitleProductId = LiveSubsystem->GetTitleProductId();
			if (TitleProductId.IsEmpty())
			{
				UE_LOG_ONLINE(Warning, TEXT("ShowStoreUI: No product id set for title!"));
				LiveSubsystem->ExecuteNextTick([this, Delegate]()
				{
					Delegate.ExecuteIfBound(false);
				});
				return false;
			}

			ShowStoreTask = Store::Product::ShowMarketplaceAsync(LiveUser, 
				ProductItemTypes::Game,
				ref new Platform::String(*TitleProductId),
				ProductItemTypes::Consumable | ProductItemTypes::Durable | ProductItemTypes::Game | ProductItemTypes::App | ProductItemTypes::GameDemo);
		}

		concurrency::create_task(ShowStoreTask).then([this](concurrency::task<void> Task)
		{
			try
			{
				Task.get();
				UE_LOG_ONLINE(Log, TEXT("ShowStoreUI: Marketplace UI now displaying."));
			}
			catch (const std::exception& Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUI::ShowStoreUI call failed with error %s"), ANSI_TO_TCHAR(Ex.what()));
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUI::ShowStoreUI call failed with 0x%0.8X %ls"), Ex->HResult, Ex->Message->Data());
			}
		});

		return true;
	}
	catch (const std::exception& Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUI::ShowStoreUI call failed with error %s"), ANSI_TO_TCHAR(Ex.what()));
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineExternalUI::ShowStoreUI call failed with 0x%0.8X %ls"), Ex->HResult, Ex->Message->Data());
	}

	HandleApplicationHasReactivated_Store();
#endif
// @ATG_CHANGE : END - UWP LIVE Support
	return false;
}

// @ATG_CHANGE : BEGIN - UWP LIVE Support
#if PLATFORM_XBOXONE
void FOnlineExternalUILive::ProductPurchased_Store(Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventArgs^ Args)
{
	check(IsInGameThread());
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	Store::Product::ProductPurchased -= ProductPurchasedToken;

	// if we received a purchase callback that means we must have bought something
	if (bShouldCallUIDelegate)
	{
		LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventStoreUIClosed>(LiveSubsystem, StoreUIClosedDelegate, true);
		bShouldCallUIDelegate = false;
	}
}

void FOnlineExternalUILive::HandleApplicationHasReactivated_Store()
{
	check(IsInGameThread());
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
	Store::Product::ProductPurchased -= ProductPurchasedToken;

	// assume that if the product purchase event hasn't fired yet then we didn't actually buy anything
	if (bShouldCallUIDelegate)
	{
		LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventStoreUIClosed>(LiveSubsystem, StoreUIClosedDelegate, false);
		bShouldCallUIDelegate = false;
	}
}
#endif
// @ATG_CHANGE : END - UWP LIVE Support

bool FOnlineExternalUILive::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
// @ATG_CHANGE : BEGIN - UWP LIVE Support
#if PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE Support
	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());

	Windows::Xbox::System::User^ LiveUser = Identity->GetUserForPlatformUserId(LocalUserNum);
	if (!LiveUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowStoreUI: Couldn't find Live user for LocalUserNum %d."), LocalUserNum);
		return false;
	}

	try
	{
		IAsyncAction^ SendMessageTask = SystemUI::ShowComposeMessageAsync(LiveUser, ref new Platform::String(*ShowParams.DisplayMessage.ToString()), nullptr);
		concurrency::create_task(SendMessageTask).then([this, Delegate](concurrency::task<void> task)
		{
			try
			{
				task.get();
				UE_LOG_ONLINE(Log, TEXT("ShowSendMessageUI: Task complete!"));
				LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventSendMessageUIClosed>(LiveSubsystem, Delegate, true);
			}
			catch (concurrency::task_canceled&)
			{
				UE_LOG_ONLINE(Warning, TEXT("ShowSendMessageUI: Task Cancelled!"));
				LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventSendMessageUIClosed>(LiveSubsystem, Delegate, false);
			}
			catch (Platform::Exception^)
			{
				UE_LOG_ONLINE(Warning, TEXT("ShowSendMessageUI: Task Failed!"));
				LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventSendMessageUIClosed>(LiveSubsystem, Delegate, false);
			}
		});
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowSendMessageUI: Failed to start task!"));
		LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventSendMessageUIClosed>(LiveSubsystem, Delegate, false);
		return false;
	}

	return true;
// @ATG_CHANGE : BEGIN - UWP LIVE Support
#else
	return false;
#endif
// @ATG_CHANGE : END
}

bool FOnlineExternalUILive::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	if(!LiveSubsystem || !LiveSubsystem->GetIdentityLive().IsValid())
	{
		return false;
	}

	Windows::Xbox::System::IUser^ RequestingUser = LiveSubsystem->GetIdentityLive()->GetUserForUniqueNetId(FUniqueNetIdLive(Requestor));
	if (!ensure(RequestingUser != nullptr))
	{
		UE_LOG_ONLINE(Warning, TEXT("Passed in invalider requester to"));
		return false;
	}

	// The string version of an FUniqueNetIdLive is the actual XUID, so we can just use ToString for the requestee here.
	auto AsyncOp = SystemUI::ShowProfileCardAsync(RequestingUser, ref new Platform::String(*Requestee.ToString()));
	concurrency::create_task(AsyncOp).then([=,this](concurrency::task<void> Task)
	{
		if (LiveSubsystem)
		{
			LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventProfileCardClosed>(LiveSubsystem, Delegate);
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
	Platform::String^ UserId = nullptr;
	FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;

	FOnlineAsyncEvent::TriggerDelegates();

	if (SignedInUser)
	{
		const auto InputInterface = GetInputInterface();
		if (InputInterface.IsValid())
		{
			// @todo Less than ideal, but SignedInUser is an IUser^ and need a User^ for other functions
#if PLATFORM_XBOXONE
			PlatformUserId = InputInterface->GetPlatformUserIdFromXboxUserId(SignedInUser->XboxUserId->Data());
#elif PLATFORM_UWP
			PlatformUserId = SignedInUser->Id;
#endif
		}

		UserId = SignedInUser->XboxUserId;
	}
	
	TSharedRef<const FUniqueNetId> UniqueId = MakeShared<FUniqueNetIdLive>(UserId);
	Delegate.ExecuteIfBound(UniqueId, PlatformUserId, FOnlineError(true));
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

FString FOnlineExternalUILive::FAsyncEventStoreUIClosed::ToString() const
{
	return TEXT("Store UI Closed");
}

void FOnlineExternalUILive::FAsyncEventStoreUIClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(bPurchasedProduct);
}

FString FOnlineExternalUILive::FAsyncEventSendMessageUIClosed::ToString() const
{
	return TEXT("SendMessage UI Closed");
}

void FOnlineExternalUILive::FAsyncEventSendMessageUIClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(bMessageSent);
}

FString FOnlineExternalUILive::FAsyncEventWebUrlUIClosed::ToString() const
{
	return TEXT("WebURL closed");
}

void FOnlineExternalUILive::FAsyncEventWebUrlUIClosed::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound(WebUrl);
}

void FOnlineExternalUILive::Tick(float DeltaTime)
{
	if (ShowWebUrlRequest.WaitForProtocolActivationTimeRemaining > 0.0f)
	{
		ShowWebUrlRequest.WaitForProtocolActivationTimeRemaining -= DeltaTime;
		if (ShowWebUrlRequest.WaitForProtocolActivationTimeRemaining <= 0.0f)
		{
			FinishShowWebUrl(MoveTemp(ShowWebUrlRequest.WebUrlBeingOpened));
		}
	}
}

void FOnlineExternalUILive::FinishShowWebUrl(FString&& FinalUrl)
{
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
#ifdef XBOXONE_HASONACTIVATEDBYPROTOCOL
	FXboxOneApplication* XboxOneApp = FXboxOneApplication::GetXboxOneApplication();
	XboxOneApp->OnActivatedByProtocol().RemoveAll(this);
#endif

	FAsyncEventWebUrlUIClosed* NewEvent = new FAsyncEventWebUrlUIClosed(LiveSubsystem, MoveTemp(ShowWebUrlRequest.WebUrlClosedDelegate), MoveTemp(FinalUrl));
	ShowWebUrlRequest.Reset();
	LiveSubsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
}
