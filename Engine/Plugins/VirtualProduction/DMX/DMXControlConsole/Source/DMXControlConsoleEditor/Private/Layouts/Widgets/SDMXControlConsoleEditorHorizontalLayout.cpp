// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorHorizontalLayout.h"

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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorHorizontalLayout"

namespace UE::DMX::ControlConsoleEditor::Private
{
	void SDMXControlConsoleEditorHorizontalLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupIntoView().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HorizontalScrollBox, SScrollBox)
						.Orientation(Orient_Horizontal)

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
								.OnClicked(this, &SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroup)
								.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorHorizontalLayout::GetAddButtonVisibility))
							]
						]

						+ SScrollBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoSize()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0.f, 8.f)
							[
								SAssignNew(FaderGroupsHorizontalBox, SHorizontalBox)
							]
						]
					]
				]
			];
	}

	bool SDMXControlConsoleEditorHorizontalLayout::CanRefresh() const
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

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementAdded()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console toolkit, can't add new element to layout correctly.")))
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
				SNew(SDMXControlConsoleEditorFaderGroupView, FaderGroup.Get(), EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupViewVisibility, FaderGroup.Get()));

			FaderGroupViews.Insert(FaderGroupWidget, Index);

			FaderGroupsHorizontalBox->InsertSlot(Index)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupWidget
				];
		}
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementRemoved()
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
				FaderGroupsHorizontalBox->RemoveSlot(FaderGroupView.Pin().ToSharedRef());
				FaderGroupViewsToRemove.Add(FaderGroupView);
			}
		}

		FaderGroupViews.RemoveAll([&FaderGroupViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
			{
				return !FaderGroupView.IsValid() || FaderGroupViewsToRemove.Contains(FaderGroupView);
			});
	}

	bool SDMXControlConsoleEditorHorizontalLayout::IsFaderGroupContained(UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (!FaderGroup)
		{
			return false;
		}

		const bool bContainsFaderGroup = Algo::FindByPredicate(FaderGroupViews,
			[FaderGroup](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
			{
				if (FaderGroupView.IsValid())
				{
					const UDMXControlConsoleFaderGroup* OtherFaderGroup = FaderGroupView.Pin()->GetFaderGroup();
					return FaderGroup == OtherFaderGroup;
				}

				return false;
			}) != nullptr;

		return bContainsFaderGroup;
	}

	FReply SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroup()
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

	void SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (!FaderGroup ||
			!HorizontalScrollBox.IsValid())
		{
			return;
		}

		const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>* FaderGroupViewPtr = 
			Algo::FindByPredicate(FaderGroupViews, [FaderGroup](TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView)
			{
					return FaderGroupView.IsValid() && FaderGroupView.Pin()->GetFaderGroup() == FaderGroup;
			});

		if (FaderGroupViewPtr && FaderGroupViewPtr->IsValid())
		{
			HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupViewPtr->Pin(), true, EDescendantScrollDestination::TopOrLeft);
		}
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup) const
	{
		if (!FaderGroup.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetAddButtonVisibility() const
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