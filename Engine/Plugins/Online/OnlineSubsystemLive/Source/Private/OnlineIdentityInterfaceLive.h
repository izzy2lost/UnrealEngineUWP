// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManager.h"

class FUserOnlineAccountLive;

class FOnlineIdentityLive
	: public IOnlineIdentity
	, public TSharedFromThis<FOnlineIdentityLive, ESPMode::ThreadSafe>
{
PACKAGE_SCOPE:

	/** Constructor
	 *
	 * @param InSubsystem The owner of this identity interface.
	 */
	explicit FOnlineIdentityLive(class FOnlineSubsystemLive* InSubsystem);

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

public:

	virtual ~FOnlineIdentityLive();

	// IOnlineIdentity

	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> GetSponsorUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;

	// @ATG_CHANGE : BEGIN - Support storing multiple tokens for different remote endpoints
	/**
	* Sets a user's XSTS token (adding the user to the internal map if they're not already in it)
	*/
	void SetUserXSTSToken(Windows::Xbox::System::User^ User, const FString& EndPointURL, const FString& AuthToken);
	// @ATG_CHANGE : END

private:

	/**
	 * Sets up event handlers for Xbox system events about users changing so that the
	 * user cache is updated accordingly
	 */
	void HookLiveEvents();

	/**
	 * Removes the event handlers that were set up in HookLiveEvents()
	 */
	void UnhookLiveEvents();

	/**
	 * Delegate called when app resumes from suspend.
	 */
	void HandleAppResume();

PACKAGE_SCOPE:
	/**
	 * Refresh cached Gamepads and Users when LIVE events fire
	 */
	void RefreshGamepadsAndUsers();

	/**
	 * Searches the cache of User^s for one that matches the UniqueId and returns it if found.
	 *
	 * @param UniqueId The unique net id to look for.
	 * @return A User^ matching UniqueId if found, nullptr if a user was not found.
	 */
	Windows::Xbox::System::User^ GetUserForUniqueNetId(const FUniqueNetIdLive& UniqueId) const;

	/**
	 * Helper method to translate Xbox One Controller Index request to User
	 *
	 * @param ControllerIndex the controller index to use
	 *
	 * @return The User^ associated with the controller index, or nullptr if no users are found
	 */
	Windows::Xbox::System::User^ GetUserForPlatformUserId(int32 ControllerIndex) const;

	/**
	 * Helper method to translate an Xbox User^ to PlatformUserId
	 *
	 * @param InUser the user to look up
	 *
	 * @return The platform user id associated with the user, or PLATFORMUSERID_NONE if not found.
	 */
	FPlatformUserId GetPlatformUserIdFromXboxUser(Windows::Xbox::System::User^ InUser) const;

	/**
	 * Helper method to get the current cached list of users
	 *
	 * @return The cached list of users
	 */
	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ GetCachedUsers() const;

private:

	/**
	 * Hook into engine initialization completion to add input delegates
	 */
	void OnEngineInitComplete();

	/** Create an FUserOnlineAccount for an Xbox User^ */
	bool AddUserAccount(Windows::Xbox::System::User^ InUser);

	/** Remove an FUserOnlineAccount for an Xbox User^ */
	bool RemoveUserAccount(Windows::Xbox::System::User^ InUser);

	/**
	 * Delegate fired when the input system adds a new user
	 *
	 * @param InUserAdded pointer to Xbox user added to the system
	 */
	void OnUserAdded(Windows::Xbox::System::User^ InUserAdded);

	/**
	 * Delegate fired when the input system adds a new user
	 *
	 * @param InUserRemoved pointer to Xbox user removed from the system
	 */
	void OnUserRemoved(Windows::Xbox::System::User^ InUserRemoved);

	/**
	* Callback for handling the Controller connection / disconnection
	*
	* @param Connected true for a connection, false for a disconnection.
	* @param UserID the user ID affected by the connection change (-1 for disconnects)
	* @param ControllerId the ID for the controller that triggered the event
	*/
	void OnControllerConnectionChange(bool Connected, int32 UserId, int32 ControllerId);

	/**
	 * Delegate fired when the input system notes a controller pairing change
	 * Can be multiple firings for one "action"  (olduser->null) then (null->newuser)
	 * User can be the same user (when connecting the USB cable for instance)
	 *
	 * @param InControllerIndex the controller index from the input system
	 * @param InNewUserId platform user id for the new pairing (can be invalid)
	 * @param InOldUserId platform user id for the old pairing (can be invalid)
	 */
	void OnControllerPairingChange(int32 InControllerIndex, FPlatformUserId InNewUserId, FPlatformUserId InOldUserId);

	/**
	 * Callback for querying the bOverallReputationIsBad state for our local users.
	 * If the user's bool is true, they are considered a bad user.
	 * 
	 * @param UserIsBadMap A Map of live net ids to a bool if they are bad or not
	 */
	void OnReputationQueryComplete(const TMap<const FUniqueNetIdLive, bool>& UserIsBadMap);

	/** Cached list of users */
	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ CachedUsers;

// @ATG_CHANGE : BEGIN UWP support
#if PLATFORM_UWP
	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserRemoved;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserAdded;
#endif
// @ATG_CHANGE : END

	/** Lock for updating/reading CachedUsers vector */
	mutable FCriticalSection CachedUsersLock;

	/** Stored delegate handle to remove the task later */
	FDelegateHandle AppInitComplete;
	FDelegateHandle UserAdded;
	FDelegateHandle UserRemoved;
	FDelegateHandle ControllerConnectionChanged;
	FDelegateHandle ControllerPairingChanged;

PACKAGE_SCOPE:
	FString LoginXSTSEndpoint;

private:

	FDelegateHandle AppResumeDelegateHandle;

	/** Map of online user accounts (using user id as key) */
	typedef TMap<FUniqueNetIdLive, TSharedRef<FUserOnlineAccountLive> > LiveUserAccountMap;
	LiveUserAccountMap OnlineUsers;

};

class FUserOnlineAccountLive :
	public FUserOnlineAccount
{
public:

	// FUserOnlineAccount
	/**
	* @return Access token which is provided to user once authenticated by the online service
	*/
	virtual FString GetAccessToken() const override;
	/**
	* @return Any additional auth data associated with a registered user
	*/
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	/**
	* @return True, if the data has been changed
	*/
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override;

	// FOnlineUser
	/** Id associated with the user account provided by the online service during registration */
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	/** Real name for the user if known */
	virtual FString GetRealName() const override;
	/** Nickname of the user if known */
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	/** Additional user data associated with a registered user */
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	/** Sets the user's access token, used to verify their authentication */
	void SetAccessToken(const FString& AuthToken);

	/** Set if this user has bad reputation or not */
	void SetBadReputation(const bool bIsBadReputation);

	/** Check if this user has his reputation state set, and if so, what the value is */
	TOptional<bool> GetIsBadReputation() const;

	/**
	 * Init/default constructor
	 */
	FUserOnlineAccountLive(Windows::Xbox::System::User^ InUser)
		: UserData(InUser)
		, UserId(new FUniqueNetIdLive(InUser->XboxUserId))
	{
		// Store our XUID as 'id' for Epic login code purposes
		// On other platforms, this isn't always just our FUniqueNetId.ToString(), so
		// we just follow convention
		UserAttributes.Emplace(TEXT("id"), UserId->ToString());
	}

	/**
	 * Destructor
	 */
	virtual ~FUserOnlineAccountLive() = default;

private:
	Windows::Xbox::System::User^ UserData;
	TMap<FString, FString> UserAttributes;
	TSharedRef<const FUniqueNetIdLive> UserId;
	FString UserXSTSToken;
};

typedef TSharedPtr<class FOnlineIdentityLive, ESPMode::ThreadSafe> FOnlineIdentityLivePtr;
