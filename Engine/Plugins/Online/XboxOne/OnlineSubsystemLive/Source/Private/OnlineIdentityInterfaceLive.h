// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemLivePackage.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLive.h"
#include "OnlineAsyncTaskManager.h"

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

#if !PLATFORM_UWP
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
// @ATG_CHANGE : BEGIN UWP LIVE support
#endif
	/** Cached list of users */
	mutable Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ CachedUsers;

// @ATG_CHANGE : END

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserRemoved;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenUserAdded;

	/** Stored token used to remove the task later */
	Windows::Foundation::EventRegistrationToken TaskTokenControllerPairingChanged;
};

typedef TSharedPtr<FOnlineIdentityLive> FOnlineIdenityLivePtr;
