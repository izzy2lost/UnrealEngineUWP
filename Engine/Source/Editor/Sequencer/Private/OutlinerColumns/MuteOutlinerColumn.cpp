// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/MuteOutlinerColumn.h"

#include "ISequencerOutlinerColumn.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/MuteEditorExtension.h"
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

	if (InParams.Editor)
	{
		FMuteEditorExtension* MuteEditorExtension = InParams.Editor->CastDynamic<FMuteEditorExtension>();
		if (MuteEditorExtension)
		{
			return MuteEditorExtension->IsNodeMutable(InParams.OutlinerExtension);
		}
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