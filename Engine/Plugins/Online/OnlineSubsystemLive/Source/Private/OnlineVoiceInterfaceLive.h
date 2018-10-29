// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"
#include "VoicePacketLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLivePackage.h"

// @ATG_CHANGE : BEGIN - UWP LIVE support
#if WITH_GAME_CHAT

#if PLATFORM_XBOXONE
namespace Windows
{
	namespace Xbox
	{
		namespace Chat
		{
			using IChatUser = ::Windows::Xbox::System::IUser;
		}
	}
}

#elif PLATFORM_UWP
namespace Microsoft
{ 
	namespace Xbox
	{
		namespace GameChat
		{
			using ChatUser = GameChatUser;
		}
	}
}

namespace Windows
{
	namespace Xbox
	{
		namespace Chat
		{
			using IChatUser = ::Microsoft::Xbox::ChatAudio::IChatUser;
		}
	}
}
#endif
// @ATG_CHANGE : END

typedef TArray< TSharedPtr<FVoicePacketLive> > FLiveVoicePacketArray;


struct FRemoteTalkerLive : public FRemoteTalker
{
	bool	ReceivedData;
	uint32	LastPacketIndex;

	/** Constructor */
	FRemoteTalkerLive() : 
		FRemoteTalker(),
		ReceivedData(false),
		LastPacketIndex(0)
	{}
};

/**
 * The Live implementation of the voice interface 
 */
class FOnlineVoiceLive : public IOnlineVoice
{
	/** Reference to the main Live subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;
	/** Reference to the profile interface */
	class FOnlineIdentityLive* IdentityInt;

	/** Maximum permitted local talkers */
	int32 MaxLocalTalkers;
	/** Maximum permitted remote talkers */
	int32 MaxRemoteTalkers;

	/** State of all possible local talkers */
	TArray<FLocalTalker> LocalTalkers;
	/** State of all possible remote talkers */
	TArray<FRemoteTalkerLive> RemoteTalkers;

	/** Time to wait for new data before triggering "not talking" */
	float VoiceNotificationDelta;

	/** Buffered voice data I/O */
	FLiveVoicePacketArray	PendingLocalPackets;
	FLiveVoicePacketArray	LocalPackets;
	FLiveVoicePacketArray	RemotePackets;

	/** Critical sections for threadsafeness */
	FCriticalSection			LocalPacketsCS;
	FCriticalSection			RemotePacketsCS;
	mutable FCriticalSection	LocalTalkersCS;

	/** Turn on or off debug display/verbose output **/
	bool						DebugDisplayEnabled;

	/** Next packet index to send. Only used to track dropped/misordered packets **/
	uint32						PacketIndex;

	/** Automatically mute remote talkers if they have bad reputation */
	bool bAutoMuteBadRepRemotePlayers;

	/** Internal Live pointers */
	Microsoft::Xbox::GameChat::ChatManager^ LiveChatManager;
	Windows::Foundation::EventRegistrationToken m_tokenOnOutgoingChatPacketReady;
	Windows::Foundation::EventRegistrationToken m_tokenOnDebugMessage;
	Windows::Foundation::EventRegistrationToken m_tokenOnCompareUniqueConsoleIdentifiers;


	/**
	 * Finds a remote talker in the cached list
	 *
	 * @param UniqueId the net id of the player to search for
	 *
	 * @return pointer to the remote talker or NULL if not found
	 */
	struct FRemoteTalkerLive* FindRemoteTalker(const FUniqueNetId& UniqueId);

	/**
	 * Hook up the XBL callbacks
	 *
	 */
	void HookLiveEvents();

	/**
	 * Clean up the XBL Callbacks
	 *
	 */
	void UnhookLiveEvents();
	
	/**
	 * Xbox Live callback saying a packet is ready to send
	 *
	 * @param args The outgoing packet from Live
	 */
	void OnOutgoingChatPacketReady(__in Microsoft::Xbox::GameChat::ChatPacketEventArgs^ args );

	/**
	 * XBL Callback to notify us of a message from the system
	 *
	 * @param Args the message received
	 */
	void OnDebugMessageReceived( __in Microsoft::Xbox::GameChat::DebugMessageEventArgs ^ args );

	/**
	 * Callback that compares two console identifiers, we use XUIDs
	 *
	 * @param uniqueRemoteConsoleIdentifier1 the first console to compare
	 * @param uniqueRemoteConsoleIdentifier2 the second console to compare
	 */
	bool CompareUniqueConsoleIdentifiers( __in Platform::Object^ uniqueRemoteConsoleIdentifier1, __in Platform::Object^ uniqueRemoteConsoleIdentifier2 ) const;
	
// @ATG_CHANGE : BEGIN - UWP LIVE support - Compatible wrapper needs implementation here
#if PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support
	/**
	 * Deal with headsets being added or removed
	 */
	void OnUserAudioDeviceAdded(__in Windows::Xbox::System::AudioDeviceAddedEventArgs^ args);
// @ATG_CHANGE : BEGIN - UWP LIVE support
#endif // PLATFORM_XBOXONE
// @ATG_CHANGE : END - UWP LIVE support
	
	/**
	 * Deal with internal User changes, e.g. pad disconnect/reconnect
	 */
	void OnControllerPairingChanged( __in Windows::Xbox::Input::ControllerPairingChangedEventArgs^ args );

	void RemoveRemoteConsole(Platform::Object^ uniqueIdentifier);

	/**
	 * Extract a byte buffer from a stream
	 *
	 * @param buffer the stream to inspect
	 * @param ppOut the content of the stream as a byte array
	 */
	void GetBufferBytes( __in Windows::Storage::Streams::IBuffer^ buffer, __out byte** ppOut );

	/**
	 * Tell Live to mute/unmute a player. Works on local or remote players
	 *
	 * @param PlayerId player to mute
	 * @param bMute true to mute, false to unmute
	 */
	bool MuteTalkerLive(const FUniqueNetId& PlayerId, bool mute);

	/**
	 * Mark a local player as being talking
	 *
	 * @param NewTalkerId player who is talking
	 */
	void SetLocalPlayerIsTalking(TSharedPtr<const FUniqueNetId> talker);

	/**
	 * Get the first registered local player
	 *
	 * @return First registered local player
	 */
	int32 GetFirstRegisteredLocalPlayer();

	/**
	 * Display all talkers and their status
	 */
	void DisplayDebugText();
	
	/**
	 * Display an individual user
	 */
	void DisplayUserStatus(FString talker, bool isTalking, Microsoft::Xbox::GameChat::ChatUser^ user = nullptr);

	// @ATG_CHANGE : BEGIN - UWP LIVE support
	Windows::Xbox::Chat::IChatUser^ GetChatUserFromLocalUser(Windows::Xbox::System::User^ user);
	// @ATG_CHANGE : END
PACKAGE_SCOPE:
	/** Constructor */
	FOnlineVoiceLive() :
		LiveSubsystem(nullptr),
		IdentityInt(nullptr),
		MaxLocalTalkers(MAX_SPLITSCREEN_TALKERS),
		MaxRemoteTalkers(MAX_REMOTE_TALKERS),
		VoiceNotificationDelta(0.0f)
	{};

	/**
	 * Processes any talking delegates that need to be fired off
	 *
	 * @param DeltaTime the amount of time that has elapsed since the last tick
	 */
	void ProcessTalkingDelegates(float DeltaTime);

	/**
	 * Submits network packets to audio system for playback
	 */
	void ProcessRemoteVoicePackets();

	/**
	 * Re-evaluates the muting list for all local talkers
	 */
	void ProcessMuteChangeNotification() override;

	/**
	 * Initialize the voice interface
	 */
	virtual bool Init() override;

public:

	/** Constructor */
	FOnlineVoiceLive(class FOnlineSubsystemLive* InLiveSubsystem);

	/** Virtual destructor to force proper child cleanup */
	virtual ~FOnlineVoiceLive();

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
	bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override;
	virtual int32 GetNumLocalTalkers() override;
	virtual void ClearVoicePackets() override;
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;
};

typedef TSharedPtr<FOnlineVoiceLive, ESPMode::ThreadSafe> FOnlineVoiceLivePtr;

// @ATG_CHANGE : BEGIN UWP LIVE support
#endif // WITH_GAME_CHAT
// @ATG_CHANGE : END
