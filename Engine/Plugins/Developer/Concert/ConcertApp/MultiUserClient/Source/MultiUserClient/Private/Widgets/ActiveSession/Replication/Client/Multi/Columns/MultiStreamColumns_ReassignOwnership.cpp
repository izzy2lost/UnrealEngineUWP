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
	
	ConcertSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReassignOwnership(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertSharedSlate::ReplicationColumns;

		auto MakeWidget =
			[ConcertClient, MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), ObjectHierarchyModelAttribute = MoveTemp(ObjectHierarchyModelAttribute), &ReassignmentLogic, &ClientManager]
			(const FReplicationTopLevelObjectColumn::FBuildArgs& InArgs)
			{
				return SNew(SReassignObjectComboBox, ConcertClient, ReassignmentLogic, ClientManager)
					.ManagedObject(InArgs.RowData.GetObjectPath())
					.ObjectHierarchyModel(ObjectHierarchyModelAttribute)
					.HighlightText(InArgs.HighlightText)
					.OnReassignAllOptionClicked_Lambda([MultiStreamModelAttribute](auto)
					{
						if (const TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor> Model = MultiStreamModelAttribute.Get())
						{
							Model->GetEditorBase().RequestObjectColumnResort(ReassignOwnershipColumnId);
						}
					});
			};
		auto IsLessThan = [ConcertClient, &ReassignmentLogic](const ConcertSharedSlate::FReplicatedObjectData& Left, const ConcertSharedSlate::FReplicatedObjectData& Right)
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
				.PopulateSearchItems_Lambda([ConcertClient, &ReassignmentLogic](const ConcertSharedSlate::FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
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