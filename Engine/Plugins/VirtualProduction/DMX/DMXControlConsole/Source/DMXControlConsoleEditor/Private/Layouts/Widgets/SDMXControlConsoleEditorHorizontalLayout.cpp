// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorHorizontalLayout.h"

#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Editor.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Views/SDMXControlConsoleEditorFaderGroupControllerView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorHorizontalLayout"

namespace UE::DMX::Private
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
		EditorModel->GetOnScrollFaderGroupControllerIntoView().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView);

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
								.OnClicked(this, &SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroupController)
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
								SAssignNew(FaderGroupControllersHorizontalBox, SHorizontalBox)
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

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		if (AllFaderGroupControllers.Num() == FaderGroupControllerViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementAdded()
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

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : AllFaderGroupControllers)
		{
			if (!FaderGroupController)
			{
				continue;
			}

			if (IsFaderGroupControllerContained(FaderGroupController))
			{
				continue;
			}

			const TSharedRef<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = MakeShared<FDMXControlConsoleFaderGroupControllerModel>(FaderGroupController, EditorModel);
			const TSharedRef<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerWidget =
				SNew(SDMXControlConsoleEditorFaderGroupControllerView, FaderGroupControllerModel, EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupControllerViewVisibility, FaderGroupController));

			const int32 Index = AllFaderGroupControllers.IndexOfByKey(FaderGroupController);
			FaderGroupControllerViews.Insert(FaderGroupControllerWidget, Index);

			FaderGroupControllersHorizontalBox->InsertSlot(Index)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupControllerWidget
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

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();

		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>>FaderGroupControllerViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView : FaderGroupControllerViews)
		{
			if (!FaderGroupControllerView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
			if (!FaderGroupController || !AllFaderGroupControllers.Contains(FaderGroupController))
			{
				FaderGroupControllersHorizontalBox->RemoveSlot(FaderGroupControllerView.Pin().ToSharedRef());
				FaderGroupControllerViewsToRemove.Add(FaderGroupControllerView);
			}
		}

		FaderGroupControllerViews.RemoveAll([&FaderGroupControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
			{
				return !FaderGroupControllerView.IsValid() || FaderGroupControllerViewsToRemove.Contains(FaderGroupControllerView);
			});
	}

	bool SDMXControlConsoleEditorHorizontalLayout::IsFaderGroupControllerContained(UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController)
		{
			return false;
		}

		const bool bContainsFaderGroupController = Algo::FindByPredicate(FaderGroupControllerViews,
			[FaderGroupController](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
			{
				if (FaderGroupControllerView.IsValid())
				{
					const UDMXControlConsoleFaderGroupController* OtherFaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
					return FaderGroupController == OtherFaderGroupController;
				}

				return false;
			}) != nullptr;

		return bContainsFaderGroupController;
	}

	FReply SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroupController()
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
		if (!NewFaderGroup)
		{
			return FReply::Unhandled();
		}

		// Create a new controller for the fader group
		NewLayoutRow->PreEditChange(nullptr);
		UDMXControlConsoleFaderGroupController* NewController = NewLayoutRow->CreateFaderGroupController(NewFaderGroup, NewFaderGroup->GetFaderGroupName());
		NewLayoutRow->PostEditChange();
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController ||
			!HorizontalScrollBox.IsValid())
		{
			return;
		}

		const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>* FaderGroupControllerViewPtr =
			Algo::FindByPredicate(FaderGroupControllerViews,
				[FaderGroupController](TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView)
				{
						return FaderGroupControllerView.IsValid() && FaderGroupControllerView.Pin()->GetFaderGroupController() == FaderGroupController;
				});

		if (FaderGroupControllerViewPtr && FaderGroupControllerViewPtr->IsValid())
		{
			HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupControllerViewPtr->Pin(), true, EDescendantScrollDestination::TopOrLeft);
		}
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupControllerViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController) const
	{
		if (!FaderGroupController.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroupController->IsActive() && FaderGroupController->IsMatchingFilter();
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
				ActiveLayout->GetAllActiveFaderGroupControllers().IsEmpty());
		}

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE