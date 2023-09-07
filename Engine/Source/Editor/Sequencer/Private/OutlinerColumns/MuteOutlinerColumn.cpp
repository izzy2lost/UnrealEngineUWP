// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/MuteOutlinerColumn.h"

#include "ISequencerOutlinerColumn.h"

#include "MVVM/SharedViewModelData.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IMutableExtension.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/OutlinerColumns/SMuteColumnWidget.h"

#define LOCTEXT_NAMESPACE "FMuteOutlinerColumn"

FName FMuteOutlinerColumn::GetColumnName() const
{
	return FName(TEXT("Mute"));
}

FText FMuteOutlinerColumn::GetColumnLabel() const
{
	return LOCTEXT("MuteColumnLabel", "Mute");
}

bool FMuteOutlinerColumn::IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	if (FMuteStateCacheExtension* MuteStateCache = InParams.OutlinerExtension.AsModel()->GetSharedData()->CastThis<FMuteStateCacheExtension>())
	{
		return EnumHasAnyFlags(MuteStateCache->GetCachedFlags(InParams.OutlinerExtension), ECachedMuteState::Mutable | ECachedMuteState::MutableChildren);
	}

	return false;
}

TSharedRef<ISequencerOutlinerColumn> FMuteOutlinerColumn::CreateOutlinerColumn()
{
	return MakeShareable(new FMuteOutlinerColumn());
}

TSharedRef<SWidget> FMuteOutlinerColumn::CreateColumnWidget(const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	return SNew(SMuteColumnWidget,
		InWeakOutlinerColumn,
		InParams);
}

#undef LOCTEXT_NAMESPACE