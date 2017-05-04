// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemLiveModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemLive.h"
#include "ModuleManager.h"
#include "PixelFormat.h"

#define INVALID_INDEX -1

/** URL Prefix when using Live socket connection */
#define LIVE_URL_PREFIX TEXT("Live.")

/** pre-pended to all Live logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("LIVE: ")

/** global SCID used for non-title-specific queries (e.g. user reputation) */
#define LIVE_GLOBAL_SCID TEXT("7492baca-c1b4-440d-a391-b7ef364a8d40")

// @ATG_CHANGE :  BEGIN UWP LIVE support
#include "OnlineError.h"
#if PLATFORM_XBOXONE
#include "XboxOneAllowPlatformTypes.h"
#define _UITHREADCTXT_SUPPORT   0
#include <ppltasks.h>
#include <ws2tcpip.h>
#include <collection.h>
#include "XboxOneHidePlatformTypes.h"

#include "Runtime/Core/Private/XboxOne/XboxOneInputInterface.h"
typedef FXboxOneInputInterface FPlatformInputInterface;

inline Windows::Xbox::System::User ^SystemUserFromXSAPIUser(Windows::Xbox::System::User ^user)
{
	return user;
}

inline Windows::Xbox::System::User ^XSAPIUserFromSystemUser(Windows::Xbox::System::User ^user)
{
	return user;
}

inline Windows::Xbox::System::User ^SystemUserFromControllerUser(Windows::Xbox::System::User ^user)
{
	return user;
}

#elif PLATFORM_UWP
#include "AllowWindowsPlatformTypes.h"
#define _UITHREADCTXT_SUPPORT   0
#include <ppltasks.h>
#include <ws2tcpip.h>
#include <collection.h>
#include "HideWindowsPlatformTypes.h"
#include "Runtime/Core/Private/UWP/UWPInputInterface.h"

// @ATG_CHANGE : sspiller@microsoft.com - BEGIN disable warning caused by build reference mismatch
#pragma warning(disable: 4691)
// @ATG_CHANGE : sspiller@microsoft.com - END

// Alias types to match Xbox
namespace Windows
{
	namespace Xbox
	{
		namespace System
		{
			using UserAddedEventArgs = EraAdapter::Windows::Xbox::System::UserAddedEventArgs;
			using SignInCompletedEventArgs = EraAdapter::Windows::Xbox::System::SignInCompletedEventArgs;
			using SignOutStartedEventArgs = EraAdapter::Windows::Xbox::System::SignOutStartedEventArgs;
			using SignOutCompletedEventArgs = EraAdapter::Windows::Xbox::System::SignOutCompletedEventArgs;
			using UserRemovedEventArgs = EraAdapter::Windows::Xbox::System::UserRemovedEventArgs;
		}

		namespace UI
		{
			using SystemUI = EraAdapter::Windows::Xbox::UI::SystemUI;
			using AccountPickerResult = EraAdapter::Windows::Xbox::UI::AccountPickerResult;
			using AccountPickerOptions = EraAdapter::Windows::Xbox::UI::AccountPickerOptions;
		}

		namespace Networking
		{
			using SecureDeviceAssociationTemplate = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociationTemplate;
			using ISecureDeviceAssociationTemplate = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociationTemplate;
			using SecureDeviceAssociation = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociation;
			using ISecureDeviceAssociation = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociation;
			using SecureDeviceAssociationStateChangedEventArgs = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociationStateChangedEventArgs;
			using SecureDeviceAssociationState = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociationState;
			using SecureDeviceAddress = EraAdapter::Windows::Xbox::Networking::SecureDeviceAddress;
			using QualityOfService = EraAdapter::Windows::Xbox::Networking::QualityOfService;
			using QualityOfServiceMetric = EraAdapter::Windows::Xbox::Networking::QualityOfServiceMetric;
			using QualityOfServiceMeasurement = EraAdapter::Windows::Xbox::Networking::QualityOfServiceMeasurement;
			using QualityOfServiceMeasurementStatus = EraAdapter::Windows::Xbox::Networking::QualityOfServiceMeasurementStatus;
			using MeasureQualityOfServiceResult = EraAdapter::Windows::Xbox::Networking::MeasureQualityOfServiceResult;
			using CreateSecureDeviceAssociationBehavior = EraAdapter::Windows::Xbox::Networking::CreateSecureDeviceAssociationBehavior;
			using SecureDeviceAssociationIncomingEventArgs = EraAdapter::Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs;
		}

#if WITH_GAME_CHAT
		namespace Chat
		{
			using ChatRestriction = ::Microsoft::Xbox::ChatAudio::ChatRestriction;
		}
#endif

		namespace Multiplayer
		{
			using MultiplayerSessionReference = EraAdapter::Windows::Xbox::Multiplayer::MultiplayerSessionReference;
		}

		namespace Input
		{
			using Controller = EraAdapter::Windows::Xbox::Input::Controller;
			using ControllerPairingChangedEventArgs = EraAdapter::Windows::Xbox::Input::ControllerPairingChangedEventArgs;
		}

		namespace Services
		{
			using XboxLiveConfiguration = ::Microsoft::Xbox::Services::XboxLiveAppConfiguration;
		}
	}
}

typedef FUWPInputInterface FPlatformInputInterface;

PACK_WINRT()

inline Windows::Xbox::System::User ^SystemUserFromXSAPIUser(Microsoft::Xbox::Services::System::XboxLiveUser ^user)
{
	return Windows::Xbox::System::User::ShimUserFromXSAPIUser(user);
}

inline Microsoft::Xbox::Services::System::XboxLiveUser ^XSAPIUserFromSystemUser(Windows::Xbox::System::User ^user)
{
	return Windows::Xbox::System::User::XSAPIUserFromShimUser(user);
}

inline Windows::Xbox::System::User ^SystemUserFromControllerUser(Windows::System::User ^user)
{
	return Windows::Xbox::System::User::ShimUserFromControllerUser(user);
}

#if !PLATFORM_64BITS
inline void ForceImportOfWinRTTypesInsidePackBlock()
{
	ref new Windows::Networking::Connectivity::NetworkStatusChangedEventHandler(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityResyncEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionErrorEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityConnectionState>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAssociationTemplate ^, Windows::Xbox::Networking::SecureDeviceAssociationIncomingEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::UserStatistics::StatisticChangeEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::Collections::MapChangedEventHandler<Platform::String ^, Platform::Object ^>(nullptr, nullptr);
	ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Xbox::UI::AccountPickerResult ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::Gaming::Input::IGameController ^, Windows::Gaming::Input::Headset ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::Gaming::Input::IGameController ^, Windows::System::UserChangedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Windows::Gaming::Input::Gamepad ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::TitlePresenceChangeEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::DevicePresenceChangeEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::Core::UnhandledErrorDetectedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Platform::Object ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::SuspendingEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::ApplicationModel::Core::CoreApplicationView ^, Windows::ApplicationModel::Activation::IActivatedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::ApplicationModel::Core::CoreApplicationView ^, Windows::ApplicationModel::Core::HostedViewClosingEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::System::UserWatcher ^, Windows::System::UserChangedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::System::UserWatcher ^, Windows::System::UserAuthenticationStatusChangingEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::System::UserWatcher ^, Platform::Object ^>(nullptr, nullptr);
#if WITH_GAME_CHAT
	ref new Microsoft::Xbox::GameChat::ProcessAudioBufferHandler(nullptr, nullptr);
	ref new Microsoft::Xbox::GameChat::CompareUniqueConsoleIdentifiersHandler(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::ChatPacketEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::DebugMessageEventArgs ^>(nullptr, nullptr);
#endif
	ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::Activation::BackgroundActivatedEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::EnteredBackgroundEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::LeavingBackgroundEventArgs ^>(nullptr, nullptr);
	ref new Windows::Foundation::TypedEventHandler<Windows::Xbox::Networking::SecureDeviceAddress ^, Platform::Object ^>(nullptr, nullptr);

}
#endif

PACK_WINRT_REVERT()

#endif
// @ATG_CHANGE :  END
