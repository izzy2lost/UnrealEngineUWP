// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/LockOutlinerColumn.h"

#include "ISequencerOutlinerColumn.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Extensions/ILockableExtension.h"
#include "MVVM/SharedViewModelData.h"
#include "Widgets/OutlinerColumns/SLockColumnWidget.h"

#define LOCTEXT_NAMESPACE "FLockOutlinerColumn"

FName FLockOutlinerColumn::GetColumnName() const
{
	return FName(TEXT("Lock"));
}

FText FLockOutlinerColumn::GetColumnLabel() const
{
	return LOCTEXT("LockColumnLabel", "Lock");
}

bool FLockOutlinerColumn::IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	if (FLockStateCacheExtension* LockStateCache = InParams.OutlinerExtension.AsModel()->GetSharedData()->CastThis<FLockStateCacheExtension>())
	{
		return EnumHasAnyFlags(LockStateCache->GetCachedFlags(InParams.OutlinerExtension), ECachedLockState::Lockable | ECachedLockState::LockableChildren);
	}

	return false;
}

TSharedRef<ISequencerOutlinerColumn> FLockOutlinerColumn::CreateOutlinerColumn()
{
	return MakeShareable(new FLockOutlinerColumn());
}

TSharedRef<SWidget> FLockOutlinerColumn::CreateColumnWidget(const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	return SNew(SLockColumnWidget,
		InWeakOutlinerColumn,
		InParams);
}

#undef LOCTEXT_NAMESPACE