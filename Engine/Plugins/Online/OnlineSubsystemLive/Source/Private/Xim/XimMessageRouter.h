#pragma once

#include "../OnlineSubsystemLiveTypes.h"
#include "Containers/Ticker.h"

namespace xbox
{
	namespace services
	{
		namespace xbox_integrated_multiplayer
		{
			class xim_player;
			enum class xim_network_exit_reason;
		}
	}
}

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimPlayerJoined, xbox::services::xbox_integrated_multiplayer::xim_player*);
typedef FOnXimPlayerJoined::FDelegate FOnXimPlayerJoinedDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimPlayerLeft, xbox::services::xbox_integrated_multiplayer::xim_player*);
typedef FOnXimPlayerLeft::FDelegate FOnXimPlayerLeftDelegate;

typedef TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> DataPayloadType;
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnXimPlayerDataReceived, xbox::services::xbox_integrated_multiplayer::xim_player*, xbox::services::xbox_integrated_multiplayer::xim_player*, DataPayloadType)
typedef FOnXimPlayerDataReceived::FDelegate FOnXimPlayerDataReceivedDelegate;

DECLARE_MULTICAST_DELEGATE(FOnXimMoveSucceeded);
typedef FOnXimMoveSucceeded::FDelegate FOnXimMoveSucceededDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimNetworkExited, xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason);
typedef FOnXimNetworkExited::FDelegate FOnXimNetworkExitedDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimSendQueueAlert, xbox::services::xbox_integrated_multiplayer::xim_player*);
typedef FOnXimSendQueueAlert::FDelegate FOnXimSendQueueAlertDelegate;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnXimPlayerPropertyChanged, xbox::services::xbox_integrated_multiplayer::xim_player*, const FString&);
typedef FOnXimPlayerPropertyChanged::FDelegate FOnXimPlayerPropertyChangedDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimNetworkPropertyChanged, const FString&);
typedef FOnXimNetworkPropertyChanged::FDelegate FOnXimNetworkPropertyChangedDelegate;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnXimJoinableUsersCompleted, const TArray<TSharedRef<FUniqueNetIdLive>>&);
typedef FOnXimJoinableUsersCompleted::FDelegate FOnXimJoinableUsersCompletedDelegate;

DECLARE_MULTICAST_DELEGATE(FOnXimInviteUICompleted);
typedef FOnXimInviteUICompleted::FDelegate FOnXimInviteUICompletedDelegate;

class FXimMessageRouter : public FTickerObjectBase
{
private:
	class FOnlineSubsystemLive* LiveSubsystem;

PACKAGE_SCOPE:
	FXimMessageRouter(class FOnlineSubsystemLive* InSubsystem);
	xbox::services::xbox_integrated_multiplayer::xim_player* GetXimPlayerForNetId(const FUniqueNetIdLive& UniqueId);
	//TSharedRef<const FUniqueNetIdLive> GetNetIdForXimPlayer(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);

	static void* XimAlloc(size_t Size, uint32_t MemoryType);
	static void XimFree(void* Pointer, uint32_t MemoryType);

public:


	virtual bool Tick(float DeltaTime) override;

	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimPlayerLeft, xbox::services::xbox_integrated_multiplayer::xim_player*);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimPlayerJoined, xbox::services::xbox_integrated_multiplayer::xim_player*);
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnXimPlayerDataReceived, xbox::services::xbox_integrated_multiplayer::xim_player*, xbox::services::xbox_integrated_multiplayer::xim_player*, DataPayloadType);
	DEFINE_ONLINE_DELEGATE(OnXimMoveSucceeded);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimNetworkExited, xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimSendQueueAlert, xbox::services::xbox_integrated_multiplayer::xim_player*);
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnXimPlayerPropertyChanged, xbox::services::xbox_integrated_multiplayer::xim_player*, const FString&);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimNetworkPropertyChanged, const FString&);
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnXimJoinableUsersCompleted, const TArray<TSharedRef<FUniqueNetIdLive>>&)
	DEFINE_ONLINE_DELEGATE(OnXimInviteUICompleted);
};