#pragma once

#include "VoiceInterface.h"
#include "../OnlineSubsystemLiveTypes.h"

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

struct FTalkerXim
{
	xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer;
	bool WasTalking;
};

class FOnlineVoiceXim : public IOnlineVoice, public TSharedFromThis<FOnlineVoiceXim, ESPMode::ThreadSafe>
{
private:
	class FOnlineSubsystemLive* LiveSubsystem;

	TMap<FUniqueNetIdLive, FTalkerXim> AllTalkers;

	FDelegateHandle XimPlayerJoinedHandle;
	FDelegateHandle XimPlayerLeftHandle;
	FDelegateHandle XimNetworkExitedHandle;

	/** Time to wait for new data before triggering "not talking" */
	float VoiceNotificationDelta;

	xbox::services::xbox_integrated_multiplayer::xim_player* FindXimPlayerForLocalUserNum(uint32 LocalUserNum) const;
	xbox::services::xbox_integrated_multiplayer::xim_player* FindXimPlayerForNetId(const FUniqueNetId& UniqueId) const;

	void OnXimPlayerJoined(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);
	void OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer);
	void OnXimNetworkExited(xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason Reason);

PACKAGE_SCOPE:
	FOnlineVoiceXim(class FOnlineSubsystemLive* InLiveSubsystem);
	virtual void ProcessMuteChangeNotification() override;
	virtual bool Init() override;

public:
	virtual void StartNetworkedVoice(uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(uint8 LocalUserNum) override;
	virtual bool RegisterLocalTalker(uint32 LocalUserNum) override;
	virtual void RegisterLocalTalkers() override;
	virtual bool UnregisterLocalTalker(uint32 LocalUserNum) override;
	virtual void UnregisterLocalTalkers() override;
	virtual bool RegisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual bool UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual void RemoveAllRemoteTalkers() override;
	virtual bool IsHeadsetPresent(uint32 LocalUserNum) override;
	virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override;
	virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override;
	virtual bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	virtual bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum);
	virtual int32 GetNumLocalTalkers() override;
	virtual void ClearVoicePackets() override;
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;
};