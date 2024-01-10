// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "SOwnerClientList.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Authority/EAuthorityMutability.h"
#include "Replication/Authority/IClientAuthoritySynchronizer.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ClientName/SClientName.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.ToggleAuthority"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName ToggleTopLevelAuthorityColumnId = TEXT("ToggleTopLevelAuthorityColumn");

	ConcertSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ToggleObjectAuthority(
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		)
	{
		using namespace ConcertSharedSlate;
		using FColumnDelegates = TReplicationColumnDelegates<FReplicatedObjectData>;
		return MakeCheckboxColumn<FReplicatedObjectData>(
			ToggleTopLevelAuthorityColumnId,
				FColumnDelegates(
					FColumnDelegates::FGetColumnCheckboxState::CreateLambda(
					[&ChangeTracker](const FReplicatedObjectData& ObjectData)
					{
						const bool bHasAuthority = ChangeTracker.GetAuthorityStateAfterApplied(ObjectData.GetObjectPath());
						return bHasAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					FColumnDelegates::FOnColumnCheckboxChanged::CreateLambda(
					[&ChangeTracker](bool bIsChecked, const FReplicatedObjectData& ObjectData)
					{
						ChangeTracker.SetAuthorityIfAllowed({ ObjectData.GetObjectPath() }, bIsChecked);
					}),
					FColumnDelegates::FGetToolTipText::CreateLambda([&ChangeTracker, &SubmissionWorkflow](const FReplicatedObjectData& ObjectData)
					{
						if (!CanEverSubmit(SubmissionWorkflow.GetUploadability()))
						{
							return LOCTEXT("ToggleAuthority.ToolTip.NotSupported", "This client cannot be remotely edited.");
						}
						
						switch (ChangeTracker.GetChangeAuthorityMutability(ObjectData.GetObjectPath()))
						{
						case EAuthorityMutability::Allowed: return LOCTEXT("ToggleAuthority.ToolTip.Allowed", "Whether to replicate this object.");
						case EAuthorityMutability::NotApplicable: return LOCTEXT("ToggleAuthority.ToolTip.NotApplicable", "Assign properties to replicate first.");
						case EAuthorityMutability::Conflict: return LOCTEXT("ToggleAuthority.ToolTip.Conflict", "Another client is replicating this property already.");
						default: checkNoEntry(); return FText::GetEmpty();
						}
					}),
					FColumnDelegates::FIsEnabled::CreateLambda([&ChangeTracker](const FReplicatedObjectData& ObjectData)
					{
						return ChangeTracker.CanSetAuthorityFor(ObjectData.GetObjectPath());
					})
				),
			FText::GetEmpty(),
			static_cast<int32>(ETopLevelObjectColumnOrder::ToggleAuthority)
		);
	}
}

#undef LOCTEXT_NAMESPACE