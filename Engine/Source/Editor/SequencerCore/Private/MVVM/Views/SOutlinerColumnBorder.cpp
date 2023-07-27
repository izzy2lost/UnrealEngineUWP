// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SOutlinerColumnBorder.h"

#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Styling/StyleColors.h"

namespace UE::Sequencer
{

void SOutlinerColumnInnerBorder::Construct(const FArguments& InArgs, const FCreateOutlinerColumnParams& InParams)
{
	bIsMouseOverInnerBorder = false;

	WeakOutlinerExtension = InParams.OutlinerExtension;
	WeakEditor = InParams.Editor;

	BackgroundBrush = FAppStyle::GetBrush("Sequencer.Column.OutlinerColumnBox");

	// Size of outliner column widgets is currently 12x12
	TSharedRef<SWidget>	FinalWidget = SNew(SBox)
		.WidthOverride(12.0f)
		.HeightOverride(12.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.BorderImage(this, &SOutlinerColumnInnerBorder::GetBorderImage)
			.Padding(FMargin(1.f))
			[
				InArgs._Content.Widget
			]
		];

	ChildSlot
	[
		FinalWidget
	];
}

void SOutlinerColumnInnerBorder::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsMouseOverInnerBorder = true;
}

void SOutlinerColumnInnerBorder::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bIsMouseOverInnerBorder = false;
}

FReply SOutlinerColumnBorder::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

const FSlateBrush* SOutlinerColumnInnerBorder::GetBorderImage() const
{
	return BackgroundBrush;
}

void SOutlinerColumnBorder::Construct(const FArguments& InArgs, const FCreateOutlinerColumnParams& InParams, const bool bHasInnerBorder)
{
	WeakOutlinerExtension = InParams.OutlinerExtension;
	WeakEditor = InParams.Editor;

	BackgroundBrush = FAppStyle::GetBrush("Sequencer.AnimationOutliner.DefaultBorder");

	TSharedPtr<SWidget> InnerWidget;
	if (bHasInnerBorder)
	{
		InnerWidget = SNew(SOutlinerColumnInnerBorder, InParams)
			[
				InArgs._Content.Widget
			];
	}
	else
	{
		InnerWidget = InArgs._Content.Widget;
	}

	TSharedRef<SWidget>	FinalWidget = SNew(SBorder)
		.VAlign(VAlign_Center)
		.BorderImage(this, &SOutlinerColumnBorder::GetBorderImage)
		.BorderBackgroundColor(this, &SOutlinerColumnBorder::GetBackgroundTint)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.0f))
		[
			InnerWidget.ToSharedRef()
		];

	ChildSlot
	[
		FinalWidget
	];
}

void SOutlinerColumnBorder::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TViewModelPtr<IOutlinerExtension> DataModel = WeakOutlinerExtension.Pin();
	TSharedPtr<FEditorViewModel> Editor = WeakEditor.Pin();
	if (DataModel && Editor)
	{
		Editor->GetOutliner()->SetHoveredItem(DataModel);
	}
	SWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SOutlinerColumnBorder::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	TSharedPtr<FEditorViewModel> Editor = WeakEditor.Pin();
	if (Editor)
	{
		Editor->GetOutliner()->SetHoveredItem(nullptr);
	}
	SWidget::OnMouseLeave(MouseEvent);
}

const FSlateBrush* SOutlinerColumnBorder::GetBorderImage() const
{
	return BackgroundBrush;
}

FSlateColor SOutlinerColumnBorder::GetBackgroundTint() const
{
	TSharedPtr<FEditorViewModel> Editor = WeakEditor.Pin();
	TViewModelPtr<IOutlinerExtension> OutlinerItem = WeakOutlinerExtension.Pin();

	if (!Editor || !OutlinerItem)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	if (!OutlinerItem->HasBackground())
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	EOutlinerSelectionState SelectionState = OutlinerItem->GetSelectionState();

	if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::SelectedDirectly))
	{
		return FStyleColors::Select;
	}
	if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::HasSelectedKeys | EOutlinerSelectionState::HasSelectedTrackAreaItems))
	{
		return FStyleColors::Header;
	}

	// If this is collapsed but has any children with selected keys or sections, we report that state on the parent
	if (!OutlinerItem->IsExpanded())
	{
		for (TViewModelPtr<IOutlinerExtension> Child : OutlinerItem.AsModel()->GetDescendantsOfType<IOutlinerExtension>())
		{
			if (EnumHasAnyFlags(SelectionState, EOutlinerSelectionState::HasSelectedKeys | EOutlinerSelectionState::HasSelectedTrackAreaItems))
			{
				return FStyleColors::Header;
			}
		}
	}

	// Use same hovered and default colors as SOutlinerItemViewBase
	if (Editor->GetOutliner()->GetHoveredItem() == OutlinerItem)
	{
		return FLinearColor(FColor(72, 72, 72, 255));
	}

	return FLinearColor(FColor(62, 62, 62, 255));
}

} // namespace UE::Sequencer

