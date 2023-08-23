// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationUtils.h"

#include "ConcertLogGlobal.h"
#include "IConcertSyncClient.h"
#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Async/Async.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ReplicationUtils"

namespace UE::MultiUserClient::Replication::Private
{
	/** Wraps join request with SNotificationItem. */
	static TPair<TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult>, TSharedPtr<SNotificationItem>> JoinSession(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args
		);

	/** Reuses SNotificationItem from join for authority change. */
	static TFuture<FJoinSessionResult> RequestAuthorityAfterJoin(
		const TSharedRef<IConcertSyncClient>& Client,
		TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult>&& JoinRequestFuture,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args,
		TSharedPtr<SNotificationItem> NotificationToReuse
		);

	/** Updates the message started by JoinSession */
	static void HandleJoinMessage(
		TSharedPtr<SNotificationItem> NotificationItem,
		ConcertSyncClient::Replication::FJoinReplicatedSessionResult ErrorResult
		);
	/** Updates the message started by RequestChangeAuthority. */
	static void HandleAuthorityMessage(
		TSharedPtr<SNotificationItem> NotificationItem,
		const int32 NumRequested,
		const int32 NumReleased,
		ConcertSyncClient::Replication::FAuthorityChangeResponse Response
		);
}

namespace UE::MultiUserClient::Replication
{
	TFuture<FJoinSessionResult> JoinSessionForMultiUser(
		const TSharedRef<IConcertSyncClient>& Client,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args
		)
	{
		using namespace ConcertSyncClient::Replication;
		IConcertClientReplicationManager* Manager = Client->GetReplicationManager();
		if (!Manager)
		{
			return MakeFulfilledPromise<FJoinSessionResult>(FJoinSessionResult{ { EJoinReplicationErrorCode::Cancelled } }).GetFuture();
		}
		
		// 1. Join the session
		auto[JoinRequestFuture, NotificationItem] = Private::JoinSession(*Manager, Args); 
		// 2. Request authority for all the client's objects
		// The notification for joining is re-used to avoid spam
		TFuture<FJoinSessionResult> RequestAuthorityFuture = Private::RequestAuthorityAfterJoin(Client, MoveTemp(JoinRequestFuture), Args, NotificationItem);
		
		return RequestAuthorityFuture;
	}
	
	TFuture<ConcertSyncClient::Replication::FAuthorityChangeResponse> RequestAuthorityChange(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FAuthorityChangeRequest Args,
		TSharedPtr<SNotificationItem> NotificationToReuse
		)
	{
		using namespace UE::ConcertSyncClient::Replication;
		const int32 NumRequested = Args.TakeAuthority.Num();
		const int32 NumReleased = Args.ReleaseAuthority.Num();
		
		// IConcertClientReplicationManager::RequestAuthorityChange already checks for this case but we don't want to show the notification.
		if (NumRequested == 0 && NumReleased == 0)
		{
			if (NotificationToReuse)
			{
				NotificationToReuse->ExpireAndFadeout();
			}
			return MakeFulfilledPromise<FAuthorityChangeResponse>(FAuthorityChangeResponse{}).GetFuture();
		}
		TFuture<FAuthorityChangeResponse> Future = Manager.RequestAuthorityChange(Args);
		
		const FText Title = LOCTEXT("AuthorityChange.Start", "Requesting Authority Change");
		TSharedPtr<SNotificationItem> NotificationItem = MoveTemp(NotificationToReuse);
		if (!NotificationItem)
		{
			FNotificationInfo Notification(Title);
			Notification.bFireAndForget = false;
			Notification.ExpireDuration = 6.f;
			Notification.bUseThrobber = true;
			NotificationItem = FSlateNotificationManager::Get().AddNotification(Notification);
		}
		else
		{
			NotificationItem->SetText(Title);
		}

		// Adding notifications should usually succeed but can theoretically fail
		if (NotificationItem)
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
			Future = Future.Next([NotificationItem = MoveTemp(NotificationItem), NumRequested, NumReleased](FAuthorityChangeResponse Response) mutable
			{
				Private::HandleAuthorityMessage(MoveTemp(NotificationItem), NumRequested, NumReleased, MoveTemp(Response));
				return Response;
			});
		}
		
		return Future;
	}
	
	void LeaveSessionForMultiUser(IConcertClientReplicationManager& Manager)
	{
		Manager.LeaveReplicationSession();
		
		FNotificationInfo Notification(LOCTEXT("Left", "Left Replication Session"));
		Notification.bFireAndForget = true;
		Notification.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
}

namespace UE::MultiUserClient::Replication::Private
{
	static TPair<TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult>, TSharedPtr<SNotificationItem>> JoinSession(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args)
	{
		using namespace ConcertSyncClient::Replication;
		TFuture<FJoinReplicatedSessionResult> Future = Manager.JoinReplicationSession(MoveTemp(Args));
		
		FNotificationInfo Notification(LOCTEXT("Joining", "Joining Replication Session"));
		Notification.bFireAndForget = false;
		Notification.ExpireDuration = 6.f;
		Notification.bUseThrobber = true;
		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Notification);

		// Adding notifications should usually succeed but can theoretically fail
		if (NotificationItem)
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
			Future = Future.Next([NotificationItem](FJoinReplicatedSessionResult ErrorResult) mutable
			{
				HandleJoinMessage(MoveTemp(NotificationItem), ErrorResult);
				return ErrorResult;
			});
		}
		
		return { MoveTemp(Future), NotificationItem };
	}
	
	static TFuture<FJoinSessionResult> RequestAuthorityAfterJoin(
		const TSharedRef<IConcertSyncClient>& Client,
		TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult>&& JoinRequestFuture,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args,
		TSharedPtr<SNotificationItem> NotificationToReuse
		)
	{
		TPromise<FJoinSessionResult> Promise;
		TFuture<FJoinSessionResult> Future = Promise.GetFuture();
		JoinRequestFuture.Next([WeakClient = Client.ToWeakPtr(), NotificationToReuse = MoveTemp(NotificationToReuse), Promise = MoveTemp(Promise), Args]
			(ConcertSyncClient::Replication::FJoinReplicatedSessionResult JoinResult) mutable
		{
			ON_SCOPE_EXIT
			{
				// Note that RequestChangeAuthority will MoveTemp this to null (if we sent an authority request)
				if (NotificationToReuse)
				{
					NotificationToReuse->ExpireAndFadeout();
				}
			};

			ConcertSyncClient::Replication::FAuthorityChangeRequest AuthorityRequest;
			for (const FReplicationStreamDescription& Stream : Args.Streams)
			{
				const FSharedReplicationStreamDescription& StreamDescription = Stream.BaseDescription;
				for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& ReplicatedObject : StreamDescription.ReplicationMap.ReplicatedObjects)
				{
					AuthorityRequest.TakeAuthority.FindOrAdd(ReplicatedObject.Key).StreamIds.Add(StreamDescription.Identifier);
				}
			}
			
			// Client pointer is needed because it is unsafe to keep the IConcertClientReplicationManager* reference
			const TSharedPtr<IConcertSyncClient> ClientPin = WeakClient.Pin();
			IConcertClientReplicationManager* ReplicationManager = ClientPin ? ClientPin->GetReplicationManager() : nullptr;
			const bool bTimedOut = IsEngineExitRequested() || !ReplicationManager;
			const bool bErrorJoining = JoinResult.ErrorCode != EJoinReplicationErrorCode::Success;
			const bool bNothingToRequest = AuthorityRequest.TakeAuthority.IsEmpty();
			// TODO DP: Here is a good place to check project settings whether to automatically request authority (but then make sure to expire NotificationToReuse).
			if (bTimedOut || bErrorJoining || bNothingToRequest)
			{
				Promise.EmplaceValue(FJoinSessionResult{ MoveTemp(JoinResult) });
				return;
			}
			
			RequestAuthorityChange(*ReplicationManager, MoveTemp(AuthorityRequest), MoveTemp(NotificationToReuse))
				.Next([Promise = MoveTemp(Promise), JoinResult = MoveTemp(JoinResult)](ConcertSyncClient::Replication::FAuthorityChangeResponse Response) mutable
				{
					Promise.EmplaceValue(FJoinSessionResult{ MoveTemp(JoinResult), MoveTemp(Response) });
				});
		});
		return Future;
	}

	static void HandleJoinMessage(
		TSharedPtr<SNotificationItem> NotificationItem,
		ConcertSyncClient::Replication::FJoinReplicatedSessionResult ErrorResult
		)
	{
		// We may already be on the game thread. In some cases, like a timeout, we'll be executed on the UDP's thread.
		// NotificationItem may invalidate itself, which accesses the FSlateApplication singleton - which must be done on the game thread.
		AsyncTask(ENamedThreads::GameThread, [NotificationItem = MoveTemp(NotificationItem), ErrorResult = MoveTemp(ErrorResult)]() mutable
		{
			const bool bSuccess = ErrorResult.ErrorCode == EJoinReplicationErrorCode::Success;
			if (bSuccess)
			{
				NotificationItem->SetText(LOCTEXT("Joined", "Joined Replication Session"));
				NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
			}
			else
			{
				const FString ErrorCodeText = ConcertSyncCore::Replication::LexJoinErrorCode(ErrorResult.ErrorCode);
				const FText Message = ErrorResult.DetailedErrorMessage.IsEmpty()
					? FText::Format(LOCTEXT("Error.MissingInfoFmt", "Error code: {0}"), FText::FromString(ErrorCodeText))
					: FText::Format(LOCTEXT("Error.DetailedInfoFmt", "{0} ({1})"), FText::FromString(ErrorResult.DetailedErrorMessage), FText::FromString(ErrorCodeText));
				NotificationItem->SetSubText(Message);
				NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			}
		});
	}
	
	static void HandleAuthorityMessage(
		TSharedPtr<SNotificationItem> NotificationItem,
		const int32 NumRequested,
		const int32 NumReleased,
		ConcertSyncClient::Replication::FAuthorityChangeResponse Response
		)
	{
		// We may already be on the game thread. In some cases, like a timeout, we'll be executed on the UDP's thread.
		// NotificationItem may invalidate itself, which accesses the FSlateApplication singleton - which must be done on the game thread.
		AsyncTask(ENamedThreads::GameThread, [NotificationItem = MoveTemp(NotificationItem), NumRequested, NumReleased, Response]()
		{
			const int32 NumRejected = Response.RejectedObjects.Num();
			const int32 NumTaken = NumRequested - NumRejected;
					
			FString FinalMessage;
			if (NumTaken > 0)
			{
				FinalMessage.Append(FText::Format(LOCTEXT("AuthorityChange.Taken", "{0} owned objects."), NumTaken).ToString());
			}
			if (NumRejected > 0)
			{
				FinalMessage.Append(FText::Format(LOCTEXT("AuthorityChange.Rejected", "{0} rejected objects (see log)."), NumRejected).ToString());
				for (const TPair<FSoftObjectPath, FConcertStreamArray>& RejectedObjectInfo : Response.RejectedObjects)
				{
					UE_LOG(LogConcert, Warning, TEXT("Rejected authority change for %s for %d streams"), *RejectedObjectInfo.Key.ToString(), RejectedObjectInfo.Value.StreamIds.Num());
				}
			}
			if (NumReleased > 0)
			{
				FinalMessage.Append(FText::Format(LOCTEXT("AuthorityChange.Released", "{0} released objects."), NumReleased).ToString());
			}

			const bool bWasSuccess = Response.RejectedObjects.IsEmpty();
			NotificationItem->SetText(bWasSuccess ? LOCTEXT("AuthorityChange.RequestAccepted", "Authority change successful.") : LOCTEXT("AuthorityChange.RequestErrorful", "Authority change has rejections"));
			NotificationItem->SetSubText(FText::FromString(FinalMessage));
			NotificationItem->SetCompletionState(bWasSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();
		});
	}
}

#undef LOCTEXT_NAMESPACE