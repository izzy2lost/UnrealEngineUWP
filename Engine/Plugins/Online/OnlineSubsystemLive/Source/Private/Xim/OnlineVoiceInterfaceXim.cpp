#include "../OnlineSubsystemLivePrivatePCH.h"

#if USE_XIM

#include "OnlineVoiceInterfaceXim.h"
#include "XimMessageRouter.h"
#include "../OnlineIdentityInterfaceLive.h"

#include <xboxintegratedmultiplayer.h>

using namespace xbox::services::xbox_integrated_multiplayer;

FOnlineVoiceXim::FOnlineVoiceXim(FOnlineSubsystemLive* InLiveSubsystem) :
	LiveSubsystem(InLiveSubsystem)
{
}

xim_player* FOnlineVoiceXim::FindXimPlayerForLocalUserNum(uint32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!UniqueId.IsValid())
	{
		return nullptr;
	}

	return FindXimPlayerForNetId(*UniqueId);
}

xim_player* FOnlineVoiceXim::FindXimPlayerForNetId(const FUniqueNetId& UniqueId) const
{
	const FTalkerXim* Talker = AllTalkers.Find(FUniqueNetIdLive(UniqueId));
	return Talker != nullptr ? Talker->XimPlayer : nullptr;
}


bool FOnlineVoiceXim::Init()
{
	XimPlayerJoinedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerJoinedDelegate_Handle(FOnXimPlayerJoinedDelegate::CreateThreadSafeSP(this, &FOnlineVoiceXim::OnXimPlayerJoined));
	XimPlayerLeftHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimPlayerLeftDelegate_Handle(FOnXimPlayerJoinedDelegate::CreateThreadSafeSP(this, &FOnlineVoiceXim::OnXimPlayerLeft));
	XimNetworkExitedHandle = LiveSubsystem->GetXimMessageRouter()->AddOnXimNetworkExitedDelegate_Handle(FOnXimNetworkExitedDelegate::CreateThreadSafeSP(this, &FOnlineVoiceXim::OnXimNetworkExited));
	return true;
}

void FOnlineVoiceXim::ProcessMuteChangeNotification()
{
}

void FOnlineVoiceXim::StartNetworkedVoice(uint8 LocalUserNum)
{
	xim_player* Player = FindXimPlayerForLocalUserNum(LocalUserNum);
	if (Player != nullptr)
	{
		Player->set_chat_muted(false);
	}
}

void FOnlineVoiceXim::StopNetworkedVoice(uint8 LocalUserNum)
{
	xim_player* Player = FindXimPlayerForLocalUserNum(LocalUserNum);
	if (Player != nullptr)
	{
		Player->set_chat_muted(true);
	}
}

bool FOnlineVoiceXim::RegisterLocalTalker(uint32 LocalUserNum)
{
	TSharedPtr<const FUniqueNetId> UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!UniqueId.IsValid())
	{
		return false;
	}

	// Consider calling set_intended_local_xbox_user_ids if this player is not already in the
	// xim network?  Could be relevant if we have scenarios that use xim for chat but MPSD for
	// gameplay session management.

	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	FString XboxUserIdString = UniqueId->ToString();
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local() && XboxUserIdString == XimPlayers[i]->xbox_user_id())
		{
			FTalkerXim NewTalker;
			NewTalker.XimPlayer = XimPlayers[i];
			NewTalker.WasTalking = false;
			AllTalkers.Add(FUniqueNetIdLive(*UniqueId), NewTalker);
			return true;
		}
	}

	return false;
}

void FOnlineVoiceXim::RegisterLocalTalkers()
{
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local())
		{
			FTalkerXim NewTalker;
			NewTalker.XimPlayer = XimPlayers[i];
			NewTalker.WasTalking = false;
			AllTalkers.Add(FUniqueNetIdLive(XimPlayers[i]->xbox_user_id()), NewTalker);
		}
	}
}

bool FOnlineVoiceXim::UnregisterLocalTalker(uint32 LocalUserNum)
{
	TSharedPtr<const FUniqueNetId> UniqueId = LiveSubsystem->GetIdentityLive()->GetUniquePlayerId(LocalUserNum);
	if (!UniqueId.IsValid())
	{
		return false;
	}

	// In the future if we call set_intended_local_xbox_user_ids in RegisterLocalTalker then we need to undo that here.

	return AllTalkers.Remove(FUniqueNetIdLive(*UniqueId)) > 0;
}

void FOnlineVoiceXim::UnregisterLocalTalkers()
{
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local())
		{
			AllTalkers.Remove(FUniqueNetIdLive(XimPlayers[i]->xbox_user_id()));
		}
	}
}

bool FOnlineVoiceXim::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	FString XboxUserIdString = UniqueId.ToString();
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (!XimPlayers[i]->local() && XboxUserIdString == XimPlayers[i]->xbox_user_id())
		{
			FTalkerXim NewTalker;
			NewTalker.XimPlayer = XimPlayers[i];
			NewTalker.WasTalking = false;
			AllTalkers.Add(FUniqueNetIdLive(UniqueId), NewTalker);
			return true;
		}
	}
	return false;
}

bool FOnlineVoiceXim::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	return AllTalkers.Remove(FUniqueNetIdLive(UniqueId)) > 0;
}

void FOnlineVoiceXim::RemoveAllRemoteTalkers()
{
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (!XimPlayers[i]->local())
		{
			AllTalkers.Remove(FUniqueNetIdLive(XimPlayers[i]->xbox_user_id()));
		}
	}
}

bool FOnlineVoiceXim::IsHeadsetPresent(uint32 LocalUserNum)
{
	xim_player* Player = FindXimPlayerForLocalUserNum(LocalUserNum);
	return Player != nullptr && Player->chat_indicator() != xim_player_chat_indicator::no_microphone;
} 

bool FOnlineVoiceXim::IsLocalPlayerTalking(uint32 LocalUserNum)
{
	xim_player* Player = FindXimPlayerForLocalUserNum(LocalUserNum);
	return Player != nullptr && Player->chat_indicator() == xim_player_chat_indicator::talking;
}

bool FOnlineVoiceXim::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	xim_player* Player = FindXimPlayerForNetId(UniqueId);
	return Player != nullptr && Player->chat_indicator() == xim_player_chat_indicator::talking;
}

bool FOnlineVoiceXim::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	xim_player* Player = FindXimPlayerForNetId(UniqueId);
	if (Player != nullptr)
	{
		switch (Player->chat_indicator())
		{
		case xim_player_chat_indicator::silent:
		case xim_player_chat_indicator::talking:
			return false;

		case xim_player_chat_indicator::muted:
		case xim_player_chat_indicator::platform_restricted:
		case xim_player_chat_indicator::no_chat_focus:
		case xim_player_chat_indicator::disabled:
		case xim_player_chat_indicator::no_microphone:
		default:
			// Note that this classes several different communication-preventing scenarios
			// as muted.  UE's interface isn't as fine-grained as XIM's, so probably appropriate.
			return true;
		}
	}
	return true;
}

bool FOnlineVoiceXim::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	xim_player* Player = FindXimPlayerForNetId(PlayerId);
	if (Player != nullptr)
	{
		Player->set_chat_muted(true);
		return true;
	}
	return false;
}

bool FOnlineVoiceXim::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	xim_player* Player = FindXimPlayerForNetId(PlayerId);
	if (Player != nullptr)
	{
		Player->set_chat_muted(false);
		return true;
	}
	return false;
}

TSharedPtr<class FVoicePacket> FOnlineVoiceXim::SerializeRemotePacket(FArchive& Ar)
{
	return nullptr;
}

TSharedPtr<class FVoicePacket> FOnlineVoiceXim::GetLocalPacket(uint32 LocalUserNum)
{
	return nullptr;
}

int32 FOnlineVoiceXim::GetNumLocalTalkers()
{
	int32 LocalTalkerCount = 0;
	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XimPlayers[i]->local())
		{
			++LocalTalkerCount;
		}
	}

	return LocalTalkerCount;
}

void FOnlineVoiceXim::ClearVoicePackets()
{
}

void FOnlineVoiceXim::Tick(float DeltaTime)
{
	for (TMap<FUniqueNetIdLive, FTalkerXim>::TIterator It(AllTalkers); It; ++It)
	{
		bool IsTalking = It->Value.XimPlayer->chat_indicator() == xim_player_chat_indicator::talking;
		if (IsTalking != It->Value.WasTalking)
		{
			TSharedRef<const FUniqueNetId> NetIdRef = MakeShareable(new FUniqueNetIdLive(It->Key));
			TriggerOnPlayerTalkingStateChangedDelegates(NetIdRef, IsTalking);
			It->Value.WasTalking = IsTalking;
		}
	}
}

FString FOnlineVoiceXim::GetVoiceDebugState() const
{
	return TEXT("");
}

void FOnlineVoiceXim::OnXimPlayerJoined(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer)
{
	FTalkerXim NewTalker;
	NewTalker.XimPlayer = XimPlayer;
	NewTalker.WasTalking = false;
	AllTalkers.Add(FUniqueNetIdLive(XimPlayer->xbox_user_id()), NewTalker);
}

void FOnlineVoiceXim::OnXimPlayerLeft(xbox::services::xbox_integrated_multiplayer::xim_player* XimPlayer)
{
	FUniqueNetIdLive UniqueId(XimPlayer->xbox_user_id());
	AllTalkers.Remove(UniqueId);
}

void FOnlineVoiceXim::OnXimNetworkExited(xbox::services::xbox_integrated_multiplayer::xim_network_exit_reason Reason)
{
	AllTalkers.Empty();
}


#endif