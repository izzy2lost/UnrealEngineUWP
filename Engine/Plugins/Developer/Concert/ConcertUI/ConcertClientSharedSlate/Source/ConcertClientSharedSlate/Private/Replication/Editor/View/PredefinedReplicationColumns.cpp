// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/PredefinedReplicationColumns.h"

#include "Replication/Editor/Model/ReplicatedPropertyData.h"

#include "Internationalization/Internationalization.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "ReplicationPropertyColumns"

namespace UE::ConcertClientSharedSlate::ReplicationPropertyColumns
{
	const FName ReplicatesColumnId = TEXT("ReplicatesColumn");

	FReplicationPropertyColumn MakePropertyCheckboxColumn(
		FGetColumnCheckboxState GetPropertyCheckboxStateDelegate,
		FOnColumnCheckboxChanged OnPropertyBoxToggledDelegate,
		FText DefaultLabel,
		FText ToolTipText,
		const float ColumnWidth,
		const int32 Priority
		)
	{
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([GetPropertyCheckboxStateDelegate, OnPropertyBoxToggledDelegate, ToolTipText = MoveTemp(ToolTipText)](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(SCheckBox)
						.ToolTipText(ToolTipText)
						.IsChecked_Lambda([GetPropertyCheckboxStateDelegate, RowData = Args.RowData]()
						{
							return GetPropertyCheckboxStateDelegate.Execute(RowData.GetProperty());
						})
						.OnCheckStateChanged_Lambda([OnPropertyBoxToggledDelegate, RowData = Args.RowData](ECheckBoxState NewState)
						{
							const bool bIsChecked = NewState == ECheckBoxState::Checked;
							OnPropertyBoxToggledDelegate.Execute(bIsChecked, RowData.GetProperty());
						});
				})
				.ColumnSortOrder(Priority),
			SHeaderRow::Column(ReplicatesColumnId)
				.DefaultLabel(DefaultLabel)
				.FixedWidth(ColumnWidth)
			);
	}
}

#undef LOCTEXT_NAMESPACE