// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineAsyncTaskManager.h"

class FUserOnlineAccountLive;

class FOnlineIdentityLive :
	public IOnlineIdentity
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
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) override;
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

	/**
	 * Refresh cached Gamepads and Users when LIVE events fire
	 */
	void RefreshGamepadsAndUsers();

PACKAGE_SCOPE:
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
	Windows::Xbox::System::User^ GetUserForControllerIndex(int32 ControllerIndex) const;

	/**
	 * Helper method to translate a User into a ControllerIndex.
	 *
	 * @param User the user to look up
	 *
	 * @return The controller index associated with the user, or -1 if not found.
	 */
	int32 GetControllerIndexForUser(Windows::Xbox::System::User^ InUser) const;

	/**
	 * Helper method to translate an ID into a ControllerIndex.
	 *
	 * @param PlayerId the user to look up
	 *
	 * @return The controller index associated with the user, or -1 if not found.
	 */
	int32 GetControllerIndexForId(const FUniqueNetId& PlayerId) const;

	/**
	 * Helper method to get the current cached list of users
	 *
	 * @return The cached list of users
	 */
	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ GetCachedUsers() const;

	/**
	 * Callback for handling the Controller connection / disconnection
	 *
	 * @param Connected true for a connection, false for a disconnection.
	 * @param UserID the user ID affected by the connection change (-1 for disconnects)
	 * @param ControllerId the ID for the controller that triggered the event
	 */
	void OnControllerConnectionChange( bool Connected, int32 UserId, int32 ControllerId);

private:
	/**
	 * Async event that notifies when a user has been added. Using a task for this because
	 * we need the delegates to be executed on the game thread.
	 */
	class FAsyncEventUserAdded : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
	private:
		Windows::Xbox::System::UserAddedEventArgs^ Args;

	public:
		FAsyncEventUserAdded(FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::UserAddedEventArgs^ InArgs);

		virtual void Finalize() override;
		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/**
	 * Async event that notifies when a user has been added. Using a task for this because
	 * we need the delegates to be executed on the game thread.
	 */
	class FAsyncEventUserRemoved : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
	private:
		Windows::Xbox::System::UserRemovedEventArgs^ Args;

	public:
		FAsyncEventUserRemoved(FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::UserRemovedEventArgs^ InArgs);

		virtual void Finalize() override;
		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/**
	 * Async event that notifies when a user has been added. Using a task for this because
	 * we need the delegates to be executed on the game thread.
	 */
	class FAsyncEventControllerPairingChanged : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
	private:
		Windows::Xbox::Input::ControllerPairingChangedEventArgs^ Args;

	public:
		FAsyncEventControllerPairingChanged(FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::Input::ControllerPairingChangedEventArgs^ InArgs);

		virtual void Finalize() override;
		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/** Cached list of users */
	Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ CachedUsers;

	/** Lock for updating/reading CachedUsers vector */
	mutable FCriticalSection CachedUsersLock;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserRemoved;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserAdded;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenControllerPairingChanged;

	/** Stored delegate handle to remove the task later */
	FDelegateHandle ControllerConnectionChanged;

PACKAGE_SCOPE:
	FString LoginXSTSEndpoint;

private:
	/** Map of online user accounts (using user id as key) */
	TMap<FUniqueNetIdLive, TSharedPtr<FUserOnlineAccount> > OnlineUsers;

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
