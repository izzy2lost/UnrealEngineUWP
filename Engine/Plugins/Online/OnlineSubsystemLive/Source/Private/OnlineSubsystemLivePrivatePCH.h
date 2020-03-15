// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// @ATG_CHANGE :  BEGIN UWP LIVE support
// Get ppltasks properly wrapped, otherwise create_async<...> instantiations 
// can cause compile errors.  This means we need to include it as early as possible
// in case later headers are including it without full protection.
#if PLATFORM_UWP
#include "HAL/Platform.h"

#include "AllowWindowsPlatformAtomics.h"
#include <ppltasks.h>
#include "HideWindowsPlatformAtomics.h"
#endif // PLATFORM_UWP
// @ATG_CHANGE :  END

#include "CoreMinimal.h"
#include "OnlineSubsystemLiveModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemLive.h"
#include "Modules/ModuleManager.h"
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

#include "XboxOneInputInterface.h"

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

inline Windows::Xbox::Input::Controller^ SystemGamepadFromShim(Windows::Xbox::Input::Controller^ controller)
{
	return controller;
}

#elif PLATFORM_UWP
#include "Windows/AllowWindowsPlatformTypes.h"
#include <ws2tcpip.h>
#include <collection.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "UWP/UWPInputInterface.h"

#include "UWP/Marketplace.h"

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
			using IGamepad = EraAdapter::Windows::Xbox::Input::Controller;
			using ControllerPairingChangedEventArgs = EraAdapter::Windows::Xbox::Input::ControllerPairingChangedEventArgs;
		}

		namespace ApplicationModel
		{
			namespace Store
			{
				using KnownPrivileges = EraAdapter::Windows::Xbox::ApplicationModel::Store::KnownPrivileges;
				using PrivilegeCheckResult = EraAdapter::Windows::Xbox::ApplicationModel::Store::PrivilegeCheckResult;
				using Product = EraAdapter::Windows::Xbox::ApplicationModel::Store::Product;
				using ProductPurchasedEventArgs = EraAdapter::Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventArgs;
				using ProductPurchasedEventHandler = EraAdapter::Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventHandler;
			}
		}
	}
}

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

inline Windows::Gaming::Input::Gamepad^ SystemGamepadFromShim(EraAdapter::Windows::Xbox::Input::Controller^ controller)
{
	return controller->Gamepad;
}

#if WITH_MARKETPLACE

inline Microsoft::Xbox::Services::Marketplace::CatalogService^ GetCatalogService(Microsoft::Xbox::Services::XboxLiveContext^ LiveContext)
{
	return Microsoft::Xbox::Services::Marketplace::CatalogService::Get();
}

inline Microsoft::Xbox::Services::Marketplace::InventoryService^ GetInventoryService(Microsoft::Xbox::Services::XboxLiveContext^ LiveContext)
{
	return Microsoft::Xbox::Services::Marketplace::InventoryService::Get();
}

#endif // WITH_MARKETPLACE

#endif
// @ATG_CHANGE :  END
/** Attribute to use to read the IsBadReputation attribute off a local-user */
#define BAD_REPUTATION_ATTRIBUTE TEXT("BadReputation")
