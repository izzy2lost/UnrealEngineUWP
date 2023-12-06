// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SOutlinerColumnBorder.h"

#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SScaleBox.h"

namespace UE::Sequencer
{

void SOutlinerColumnInnerBorder::Construct(const FArguments& InArgs, const FCreateOutlinerColumnParams& InParams)
{
	bIsMouseOverInnerBorder = false;

	WeakOutlinerExtension = InParams.OutlinerExtension;
	WeakEditor = InParams.Editor;

	TSharedPtr<SScaleBox> UniformScaleBox;

	// Size of outliner column widgets stretch to desired height and are usually 14x14 based off padding and fixed width of columns
	TSharedRef<SWidget>	FinalWidget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(UniformScaleBox, SScaleBox)
			[
				InArgs._Content.Widget
			]
		];

	UniformScaleBox->SetStretch(EStretch::ScaleToFit);

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

void SOutlinerColumnBorder::Construct(const FArguments& InArgs, const FCreateOutlinerColumnParams& InParams, const bool bHasInnerBorder)
{
	WeakOutlinerExtension = InParams.OutlinerExtension;
	WeakEditor = InParams.Editor;

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

	ChildSlot
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		InnerWidget.ToSharedRef()
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

} // namespace UE::Sequencer

