// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/SoloOutlinerColumn.h"

#include "ISequencerOutlinerColumn.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/SoloEditorExtension.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/OutlinerColumns/SSoloColumnWidget.h"

#define LOCTEXT_NAMESPACE "FSoloOutlinerColumn"

FName FSoloOutlinerColumn::GetColumnName() const
{
	return FName(TEXT("Solo"));
}

FText FSoloOutlinerColumn::GetColumnLabel() const
{
	return LOCTEXT("SoloColumnLabel", "Solo");
}

bool FSoloOutlinerColumn::IsItemCompatibleWithColumn(const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	if (InParams.Editor)
	{
		FSoloEditorExtension* SoloEditorExtension = InParams.Editor->CastDynamic<FSoloEditorExtension>();
		if (SoloEditorExtension)
		{
			return SoloEditorExtension->IsNodeSoloable(InParams.OutlinerExtension);
		}
	}

	return false;
}

TSharedRef<ISequencerOutlinerColumn> FSoloOutlinerColumn::CreateOutlinerColumn()
{
	return MakeShareable(new FSoloOutlinerColumn());
}

TSharedRef<SWidget> FSoloOutlinerColumn::CreateColumnWidget(const TWeakPtr<ISequencerOutlinerColumn> InWeakOutlinerColumn, const UE::Sequencer::FCreateOutlinerColumnParams& InParams) const
{
	using namespace UE::Sequencer;

	return SNew(SSoloColumnWidget,
		InWeakOutlinerColumn,
		InParams);
}

#undef LOCTEXT_NAMESPACE