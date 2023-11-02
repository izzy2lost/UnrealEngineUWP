// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorVerticalLayout.h"

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
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorVerticalLayout"

namespace UE::DMX::ControlConsoleEditor::Private
{
	void SDMXControlConsoleEditorVerticalLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorVerticalLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupIntoView().AddSP(this, &SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView);

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
						SNew(SScrollBox)
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
									.OnClicked(this, &SDMXControlConsoleEditorVerticalLayout::OnAddFirstFaderGroup)
									.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorVerticalLayout::GetAddButtonVisibility))
								]
							]

							+ SScrollBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Top)
							.AutoSize()
							[
								SAssignNew(FaderGroupsVerticalBox, SVerticalBox)
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

	bool SDMXControlConsoleEditorVerticalLayout::CanRefresh() const
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

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
		if (AllFaderGroups.Num() == FaderGroupViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorVerticalLayout::OnLayoutElementAdded()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
		{
			if (!FaderGroup.IsValid())
			{
				continue;
			}

			if (IsFaderGroupContained(FaderGroup.Get()))
			{
				continue;
			}

			const int32 Index = AllFaderGroups.IndexOfByKey(FaderGroup);

			const TSharedRef<SDMXControlConsoleEditorFaderGroupView> FaderGroupWidget =
				SNew(SDMXControlConsoleEditorFaderGroupView, FaderGroup.Get(), EditorModel.Get());

			FaderGroupViews.Insert(FaderGroupWidget, Index);

			FaderGroupsVerticalBox->InsertSlot(Index)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.Padding(0.f, 8.f)
				[
					SNew(SHorizontalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorVerticalLayout::GetFaderGroupViewVisibility, FaderGroup.Get()))
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(8.f, 0.f)
					[
						FaderGroupWidget
					]
				];
		}
	}

	void SDMXControlConsoleEditorVerticalLayout::OnLayoutElementRemoved()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, can't remove element from the layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();

		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>>FaderGroupViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView : FaderGroupViews)
		{
			if (!FaderGroupView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupView.Pin()->GetFaderGroup();
			if (!FaderGroup || !AllFaderGroups.Contains(FaderGroup))
			{
				const TSharedPtr<SWidget> ParentWidget = FaderGroupView.Pin()->GetParentWidget();
				if (ParentWidget.IsValid())
				{
					FaderGroupsVerticalBox->RemoveSlot(ParentWidget.ToSharedRef());
					FaderGroupViewsToRemove.Add(FaderGroupView);
				}
			}
		}

		FaderGroupViews.RemoveAll([&FaderGroupViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
			{
				return !FaderGroupView.IsValid() || FaderGroupViewsToRemove.Contains(FaderGroupView);
			});
	}

	bool SDMXControlConsoleEditorVerticalLayout::IsFaderGroupContained(UDMXControlConsoleFaderGroup* FaderGroup)
	{
		const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroupWeakPtr = FaderGroup;

		auto IsContainedLambda = [FaderGroupWeakPtr](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
		{
			if (!FaderGroupView.IsValid())
			{
				return false;
			}

			const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup = FaderGroupView.Pin()->GetFaderGroup();
			if (!FaderGroup.IsValid())
			{
				return false;
			}

			return FaderGroup == FaderGroupWeakPtr;
		};

		return FaderGroupViews.ContainsByPredicate(IsContainedLambda);
	}

	FReply SDMXControlConsoleEditorVerticalLayout::OnAddFirstFaderGroup()
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

	void SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (!FaderGroup)
		{
			return;
		}

		if (VerticalScrollBox.IsValid())
		{
			const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>* FaderGroupView =
				Algo::FindByPredicate(FaderGroupViews, [FaderGroup](TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView)
					{
						return FaderGroupView.IsValid() && FaderGroupView.Pin()->GetFaderGroup() == FaderGroup;
					});

			if (FaderGroupView && FaderGroupView->IsValid())
			{
				VerticalScrollBox->ScrollDescendantIntoView(FaderGroupView->Pin(), true, EDescendantScrollDestination::Center);
			}
		}
	}

	EVisibility SDMXControlConsoleEditorVerticalLayout::GetFaderGroupViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup) const
	{
		if (!FaderGroup.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorVerticalLayout::GetAddButtonVisibility() const
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