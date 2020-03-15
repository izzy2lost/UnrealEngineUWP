// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineSubsystemLive.h"
#include "OnlineVoiceInterfaceLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineSessionInterfaceLive.h"
#include "Voice.h"
#include "OnlineAsyncTaskManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"

#include "Misc/Crc.h"

#include <collection.h>
#include <Robuffer.h>

// @ATG_CHANGE : BEGIN UWP LIVE support
#if WITH_GAME_CHAT
// @ATG_CHANGE : END

using namespace concurrency;
using namespace Microsoft::Xbox::GameChat;
using namespace Windows::Foundation::Collections;
using Windows::Foundation::IAsyncAction;
using Windows::Xbox::System::User;

/** Limit for the number of voice packets to keep in the buffer in case they can't be sent immediately. */
static const int32 MaxBufferedVoicePackets = 128;

/** Constructor */
FOnlineVoiceLive::FOnlineVoiceLive(FOnlineSubsystemLive* InLiveSubsystem)
	: LiveSubsystem(InLiveSubsystem)
	, IdentityInt(nullptr)
	, MaxLocalTalkers(0)
	, MaxRemoteTalkers(0)
	, VoiceNotificationDelta(0.0f)
	, DebugDisplayEnabled(false)
	, PacketIndex(0)
	, bAutoMuteBadRepRemotePlayers(false)
{
	// @ATG_CHANGE : Init now called separately
}

FOnlineVoiceLive::~FOnlineVoiceLive()
{
	UnhookLiveEvents();
}

/**
 * Initialize the voice interface
 */
bool FOnlineVoiceLive::Init()
{
	bool bSuccess = false;

	DebugDisplayEnabled = false;
	PacketIndex = 0;

	if (!GConfig->GetInt(TEXT("OnlineSubsystem"),TEXT("MaxLocalTalkers"), MaxLocalTalkers, GEngineIni))
	{
		MaxLocalTalkers = MAX_SPLITSCREEN_TALKERS;
		UE_LOG(LogVoice, Warning, TEXT("Missing MaxLocalTalkers key in OnlineSubsystem of DefaultEngine.ini"));
	}
	if (MaxLocalTalkers < 0)
	{
		UE_LOG(LogVoice, Warning, TEXT("Invalid MaxLocalTalkers value of %d, setting to 0"), MaxLocalTalkers);
		MaxLocalTalkers = 0;
	}

	if (!GConfig->GetInt(TEXT("OnlineSubsystem"),TEXT("MaxRemoteTalkers"), MaxRemoteTalkers, GEngineIni))
	{
		MaxRemoteTalkers = MAX_REMOTE_TALKERS;
		UE_LOG(LogVoice, Warning, TEXT("Missing MaxRemoteTalkers key in OnlineSubsystem of DefaultEngine.ini"));
	}
	if (MaxRemoteTalkers < 0)
	{
		UE_LOG(LogVoice, Warning, TEXT("Invalid MaxRemoteTalkers value of %d, setting to 0"), MaxRemoteTalkers);
		MaxRemoteTalkers = 0;
	}

	if (!GConfig->GetFloat(TEXT("OnlineSubsystem"),TEXT("VoiceNotificationDelta"), VoiceNotificationDelta, GEngineIni))
	{
		VoiceNotificationDelta = 0.2f;
		UE_LOG(LogVoice, Warning, TEXT("Missing VoiceNotificationDelta key in OnlineSubsystem of DefaultEngine.ini"));
	}

	GConfig->GetBool(TEXT("OnlineSubsystemLive"),TEXT("bAutoMuteBadRepRemotePlayers"), bAutoMuteBadRepRemotePlayers, GEngineIni);

	GConfig->GetBool(TEXT("OnlineSubsystem"),TEXT("VoiceDebugDisplay"),DebugDisplayEnabled,GEngineIni);

	if (LiveSubsystem)
	{
		IdentityInt = (FOnlineIdentityLive*)LiveSubsystem->GetIdentityInterface().Get();

		if (IdentityInt)
		{
			FScopeLock LocalTalkersLock( &LocalTalkersCS );

			bSuccess = true;
			LocalTalkers.Init(FLocalTalker(), MaxLocalTalkers);
			RemoteTalkers.Empty(MaxRemoteTalkers);
		}
	}

	HookLiveEvents();

	return bSuccess;
}

/**
 * Clears all voice packets currently queued for send
 */
void FOnlineVoiceLive::ClearVoicePackets()
{
	// this is called once per frame and deletes any packets not yet sent
	// this is bad for reliable packets, so we make sure they don't get deleted
	FScopeLock PacketLock(&LocalPacketsCS);

	for (int32 i = (LocalPackets.Num() - 1); i >= 0; --i)
	{
		if (!LocalPackets[i]->IsReliable())
		{
			LocalPackets.RemoveAt(i, 1, false);
		}
	}

	LocalPackets.Append(PendingLocalPackets);
	PendingLocalPackets.Empty();
}

/**
 * Allows for platform specific servicing of devices, etc.
 *
 * @param DeltaTime the amount of time that has elapsed since the last update
 */
void FOnlineVoiceLive::Tick(float DeltaTime)
{
	// Submit queued packets to audio system
	ProcessRemoteVoicePackets();
	// Fire off any talking notifications for hud display
	ProcessTalkingDelegates(DeltaTime);

	DisplayDebugText();
}

/**
 * Tells the voice layer that networked processing of the voice data is allowed
 * for the specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to allow network transmission for
 */
void FOnlineVoiceLive::StartNetworkedVoice(uint8 LocalUserNum)
{
	// Validate the range of the entry
	if (LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);

		if(LocalUserId.IsValid())
		{
			MuteTalkerLive(*LocalUserId, false);
		}
		//UE_LOG(LogVoice, Log, TEXT("Starting networked voice for user: %d"), LocalUserNum);
	}
	else
	{
		UE_LOG(LogVoice, Warning, TEXT("Invalid user specified in StartNetworkedVoice(%d)"), static_cast<uint32>(LocalUserNum));
	}
}

/**
 * Tells the voice layer to stop processing networked voice support for the
 * specified player. This allows for push-to-talk style voice communication
 *
 * @param LocalUserNum the local user to disallow network transmission for
 */
void FOnlineVoiceLive::StopNetworkedVoice(uint8 LocalUserNum)
{
	// Validate the range of the entry
	if (LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);

		if(LocalUserId.IsValid())
		{
			MuteTalkerLive(*LocalUserId, true);
		}
		UE_LOG(LogVoice, Log, TEXT("Stopping networked voice for user: %d"), LocalUserNum);
	}
	else
	{
		UE_LOG(LogVoice, Warning, TEXT("Invalid user specified in StopNetworkedVoice(%d)"), static_cast<uint32>(LocalUserNum));
	}
}

/**
 * Registers the user index as a local talker (interested in voice data)
 *
 * @param UserIndex the user index that is going to be a talker
 *
 * @return 0 upon success, an error code otherwise
 */
bool FOnlineVoiceLive::RegisterLocalTalker(uint32 LocalUserNum)
{
	FScopeLock LocalTalkersLock( &LocalTalkersCS );

	if (static_cast<int32>(LocalUserNum) >= LocalTalkers.Num())
	{
		UE_LOG( LogVoice, Warning, TEXT( "RegisterLocalTalker: LocalUserNum exceeds size of LocalTalkers array. MaxLocalTalkers may not be configured correctly." ) );
		return false;
	}

	uint32 Return = E_FAIL;
	if (LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		UE_LOG(LogVoice, Warning, TEXT("Invalid user specified in RegisterLocalTalker(%d)"), LocalUserNum);
		return false;
	}

	User^ LocalXboxUser = IdentityInt->GetUserForPlatformUserId(LocalUserNum);
	if (!LocalXboxUser)
	{
		UE_LOG(LogVoice, Warning, TEXT("RegisterLocalTalker: Unable to register local talker %u, unable to find Xbox LocalUser"), LocalUserNum);
		return false;
	}

	// Get at the local talker's cached data
	FLocalTalker& Talker = LocalTalkers[LocalUserNum];

	// Check if we're already registered
	if (Talker.bIsRegistered)
	{
		// If we're already registered, don't re-register.  Count as a "success" to the user by returning true
		return true;
	}

	UE_LOG(LogVoice, Log, TEXT("Registering Local talker %d"), LocalUserNum);

	// 0 here is the channel number. This is arbitrary, but in order to talk to each other everyone must have the same channel
	// Currently there's no way in the UE interface to set this from a game level
	constexpr const uint8 ChannelNumber = 0;

	// @ATG_CHANGE : BEGIN - UWP LIVE support
	IAsyncAction^ AsyncOp = LiveChatManager->AddLocalUserToChatChannelAsync(ChannelNumber, GetChatUserFromLocalUser(LocalXboxUser));
	// @ATG_CHANGE : END
	create_task(AsyncOp)
		.then([this, LocalUserNum](task<void> t)
	{
		// Error handling
		try
		{
			t.get();
			{
				FScopeLock LocalTalkersLock( &LocalTalkersCS );
				FLocalTalker& Talker = LocalTalkers[LocalUserNum];
				Talker.bIsRegistered = true;

				// Ideally we'd call a delegate that we're done registering here, but that doesn't exist.  Hope nobody tried to talk yet!
			}
			UE_LOG(LogVoice, Log, TEXT("Registered Local talker %d"), LocalUserNum);
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogVoice, Warning, L"AddLocalUserToChatChannelAsync failed 0x%x", Ex->HResult);
		}
	});

	return true;
}

/**
 * Registers all signed in local talkers
 */
void FOnlineVoiceLive::RegisterLocalTalkers()
{
	UE_LOG(LogVoice, Log, TEXT("Registering all local talkers"));
	// Loop through the available players and register them
	for (int32 Index = 0; Index < MaxLocalTalkers; Index++)
	{
		// Register the local player as a local talker
		RegisterLocalTalker(Index);
	}
}

/**
 * Unregisters the user index as a local talker (not interested in voice data)
 *
 * @param UserIndex the user index that is no longer a talker
 *
 * @return 0 upon success, an error code otherwise
 */
bool FOnlineVoiceLive::UnregisterLocalTalker(uint32 LocalUserNum)
{
	FScopeLock LocalTalkersLock( &LocalTalkersCS );

	if (static_cast<int32>(LocalUserNum) >= LocalTalkers.Num())
	{
		UE_LOG(LogVoice, Warning, TEXT( "UnregisterLocalTalker: LocalUserNum exceeds size of LocalTalkers array. MaxLocalTalkers may not be configured correctly."));
		return false;
	}

	uint32 Return = S_OK;

	if (LocalUserNum < MAX_LOCAL_PLAYERS)
	{
		// Get at the local talker's cached data
		FLocalTalker& Talker = LocalTalkers[LocalUserNum];
		// Skip the unregistration if not registered
		if (Talker.bIsRegistered == true)
		{
			if (OnPlayerTalkingStateChangedDelegates.IsBound() && (Talker.bIsTalking || Talker.bWasTalking))
			{
				IOnlineIdentityPtr IdentityInt = LiveSubsystem->GetIdentityInterface();
				if (IdentityInt.IsValid())
				{
					TSharedPtr<const FUniqueNetId> UniqueId = IdentityInt->GetUniquePlayerId(LocalUserNum);
					if (UniqueId.IsValid())
					{
						OnPlayerTalkingStateChangedDelegates.Broadcast(UniqueId.ToSharedRef(), false);
					}
					else
					{
						UE_LOG(LogVoice, Warning, TEXT("Invalid UserId for local player %d in UnregisterLocalTalker"), LocalUserNum);
					}
				}
			}

			Windows::Xbox::System::User^ LocalUser = IdentityInt->GetUserForPlatformUserId(LocalUserNum);
			if (LocalUser)
			{
				// @ATG_CHANGE : BEGIN - UWP LIVE support
				auto asyncOp = LiveChatManager->RemoveLocalUserFromChatChannelAsync(0, GetChatUserFromLocalUser(LocalUser));
				// @ATG_CHANGE : END - UWP LIVE support
				create_task(asyncOp)
					.then([](task<void> t)
				{
					// Error handling
					try
					{
						t.get();
					}
					catch (Platform::Exception^ ex)
					{
						UE_LOG(LogVoice, Warning, TEXT("RemoveLocalUserFromChatChannel failed 0x%x"), ex->HResult);
					}
				});
			}
			Talker.bIsTalking = false;
			Talker.bWasTalking = false;
			Talker.bIsRegistered = false;
		}
		UE_LOG(LogVoice, Log, TEXT("Local talker %d unregistered"), LocalUserNum);
	}
	else
	{
		UE_LOG(LogVoice, Log, TEXT("Invalid user specified in UnregisterLocalTalker(%d)"), LocalUserNum);
	}

	return Return == S_OK;
}

/**
 * Unregisters all signed in local talkers
 */
void FOnlineVoiceLive::UnregisterLocalTalkers()
{
	UE_LOG(LogVoice, Log, TEXT("Unregistering all local talkers"));
	// Loop through the 4 available players and unregister them
	for (int32 Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		// Unregister the local player as a local talker
		UnregisterLocalTalker(Index);
	}
}

/**
 * Registers the unique player id as a remote talker (submitted voice data only)
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
bool FOnlineVoiceLive::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	// See if this talker has already been registered or not
	FRemoteTalkerLive* Talker = FindRemoteTalker(UniqueId);
	if (Talker == NULL)
	{
		// Add a new talker to our list
		int32 AddIndex = RemoteTalkers.AddZeroed();
		Talker = &RemoteTalkers[AddIndex];
		// Copy the UniqueId
		const FUniqueNetIdLive& UniqueIdLive = (const FUniqueNetIdLive&)UniqueId;
		Talker->TalkerId = MakeShared<FUniqueNetIdLive>(UniqueIdLive);

		// register the talker with Live
		LiveChatManager->HandleNewRemoteConsole(ref new Platform::String(*UniqueId.ToString()));

		UE_LOG(LogVoice, Log, TEXT("Remote talker %s registered"), *UniqueId.ToDebugString());
	}
	else
	{
		UE_LOG(LogVoice, VeryVerbose, TEXT("Remote talker %s is being re-registered"), *UniqueId.ToDebugString());
	}

	return true;
}

/**
 * Unregisters the unique player id as a remote talker
 *
 * @param UniqueId the id of the remote talker
 *
 * @return 0 upon success, an error code otherwise
 */
bool FOnlineVoiceLive::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	// Find them in the talkers array and remove them
	for (int32 Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		const FRemoteTalkerLive& Talker = RemoteTalkers[Index];
		// Is this the remote talker?
		if (*Talker.TalkerId == UniqueId)
		{
			if (OnPlayerTalkingStateChangedDelegates.IsBound() && (Talker.bIsTalking || Talker.bWasTalking))
			{
				OnPlayerTalkingStateChangedDelegates.Broadcast(Talker.TalkerId.ToSharedRef(), false);
			}

			RemoteTalkers.RemoveAtSwap(Index);
			RemoveRemoteConsole(ref new Platform::String(*UniqueId.ToString()));
			UE_LOG(LogVoice, Log, TEXT("Remote talker %s unregistered"), *UniqueId.ToDebugString());
			return true;
		}
	}
	return false;
}

/**
 * Wrapper for the Live call to remove a remote talker
 *
 * @param uniqueIdentifier the identifier of the remote console
 *
 */
void FOnlineVoiceLive::RemoveRemoteConsole(Platform::Object^ uniqueIdentifier)
{
	auto asyncOp = LiveChatManager->RemoveRemoteConsoleAsync(uniqueIdentifier);

	create_task( asyncOp )
		.then( [] ( task<void> t )
	{
		// Error handling
		try
		{
			t.get();
		}
		catch ( Platform::Exception^ ex )
		{
			UE_LOG( LogVoice, Warning, L"RemoveRemoteConsoleAsync failed 0x%x", ex->HResult );
		}
	});
}

/**
 * Iterates the current remote talker list unregistering them with the
 * voice engine and our internal state
 */
void FOnlineVoiceLive::RemoveAllRemoteTalkers()
{
	for(ChatUser^ user : LiveChatManager->GetChatUsers())
	{
		if(!user->IsLocal)
		{
			RemoveRemoteConsole(user->UniqueConsoleIdentifier);
		}
	}

	UE_LOG(LogVoice, Log, TEXT("Removing all remote talkers"));
	// Work backwards through array removing the talkers
	for (int32 Index = RemoteTalkers.Num() - 1; Index >= 0; Index--)
	{
		const FRemoteTalkerLive& Talker = RemoteTalkers[Index];

		if (OnPlayerTalkingStateChangedDelegates.IsBound() && (Talker.bIsTalking || Talker.bWasTalking))
		{
			OnPlayerTalkingStateChangedDelegates.Broadcast(Talker.TalkerId.ToSharedRef(), false);
		}
	}

	// Empty the array now that they are all unregistered
	RemoteTalkers.Empty();
}

/**
 * Finds a remote talker in the cached list
 *
 * @param UniqueId the net id of the player to search for
 *
 * @return pointer to the remote talker or NULL if not found
 */
FRemoteTalkerLive* FOnlineVoiceLive::FindRemoteTalker(const FUniqueNetId& UniqueId)
{
	for (int32 Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		FRemoteTalkerLive& Talker = RemoteTalkers[Index];
		// Compare net ids to see if they match
		if (*Talker.TalkerId == UniqueId)
		{
			return &RemoteTalkers[Index];
		}
	}
	return NULL;
}

/**
 * Checks whether a local user index has a headset present or not
 *
 * @param UserIndex the user to check status for
 *
 * @return true if there is a headset, false otherwise
 */
bool FOnlineVoiceLive::IsHeadsetPresent(uint32 LocalUserNum)
{
	return 	LiveChatManager->HasMicFocus;
}

/**
 * Determines whether a local user index is currently talking or not
 *
 * @param UserIndex the user to check status for
 *
 * @return true if the user is talking, false otherwise
 */
bool FOnlineVoiceLive::IsLocalPlayerTalking(uint32 LocalUserNum)
{
	auto UserId = ref new Platform::String(*IdentityInt->GetUniquePlayerId(LocalUserNum)->ToString());
	for(ChatUser^ user : LiveChatManager->GetChatUsers())
	{
		if(user->IsLocal && CompareUniqueConsoleIdentifiers(user->UniqueConsoleIdentifier,UserId))
		{
			return user->TalkingMode == ChatUserTalkingMode::TalkingOverHeadset || user->TalkingMode == ChatUserTalkingMode::TalkingOverKinect;
		}
	}
	return false;
}

/**
 * Determines whether a remote talker is currently talking or not
 *
 * @param UniqueId the unique id of the talker to check status on
 *
 * @return true if the user is talking, false otherwise
 */
bool FOnlineVoiceLive::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	auto consoleId = ref new Platform::String(*UniqueId.ToString());
	for(ChatUser^ user : LiveChatManager->GetChatUsers())
	{
		if(!user->IsLocal && CompareUniqueConsoleIdentifiers(user->UniqueConsoleIdentifier,consoleId))
		{
			return user->TalkingMode == ChatUserTalkingMode::TalkingOverHeadset || user->TalkingMode == ChatUserTalkingMode::TalkingOverKinect;
		}
	}
	return false;
}

/**
 * Checks that a unique player id is on the specified user's mute list
 *
 * @param LocalUserNum the controller number of the associated user
 * @param UniqueId the id of the player being checked
 *
 * @return true if the specified user is muted, false otherwise
 */
bool FOnlineVoiceLive::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	auto consoleId = ref new Platform::String(*UniqueId.ToString());
	for(ChatUser^ user : LiveChatManager->GetChatUsers())
	{
		if(!user->IsLocal && CompareUniqueConsoleIdentifiers(user->UniqueConsoleIdentifier,consoleId))
		{
			return user->IsMuted;
		}
	}
	return false;
}

/**
 * Mutes a remote talker for the specified local player.
 * NOTE: bIsSystemWide not supported on Live
 *
 * @param LocalUserNum the user that is muting the remote talker
 * @param PlayerId the remote talker that is being muted
 * @param bIsSystemWide whether to try to mute them globally or not
 *
 * @return true if the function succeeds, false otherwise
 */
bool FOnlineVoiceLive::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	return MuteTalkerLive(PlayerId, true);
}

/**
 * Allows a remote talker to talk to the specified local player.
 * NOTE: bIsSystemWide not supported on Live
 *
 * @param LocalUserNum the user that is allowing the remote talker to talk
 * @param PlayerId the remote talker that is being restored to talking
 * @param bIsSystemWide whether to try to unmute them globally or not
 *
 * @return TRUE if the function succeeds, FALSE otherwise
 */
bool FOnlineVoiceLive::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	return MuteTalkerLive(PlayerId, false);
}

/**
 * Re-evaluates the muting list for all local talkers
 */
void FOnlineVoiceLive::ProcessMuteChangeNotification()
{
	// irrelevant to Live
}

/**
 * Convert generic network packet data back into voice data
 *
 * @param Ar archive to read data from
 */
TSharedPtr<FVoicePacket> FOnlineVoiceLive::SerializeRemotePacket(FArchive& Ar)
{
	TSharedPtr<FVoicePacketLive> NewPacket = MakeShared<FVoicePacketLive>();
	NewPacket->Serialize(Ar);

	FScopeLock PacketLock( &RemotePacketsCS );

	if (Ar.IsError() == false && NewPacket->GetBufferSize() > 0)
	{
		RemotePackets.Add(NewPacket);

		return NewPacket;
	}

	return NULL;
}

/**
 * Get the local voice packet intended for send
 *
 * @param LocalUserNum user index voice packet to retrieve
 */
TSharedPtr<FVoicePacket> FOnlineVoiceLive::GetLocalPacket(uint32 LocalUserNum)
{
	TSharedPtr<FVoicePacket> ReturnVoicePacket;
	FScopeLock PacketLock( &LocalPacketsCS );

	// duplicate the local copy of the data and set it on a shared pointer for destruction elsewhere
	if (LocalPackets.Num() > 0)
	{
		// Live doesn't have the same ownership of packets, so we just grab the first packet from the queue to send
		TSharedPtr<FVoicePacket> VoicePacket = LocalPackets[0];
		if (VoicePacket->GetBufferSize() > 0)
		{
			ReturnVoicePacket = VoicePacket;
		}

		LocalPackets.RemoveAt(0,1,false);
	}

	return ReturnVoicePacket;
}

/**
 * Processes any talking delegates that need to be fired off
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void FOnlineVoiceLive::ProcessTalkingDelegates(float DeltaTime)
{
	FScopeLock LocalTalkersLock( &LocalTalkersCS );

	// Fire off any talker notification delegates for local talkers
	for (int32 LocalUserNum = 0; LocalUserNum < LocalTalkers.Num(); LocalUserNum++)
	{
		FLocalTalker& Talker = LocalTalkers[LocalUserNum];

		// Only check players with voice
		if (Talker.bIsRegistered)
		{
			// If the talker was not previously talking, but now is trigger the event
			bool bShouldNotify = !Talker.bWasTalking && Talker.bIsTalking;
			// If the talker was previously talking, but now isn't time delay the event
			if (!bShouldNotify && Talker.bWasTalking)
			{
				Talker.LastNotificationTime -= DeltaTime;
				if (Talker.LastNotificationTime <= 0.f)
				{
					// Clear the flag so it only activates when needed
					Talker.bIsTalking = false;
					Talker.LastNotificationTime = VoiceNotificationDelta;
					bShouldNotify = true;
				}
			}

			if (bShouldNotify)
			{
				// Skip all delegate handling if none are registered
				if (OnPlayerTalkingStateChangedDelegates.IsBound())
				{
					IOnlineIdentityPtr IdentityInt = LiveSubsystem->GetIdentityInterface();
					if (IdentityInt.IsValid())
					{
						TSharedPtr<const FUniqueNetId> UniqueId = IdentityInt->GetUniquePlayerId(LocalUserNum);
						if ( UniqueId.IsValid() )
						{
							OnPlayerTalkingStateChangedDelegates.Broadcast(UniqueId.ToSharedRef(), Talker.bIsTalking);
						}
					}
				}

				Talker.bWasTalking = Talker.bIsTalking;
				UE_LOG(LogVoice, VeryVerbose, TEXT("Trigger %sTALKING"), Talker.bIsTalking ? TEXT("") : TEXT("NOT"));
			}
		}
	}
	// Now check all remote talkers
	for (int32 Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		FRemoteTalkerLive& Talker = RemoteTalkers[Index];

		// If the talker was not previously talking, but now is trigger the event
		bool bShouldNotify = !Talker.bWasTalking && Talker.bIsTalking;
		// If the talker was previously talking, but now isn't time delay the event
		if (!bShouldNotify && Talker.bWasTalking && !Talker.bIsTalking)
		{
			Talker.LastNotificationTime -= DeltaTime;
			if (Talker.LastNotificationTime <= 0.f)
			{
				bShouldNotify = true;
			}
		}

		if (bShouldNotify)
		{
			// Skip all delegate handling if none are registered
			if (OnPlayerTalkingStateChangedDelegates.IsBound())
			{
				OnPlayerTalkingStateChangedDelegates.Broadcast(Talker.TalkerId.ToSharedRef(), Talker.bIsTalking);
			}

			UE_LOG(LogVoice, VeryVerbose, TEXT("Trigger %sTALKING"), Talker.bIsTalking ? TEXT("") : TEXT("NOT"));

			// Clear the flag so it only activates when needed
			Talker.bWasTalking = Talker.bIsTalking;
			Talker.LastNotificationTime = VoiceNotificationDelta;
		}
	}
}

/**
 * Submits network packets to audio system for playback
 */
void FOnlineVoiceLive::ProcessRemoteVoicePackets()
{
	FScopeLock PacketLock( &RemotePacketsCS );

	// Clear the talking state for remote players
	for (int32 Index = 0; Index < RemoteTalkers.Num(); Index++)
	{
		RemoteTalkers[Index].bIsTalking = false;
	}

	// Now process all pending packets from the server
	for (int32 Index = 0; Index < RemotePackets.Num(); Index++)
	{
		TSharedPtr<FVoicePacketLive> VoicePacket = StaticCastSharedPtr<FVoicePacketLive>(RemotePackets[Index]);
		if (VoicePacket.IsValid())
		{
			// Get the size since it is an in/out param
			uint32 VoiceBufferSize = VoicePacket->GetBufferSize();

			check(!LiveSubsystem->IsLocalPlayer(*VoicePacket->Sender));

			// check to see if this packet was meant for us
			if(VoicePacket->Broadcast || LiveSubsystem->IsLocalPlayer(*VoicePacket->Target))
			{
				FRemoteTalkerLive* RemoteTalker = FindRemoteTalker(*VoicePacket->Sender);

				if(!RemoteTalker)
				{
					// if we don't know about this user then add them
					RegisterRemoteTalker(*VoicePacket->Sender);
					RemoteTalker = FindRemoteTalker(*VoicePacket->Sender);
				}

				check(RemoteTalker);

				// ignore this code if this is our first packet, or if the voice packet is non-indexed (likely due to this being a control message)
				if(RemoteTalker->LastPacketIndex != 0 && VoicePacket->PacketIndex != 0)
				{
					if( VoicePacket->PacketIndex != RemoteTalker->LastPacketIndex + 1)
					{
						// this is all UDP, so some packet loss is expected, this printout is just designed to help us spot massive loss/mis-ordering
						if(RemoteTalker->LastPacketIndex < VoicePacket->PacketIndex)
						{
							UE_LOG(LogVoice, Log, TEXT("Missed %d packets, expected %d got %d"), VoicePacket->PacketIndex - RemoteTalker->LastPacketIndex - 1, RemoteTalker->LastPacketIndex + 1, VoicePacket->PacketIndex);
						}
						else
						{
							UE_LOG(LogVoice, Log, TEXT("Received out of order packet, expected %d got %d"), RemoteTalker->LastPacketIndex + 1, VoicePacket->PacketIndex);
						}
					}
				}
				RemoteTalker->LastPacketIndex = VoicePacket->PacketIndex;

				// Submit this packet to the voice engine
				bool bVoiceDataPacket = false;
				{
					Windows::Storage::Streams::IBuffer^ destBuffer = ref new Windows::Storage::Streams::Buffer( VoiceBufferSize );
					byte* destBufferBytes = nullptr;
					GetBufferBytes( destBuffer, &destBufferBytes );
					errno_t err = memcpy_s( destBufferBytes, destBuffer->Capacity, VoicePacket->Buffer.GetData(), VoiceBufferSize );
					check(err == 0);
					destBuffer->Length = VoiceBufferSize;

					Microsoft::Xbox::GameChat::ChatMessageType chatMessageType = LiveChatManager->ProcessIncomingChatMessage(destBuffer, ref new Platform::String(*VoicePacket->Sender->ToString()));
					if(chatMessageType == ChatMessageType::ChatVoiceDataMessage)
					{
						UE_LOG(LogVoice, VeryVerbose, TEXT("Received message of type %s"), chatMessageType.ToString()->Data());
						bVoiceDataPacket = true;
					}
					else
					{
						UE_LOG(LogVoice, Log, TEXT("Received message of type %s"), chatMessageType.ToString()->Data());
					}
				}

				if(bVoiceDataPacket)
				{
					// only set this remote talker to talking if we received a voice packet (as opposed to a control message)
					RemoteTalker->bIsTalking = true;
					RemoteTalker->LastNotificationTime = VoiceNotificationDelta;
					RemoteTalker->ReceivedData = true;
				}
			}
			else
			{
				UE_LOG(LogVoice, VeryVerbose, TEXT("Rejecting message for %s"),*VoicePacket->Target->ToString() );
			}
		}
	}
	// Zero the list without causing a free/realloc
	RemotePackets.Reset();
}

/**
 * Get information about the voice state for display
 */
FString FOnlineVoiceLive::GetVoiceDebugState() const
{
	FString Output;
	TSharedPtr<const FUniqueNetId> UniqueId;

	FScopeLock LocalTalkersLock( &LocalTalkersCS );

	Output += TEXT("Local Talkers:\n");
	for (int32 idx=0; idx < LocalTalkers.Num(); idx++)
	{
		UniqueId = IdentityInt->GetUniquePlayerId(idx);

		const FLocalTalker& Talker = LocalTalkers[idx];
		Output += FString::Printf(TEXT("ID: %s\n Registered: %d\n Networked: %d\n Talking: %d\n "),
			UniqueId.IsValid() ? *UniqueId->ToDebugString() : TEXT("NULL"),
			Talker.bIsRegistered,
			Talker.bHasNetworkedVoice,
			Talker.bIsTalking);
	}

	Output += TEXT("Remote Talkers:\n");
	for (int32 idx=0; idx < RemoteTalkers.Num(); idx++)
	{
		const FRemoteTalkerLive& Talker = RemoteTalkers[idx];
		Output += FString::Printf(TEXT("ID: %s\n IsTalking: %d\n Muted: %s\n"),
			*Talker.TalkerId->ToDebugString(),
			Talker.bIsTalking,
			IsMuted(0, *Talker.TalkerId) ? TEXT("1") : TEXT("0"));

	}

	return Output;
}

void FOnlineVoiceLive::OnOutgoingChatPacketReady(__in Microsoft::Xbox::GameChat::ChatPacketEventArgs^ args )
{
	if(args->ChatMessageType == ChatMessageType::ChatVoiceDataMessage)
	{
		UE_LOG(LogVoice, VeryVerbose, TEXT("OnOutgoingChatPacketReady: %ls"), args->ChatMessageType.ToString()->Data());
	}
	else
	{
		UE_LOG(LogVoice, Log, TEXT("OnOutgoingChatPacketReady: %ls"), args->ChatMessageType.ToString()->Data());
	}

	const int32 RegisteredTalkerIndex = GetFirstRegisteredLocalPlayer();
	if (RegisteredTalkerIndex < 0)
	{
		// We have no registered talkers, so drop this packet?
		return;
	}

	TSharedPtr<FVoicePacketLive> NewPacket = MakeShared<FVoicePacketLive>();

	BYTE* byteBufferPointer;
	GetBufferBytes(args->PacketBuffer, &byteBufferPointer);

	int32 Length = args->PacketBuffer->Length;
	NewPacket->Buffer.AddUninitialized(Length);

	memcpy(NewPacket->Buffer.GetData(),byteBufferPointer,Length);
	NewPacket->Length = Length;

	// Don't use the args->ChatUser for the sender, because Live doesn't always give us one,
	// and for splitscreen to work the Sender needs to be consistent - we use it as the
	// unique console identifier for the GameChat library.
	// Just use the first registered local talker
	// @todo: This probably breaks per-user muting in splitscreen
	NewPacket->Sender = IdentityInt->GetUniquePlayerId(RegisteredTalkerIndex);

	// If we couldn't find a sender, all local users probably signed out.  Dropping the packet
	// for now.
	if (!NewPacket->Sender.IsValid())
	{
		return;
	}

	SetLocalPlayerIsTalking(NewPacket->Sender);

	if(!args->SendPacketToAllConnectedConsoles)
	{
		auto  Target = dynamic_cast<Platform::String^>(args->UniqueTargetConsoleIdentifier);
		NewPacket->Target = MakeShared<FUniqueNetIdLive>(Target->Data());
		NewPacket->Broadcast = false;
	}
	else
	{
		NewPacket->Broadcast = true;
		NewPacket->Target = MakeShared<FUniqueNetIdLive>(TEXT(""));
	}

	NewPacket->Reliable = args->SendReliable;

	if(NewPacket->Broadcast)
	{
		NewPacket->PacketIndex = PacketIndex++;
	}
	else
	{
		// only use the packet index for broadcast packets so that all clients have the same expectation
		// non-broadcast packets will generally be reliable anyway
		NewPacket->PacketIndex = 0;
	}

	check(NewPacket->GetTotalPacketSize() < MAX_VOICE_DATA_SIZE);

	FScopeLock PacketLock( &LocalPacketsCS );

	// Packets are only cleared when a net driver calls ClearVoicePackets.
	// If this isn't happening, packets can stack up indefinitely, so clear
	// out an old null or unreliable one if possible.
	if (PendingLocalPackets.Num() >= MaxBufferedVoicePackets)
	{
		const int32 IndexToRemove = PendingLocalPackets.IndexOfByPredicate([](const TSharedPtr<FVoicePacketLive>& Packet)
		{
			return !Packet.IsValid() || !Packet->IsReliable();
		});

		if (IndexToRemove != INDEX_NONE)
		{
			PendingLocalPackets.RemoveAt(IndexToRemove);
		}
		else
		{
			// No unreliable packets found, the buffer is full of reliable packets. Warn that one will be dropped.
			UE_LOG(LogVoice, Warning, TEXT("FOnlineVoiceLive: PendingLocalPackets buffer reliable overflow - has MaxBufferedVoicePackets (%d) reliable packets queued. Reliable packet will be dropped!"), MaxBufferedVoicePackets);

			PendingLocalPackets.RemoveAt(0);
		}
	}

	ensure(PendingLocalPackets.Num() < MaxBufferedVoicePackets);
	PendingLocalPackets.Add(NewPacket);
}

void FOnlineVoiceLive::HookLiveEvents()
{
	// @ATG_CHANGE : BEGIN UWP LIVE support
	// Double-init is bad.  Make sure we don't do it.
	check(!LiveChatManager);
	// @ATG_CHANGE : END

	LiveChatManager = ref new Microsoft::Xbox::GameChat::ChatManager();

	check(LiveChatManager);

	if(DebugDisplayEnabled)
	{
		LiveChatManager->ChatSettings->DiagnosticsTraceLevel = GameChatDiagnosticsTraceLevel::Verbose;
	}

	// If users have bad reputation, we may want to mute them automatically to bypass Matchmaking XR requirements
	// See XR 068 for more details
	LiveChatManager->ChatSettings->AutoMuteBadReputationUsers = bAutoMuteBadRepRemotePlayers;

	m_tokenOnOutgoingChatPacketReady = LiveChatManager->OnOutgoingChatPacketReady +=
		ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::ChatPacketEventArgs^>(
		[this](Platform::Object^, Microsoft::Xbox::GameChat::ChatPacketEventArgs^ Args)
	{
		// Queue the event to be handled on the game thread
		LiveSubsystem->ExecuteNextTick([this, Args]()
		{
			OnOutgoingChatPacketReady(Args);
		});
	});

	m_tokenOnDebugMessage = LiveChatManager->OnDebugMessage +=
		ref new Windows::Foundation::EventHandler<Microsoft::Xbox::GameChat::DebugMessageEventArgs^>(
		[this] ( Platform::Object^, Microsoft::Xbox::GameChat::DebugMessageEventArgs^ args )
	{
		OnDebugMessageReceived(args);
	});

	m_tokenOnCompareUniqueConsoleIdentifiers = LiveChatManager->OnCompareUniqueConsoleIdentifiers +=
		ref new Microsoft::Xbox::GameChat::CompareUniqueConsoleIdentifiersHandler(
		[this] ( Platform::Object^ obj1, Platform::Object^ obj2 )
	{
		return CompareUniqueConsoleIdentifiers(obj1, obj2);
	});

// @ATG_CHANGE : BEGIN - UWP LIVE support - Compatible wrapper needs implementation here
#if PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support
	Windows::Xbox::System::User::AudioDeviceAdded +=
		ref new Windows::Foundation::EventHandler<Windows::Xbox::System::AudioDeviceAddedEventArgs^>(
		[this]( Platform::Object^, Windows::Xbox::System::AudioDeviceAddedEventArgs^ args)
	{
		OnUserAudioDeviceAdded(args);
	});

	Windows::Xbox::Input::Controller::ControllerPairingChanged +=
		ref new Windows::Foundation::EventHandler<Windows::Xbox::Input::ControllerPairingChangedEventArgs^>(
		[this]( Platform::Object^, Windows::Xbox::Input::ControllerPairingChangedEventArgs^ args)
	{
		OnControllerPairingChanged(args);
	});
// @ATG_CHANGE : BEGIN - UWP LIVE support
#endif // PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support
}

void FOnlineVoiceLive::UnhookLiveEvents()
{
	LiveChatManager->OnOutgoingChatPacketReady -= m_tokenOnOutgoingChatPacketReady;
	LiveChatManager->OnDebugMessage -= m_tokenOnDebugMessage;
	LiveChatManager->OnCompareUniqueConsoleIdentifiers -= m_tokenOnCompareUniqueConsoleIdentifiers;
}

void FOnlineVoiceLive::OnDebugMessageReceived( __in Microsoft::Xbox::GameChat::DebugMessageEventArgs^ Args )
{
	if (Args->ErrorCode == S_OK )
	{
		UE_LOG( LogVoice, Log, L"ChatManager: %s", Args->Message->Data() );
	}
	else
	{
		UE_LOG( LogVoice, Warning, L"ChatManager: %s, error code %x", Args->Message->Data(), Args->ErrorCode);
	}
}

bool FOnlineVoiceLive::CompareUniqueConsoleIdentifiers( __in Platform::Object^ uniqueRemoteConsoleIdentifier1, __in Platform::Object^ uniqueRemoteConsoleIdentifier2 ) const
{
	if (uniqueRemoteConsoleIdentifier1 == nullptr || uniqueRemoteConsoleIdentifier2 == nullptr)
	{
		return false;
	}

	auto  id1 = dynamic_cast<Platform::String^>(uniqueRemoteConsoleIdentifier1);
	auto  id2 = dynamic_cast<Platform::String^>(uniqueRemoteConsoleIdentifier2);
	bool ret = Platform::String::CompareOrdinal(id1,id2) == 0;

	//UE_LOG( LogVoice, Verbose, L"Comparing %s and %s: %s match", id1->Data(), id2->Data(), ret ? TEXT("") : TEXT("no") );

	return ret;
}

void FOnlineVoiceLive::GetBufferBytes( __in Windows::Storage::Streams::IBuffer^ buffer, __out byte** ppOut )
{
	if ( ppOut == nullptr || buffer == nullptr )
	{
		throw ref new Platform::InvalidArgumentException();
	}

	*ppOut = nullptr;

	Microsoft::WRL::ComPtr<IInspectable> srcBufferInspectable(reinterpret_cast<IInspectable*>( buffer ));
	Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> srcBufferByteAccess;
	srcBufferInspectable.As(&srcBufferByteAccess);
	srcBufferByteAccess->Buffer(ppOut);
}

bool FOnlineVoiceLive::MuteTalkerLive(const FUniqueNetId& PlayerId, bool bMute)
{
	auto consoleId = ref new Platform::String(*PlayerId.ToString());
	for(ChatUser^ user : LiveChatManager->GetChatUsers())
	{
		if(CompareUniqueConsoleIdentifiers(user->UniqueConsoleIdentifier,consoleId))
		{
			if(bMute)
			{
				LiveChatManager->MuteUserFromAllChannels(user);
			}
			else
			{
				LiveChatManager->UnmuteUserFromAllChannels(user);
			}
			return true;
		}
	}
	return false;
}

void FOnlineVoiceLive::SetLocalPlayerIsTalking(TSharedPtr<const FUniqueNetId> NewTalkerId)
{
	FScopeLock LocalTalkersLock( &LocalTalkersCS );

	// Find the remote talker and mark them as talking
	for (int32 Index = 0; Index < LocalTalkers.Num(); Index++)
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(Index);

		// Compare the ids
		if (NewTalkerId.IsValid() && LocalUserId.IsValid() && (*NewTalkerId == *LocalUserId))
		{
			FLocalTalker& Talker = LocalTalkers[Index];

			Talker.bIsTalking = true;
			Talker.LastNotificationTime = VoiceNotificationDelta;

			return;
		}
	}
}

int32 FOnlineVoiceLive::GetFirstRegisteredLocalPlayer()
{
	for (int32 Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		if (LocalTalkers[Index].bIsRegistered)
		{
			return Index;
		}
	}

	UE_LOG(LogVoice, Warning, TEXT("Found no local talkers"));

	return -1;
}

int32 FOnlineVoiceLive::GetNumLocalTalkers()
{
	return LocalPackets.Num();
}

void FOnlineVoiceLive::DisplayDebugText()
{
	if(DebugDisplayEnabled && GEngine != nullptr)
	{
		FString Output;
		TSharedPtr<const FUniqueNetId> UniqueId;

		FScopeLock LocalTalkersLock( &LocalTalkersCS );

		for (int32 idx=0; idx < LocalTalkers.Num(); idx++)
		{
			UniqueId = IdentityInt->GetUniquePlayerId(idx);

			if (UniqueId.IsValid())
			{
				const FLocalTalker& Talker = LocalTalkers[idx];

				DisplayUserStatus(UniqueId->ToString(), Talker.bIsTalking);
			}
		}

		for (int32 idx=0; idx < RemoteTalkers.Num(); idx++)
		{
			const FRemoteTalkerLive& Talker = RemoteTalkers[idx];

			DisplayUserStatus(Talker.TalkerId->ToString(), Talker.bIsTalking);
		}

		// live method
		IVectorView<ChatUser^ >^ chatusers = LiveChatManager->GetChatUsers();

		for each(auto user in chatusers)
		{
			DisplayUserStatus(FString(user->XboxUserId->Data()) + FString(TEXT("_1")), user->TalkingMode != ChatUserTalkingMode::NotTalking, user );
		}

	}
}

void FOnlineVoiceLive::DisplayUserStatus(FString talker, bool isTalking, Microsoft::Xbox::GameChat::ChatUser^ user)
{
	uint64 key = FCrc::StrCrc32( *talker );

	FString text = talker + (isTalking ? FString(TEXT(" is talking")) : FString(TEXT(" is not talking")));

	FColor displayColor = FColor::Green;

	if(isTalking)
	{
		displayColor = FColor::White;
	}

	if(user)
	{
		text += FString::Printf(TEXT(" Pending packets %d") , user->NumberOfPendingAudioPacketsToPlay);

		if(user->RestrictionMode != Windows::Xbox::Chat::ChatRestriction::None)
		{
			displayColor = FColor::Red;
			text += FString::Printf(TEXT(" Restriction %d") , (int)user->RestrictionMode);
		}

		if((user->IsLocal && user->IsLocalUserMuted) || (!user->IsLocal && user->IsMuted))
		{
			displayColor = FColor::Blue;
		}

		auto channels = user->GetAllChannels();

		for each(auto channel in channels)
		{
			text += FString::Printf(TEXT(" Channel %d") , channel);
		}
	}

	GEngine->AddOnScreenDebugMessage( key, 0.98f, displayColor, text );

}

// @ATG_CHANGE : BEGIN - UWP LIVE support
Windows::Xbox::Chat::IChatUser^ FOnlineVoiceLive::GetChatUserFromLocalUser(Windows::Xbox::System::User^ user)
{
#if PLATFORM_XBOXONE
	return user;
#else
	return ref new Microsoft::Xbox::ChatAudio::LocalChatUser(user->XboxUserId);
#endif
}
// @ATG_CHANGE : END


// @ATG_CHANGE : BEGIN - UWP LIVE support - Compatible wrapper needs implementation here
#if PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support
void FOnlineVoiceLive::OnUserAudioDeviceAdded(__in Windows::Xbox::System::AudioDeviceAddedEventArgs^ args)
{
	UE_LOG(LogVoice, Log, TEXT("User audio device added"));

	Windows::Xbox::System::User^ user = args->User;
	if( user != nullptr )
	{
		// TODO! This should only be done for the active user
		LiveChatManager->AddLocalUserToChatChannelAsync( 0, user );
	}
}

void FOnlineVoiceLive::OnControllerPairingChanged( __in Windows::Xbox::Input::ControllerPairingChangedEventArgs^ args )
{
	auto controller = args->Controller;
	if ( controller )
	{
		if ( controller->Type == L"Windows.Xbox.Input.Gamepad" )
		{
			// Either add the user or sign one in
			Windows::Xbox::System::User^ user = args->User;
			if ( user != nullptr )
			{
				// TODO! This should only be done for the active user
				LiveChatManager->AddLocalUserToChatChannelAsync( 0, user );
			}
		}
	}
}
// @ATG_CHANGE : BEGIN - UWP LIVE support
#endif // PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support

// @ATG_CHANGE : BEGIN UWP LIVE support
#endif // WITH_GAME_CHAT
// @ATG_CHANGE : END
