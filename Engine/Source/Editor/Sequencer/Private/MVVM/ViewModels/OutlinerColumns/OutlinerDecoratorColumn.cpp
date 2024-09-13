// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/OutlinerColumns/OutlinerDecoratorColumn.h"

#include "Sequencer.h"
#include "MVVM/Extensions/IConditionableExtension.h"
#include "MVVM/SharedViewModelData.h"
#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecorator.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/OutlinerDecorators/SConditionDecoratorWidget.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FOutlinerDecoratorColumn"

namespace UE::Sequencer
{

FOutlinerDecoratorColumn::FOutlinerDecoratorColumn()
{
	Name = FCommonOutlinerNames::Decorator;
	Label = LOCTEXT("DecoratorColumnLabel", "Decorators");
	Position = FOutlinerColumnPosition{ 0, EOutlinerColumnGroup::FarLeftGutter };
	Layout = FOutlinerColumnLayout{ 14, FMargin(0.f, 0.f), HAlign_Fill, VAlign_Fill, EOutlinerColumnSizeMode::Fixed, EOutlinerColumnFlags::None };
}

bool FOutlinerDecoratorColumn::IsItemCompatibleWithColumn(const FCreateOutlinerColumnParams& InParams) const
{
	TViewModelPtr<FSequencerEditorViewModel> Editor = InParams.Editor->CastThisShared<FSequencerEditorViewModel>();
	if (!Editor)
	{
		return false;
	}

	TSharedPtr<FSequencer> Sequencer = Editor->GetSequencerImpl();
	if (!Sequencer)
	{
		return false;
	}

	for (const TTuple< FName, TSharedPtr<IOutlinerDecorator> >& OutlinerDecorator : Sequencer->GetOutlinerDecorators())
	{
		if (OutlinerDecorator.Value.Get()->IsItemCompatibleWithDecorator(InParams))
		{
			return true;
		}
	}

	return false;
}

TSharedPtr<SWidget> FOutlinerDecoratorColumn::CreateColumnWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow)
{
	TViewModelPtr<FSequencerEditorViewModel> Editor = InParams.Editor->CastThisShared<FSequencerEditorViewModel>();
	if (!Editor)
	{
		return nullptr;
	}

	TSharedPtr<FSequencer> Sequencer = Editor->GetSequencerImpl();
	if (!Sequencer)
	{
		return nullptr;
	}

	ColumnWidget = SNew(SHorizontalBox);

	TArray<TSharedPtr<IOutlinerDecorator> > CompatibleDecorators;

	for (const TTuple< FName, TSharedPtr<IOutlinerDecorator> >& OutlinerDecorator : Sequencer->GetOutlinerDecorators())
	{
		TSharedPtr<IOutlinerDecorator> Decorator = OutlinerDecorator.Value;

		if (Decorator->IsItemCompatibleWithDecorator(InParams))
		{
			CompatibleDecorators.Add(Decorator);
		}
	}

	const int32 NumCompatibleDecorators = CompatibleDecorators.Num();

	for (TSharedPtr<IOutlinerDecorator>& Decorator : CompatibleDecorators)
	{
		ColumnWidget->AddSlot()
		[
			Decorator->CreateDecoratorWidget(InParams, TreeViewRow, SharedThis(this), NumCompatibleDecorators).ToSharedRef()
		];
	}

	return ColumnWidget;
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE