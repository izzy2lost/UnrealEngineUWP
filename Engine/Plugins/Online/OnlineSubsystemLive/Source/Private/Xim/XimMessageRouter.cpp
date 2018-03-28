#include "../OnlineSubsystemLivePrivatePCH.h"

#if USE_XIM

#include "XimMessageRouter.h"

#include <xboxintegratedmultiplayer.h>
#include <xboxintegratedmultiplayerimpl.h>


DECLARE_MEMORY_STAT(TEXT("XIM memory"), STAT_XimMemory, STATGROUP_Online);

FXimMessageRouter::FXimMessageRouter(FOnlineSubsystemLive* InSubsystem) :
	LiveSubsystem(InSubsystem)
{
	using namespace xbox::services::xbox_integrated_multiplayer;

	Microsoft::Xbox::Services::XboxLiveAppConfiguration^ AppConfig = Microsoft::Xbox::Services::XboxLiveAppConfiguration::SingletonInstance;
	check(AppConfig != nullptr);

	xim::set_memory_callbacks((xim_allocate_memory_callback)&FXimMessageRouter::XimAlloc, (xim_free_memory_callback)&FXimMessageRouter::XimFree);
	xim::singleton_instance().initialize(AppConfig->ServiceConfigurationId->Data(), AppConfig->TitleId);

}

bool FXimMessageRouter::Tick(float DeltaTime)
{
	using namespace xbox::services::xbox_integrated_multiplayer;

	uint32_t StateChangeCount;
	xim_state_change_array StateChanges;
	xim::singleton_instance().start_processing_state_changes(&StateChangeCount, &StateChanges);
	for (uint32_t i = 0; i < StateChangeCount; ++i)
	{
		switch (StateChanges[i]->state_change_type)
		{
		case xim_state_change_type::player_joined:
			{
				const xim_player_joined_state_change* JoinedStateChange = static_cast<const xim_player_joined_state_change*>(StateChanges[i]);
				xim_player* XimPlayer = JoinedStateChange->player;
				TriggerOnXimPlayerJoinedDelegates(XimPlayer);
			}
			break;

		case xim_state_change_type::player_left:
			{
				const xim_player_left_state_change* LeftStateChange = static_cast<const xim_player_left_state_change*>(StateChanges[i]);
				xim_player* XimPlayer = LeftStateChange->player;
				TriggerOnXimPlayerLeftDelegates(XimPlayer);
			}
			break;

		case xim_state_change_type::move_to_network_succeeded:
			{
				TriggerOnXimMoveSucceededDelegates();
			}
			break;

		case xim_state_change_type::network_exited:
			{
				const xim_network_exited_state_change* ExitedStateChange = static_cast<const xim_network_exited_state_change*>(StateChanges[i]);
				TriggerOnXimNetworkExitedDelegates(ExitedStateChange->reason);
			}
			break;

		case xim_state_change_type::player_to_player_data_received:
			{
				const xim_player_to_player_data_received_state_change* DataReceivedStateChange = static_cast<const xim_player_to_player_data_received_state_change*>(StateChanges[i]);
				DataPayloadType Payload = MakeShareable(new TArray<uint8>());
				Payload->Insert(static_cast<const uint8*>(DataReceivedStateChange->message), DataReceivedStateChange->message_size, 0);
				for (uint32_t j = 0; j < DataReceivedStateChange->receiver_count; ++j)
				{
					TriggerOnXimPlayerDataReceivedDelegates(DataReceivedStateChange->sender, DataReceivedStateChange->receivers[j], Payload);
				}
			}
			break;

		case xim_state_change_type::send_queue_alert_triggered:
			{
				const xim_send_queue_alert_triggered_state_change* SendQueueAlertStateChange = static_cast<const xim_send_queue_alert_triggered_state_change*>(StateChanges[i]);
				TriggerOnXimSendQueueAlertDelegates(SendQueueAlertStateChange->target_player);
			}
			break;

		case xim_state_change_type::player_custom_properties_changed:
			{
				const xim_player_custom_properties_changed_state_change* PropertiesStateChange = static_cast<const xim_player_custom_properties_changed_state_change*>(StateChanges[i]);
				for (uint32_t j = 0; j < PropertiesStateChange->property_count; ++j)
				{
					TriggerOnXimPlayerPropertyChangedDelegates(PropertiesStateChange->player, FString(PropertiesStateChange->property_names[j]));
				}
			}
			break;

		case xim_state_change_type::network_custom_properties_changed:
			{
				const xim_network_custom_properties_changed_state_change* PropertiesStateChange = static_cast<const xim_network_custom_properties_changed_state_change*>(StateChanges[i]);
				for (uint32_t j = 0; j < PropertiesStateChange->property_count; ++j)
				{
					TriggerOnXimNetworkPropertyChangedDelegates(FString(PropertiesStateChange->property_names[j]));
				}
			}
			break;

		case xim_state_change_type::request_joinable_xbox_user_ids_completed:
			{
				const xim_request_joinable_xbox_user_ids_completed_state_change* JoinableUserIdsStateChange = static_cast<const xim_request_joinable_xbox_user_ids_completed_state_change*>(StateChanges[i]);
				TArray<TSharedRef<FUniqueNetIdLive>> UserIds;
				for (uint32_t j = 0; j < JoinableUserIdsStateChange->joinable_xbox_user_id_count; ++j)
				{
					UserIds.Emplace(MakeShareable(new FUniqueNetIdLive(JoinableUserIdsStateChange->joinable_xbox_user_ids[j])));
				}
				TriggerOnXimJoinableUsersCompletedDelegates(UserIds);
			}
			break;

		case xim_state_change_type::show_invite_ui_completed:
			{
				TriggerOnXimInviteUICompletedDelegates();
			}
			break;

		default:
			break;
		}

	}
	xim::singleton_instance().finish_processing_state_changes(StateChanges);

	return true;
}

xbox::services::xbox_integrated_multiplayer::xim_player* FXimMessageRouter::GetXimPlayerForNetId(const FUniqueNetIdLive& UniqueId)
{
	using namespace xbox::services::xbox_integrated_multiplayer;

	uint32_t XimPlayerCount;
	xim_player_array XimPlayers;
	xim::singleton_instance().get_players(&XimPlayerCount, &XimPlayers);
	FString XboxUserIdString = UniqueId.ToString();
	for (uint32_t i = 0; i < XimPlayerCount; ++i)
	{
		if (XboxUserIdString == XimPlayers[i]->xbox_user_id())
		{
			return XimPlayers[i];
		}
	}
	return nullptr;
}

void* FXimMessageRouter::XimAlloc(size_t Size, uint32_t MemoryType)
{
	void* Result = FMemory::Malloc(Size);
#if STATS
	const SIZE_T ActualSize = FMemory::GetAllocSize(Result);
	INC_MEMORY_STAT_BY(STAT_XimMemory, ActualSize);
#endif // STATS
	return Result;
}

void FXimMessageRouter::XimFree(void* Pointer, uint32_t MemoryType)
{
#if STATS
	const SIZE_T Size = FMemory::GetAllocSize(Pointer);
	DEC_MEMORY_STAT_BY(STAT_XimMemory, Size);
#endif // STATS
	FMemory::Free(Pointer);
}

#endif