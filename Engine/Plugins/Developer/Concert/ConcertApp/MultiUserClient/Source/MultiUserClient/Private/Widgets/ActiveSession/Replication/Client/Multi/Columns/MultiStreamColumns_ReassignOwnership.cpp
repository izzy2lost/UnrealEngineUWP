// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertClient.h"
#include "MultiStreamColumns.h"

#include "MultiUserReplicationStyle.h"
#include "SReassignObjectComboBox.h"

#define LOCTEXT_NAMESPACE "ReassignOwnershipColumn"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName ReassignOwnershipColumnId(TEXT("ReassignOwnershipColumn"));
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<ConcertClientSharedSlate::IReplicationStreamModel*> ConsolidatedModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertClientSharedSlate::ReplicationColumns;

		auto MakeWidget =
			[ConcertClient, ConsolidatedModelAttribute = MoveTemp(ConsolidatedModelAttribute), &ReassignmentLogic, &ClientManager]
			(const FReplicationTopLevelObjectColumn::FBuildArgs& InArgs)
			{
				return SNew(SReassignObjectComboBox, ConcertClient, ReassignmentLogic, ClientManager)
					.ManagedObject(InArgs.RowData.GetObjectPath())
					.ConsolidatedModel(ConsolidatedModelAttribute)
					.HighlightText(InArgs.HighlightText);
			};
		
		return FReplicationTopLevelObjectColumn(
			FReplicationTopLevelObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda(MoveTemp(MakeWidget))
				.PopulateSearchItems_Lambda([ConcertClient, &ReassignmentLogic](const ConcertClientSharedSlate::FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					SReassignObjectComboBox::PopulateSearchTerms(*ConcertClient->GetCurrentSession(), ReassignmentLogic, ObjectData.GetObjectPath(), InOutSearchStrings);
				})
				.ColumnSortOrder(ColumnsSortPriority),
			SHeaderRow::Column(ReassignOwnershipColumnId)
				.DefaultLabel(LOCTEXT("Owner", "Owner"))
				.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.OwnerColumnWidth")))
			);
	}
}

#undef LOCTEXT_NAMESPACE