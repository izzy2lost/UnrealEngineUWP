// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertClient.h"
#include "MultiStreamColumns.h"

#include "MultiUserReplicationStyle.h"
#include "SReassignObjectComboBox.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"

#define LOCTEXT_NAMESPACE "ReassignOwnershipColumn"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName ReassignOwnershipColumnId(TEXT("ReassignOwnershipColumn"));
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertClientSharedSlate::ReplicationColumns;

		auto MakeWidget =
			[ConcertClient, MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), &ReassignmentLogic, &ClientManager]
			(const FReplicationTopLevelObjectColumn::FBuildArgs& InArgs)
			{
				return SNew(SReassignObjectComboBox, ConcertClient, ReassignmentLogic, ClientManager)
					.ManagedObject(InArgs.RowData.GetObjectPath())
					.ConsolidatedModel_Lambda([MultiStreamModelAttribute]()
					{
						const TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor> Editor = MultiStreamModelAttribute.Get();
						return Editor
							? &MultiStreamModelAttribute.Get()->GetConsolidatedModel()
							: nullptr;
					})
					.HighlightText(InArgs.HighlightText)
					.OnReassignAllOptionClicked_Lambda([MultiStreamModelAttribute](auto)
					{
						if (const TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor> Model = MultiStreamModelAttribute.Get())
						{
							Model->GetEditorBase().RequestObjectColumnResort(ReassignOwnershipColumnId);
						}
					});
			};
		auto IsLessThan = [ConcertClient, &ReassignmentLogic](const ConcertClientSharedSlate::FReplicatedObjectData& Left, const ConcertClientSharedSlate::FReplicatedObjectData& Right)
		{
			const TOptional<FString> LeftClientDisplayString = SReassignObjectComboBox::GetDisplayString(ConcertClient, ReassignmentLogic, Left.GetObjectPath());
			const TOptional<FString> RightClientDisplayString = SReassignObjectComboBox::GetDisplayString(ConcertClient, ReassignmentLogic, Right.GetObjectPath());
			
			if (LeftClientDisplayString && RightClientDisplayString)
			{
				return *LeftClientDisplayString < *RightClientDisplayString;
			}
			// Our rule: set < unset. This way unassigned appears last.
			return LeftClientDisplayString.IsSet() && !RightClientDisplayString.IsSet();
		};
		
		return FReplicationTopLevelObjectColumn(
			FReplicationTopLevelObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda(MoveTemp(MakeWidget))
				.PopulateSearchItems_Lambda([ConcertClient, &ReassignmentLogic](const ConcertClientSharedSlate::FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					SReassignObjectComboBox::PopulateSearchTerms(*ConcertClient->GetCurrentSession(), ReassignmentLogic, ObjectData.GetObjectPath(), InOutSearchStrings);
				})
				.IsLessThan_Lambda(MoveTemp(IsLessThan))
				.ColumnSortOrder(ColumnsSortPriority),
			SHeaderRow::Column(ReassignOwnershipColumnId)
				.DefaultLabel(LOCTEXT("Owner.Label", "Assigned Clients"))
				.ToolTipText(LOCTEXT("Owner.ToolTip", "Clients that have registered properties for an object"))
				.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.OwnerColumnWidth")))
			);
	}
}

#undef LOCTEXT_NAMESPACE