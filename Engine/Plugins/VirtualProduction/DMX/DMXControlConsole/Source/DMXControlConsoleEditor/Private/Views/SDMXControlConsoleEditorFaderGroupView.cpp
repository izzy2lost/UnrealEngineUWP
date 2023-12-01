// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupView.h"

#include "Algo/Find.h"
#include "Controllers/DMXControlConsoleElementController.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleRawFader.h"
#include "Framework/Application/SlateApplication.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Views/SDMXControlConsoleEditorElementControllerView.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupToolbar.h"
#include "Widgets/SDMXControlConsoleEditorMatrixCell.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupView"

namespace UE::DMX::Private
{
	namespace DMXControlConsoleEditorFaderGroupView
	{
		namespace Private
		{
			constexpr float CollapsedViewModeHeight = 280.f;
			constexpr float ExpandedViewModeHeight = 360.f;
		}
	}

	SDMXControlConsoleEditorFaderGroupView::SDMXControlConsoleEditorFaderGroupView()
		: ViewMode(EDMXControlConsoleEditorViewMode::Expanded)
	{}

	void SDMXControlConsoleEditorFaderGroupView::Construct(const FArguments& InArgs, UDMXControlConsoleFaderGroup* InFaderGroup, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct fader group view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroup, TEXT("Invalid fader group, cannot create fader group view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		FaderGroup = InFaderGroup;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnElementControllerAdded);
		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnElementControllerRemoved);
		FaderGroup->GetOnFixturePatchChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnFaderGroupFixturePatchChanged);
		FaderGroup->GetOnFaderGroupExpanded().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::UpdateExpansionState);

		UDMXControlConsoleEditorData* EditorData = EditorModel->GetControlConsoleEditorData();
		if (EditorData)
		{
			EditorData->GetOnFaderGroupsViewModeChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnViewModeChanged);
		}

		ChildSlot
			[
				SNew(SBorder)
				.BorderBackgroundColor(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderColor)
				.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.01f, 1.f))
					.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage)
					[
						SNew(SBorder)
						.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBackgroundBorderImage)
						.Padding(6.f)
						[
							SNew(SBox)
							.MinDesiredHeight(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewHeightByFadersViewMode))
							[
								SNew(SVerticalBox)

								// Toolbar section
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Top)
								.AutoHeight()
								[
									SAssignNew(FaderGroupToolbar, SDMXControlConsoleEditorFaderGroupToolbar, SharedThis(this), EditorModel.Get())
									.OnAddFaderGroup(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroup)
									.OnAddFaderGroupRow(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRow)
									.OnExpanded(this, &SDMXControlConsoleEditorFaderGroupView::OnExpandArrowClicked)
								]

								// Core section
								+ SVerticalBox::Slot()
								[
									SNew(SHorizontalBox)

									// Fader Group Core section
									+ SHorizontalBox::Slot()
									.Padding(20.f, 20.f, 8.f, 8.f)
									.MaxWidth(116.f)
									[
										SNew(SDMXControlConsoleEditorFaderGroupPanel, SharedThis(this))
										.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetViewModeVisibility, EDMXControlConsoleEditorViewMode::Collapsed))
									]

									// Add button section
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Center)
									.MaxWidth(16.f)
									.AutoWidth()
									[
										SNew(SDMXControlConsoleEditorAddButton)
										.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupClicked)
										.ToolTipText(LOCTEXT("AddFaderGroupButton_ToolTip", "Add a new Fader Group next."))
										.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddButtonVisibility))
									]

									// Faders widget section
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Center)
									.Padding(4.f, 2.f)
									.AutoWidth()
									[
										GenerateElementControllersWidget()
									]
								]

								// Add row button
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Bottom)
								.AutoHeight()
								[
									SNew(SDMXControlConsoleEditorAddButton)
									.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRowClicked)
									.ToolTipText(LOCTEXT("AddFaderGroupOnNewRowButton_ToolTip", "Add a new Fader Group on the next row."))
									.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddRowButtonVisibility))
								]
							]
						]
					]
				]
			];

		UpdateExpansionState();
	}

	int32 SDMXControlConsoleEditorFaderGroupView::GetIndex() const
	{
		if (FaderGroup.IsValid())
		{
			return FaderGroup->GetIndex();
		}

		return INDEX_NONE;
	}

	FString SDMXControlConsoleEditorFaderGroupView::GetFaderGroupName() const
	{
		if (FaderGroup.IsValid())
		{
			return FaderGroup->GetFaderGroupName();
		}

		return FString();
	}

	bool SDMXControlConsoleEditorFaderGroupView::CanAddFaderGroup() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return false;
		}

		// True if active layout is User Layout, no vertical layout mode and no global filter
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		return
			IsValid(ActiveLayout) &&
			ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked() &&
			ActiveLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
			ControlConsoleData->FilterString.IsEmpty();
	}

	bool SDMXControlConsoleEditorFaderGroupView::CanAddFaderGroupRow() const
	{
		if (!FaderGroup.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return false;
		}

		// True if active layout is user layout and there's no global filter
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return false;
		}

		bool bCanAdd =
			ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked() &&
			ActiveLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
			ControlConsoleData->FilterString.IsEmpty();

		// True if grid layout mode and this is the first active fader group in the row
		if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Grid)
		{
			bCanAdd &=
				FaderGroup->IsActive() &&
				ActiveLayout->GetFaderGroupColumnIndex(FaderGroup.Get()) == 0;
		}

		return bCanAdd;
	}

	bool SDMXControlConsoleEditorFaderGroupView::CanAddFader() const
	{
		// True if fader group has no Fixture Patch and there's no global filter
		bool bCanAdd = FaderGroup.IsValid() && !FaderGroup->HasFixturePatch();

		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (ControlConsoleData)
		{
			bCanAdd &= ControlConsoleData->FilterString.IsEmpty();
		}

		return bCanAdd;
	}

	FReply SDMXControlConsoleEditorFaderGroupView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (EditorModel.IsValid() && FaderGroup.IsValid())
			{
				const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();

				if (MouseEvent.IsLeftShiftDown())
				{
					SelectionHandler->Multiselect(FaderGroup.Get());
				}
				else if (MouseEvent.IsControlDown())
				{
					if (IsSelected())
					{
						SelectionHandler->RemoveFromSelection(FaderGroup.Get());
					}
					else
					{
						SelectionHandler->AddToSelection(FaderGroup.Get());
					}
				}
				else
				{
					constexpr bool bNotifySelectionChange = false;
					SelectionHandler->ClearSelection(bNotifySelectionChange);
					SelectionHandler->AddToSelection(FaderGroup.Get());
				}
			}
		}

		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && FaderGroupToolbar.IsValid())
		{
			const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, FaderGroupToolbar->GenerateSettingsMenuWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorFaderGroupView::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
			if (ExpandArrowButton.IsValid())
			{
				ExpandArrowButton->ToggleExpandArrow();

				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}

	void SDMXControlConsoleEditorFaderGroupView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (!ensureMsgf(FaderGroup.IsValid(), TEXT("Invalid fader group, cannot update fader group view state correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroup->GetElementControllers();
		if (ElementControllers.Num() == ElementControllerWidgets.Num())
		{
			return;
		}

		if (ElementControllers.Num() > ElementControllerWidgets.Num())
		{
			OnElementControllerAdded();
		}
		else
		{
			OnElementControllerRemoved();
		}
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupView::GenerateElementControllersWidget()
	{
		const TSharedRef<SWidget> ElementsWidget =
			SNew(SHorizontalBox)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementControllersHorizontalBoxVisibility))

			//Add Faders Horizontal Box
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SAssignNew(ElementControllersHorizontalBox, SHorizontalBox)
			]

			//Add Fader button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.MaxWidth(20.f)
			.Padding(2.f, 4.f)
			.AutoWidth()
			[
				SNew(SDMXControlConsoleEditorAddButton)
					.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderClicked)
					.ToolTipText(LOCTEXT("AddFaderButton_ToolTip", "Add a new Raw Fader."))
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddFaderButtonVisibility))
			];

		return ElementsWidget;
	}

	bool SDMXControlConsoleEditorFaderGroupView::IsSelected() const
	{
		if (!EditorModel.IsValid())
		{
			return false;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		return SelectionHandler->IsSelected(FaderGroup.Get());
	}

	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> SDMXControlConsoleEditorFaderGroupView::GetExpandArrowButton() const
	{
		return FaderGroupToolbar.IsValid() ? FaderGroupToolbar->GetExpandArrowButton() : nullptr;
	}

	void SDMXControlConsoleEditorFaderGroupView::OnElementControllerAdded()
	{
		if (!FaderGroup.IsValid())
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroup->GetElementControllers();

		for (UDMXControlConsoleElementController* ElementController : ElementControllers)
		{
			if (!ElementController)
			{
				continue;
			}

			if (ContainsElementController(ElementController))
			{
				continue;
			}

			AddElementController(ElementController);
		}
	}

	void SDMXControlConsoleEditorFaderGroupView::AddElementController(UDMXControlConsoleElementController* ElementController)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot add new fader correctly.")))
		{
			return;
		}

		if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot add new controller widget correctly.")))
		{
			return;
		}

		if (!ElementControllersHorizontalBox.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleElementControllerModel> ElementControllerModel = MakeShared<FDMXControlConsoleElementControllerModel>(ElementController);
		TSharedPtr<SWidget> ElementControllerWidget = nullptr;

		if (ElementControllerModel->GetMatrixCellElement())
		{
			SAssignNew(ElementControllerWidget, SDMXControlConsoleEditorMatrixCell, ElementControllerModel, EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementControllerWidgetVisibility, ElementControllerModel.ToSharedPtr()));
		}
		else
		{
			SAssignNew(ElementControllerWidget, SDMXControlConsoleEditorElementControllerView, ElementControllerModel, EditorModel.Get())
				.Padding(FMargin(4.f, 0.f))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementControllerWidgetVisibility, ElementControllerModel.ToSharedPtr()));
		}

		ElementControllerWidgets.Add(ElementControllerWidget);

		const int32 Index = ElementController->GetIndex();
		ElementControllersHorizontalBox->InsertSlot(Index)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				ElementControllerWidget.ToSharedRef()
			];
	}

	void SDMXControlConsoleEditorFaderGroupView::OnElementControllerRemoved()
	{
		if (!FaderGroup.IsValid())
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroup->GetElementControllers();

		TArray<TWeakPtr<SWidget>> ElementControllerWidgetsToRemove;
		for (TWeakPtr<SWidget>& Widget : ElementControllerWidgets)
		{
			if (!Widget.IsValid())
			{
				continue;
			}
			
			if (const TSharedPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView = StaticCastSharedPtr<SDMXControlConsoleEditorElementControllerView>(Widget.Pin()))
			{
				UDMXControlConsoleElementController* ElementController = ElementControllerView.IsValid() ? ElementControllerView->GetElementController() : nullptr;
				if (ElementController && ElementControllers.Contains(ElementController))
				{
					continue;
				}
			}
			else if (const TSharedPtr<SDMXControlConsoleEditorMatrixCell> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCell>(Widget.Pin()))
			{
				UDMXControlConsoleElementController* ElementController = MatrixCellWidget.IsValid() ? MatrixCellWidget->GetElementController() : nullptr;
				if (ElementController && ElementControllers.Contains(ElementController))
				{
					continue;
				}
			}

			ElementControllersHorizontalBox->RemoveSlot(Widget.Pin().ToSharedRef());
			ElementControllerWidgetsToRemove.Add(Widget);
		}

		ElementControllerWidgets.RemoveAll([&ElementControllerWidgetsToRemove](TWeakPtr<SWidget> ElementControllerWidget)
			{
				return !ElementControllerWidget.IsValid() || ElementControllerWidgetsToRemove.Contains(ElementControllerWidget);
			});
	}

	bool SDMXControlConsoleEditorFaderGroupView::ContainsElementController(const UDMXControlConsoleElementController* InElementController) const
	{
		auto IsElementControllerInUseLambda = [InElementController](const TWeakPtr<SWidget> Widget)
			{
				if (!Widget.IsValid())
				{
					return false;
				}

				const TSharedPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView = StaticCastSharedPtr<SDMXControlConsoleEditorElementControllerView>(Widget.Pin());
				UDMXControlConsoleElementController* ElementController = ElementControllerView.IsValid() ? ElementControllerView->GetElementController() : nullptr;
				if (ElementController)
				{
					return ElementController == InElementController;
				}

				const TSharedPtr<SDMXControlConsoleEditorMatrixCell> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCell>(Widget.Pin());
				ElementController = MatrixCellWidget.IsValid() ? MatrixCellWidget->GetElementController() : nullptr;
				if (ElementController)
				{
					return ElementController == InElementController;
				}

				return false;
			};

		return Algo::FindByPredicate(ElementControllerWidgets, IsElementControllerInUseLambda) != nullptr;
	}

	void SDMXControlConsoleEditorFaderGroupView::UpdateExpansionState()
	{
		if (FaderGroup.IsValid())
		{
			TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
			if (ExpandArrowButton.IsValid())
			{
				// Get expansion state from model
				const bool bIsExpanded = FaderGroup->IsExpanded();
				ExpandArrowButton->SetExpandArrow(FaderGroup->IsExpanded());
			}
		}
	}

	void SDMXControlConsoleEditorFaderGroupView::OnExpandArrowClicked(bool bExpand)
	{
		if (FaderGroup.IsValid())
		{
			ViewMode = bExpand ? EDMXControlConsoleEditorViewMode::Expanded : EDMXControlConsoleEditorViewMode::Collapsed;

			constexpr bool bNotifyExpansionStateChange = false;
			FaderGroup->Modify();
			FaderGroup->SetIsExpanded(bExpand, bNotifyExpansionStateChange);
		}
	}

	void SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroup() const
	{
		if (!FaderGroup.IsValid())
		{
			return;
		}

		const FScopedTransaction FaderGroupClickedTransaction(LOCTEXT("FaderGroupClickedTransaction", "Add Fader Group"));

		UDMXControlConsoleFaderGroupRow& FaderGroupRow = FaderGroup->GetOwnerFaderGroupRowChecked();
		FaderGroupRow.PreEditChange(nullptr);
		UDMXControlConsoleFaderGroup* NewFaderGroup = FaderGroupRow.AddFaderGroup(GetIndex() + 1);
		FaderGroupRow.PostEditChange();

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleLayouts || !NewFaderGroup)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->GetLayoutRow(FaderGroup.Get());
		if (!LayoutRow)
		{
			return;
		}

		const int32 Index = LayoutRow->GetIndex(FaderGroup.Get());
		LayoutRow->PreEditChange(nullptr);
		LayoutRow->AddToLayoutRow(NewFaderGroup, Index + 1);
		LayoutRow->PostEditChange();

		ActiveLayout->PreEditChange(nullptr);
		ActiveLayout->AddToActiveFaderGroups(NewFaderGroup);
		ActiveLayout->PostEditChange();
	}

	void SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRow() const
	{
		if (!FaderGroup.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleLayouts)
		{
			return;
		}

		// Add Fader Group next if vertical sorting
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
		{
			OnAddFaderGroup();
			return;
		}

		UDMXControlConsoleFaderGroupRow& FaderGroupRow = FaderGroup->GetOwnerFaderGroupRowChecked();
		UDMXControlConsoleData& ControlConsoleData = FaderGroupRow.GetOwnerControlConsoleDataChecked();

		const FScopedTransaction FaderGroupRowClickedTransaction(LOCTEXT("FaderGroupRowClickedTransaction", "Add Fader Group"));
		const int32 RowIndex = FaderGroupRow.GetRowIndex();

		ControlConsoleData.PreEditChange(nullptr);
		const UDMXControlConsoleFaderGroupRow* NewRow = ControlConsoleData.AddFaderGroupRow(RowIndex + 1);
		ControlConsoleData.PostEditChange();

		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->GetLayoutRow(FaderGroup.Get());
		if (!LayoutRow)
		{
			return;
		}

		const int32 LayoutRowIndex = LayoutRow->GetRowIndex();
		ActiveLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(LayoutRowIndex + 1);
		if (NewLayoutRow)
		{
			UDMXControlConsoleFaderGroup* NewFaderGroup = NewRow && !NewRow->GetFaderGroups().IsEmpty() ? NewRow->GetFaderGroups()[0] : nullptr;
			NewLayoutRow->PreEditChange(nullptr);
			NewLayoutRow->AddToLayoutRow(NewFaderGroup);
			NewLayoutRow->PostEditChange();

			ActiveLayout->AddToActiveFaderGroups(NewFaderGroup);
		}

		ActiveLayout->PostEditChange();
	}

	void SDMXControlConsoleEditorFaderGroupView::OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* InFaderGroup, UDMXEntityFixturePatch* FixturePatch)
	{
		if (FaderGroup.IsValid() && FaderGroup == InFaderGroup)
		{
			OnElementControllerAdded();
			OnElementControllerRemoved();
		}
	}

	FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupClicked() const
	{
		OnAddFaderGroup();
		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRowClicked() const
	{
		OnAddFaderGroupRow();
		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderClicked()
	{
		if (FaderGroup.IsValid())
		{
			const FScopedTransaction FaderClickedTransaction(LOCTEXT("FaderClickedTransaction", "Add Fader"));
			FaderGroup->PreEditChange(nullptr);
			UDMXControlConsoleRawFader* RawFader = FaderGroup->AddRawFader();
			FaderGroup->CreateElementController(RawFader);
			FaderGroup->PostEditChange();
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorFaderGroupView::OnViewModeChanged()
	{
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!EditorData)
		{
			return;
		}

		ViewMode = EditorData->GetFaderGroupsViewMode();

		if (!FaderGroup.IsValid())
		{
			return;
		}

		switch (ViewMode)
		{
		case EDMXControlConsoleEditorViewMode::Collapsed:
		{
			FaderGroup->SetIsExpanded(false);
			break;
		}
		case EDMXControlConsoleEditorViewMode::Expanded:
			FaderGroup->SetIsExpanded(true);
			break;
		}
	}

	bool SDMXControlConsoleEditorFaderGroupView::IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const
	{
		return ViewMode == InViewMode;
	}

	FOptionalSize SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewHeightByFadersViewMode() const
	{
		using namespace DMXControlConsoleEditorFaderGroupView::Private;
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const EDMXControlConsoleEditorViewMode FadersViewMode = EditorData->GetFadersViewMode();
			return FadersViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
		}

		return CollapsedViewModeHeight;
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderColor() const
	{
		if (!FaderGroup.IsValid())
		{
			return FLinearColor::White;
		}

		return FaderGroup->GetEditorColor();
	}

	const FSlateBrush* SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage() const
	{
		if (IsHovered())
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush_Tansparent");
			}
		}
		else
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
			}
			else
			{
				return FAppStyle::GetBrush("NoBorder");
			}
		}
	}

	const FSlateBrush* SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBackgroundBorderImage() const
	{
		if (IsHovered())
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Highlighted");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
			}
		}
		else
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Selected");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
			}
		}
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const
	{
		return  IsCurrentViewMode(InViewMode) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel) const
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		const bool bIsVisible = ElementController && ElementController->IsMatchingFilter();
		return  bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddButtonVisibility() const
	{
		const bool bIsVisible =
			FaderGroup.IsValid() &&
			CanAddFaderGroup() &&
			IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Collapsed);

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddRowButtonVisibility() const
	{
		if (IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded))
		{
			return EVisibility::Collapsed;
		}

		return CanAddFaderGroupRow() ? EVisibility::Visible : EVisibility::Hidden;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetElementControllersHorizontalBoxVisibility() const
	{
		const bool bIsVisible =
			GetExpandArrowButton().IsValid() &&
			GetExpandArrowButton()->IsExpanded() &&
			IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded);

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddFaderButtonVisibility() const
	{
		return CanAddFader() ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
