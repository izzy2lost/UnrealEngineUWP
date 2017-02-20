// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemLive.h"
#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"

/** 
 * Implementation for the Live external UIs
 */
class FOnlineExternalUILive : public IOnlineExternalUI
{
private:
	/**
	 *	Async event that notifies when the Live account picker has been closed
	 */
	class FAsyncEventAccountPickerClosed : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventAccountPickerClosed() :
			FOnlineAsyncEvent(NULL),
			SignedInUser(nullptr)
		{
		}

		/** The user that signed in through the account picker. Will be null if no user signed in. */
		Windows::Xbox::System::IUser^ SignedInUser;

		/** The controller index corresponding to the controller with which the user has signed in. */
		int ControllerIndex;

		/** The delegate to execute when the account picker is closed. */
		FOnLoginUIClosedDelegate Delegate;

	public:

		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InUser The user that signed in through the account picker, if any. Can be null.
		 * @param InControllerIndex The controller that was used to sign in.
		 * @param InDelegate The delegate to execute on the game thread.
		 */
		FAsyncEventAccountPickerClosed(FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::IUser^ InUser, const int InControllerIndex, const FOnLoginUIClosedDelegate& InDelegate) :
			FOnlineAsyncEvent(InLiveSubsystem),
			SignedInUser(InUser),
			ControllerIndex(InControllerIndex),
			Delegate(InDelegate)
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/**
	 *	Async event that notifies when the profile card has been closed
	 */
	class FAsyncEventProfileCardClosed : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventProfileCardClosed() :
			FOnlineAsyncEvent(NULL)
		{
		}
		
		/** The delegate to execute when the account picker is closed. */
		FOnProfileUIClosedDelegate Delegate;

	public:

		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InDelegate The delegate to execute on the game thread.
		 */
		FAsyncEventProfileCardClosed(FOnlineSubsystemLive* InLiveSubsystem, const FOnProfileUIClosedDelegate& InDelegate) :
			FOnlineAsyncEvent(InLiveSubsystem),
			Delegate(InDelegate)
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

PACKAGE_SCOPE:

	/** Constructor
	 *
	 * @param InSubsystem The owner of this external UI interface.
	 */
	explicit FOnlineExternalUILive(FOnlineSubsystemLive* InSubsystem) :
		LiveSubsystem(InSubsystem)
	{
	}

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

public:

	/**
	 * Destructor.
	 */
	virtual ~FOnlineExternalUILive()
	{
	}

	// IOnlineExternalUI
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionMame = GameSessionName) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;
};

typedef TSharedPtr<FOnlineExternalUILive, ESPMode::ThreadSafe> FOnlineExternalUILivePtr;

