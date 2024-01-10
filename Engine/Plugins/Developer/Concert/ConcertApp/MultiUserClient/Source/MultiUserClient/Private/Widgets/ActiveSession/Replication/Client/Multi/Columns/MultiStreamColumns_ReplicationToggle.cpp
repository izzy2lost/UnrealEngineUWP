// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "MultiUserReplicationStyle.h"
#include "SReplicationMultiToggleCheckbox.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "ReplicationToggle"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName ReplicationToggleColumnId(TEXT("ReplicationToggleColumn"));
	
	ConcertSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ReplicationToggle(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<ConcertSharedSlate::IReplicationStreamModel*> ConsolidatedStreamModelAttribute,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertSharedSlate::ReplicationColumns;
		
		return FReplicationTopLevelObjectColumn(
			FReplicationTopLevelObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([ConcertClient = MoveTemp(ConcertClient), ConsolidatedStreamModelAttribute = MoveTemp(ConsolidatedStreamModelAttribute), &ClientManager](const FReplicationTopLevelObjectColumn::FBuildArgs& InArgs)
				{
					return SNew(SBox)
					.HAlign(HAlign_Left) // Warning icon is sometimes collapsed - we don't want the widget to be centered
					[
						SNew(SReplicationMultiToggleCheckbox, ClientManager, ConcertClient)
						.Object(InArgs.RowData.GetObjectPath())
						.ConsolidatedStreamModelAttribute(ConsolidatedStreamModelAttribute)
					];
				})
				.ColumnSortOrder(ColumnsSortPriority),
			SHeaderRow::Column(ReplicationToggleColumnId)
				.DefaultLabel(FText::GetEmpty())
				.ToolTipText(LOCTEXT("Replicates.ToolTip", "Assign properties first.\nControls whether the object should replicate."))
				.FixedWidth(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.ReplicationToggle")))
			);
	}
}

#undef LOCTEXT_NAMESPACE