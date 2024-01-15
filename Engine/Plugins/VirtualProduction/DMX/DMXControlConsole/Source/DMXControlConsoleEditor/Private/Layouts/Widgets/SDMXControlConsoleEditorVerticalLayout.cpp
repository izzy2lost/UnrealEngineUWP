// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorVerticalLayout.h"

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
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorVerticalLayout"

namespace UE::DMX::Private
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
		EditorModel->GetOnScrollFaderGroupControllerIntoView().AddSP(this, &SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView);

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
									.OnClicked(this, &SDMXControlConsoleEditorVerticalLayout::OnAddFirstFaderGroupController)
									.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorVerticalLayout::GetAddButtonVisibility))
								]
							]

							+ SScrollBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Top)
							.AutoSize()
							[
								SAssignNew(FaderGroupControllersVerticalBox, SVerticalBox)
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

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		if (AllFaderGroupControllers.Num() == FaderGroupControllerViews.Num())
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
				SNew(SDMXControlConsoleEditorFaderGroupControllerView, FaderGroupControllerModel, EditorModel.Get());

			const int32 Index = AllFaderGroupControllers.IndexOfByKey(FaderGroupController);
			FaderGroupControllerViews.Insert(FaderGroupControllerWidget, Index);

			FaderGroupControllersVerticalBox->InsertSlot(Index)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.Padding(0.f, 8.f)
				[
					SNew(SHorizontalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorVerticalLayout::GetFaderGroupControllerViewVisibility, FaderGroupController))
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(8.f, 0.f)
					[
						FaderGroupControllerWidget
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
				const TSharedPtr<SWidget> ParentWidget = FaderGroupControllerView.Pin()->GetParentWidget();
				if (ParentWidget.IsValid())
				{
					FaderGroupControllersVerticalBox->RemoveSlot(ParentWidget.ToSharedRef());
					FaderGroupControllerViewsToRemove.Add(FaderGroupControllerView);
				}
			}
		}

		FaderGroupControllerViews.RemoveAll([&FaderGroupControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
			{
				return !FaderGroupControllerView.IsValid() || FaderGroupControllerViewsToRemove.Contains(FaderGroupControllerView);
			});
	}

	bool SDMXControlConsoleEditorVerticalLayout::IsFaderGroupControllerContained(UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		const TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupControllerWeakPtr = FaderGroupController;

		auto IsContainedLambda = [FaderGroupControllerWeakPtr](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
		{
			if (!FaderGroupControllerView.IsValid())
			{
				return false;
			}

			const TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
			if (!FaderGroupController.IsValid())
			{
				return false;
			}

			return FaderGroupController == FaderGroupControllerWeakPtr;
		};

		return FaderGroupControllerViews.ContainsByPredicate(IsContainedLambda);
	}

	FReply SDMXControlConsoleEditorVerticalLayout::OnAddFirstFaderGroupController()
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

	void SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController)
		{
			return;
		}

		if (VerticalScrollBox.IsValid())
		{
			const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>* FaderGroupControllerView =
				Algo::FindByPredicate(FaderGroupControllerViews, [FaderGroupController](TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView)
					{
						return FaderGroupControllerView.IsValid() && FaderGroupControllerView.Pin()->GetFaderGroupController() == FaderGroupController;
					});

			if (FaderGroupControllerView && FaderGroupControllerView->IsValid())
			{
				VerticalScrollBox->ScrollDescendantIntoView(FaderGroupControllerView->Pin(), true, EDescendantScrollDestination::Center);
			}
		}
	}

	EVisibility SDMXControlConsoleEditorVerticalLayout::GetFaderGroupControllerViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController) const
	{
		if (!FaderGroupController.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroupController->IsActive() && FaderGroupController->IsMatchingFilter();
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
				ActiveLayout->GetAllActiveFaderGroupControllers().IsEmpty());
		}

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE