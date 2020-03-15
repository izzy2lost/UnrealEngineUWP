//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace System { ref class User; }
namespace ApplicationModel {
namespace Store {

///////////////////////////////////////////////////////////////////////////////
//
//  Enumerations
//
public enum class KnownPrivileges
{
	XPRIVILEGE_ADD_FRIEND					= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::AddFriend,
	XPRIVILEGE_BROADCAST					= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::Broadcast,
	XPRIVILEGE_CLOUD_GAMING_JOIN_SESSION	= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::CloudGamingJoinSession,
	XPRIVILEGE_CLOUD_GAMING_MANAGE_SESSION	= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::CloudGamingManageSession,
	XPRIVILEGE_CLOUD_SAVED_GAMES			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::CloudSavedGames,
	XPRIVILEGE_COMMUNICATIONS				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::Communications,
	XPRIVILEGE_COMMUNICATION_VOICE_INGAME	= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::CommunicationVoiceIngame,
	XPRIVILEGE_COMMUNICATION_VOICE_SKYPE	= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::CommunicationVoiceSkype,
	XPRIVILEGE_GAME_DVR						= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::GameDVR,
	XPRIVILEGE_MULTIPLAYER_PARTIES			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::MultiplayerParties,
	XPRIVILEGE_MULTIPLAYER_SESSIONS			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::MultiplayerSessions,
	XPRIVILEGE_PREMIUM_CONTENT				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::PremiumContent,
	XPRIVILEGE_PREMIUM_VIDEO				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::PremiumVideo,
	XPRIVILEGE_PROFILE_VIEWING				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::ProfileViewing,
	XPRIVILEGE_PURCHASE_CONTENT				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::PurchaseContent,
	XPRIVILEGE_SHARE_CONTENT				= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::ShareContent,
	XPRIVILEGE_SHARE_KINECT_CONTENT			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::ShareKinectContent,
	XPRIVILEGE_SOCIAL_NETWORK_SHARING		= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::SocialNetworkSharing,
	XPRIVILEGE_SUBSCRIPTION_CONTENT			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::SubscriptionContent,
	XPRIVILEGE_USER_CREATED_CONTENT			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::UserCreatedContent,
	XPRIVILEGE_VIDEO_COMMUNICATIONS			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::VideoCommunications,
	XPRIVILEGE_VIEW_FRIENDS_LIST			= (int)::Microsoft::Xbox::Services::System::GamingPrivilege::ViewFriendsList,
};

public enum class PrivilegeCheckResult
{
	Aborted				= 0x02,
	Banned				= 0x04,
	NoIssue				= 0x00,
	PurchaseRequired	= 0x01,
	Restricted			= 0x08,
};

public ref class ProductPurchasedEventArgs sealed
{
public:
	property Platform::String^ Receipt
	{
		Platform::String^ get();
	}

internal:
	ProductPurchasedEventArgs(
		Platform::String^ receipt
	);

private:
	Platform::String^ _receipt;
};

public delegate void ProductPurchasedEventHandler(ProductPurchasedEventArgs^ args);

///////////////////////////////////////////////////////////////////////////////
//
//  Product
//
public ref class Product sealed
{
public:
	static ::Windows::Foundation::IAsyncOperation<PrivilegeCheckResult>^
		CheckPrivilegeAsync(
			EraAdapter::Windows::Xbox::System::User^ user,
			uint32 privilegeId,
			bool attemptResolution,
			Platform::String^ friendlyDisplay
		);

	static ::Windows::Foundation::IAsyncAction^ ShowPurchaseAsync(
		EraAdapter::Windows::Xbox::System::User^ user,
		Platform::String^ offer
	);

	static ::Windows::Foundation::IAsyncAction^ ShowPurchaseForStoreIdAsync(
		EraAdapter::Windows::Xbox::System::User^ user,
		Platform::String^ offer
	);

	static ::Windows::Foundation::IAsyncAction^ ShowRedeemCodeAsync(
		EraAdapter::Windows::Xbox::System::User^ user,
		Platform::String^ offer
	);

	// Note: currently no code triggers this event
	static event ProductPurchasedEventHandler^ ProductPurchased
	{
		::Windows::Foundation::EventRegistrationToken add(ProductPurchasedEventHandler^ handler);
		void remove(::Windows::Foundation::EventRegistrationToken token);
	}

	// Not part of the original surface we're shimming, but used as a helper for ShowPurchaseAsync
	// and could be useful as publically callable in its own right.
	static ::Windows::Foundation::IAsyncAction^ RequestDownloadOrUpdateContentForOfferAsync(
		EraAdapter::Windows::Xbox::System::User^ user,
		Platform::String^ offer
	);

private:

	static concurrency::task<void> FinishTaskChainForDownloadOperation(
		::Windows::Foundation::IAsyncOperationWithProgress<::Windows::Services::Store::StorePackageUpdateResult^, ::Windows::Services::Store::StorePackageUpdateStatus>^ downloadOperation);

	static event ::Windows::Foundation::EventHandler<ProductPurchasedEventArgs^>^ _productPurchased;

	static ::Windows::Services::Store::StoreContext^ s_context;

};

}
}
}
}
}
