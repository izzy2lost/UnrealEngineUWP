// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationUtils.h"

#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"
#include "Replication/IConcertClientReplicationManager.h"

#include "Async/Async.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ReplicationUtils"

namespace UE::MultiUserClient::ReplicationUtils
{
	TFuture<ConcertSyncClient::Replication::FJoinReplicatedSessionResult> JoinSessionWithEditorNotifications(
		IConcertClientReplicationManager& Manager,
		ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args)
	{
		using namespace ConcertSyncClient::Replication;
		FNotificationInfo Notification(LOCTEXT("Joining", "Joining Replication Session"));
		Notification.bFireAndForget = false;
		Notification.ExpireDuration = 6.f;
		Notification.bUseThrobber = true;
		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Notification);

		TPromise<FJoinReplicatedSessionResult> Promise;
		TFuture<FJoinReplicatedSessionResult> Future = Promise.GetFuture();
		Manager.JoinReplicationSession(MoveTemp(Args))
			.Next([Promise = MoveTemp(Promise), NotificationItem](FJoinReplicatedSessionResult ErrorResult) mutable
			{
				// We may already be on the game thread. In some cases, like a timeout, we'll be executed on the UDP's thread.
				// NotificationItem may invalidate itself, which accesses the FSlateApplication singleton - which must be done on the game thread.
				AsyncTask(ENamedThreads::GameThread, [Promise = MoveTemp(Promise), NotificationItem, ErrorResult]() mutable
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
							? FText::Format(LOCTEXT("Error.MissingInfoFmt", "Server returned error code {0}"), FText::FromString(ErrorCodeText))
							: FText::Format(LOCTEXT("Error.DetailedInfoFmt", "{0} ({1})"), FText::FromString(ErrorResult.DetailedErrorMessage), FText::FromString(ErrorCodeText));
						NotificationItem->SetSubText(Message);
						NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
					}
					NotificationItem->ExpireAndFadeout();
					Promise.EmplaceValue(ErrorResult);
				});
			});
		return Future;
	}
	
	void LeaveSessionWithEditorNotifications(IConcertClientReplicationManager& Manager)
	{
		Manager.LeaveReplicationSession();
		
		FNotificationInfo Notification(LOCTEXT("Left", "Left Replication Session"));
		Notification.bFireAndForget = true;
		Notification.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
}

#undef LOCTEXT_NAMESPACE