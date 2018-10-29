// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemLive.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemLivePackage.h"

/** 
 * Implementation for the Live external UIs
 */
class FOnlineExternalUILive
	: public IOnlineExternalUI
	, public TSharedFromThis<FOnlineExternalUILive, ESPMode::ThreadSafe>
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
		//int ControllerIndex;

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
		FAsyncEventAccountPickerClosed(FOnlineSubsystemLive* InLiveSubsystem, Windows::Xbox::System::IUser^ InUser/*, const int InControllerIndex*/, const FOnLoginUIClosedDelegate& InDelegate) :
			FOnlineAsyncEvent(InLiveSubsystem),
			SignedInUser(InUser),
			//ControllerIndex(InControllerIndex),
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


	void HandleApplicationHasReactivated_Store();
// @ATG_CHANGE : BEGIN - UWP LIVE Support
#if PLATFORM_XBOXONE
	void ProductPurchased_Store(Windows::Xbox::ApplicationModel::Store::ProductPurchasedEventArgs^ Args);
#endif
// @ATG_CHANGE : END - UWP LIVE Support

	/**
	*	Async event that notifies when the store UI has been closed
	*/
	class FAsyncEventStoreUIClosed : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventStoreUIClosed() :
			FOnlineAsyncEvent(NULL)
		{
		}

		/** The delegate to execute when the store UI is closed. */
		FOnShowStoreUIClosedDelegate Delegate;

		/** Whether a purchase was made */
		bool bPurchasedProduct;

	public:

		/**
		* Constructor.
		*
		* @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		* @param InDelegate The delegate to execute on the game thread.
		* @param PurchasedProduct A flag determining whether a purchase was made.
		*/
		FAsyncEventStoreUIClosed(FOnlineSubsystemLive* InLiveSubsystem, const FOnShowStoreUIClosedDelegate& InDelegate, bool PurchasedProduct) :
			FOnlineAsyncEvent(InLiveSubsystem),
			Delegate(InDelegate),
			bPurchasedProduct(PurchasedProduct)
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	/**
	*	Async event that notifies when the message UI has been closed
	*/
	class FAsyncEventSendMessageUIClosed : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventSendMessageUIClosed() :
			FOnlineAsyncEvent(NULL)
		{
		}

		/** The delegate to execute when the store UI is closed. */
		FOnShowSendMessageUIClosedDelegate Delegate;

		/** Whether a message was sent */
		bool bMessageSent;

	public:

		/**
		* Constructor.
		*
		* @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		* @param InDelegate The delegate to execute on the game thread.
		* @param MessageSent A flag determining whether a message was sent.
		*/
		FAsyncEventSendMessageUIClosed(FOnlineSubsystemLive* InLiveSubsystem, const FOnShowSendMessageUIClosedDelegate& InDelegate, bool MessageSent) :
			FOnlineAsyncEvent(InLiveSubsystem),
			Delegate(InDelegate),
			bMessageSent(MessageSent)
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	void HandleApplicationHasReactivated_WebUrl();
	void HandleApplicationReactivatedByProtocol_WebUrl(FString ActivationUri);

	/**
	*	Async event that notifies when the Web Url has closed
	*/
	class FAsyncEventWebUrlUIClosed : public FOnlineAsyncEvent<FOnlineSubsystemLive>
	{
		/** Hidden on purpose */
		FAsyncEventWebUrlUIClosed() :
			FOnlineAsyncEvent(NULL)
		{
		}

		/** The delegate to execute when the WebUrl UI is closed. */
		FOnShowWebUrlClosedDelegate Delegate;

		FString WebUrl;

	public:

		/**
		 * Constructor.
		 *
		 * @param InLiveSubsystem The owner of the external UI interface that triggered this event.
		 * @param InDelegate The delegate to execute on the game thread.
		 * @param InWebUrl The URL that was opened
		 */
		FAsyncEventWebUrlUIClosed(FOnlineSubsystemLive* InLiveSubsystem, FOnShowWebUrlClosedDelegate&& InDelegate, FString&& InWebUrl) :
			FOnlineAsyncEvent(InLiveSubsystem),
			Delegate(MoveTemp(InDelegate)),
			WebUrl(MoveTemp(InWebUrl))
		{
		}

		virtual FString ToString() const override;
		virtual void TriggerDelegates() override;
	};

	Windows::Foundation::EventRegistrationToken ProductPurchasedToken;
	FCriticalSection CallbackLock;

PACKAGE_SCOPE:

	/** Constructor
	 *
	 * @param InSubsystem The owner of this external UI interface.
	 */
	explicit FOnlineExternalUILive(FOnlineSubsystemLive* InSubsystem);

	/** Reference to the owning subsystem */
	class FOnlineSubsystemLive* LiveSubsystem;

	/** Allow guests to login */
	bool bAllowGuestLogin;

	/** delegates to hold onto for particular callbacks */
	bool bShouldCallUIDelegate;
	FOnShowStoreUIClosedDelegate StoreUIClosedDelegate;
	
	struct FShowWebUrlRequest
	{
		FShowWebUrlRequest()
		{
			Reset();
		}
		void Reset()
		{
			WebUrlBeingOpened.Empty();
			WebUrlClosedDelegate = FOnShowWebUrlClosedDelegate();
			WaitForProtocolActivationTimeRemaining = 0.0f;
		}
		FString WebUrlBeingOpened;
		FOnShowWebUrlClosedDelegate WebUrlClosedDelegate;
		float WaitForProtocolActivationTimeRemaining;
	};
	FShowWebUrlRequest ShowWebUrlRequest;

	void FinishShowWebUrl(FString&& FinalUrl);

public:

	/**
	 * Destructor.
	 */
	virtual ~FOnlineExternalUILive()
	{
	}

	// IOnlineExternalUI
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowAccountCreationUI(const int ControllerIndex, const FOnAccountCreationUIClosedDelegate& Delegate = FOnAccountCreationUIClosedDelegate()) override { /** NYI */ return false; }
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionName = NAME_GameSession) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;
	void Tick(float DeltaTime);
};

typedef TSharedPtr<FOnlineExternalUILive, ESPMode::ThreadSafe> FOnlineExternalUILivePtr;
