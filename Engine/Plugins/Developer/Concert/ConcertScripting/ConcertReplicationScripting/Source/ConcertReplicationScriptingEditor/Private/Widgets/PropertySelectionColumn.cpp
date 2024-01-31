// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertySelectionColumn.h"

#include "Algo/AnyOf.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"

#define LOCTEXT_NAMESPACE "PropertySelectionCheckboxColumn"

namespace UE::ConcertReplicationScriptingEditor
{
	const FName PropertySelectionCheckboxColumnId(TEXT("AddPropertyCheckboxColumn"));
	
	ConcertSharedSlate::ReplicationColumns::FReplicationPropertyColumn MakePropertySelectionCheckboxColumn(
		const TSet<FConcertPropertyChain>& SelectedProperties,
		FOnSelectProperty OnSelectPropertyDelegate,
		bool bIsEditable,
		int32 SortPriority
		)
	{
		using namespace ConcertSharedSlate;
		using FPropertyColumnDelegates = TReplicationColumnDelegates<FReplicatedPropertyData>;
		return MakeCheckboxColumn<FReplicatedPropertyData>(
			PropertySelectionCheckboxColumnId,
			FPropertyColumnDelegates(
				FPropertyColumnDelegates::FGetColumnCheckboxState::CreateLambda(
				[&SelectedProperties](const FReplicatedPropertyData& Data)
				{
					const FConcertPropertyChain& DisplayProperty = Data.GetProperty();
					const bool bShouldBeSelected = SelectedProperties.Contains(DisplayProperty)
						|| Algo::AnyOf(SelectedProperties, [&DisplayProperty](const FConcertPropertyChain& SelectedProperty)
						{
							return SelectedProperty.IsChildOf(DisplayProperty);
						});
					return bShouldBeSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}),
				FPropertyColumnDelegates::FOnColumnCheckboxChanged::CreateLambda(
				[OnSelectPropertyDelegate = MoveTemp(OnSelectPropertyDelegate)](bool bIsChecked, const FReplicatedPropertyData& Data)
				{
					OnSelectPropertyDelegate.Execute(Data.GetProperty(), bIsChecked);
				}),
				FPropertyColumnDelegates::FGetToolTipText::CreateLambda(
				[](const FReplicatedPropertyData& Data)
				{
					return LOCTEXT("IncludePropertyTooltip", "Whether the property is included");
				}),
				FPropertyColumnDelegates::FIsEnabled::CreateLambda([bIsEditable](const FReplicatedPropertyData& Data)
				{
					return bIsEditable;
				})),
			FText::GetEmpty(),
			SortPriority,
			20.f
		);
	}
}

#undef LOCTEXT_NAMESPACE