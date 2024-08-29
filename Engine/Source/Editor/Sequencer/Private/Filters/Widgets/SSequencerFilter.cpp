// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/Widgets/SSequencerFilter.h"
#include "Filters/Menus/SequencerTrackFilterContextMenu.h"
#include "Filters/SequencerFilterBar.h"
#include "Filters/Widgets/SSequencerFilterCheckBox.h"

#define LOCTEXT_NAMESPACE "SSequencerFilter"

void SSequencerFilter::Construct(const FArguments& InArgs
	, const TSharedRef<FSequencerFilterBar>& InFilterBar
	, const TSharedRef<FSequencerTrackFilter>& InFilter)
{
	FilterBar = InFilterBar;
	Filter = InFilter;

	ContextMenu = MakeShared<FSequencerTrackFilterContextMenu>();

	TSharedPtr<SWidget> ContentWidget;
	FName BrushName;

	switch(InArgs._FilterPillStyle)
	{
	case EFilterPillStyle::Basic:
		{
			ContentWidget = ConstructBasicFilterWidget();
			BrushName = TEXT("FilterBar.BasicFilterButton");
			break;
		}
	case EFilterPillStyle::Default:
	default:
		{
			ContentWidget = ConstructDefaultFilterWidget();
			BrushName = TEXT("FilterBar.FilterButton");
			break;
		}
	}

	ChildSlot
	[
		SAssignNew(ToggleButtonPtr, SSequencerFilterCheckBox)
		.Style(FAppStyle::Get(), BrushName)
		.ToolTipText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(Filter.ToSharedRef(), &FSequencerTrackFilter::GetToolTipText)))
		.IsChecked(this, &SSequencerFilter::IsChecked)
		.OnCheckStateChanged(this, &SSequencerFilter::OnFilterToggled)
		.CheckBoxContentUsesAutoWidth(false)
		.OnGetMenuContent(this, &SSequencerFilter::GetRightClickMenuContent)
		[
			ContentWidget.ToSharedRef()
		]
	];

	ToggleButtonPtr->SetOnMouseUp(SSequencerFilterCheckBox::FOnPointerEvent::CreateSP(this, &SSequencerFilter::OnFilterMouseUp));
	ToggleButtonPtr->SetOnDoubleClick(SSequencerFilterCheckBox::FOnPointerEvent::CreateSP(this, &SSequencerFilter::OnFilterDoubleClicked));
}

TSharedRef<SWidget> SSequencerFilter::ConstructBasicFilterWidget()
{
	return SNew(STextBlock)
		.Margin(0.f)
		.TextStyle(FAppStyle::Get(), TEXT("SmallText"))
		.Text(Filter.ToSharedRef(), &FSequencerTrackFilter::GetDisplayName);
}

TSharedRef<SWidget> SSequencerFilter::ConstructDefaultFilterWidget()
{
	const TSharedRef<FSequencerFilterBar> FilterBarRef = FilterBar.ToSharedRef();
	const TSharedRef<FSequencerTrackFilter> FilterRef = Filter.ToSharedRef();

	return SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("FilterBar.FilterBackground")))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(8, 16)) // was 22
				.Image(FAppStyle::Get().GetBrush(TEXT("FilterBar.FilterImage")))
				.ColorAndOpacity(this, &SSequencerFilter::GetFilterImageColorAndOpacity)
			]
			+ SHorizontalBox::Slot()
			.Padding(TAttribute<FMargin>(this, &SSequencerFilter::GetFilterNamePadding))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
				.Text(FilterRef, &FSequencerTrackFilter::GetDisplayName)
				.IsEnabled(FilterBarRef, &FSequencerFilterBar::IsFilterActive, FilterRef)
			]
		];
}

const TSharedPtr<FSequencerTrackFilter> SSequencerFilter::GetFilter() const
{
	return Filter;
}

bool SSequencerFilter::IsActive() const
{
	return FilterBar->IsFilterActive(Filter.ToSharedRef());
}

void SSequencerFilter::OnFilterToggled(const ECheckBoxState NewState)
{
	const bool bNewActive = NewState == ECheckBoxState::Checked;
	FilterBar->SetFilterActive(Filter.ToSharedRef(), bNewActive, true);
}

FReply SSequencerFilter::OnFilterMouseUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsControlDown())
	{
		FilterBar->ActivateCommonFilters(true, {}, {});
	}
	else if (InMouseEvent.IsAltDown())
	{
		FilterBar->ActivateCommonFilters(false, {}, {});
	}
	else if(InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		FilterBar->SetFilterEnabled(Filter.ToSharedRef(), false, true);
	}

	return FReply::Handled();
}

FReply SSequencerFilter::OnFilterDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	FilterBar->ActivateCommonFilters(false, {}, {});
	FilterBar->ActivateCustomTextFilters(false, {});

	FilterBar->SetFilterActive(Filter.ToSharedRef(), true, true);

	return FReply::Handled();
}

TSharedRef<SWidget> SSequencerFilter::GetRightClickMenuContent()
{
	return ContextMenu->CreateMenuWidget(SharedThis(this));
}

ECheckBoxState SSequencerFilter::IsChecked() const
{
	return IsActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FSlateColor SSequencerFilter::GetFilterImageColorAndOpacity() const
{
	return IsActive() ? Filter->GetColor() : FAppStyle::Get().GetSlateColor(TEXT("Colors.Recessed"));
}

EVisibility SSequencerFilter::GetFilterOverlayVisibility() const
{
	return IsActive() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FMargin SSequencerFilter::GetFilterNamePadding() const
{
	return ToggleButtonPtr->IsPressed() ? FMargin(3, 1, 3, 0) : FMargin(3, 0, 3, 0);
}

#undef LOCTEXT_NAMESPACE
