// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/IConcertClientReplicationManager.h"
#include "Replication/IToken.h"
#include "Replication/Data/ObjectReplicationMap.h"
#include "Templates/UnrealTemplate.h"

class IConcertSyncClient;

namespace UE::ConcertSyncClient::Replication
{
	struct FAuthorityChangeResponse;
	struct FAuthorityChangeRequest;
}
namespace UE::MultiUserClient
{
	class FLocalClientStreamSynchronizer;
}

namespace UE::MultiUserClient
{
	/**
	 * Manages authority for the local client in the Multi User application.
	 * Automatically requests authority over newly added objects.
	 *
	 * FMultiUserReplicationManager ensures that the passed in arguments, like LocalClientManager, are kept alive for
	 * the duration of FAuthorityPolicy's life.
	 */
	class FAuthorityPolicy : public FNoncopyable
	{
	public:
		
		FAuthorityPolicy(FLocalClientStreamSynchronizer& InLocalClientManager, TSharedRef<IConcertSyncClient> InClient);
		~FAuthorityPolicy();

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnAuthorityRequestSent,
			const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request
			);
		/** Called when an authority request is sent to the server. */
		FOnAuthorityRequestSent& OnAuthorityRequestSent_AnyThread() { return OnAuthorityRequestSentDelegate; }

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAuthorityResponseReceived,
			const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
			const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response
			);
		/** Called when an authority response is received from the server. */
		FOnAuthorityResponseReceived& OnAuthorityResponseReceived_AnyThread() { return OnAuthorityResponseReceivedDelegate; }
	
	private:

		/** Referenced by the authority requests to detect destruction of FAuthorityPolicy */
		const TSharedRef<FToken> LifetimeToken = FToken::Make();

		/** Informs us when changes have been successfully submitted to the server. */
		FLocalClientStreamSynchronizer& LocalClientManager;

		/** Used to send authority requests to the server. */
		const TSharedRef<IConcertSyncClient> Client;

		FOnAuthorityRequestSent OnAuthorityRequestSentDelegate;
		FOnAuthorityResponseReceived OnAuthorityResponseReceivedDelegate;

		/** After changes have been accepted by the server, requests authority over said objects & properties. */
		void OnChangesAccepted_AnyThread(const FObjectReplicationMap& OldState, const ConcertSyncClient::Replication::FChangeStreamRequest& AcceptedRequest);
	};
}

