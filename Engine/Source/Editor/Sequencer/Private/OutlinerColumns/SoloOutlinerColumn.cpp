// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutlinerColumns/SoloOutlinerColumn.h"

#include "ISequencerOutlinerColumn.h"
#include "MVVM/SharedViewModelData.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ISoloableExtension.h"
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

	if (FSoloStateCacheExtension* SoloStateCache = InParams.OutlinerExtension.AsModel()->GetSharedData()->CastThis<FSoloStateCacheExtension>())
	{
		return EnumHasAnyFlags(SoloStateCache->GetCachedFlags(InParams.OutlinerExtension), ECachedSoloState::Soloable | ECachedSoloState::SoloableChildren);
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