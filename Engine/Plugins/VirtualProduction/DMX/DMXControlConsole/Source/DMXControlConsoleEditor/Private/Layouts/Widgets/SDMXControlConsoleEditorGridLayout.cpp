// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorGridLayout.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Editor.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Views/SDMXControlConsoleEditorLayoutRowView.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorGridLayout"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorGridLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorGridLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupIntoView().AddSP(this, &SDMXControlConsoleEditorGridLayout::OnScrollIntoView);

		const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical);

		const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HorizontalScrollBox, SScrollBox)
						.ExternalScrollbar(HorizontalScrollBar)
						.Orientation(Orient_Horizontal)

						+ SScrollBox::Slot()
						[
							SAssignNew(VerticalScrollBox, SScrollBox)
							.ExternalScrollbar(VerticalScrollBar)
							.Orientation(Orient_Vertical)

							+ SScrollBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(50.f)
								.HeightOverride(50.f)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(SDMXControlConsoleEditorAddButton)
									.OnClicked(this, &SDMXControlConsoleEditorGridLayout::OnAddFirstFaderGroup)
									.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorGridLayout::GetAddButtonVisibility))
								]
							]

							+ SScrollBox::Slot()
							[
								SAssignNew(LayoutRowsVerticalBox, SVerticalBox)
							]
						]
					]

					// Horizontal ScrollBar slot
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VerticalScrollBar
					]
				]

				// Vertical Scrollbar slot
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					HorizontalScrollBar
				]
			];
	}

	bool SDMXControlConsoleEditorGridLayout::CanRefresh() const
	{
		if (!EditorModel.IsValid() || !EditorLayout.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return false;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return false;
		}

		if (ActiveLayout->GetLayoutRows().Num() == LayoutRowViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorGridLayout::OnLayoutElementAdded()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid Control Console Layouts, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = ActiveLayout->GetLayoutRows();
		for (UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
		{
			if (!LayoutRow)
			{
				continue;
			}

			if (IsLayoutRowContained(LayoutRow))
			{
				continue;
			}

			const TSharedRef<SDMXControlConsoleEditorLayoutRowView> LayoutRowWidget =
				SNew(SDMXControlConsoleEditorLayoutRowView, LayoutRow, EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorGridLayout::GetLayoutRowViewVisibility, LayoutRow));

			const int32 RowIndex = LayoutRow->GetRowIndex();
			if (ensureMsgf(RowIndex == 0 || LayoutRowViews.IsValidIndex(RowIndex - 1), TEXT("Unexpected, invalid layout row index when trying to add layout row view.")))
			{
				LayoutRowViews.Insert(LayoutRowWidget, RowIndex);

				LayoutRowsVerticalBox->InsertSlot(RowIndex)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.Padding(0.f, 8.f)
					[
						LayoutRowWidget
					];
			}
			else
			{
				LayoutRowViews.Add(LayoutRowWidget);

				LayoutRowsVerticalBox->AddSlot()
					.AutoHeight()
					.VAlign(VAlign_Top)
					.Padding(0.f, 8.f)
					[
						LayoutRowWidget
					];
			}
		}
	}

	void SDMXControlConsoleEditorGridLayout::OnLayoutElementRemoved()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, can't remove element from the layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = ActiveLayout->GetLayoutRows();

		TArray<TWeakPtr<SDMXControlConsoleEditorLayoutRowView>> LayoutRowViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& LayoutRowView : LayoutRowViews)
		{
			if (!LayoutRowView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRowView.Pin()->GetLayoutRow();
			if (!LayoutRow || !LayoutRows.Contains(LayoutRow))
			{
				LayoutRowsVerticalBox->RemoveSlot(LayoutRowView.Pin().ToSharedRef());
				LayoutRowViewsToRemove.Add(LayoutRowView);
			}
		}

		LayoutRowViews.RemoveAll([&LayoutRowViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView)
			{
				return !LayoutRowView.IsValid() || LayoutRowViewsToRemove.Contains(LayoutRowView);
			});
	}

	bool SDMXControlConsoleEditorGridLayout::IsLayoutRowContained(UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRowWeakPtr = LayoutRow;

		auto IsContainedLambda = [LayoutRowWeakPtr](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView)
		{
			if (!LayoutRowView.IsValid())
			{
				return false;
			}

			const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow = LayoutRowView.Pin()->GetLayoutRow();
			if (!LayoutRow.IsValid())
			{
				return false;
			}

			return LayoutRow == LayoutRowWeakPtr;
		};

		return LayoutRowViews.ContainsByPredicate(IsContainedLambda);
	}

	TSharedPtr<SDMXControlConsoleEditorLayoutRowView> SDMXControlConsoleEditorGridLayout::FindLayoutRowView(const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		if (LayoutRow)
		{
			const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>* LayoutRowViewPtr = Algo::FindByPredicate(LayoutRowViews, [LayoutRow](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& WeakLayoutRowView)
				{
					return WeakLayoutRowView.IsValid() && WeakLayoutRowView.Pin()->GetLayoutRow() == LayoutRow;
				});

			if (LayoutRowViewPtr && LayoutRowViewPtr->IsValid())
			{
				return LayoutRowViewPtr->Pin();
			}
		}

		return nullptr;
	}

	FReply SDMXControlConsoleEditorGridLayout::OnAddFirstFaderGroup()
	{
		UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, can't add fader group correctly.")))
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction AddFaderGroupTransaction(LOCTEXT("AddFaderGroupTransaction", "Add Fader Group"));
		ControlConsoleData->PreEditChange(nullptr);
		const UDMXControlConsoleFaderGroupRow* NewRow = ControlConsoleData->AddFaderGroupRow(0);
		ControlConsoleData->PostEditChange();

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, can't add fader group correctly.")))
		{
			return FReply::Unhandled();
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return FReply::Unhandled();
		}

		ActiveLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(0);
		ActiveLayout->PostEditChange();
		if (!NewRow || !NewLayoutRow)
		{
			return FReply::Unhandled();
		}

		UDMXControlConsoleFaderGroup* NewFaderGroup = !NewRow->GetFaderGroups().IsEmpty() ? NewRow->GetFaderGroups()[0] : nullptr;
		NewLayoutRow->PreEditChange(nullptr);
		NewLayoutRow->AddToLayoutRow(NewFaderGroup);
		NewLayoutRow->PostEditChange();

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorGridLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleLayouts || !FaderGroup)
		{
			return;
		}

		if (!HorizontalScrollBox.IsValid() || !VerticalScrollBox.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->GetLayoutRow(FaderGroup);
		const TSharedPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView = FindLayoutRowView(LayoutRow);
		if (LayoutRowView.IsValid())
		{
			const TSharedPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView = LayoutRowView->FindFaderGroupView(FaderGroup);
			if (FaderGroupView.IsValid())
			{
				VerticalScrollBox->ScrollDescendantIntoView(LayoutRowView, true, EDescendantScrollDestination::Center);
				HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupView, true, EDescendantScrollDestination::TopOrLeft);
			}
		}
	}

	EVisibility SDMXControlConsoleEditorGridLayout::GetLayoutRowViewVisibility(TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow) const
	{
		if (!LayoutRow.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const auto IsAnyFaderGroupVisibleLambda = [](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			if (!FaderGroup.IsValid())
			{
				return false;
			}

			return FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
		};

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
		const bool bIsVisible = Algo::AnyOf(FaderGroups, IsAnyFaderGroupVisibleLambda);
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorGridLayout::GetAddButtonVisibility() const
	{
		bool bIsVisible = false;

		// Visible if there are no layout rows and there's no global filter
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (ControlConsoleData && ControlConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			bIsVisible =
				IsValid(ActiveLayout) &&
				ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked() &&
				ControlConsoleData->FilterString.IsEmpty() &&
				(ActiveLayout->GetLayoutRows().IsEmpty() ||
				ActiveLayout->GetAllActiveFaderGroups().IsEmpty());
		}

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE