// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationJoinedView.h"

#include "ConcertLogGlobal.h"
#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"
#include "Replication/Editor/View/IReplicationEditorView.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/ClientStreamRepository.h"
#include "SReplicationJoinedToolbar.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SReplicationJoinedWidget"

namespace UE::MultiUserClient
{
	void SReplicationJoinedView::Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager)
	{
		ReplicationManager = InReplicationManager;
		
		using namespace ConcertClientSharedSlate;
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			ReplicationManager->GetStreamSynchronizer()->GetLocalClientEditModel(),
			MakeShared<FEditorObjectSelectionSourceModel>(),
			MakeShared<FSelectPropertyFromUClassModel>()
		};

		const TSharedRef<IReplicationEditorView> NewEditorView = CreateEditor(ReplicationEditorCreationParams);
		WeakEditorView = NewEditorView;
		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.f)
			[
				SNew(SReplicationJoinedToolbar, InReplicationManager)
			]

			// Editor
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				NewEditorView
			]
		];
		
		// Refresh UI if streams change externally, e.g. a remote client changed what they sent
		ReplicationManager->GetStreamSynchronizer()->OnModelChanged_GameThread().AddSP(this, &SReplicationJoinedView::OnModelChanged);

		// Show notifications about changing authority
		ReplicationManager->GetAuthorityPolicy()->OnAuthorityRequestSent_AnyThread().AddSP(this, &SReplicationJoinedView::OnAuthorityRequestSent_AnyThread);
		ReplicationManager->GetAuthorityPolicy()->OnAuthorityResponseReceived_AnyThread().AddSP(this, &SReplicationJoinedView::OnAuthorityResponseReceived_AnyThread);
	}

	void SReplicationJoinedView::OnModelChanged() const
	{
		if (const TSharedPtr<ConcertClientSharedSlate::IReplicationEditorView> EditorView = WeakEditorView.Pin())
		{
			EditorView->Refresh();
		}
	}

	void SReplicationJoinedView::OnAuthorityRequestSent_AnyThread(
		const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request
		)
	{
		ExecuteOnGameThread(TEXT("OnAuthorityRequestSent"), [WeakThis = TWeakPtr<SReplicationJoinedView>(SharedThis(this)), Request]()
		{
			if (const TSharedPtr<SReplicationJoinedView> ThisPin = WeakThis.Pin())
			{
				FNotificationInfo NotificationInfo(LOCTEXT("Authority.InProgress.Text", "Requesting authority."));
				NotificationInfo.bUseThrobber = true;
				NotificationInfo.bUseSuccessFailIcons = true;
				NotificationInfo.bFireAndForget = false;
				ThisPin->AuthorityChangeNotification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
				ThisPin->AuthorityChangeNotification->SetSubText(
					FText::Format(LOCTEXT("Authority.InProgress.SubTextFmt", "Taking {0}|plural(one=object,other\nReleasing {1}|plural(one=object,other=objects)"),
						Request.TakeAuthority.Num(),
						Request.ReleaseAuthority.Num()
					));
			}
		});
	}

	void SReplicationJoinedView::OnAuthorityResponseReceived_AnyThread(
		const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
		const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response
		)
	{
		ExecuteOnGameThread(TEXT("OnAuthorityResponseReceived"), [WeakThis = TWeakPtr<SReplicationJoinedView>(SharedThis(this)), Request, Response]()
		{
			if (const TSharedPtr<SReplicationJoinedView> ThisPin = WeakThis.Pin()
				; ThisPin && ensure(ThisPin->AuthorityChangeNotification))
			{
				const bool bSuccess = Response.RejectedObjects.IsEmpty();
				const FText Text = bSuccess
					? LOCTEXT("Authority.Text.Accepted", "Authority change accepted.")
					: LOCTEXT("Authority.Text.Rejection", "Authority change had rejections.");
				const FText SubText = bSuccess
					? FText::GetEmpty()
					: FText::Format(LOCTEXT("Authority.SubText.RejectionFmt", "{0} accepted {1} rejected\nSee logs for details."),
						Request.TakeAuthority.Num() - Response.RejectedObjects.Num(),
						Response.RejectedObjects.Num()
						);
				
				ThisPin->AuthorityChangeNotification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
				ThisPin->AuthorityChangeNotification->SetText(Text);
				ThisPin->AuthorityChangeNotification->SetSubText(SubText);

				if (!bSuccess)
				{
					const FString Error = FString::JoinBy(Response.RejectedObjects, TEXT("\n"), [](const TPair<FSoftObjectPath, FConcertStreamArray>& Pair)
					{
						return Pair.Key.ToString();
					});
					UE_LOG(LogConcert, Error, TEXT("Authority change rejected objects:\n%s"), *Error);
				}
				
				ThisPin->AuthorityChangeNotification->ExpireAndFadeout();
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE