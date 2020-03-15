// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "Misc/Guid.h"

// @ATG_CHANGE : UWP LIVE support - moved platform specific includes to pch
#include "OnlineSubsystemLivePrivatePCH.h"

#include "OnlineSubsystemLivePackage.h"
#include "OnlineSessionSettings.h"

// @ATG_CHANGE : BEGIN
using namespace Windows::Foundation::Collections;
// @ATG_CHANGE : END

class FOnlineSessionSearch;

// This is token used for LIVE delegates.  Please make sure to properly remove LIVE callbacks on cleanups
typedef Windows::Foundation::EventRegistrationToken LiveEventToken;

const TCHAR* const INVALID_NETID = TEXT("INVALID");

/** 
 * LIVE Unique Id implementation - wrapping XUID
 */
class FUniqueNetIdLive
	: public FUniqueNetIdString
{
PACKAGE_SCOPE:
	/** Hidden on purpose */
	FUniqueNetIdLive()
		: FUniqueNetIdString(INVALID_NETID)
	{
	}

public:
	virtual ~FUniqueNetIdLive() = default;
	FUniqueNetIdLive(const FUniqueNetIdLive&) = default;
	FUniqueNetIdLive(FUniqueNetIdLive&&) = default;
	FUniqueNetIdLive& operator=(const FUniqueNetIdLive&) = default;
	FUniqueNetIdLive& operator=(FUniqueNetIdLive&&) = default;

	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdLive(const FString& InUniqueNetId)
		: FUniqueNetIdString(InUniqueNetId)
	{
	}

	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdLive(FString&& InUniqueNetId)
		: FUniqueNetIdString(MoveTemp(InUniqueNetId))
	{
	}

	/**
	 * Constructs this object with a FUniqueNetId
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdLive(const FUniqueNetId& Src)
		: FUniqueNetIdString(Src.ToString())
	{
	}

	/**
	 * Construct from Platform String
	 *
	 * @param Src the id as a platform string
	 */
	explicit FUniqueNetIdLive(Platform::String^ Src)
		: FUniqueNetIdString(FString(Src == nullptr ? INVALID_NETID : Src->Data()))
	{
	}

	/** Is our structure currently pointing to a valid XUID? */
	virtual bool IsValid() const override
	{
		static const FString InvalidId(INVALID_NETID);
		return !UniqueNetIdStr.Equals(InvalidId, ESearchCase::CaseSensitive);
	}

	friend FORCEINLINE uint32 GetTypeHash(const FUniqueNetIdLive& Id)
	{
		const FUniqueNetIdString& SuperId = Id;
		return GetTypeHash(SuperId);
	}
};

/** State of a match ticket */
namespace EOnlineLiveMatchmakingState
{
	enum Type
	{
		None,
		CreatingMatchSession,
		SubmittingInitialTicket,
		WaitingForGameSession,
		Active,
		UserCancelled
	};
}

/** 
 * LIVE Match ticket wrapper
 */
class FOnlineMatchTicketInfo
{
public:
	FOnlineMatchTicketInfo()
		: EstimatedWaitTimeInSeconds(0.0f) 
	{
	}

	FOnlineMatchTicketInfo(
		const FString& InHopperName,
		const FString& InTicketID,
		float InWaitTimeInSeconds
	) 
		: HopperName( InHopperName )
		, TicketId( InTicketID )
		, EstimatedWaitTimeInSeconds( InWaitTimeInSeconds )
		, MatchmakingState(EOnlineLiveMatchmakingState::None)
	{
	}

	FString	HopperName;
	FString	TicketId;
	float	EstimatedWaitTimeInSeconds;

private:
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ SessionReference;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LastDiffedLiveSession;

	/** Easy access to the hosting User^ of this session (will be null on clients) */
	Windows::Xbox::System::User^ HostUser;

public:
	FName SessionName;
	FOnlineSessionSettings SessionSettings;

	/** The state of the matchmaking process, will be None for non-matchmaking (custom) sessions */
	EOnlineLiveMatchmakingState::Type MatchmakingState;
	
	/** Search settings are used during match ticket resubmission */
	TSharedPtr<FOnlineSessionSearch> SessionSearch;


	Windows::Xbox::System::User^ GetHostUser() const
	{
		return HostUser;
	}

	void SetHostUser(Windows::Xbox::System::User^ InHostUser)
	{
		HostUser = InHostUser;
	}

	/** Returns the Xbox-specific match session pointer. */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLiveSession() const
	{
		return LiveSession;
	}

	/** Returns the Xbox-specific match session reference pointer. */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ GetLiveSessionRef() const
	{
		return SessionReference;
	}

	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLastDiffedSession() const
	{
		return LastDiffedLiveSession;
	}

	/** Stores the last game session seen in OnSessionChanged. */
	void SetLastDiffedSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
	{
		LastDiffedLiveSession = LatestSession;
	}
	
	void RefreshLiveInfo(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
	{
		LiveSession = LatestSession;
		if (LatestSession)
		{
			SessionReference = LatestSession->SessionReference;
		}

		if (!LastDiffedLiveSession)
		{
			LastDiffedLiveSession = LatestSession;
		}
	}

	void RefreshLiveInfo(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LatestSession)
	{
		LiveSession = nullptr;
		LastDiffedLiveSession = nullptr;
		if (LatestSession)
		{
			SessionReference = LatestSession;
		}
	}
};

typedef TSharedPtr<FOnlineMatchTicketInfo> FOnlineMatchTicketInfoPtr;

/** 
 * LIVE Session information wrapper, as well as convenient place to store Host Addr as well as map of xuid-FOnlineAssociateLive
 */
class FOnlineSessionInfoLive : public FOnlineSessionInfo
{

public:
	/**
	 * Constructor
	 */
	FOnlineSessionInfoLive() 
		: FOnlineSessionInfo()
		, LiveSession( nullptr )
		, LiveSessionRef( nullptr )
		, LastDiffedGameSession( nullptr )
		, Association(nullptr)
		, IsReady(false)
	{
	}

	/**
	 * Constructor
	 *
	 * @param InLiveSessionRef The session reference corresponding to this object
	 */
	FOnlineSessionInfoLive(	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InLiveSessionRef ) 
		: FOnlineSessionInfo()
		, LiveSession( nullptr )
		, LiveSessionRef( InLiveSessionRef )
		, LastDiffedGameSession( nullptr )
		, Association(nullptr)
		, IsReady(false)
	{
	}

	/**
	 * Constructor
	 *
	 * @param InLiveSessionRef The session reference corresponding to this object
	 */
	// @ATG_CHANGE :  BEGIN - VS 2017 fix
	FOnlineSessionInfoLive(	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^	InLiveSession ) 
	// @ATG_CHANGE :  END
		: FOnlineSessionInfo()
		, LiveSession( InLiveSession )
		, LastDiffedGameSession( InLiveSession )
		, Association(nullptr)
		, IsReady(false)
	{
		if( InLiveSession )
		{
			LiveSessionRef = InLiveSession->SessionReference;
			SessionId = FUniqueNetIdString(FString(LiveSessionRef->ToUriPath()->Data()));
		}
	}

	/** Destructor */
	virtual ~FOnlineSessionInfoLive() 
	{
	}

	virtual const uint8* GetBytes() const override
	{
		check(false);  // NOTIMPL for XboxOne
		return NULL;
	}

	virtual int32 GetSize() const override
	{
		return sizeof(FOnlineSessionInfoLive);
	}

	virtual bool IsValid() const override
	{
		return ( LiveSessionRef != nullptr );
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("[Game session ref: %s]"),
			LiveSessionRef == nullptr ? TEXT("NULL") : LiveSessionRef->SessionName->Data());
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("[Game session ref: %s]"),
			LiveSessionRef == nullptr ? TEXT("NULL") : LiveSessionRef->SessionName->Data());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return SessionId;
	}

	//
	// XboxOne Platform specific APIs
	//

	/** Returns the Xbox-specific session pointer. */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLiveMultiplayerSession() const
	{
		return LiveSession;
	}

	/** Returns the Xbox-specific session reference pointer. */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ GetLiveMultiplayerSessionRef() const
	{
		return LiveSessionRef;
	}

	/**
	 * Updates the cached session and session reference pointers with a new session.
	 * Useful after WriteSessionAsync operations because the old sessions are no longer valid.
	 *
	 * @param LatestSession The new session to replace the cached version with.
	 */
	void RefreshLiveInfo(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
	{
		LiveSession = LatestSession;
		if (LatestSession)
		{
			LiveSessionRef = LatestSession->SessionReference;
			SessionId = FUniqueNetIdString(FString(LiveSessionRef->ToUriPath()->Data()));
		}
		else
		{
			LiveSessionRef = nullptr;
			SessionId = FUniqueNetIdString();
		}

		if (!LastDiffedGameSession)
		{
			LastDiffedGameSession = LatestSession;
		}
	}

	// The design below feels kind of ugly, but we need a way to explicitly keep track of the last version
	// of the sessions seen by OnSessionChanged, so we can ensure all changes are detected the next time
	// the session subscription fires. We can't use GetLiveMultiplayerSession/GetLiveMatchSession for this,
	// because processing a session change may itself update these, and cause changes made on the server to
	// be missed when the next diff is run.

	/** Returns the last game session seen in OnSessionChanged. */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ GetLastDiffedMultiplayerSession() const
	{
		return LastDiffedGameSession;
	}

	/** Stores the last game session seen in OnSessionChanged. */
	void SetLastDiffedMultiplayerSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LatestSession)
	{
		LastDiffedGameSession = LatestSession;
	}

	/** Returns the address of the host of this session. */
	TSharedPtr<class FInternetAddr> GetHostAddr() const
	{
		return HostAddr;
	}

	/** Sets the address of the host of this session. */
	void SetHostAddr(TSharedPtr<class FInternetAddr> InAddr)
	{
		HostAddr = InAddr;
	}

	/** Set the association for the session */
	void SetAssociation(Windows::Xbox::Networking::SecureDeviceAssociation^ InAssociation)
	{
		Association = InAssociation;
	}

	/** Returns the association for the session */
	Windows::Xbox::Networking::SecureDeviceAssociation^ GetAssociation() const
	{
		return Association;
	}

	FGuid GetRoundId() const
	{
		return RoundId;
	}

	void SetRoundId(const FGuid& InRoundId)
	{
		RoundId = InRoundId;
	}

	bool IsSessionReady() const 
	{
		return IsReady;
	}

	void SetSessionReady() 
	{
		IsReady = true;
	}

	// @ATG_CHANGE : BEGIN Adding XIM
	const WCHAR* GetMultiplayerCorrelationId()
	{
		return LiveSession != nullptr ? LiveSession->MultiplayerCorrelationId->Data() : nullptr;
	}
	// @ATG_CHANGE : END

	void SetSessionInviteHandle(const FString& InSessionHandle)
	{
		SessionInviteHandle = InSessionHandle;
	}

	void SetSessionInviteHandle(FString&& InSessionHandle)
	{
		SessionInviteHandle = MoveTemp(InSessionHandle);
	}

	const TOptional<FString>& GetSessionInviteHandle() const
	{
		return SessionInviteHandle;
	}

private:

	bool IsReady;

	/** Wrapped LIVE session */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LiveSession;

	/** Wrapped LIVE session reference */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ LiveSessionRef;

	/** Last version of LIVE game session compared in OnSessionChanged */
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ LastDiffedGameSession;

	/** Secure device association for the session */
	Windows::Xbox::Networking::SecureDeviceAssociation^ Association;
	
	/** Host address */
	TSharedPtr<class FInternetAddr> HostAddr;

	/** Placeholder unique id */
	FUniqueNetIdString SessionId;

	/** Stored RoundId, used when triggering Xbox events */
	FGuid RoundId;

	/** Session handle if this session is from an to-be-joined invite */
	TOptional<FString> SessionInviteHandle;
};

typedef TSharedPtr<FOnlineSessionInfoLive> FOnlineSessionInfoLivePtr;

static const int32 XBOX_MAX_PLAYER_NAME_LENGTH = 16;

class FOnlineUserInfoLive
	: public FOnlineUser
{
public:
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		return FilterPlayerName(UserProfile->GameDisplayName);
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return FilterPlayerName(UserProfile->GameDisplayName);
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		const FString* const FoundUserAttribute = UserAttributes.Find(AttrName);
		if (FoundUserAttribute == nullptr)
		{
			OutAttrValue.Empty();
			return false;
		}

		OutAttrValue = *FoundUserAttribute;
		return true;
	}

PACKAGE_SCOPE:
	FOnlineUserInfoLive(Microsoft::Xbox::Services::Social::XboxUserProfile^ InUserProfile)
		: UserProfile(InUserProfile)
		, UserId(MakeShared<FUniqueNetIdLive>(InUserProfile->XboxUserId))
	{
		check(UserProfile);

		UserAttributes.Emplace(FString(TEXT("Gamerscore")), FString(UserProfile->Gamerscore->Data()));
		// This is a the URI to a resizeable display image for the user.  For example, &format=png&w=64&h=64
		// Valid Format: png
		// Valid Width/Height: 64/64, 208/208, or 424/424
		UserAttributes.Emplace(FString(TEXT("DisplayPictureUri")), FString(UserProfile->GameDisplayPictureResizeUri->AbsoluteCanonicalUri->Data()));
	}

	virtual ~FOnlineUserInfoLive() = default;

	static FString FilterPlayerName(Platform::String^ InPlayerName)
	{
		FString OutName = InPlayerName->Data();

		// If our name exceeds the max length, we want to truncate it
		const int32 SizeOverage = OutName.Len() - XBOX_MAX_PLAYER_NAME_LENGTH;
		if (SizeOverage > 0)
		{
			// Truncate in-place to max name length
			OutName.RemoveAt(XBOX_MAX_PLAYER_NAME_LENGTH, SizeOverage, false);
			// Append ellipsis character to show the name goes on
			OutName.AppendChar(L'\u2026');
		}

		return OutName;
	}

PACKAGE_SCOPE:
	Microsoft::Xbox::Services::Social::XboxUserProfile^ UserProfile;

	TSharedRef<const FUniqueNetIdLive> UserId;

	TMap<FString, FString> UserAttributes;
};