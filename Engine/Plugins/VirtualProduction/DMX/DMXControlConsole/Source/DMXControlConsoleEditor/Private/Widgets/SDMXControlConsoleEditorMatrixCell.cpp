// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorMatrixCell.h"

#include "Algo/AnyOf.h"
#include "Controllers/DMXControlConsoleElementController.h"
#include "Controllers/DMXControlConsoleMatrixCellController.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorElementControllerView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorMatrixCell"

namespace UE::DMX::Private
{
	namespace DMXControlConsoleEditorMatrixCell
	{
		namespace Private
		{
			constexpr float CollapsedViewModeHeight = 200.f;
			constexpr float ExpandedViewModeHeight = 280.f;
		}
	}

	void SDMXControlConsoleEditorMatrixCell::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct matrix cell widget correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InElementControllerModel.IsValid(), TEXT("Invalid element controller model, cannot create matrix cell widget correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		ElementControllerModel = InElementControllerModel;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorMatrixCell::OnMatrixCellControllerAdded);
		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorMatrixCell::OnMatrixCellControllerRemoved);

		ChildSlot
			[
				SNew(SHorizontalBox)
				// Matrix Cell section
				+ SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(20.f)
					.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetMatrixCellHeightByFadersViewMode))
					[
						SNew(SBorder)
						.BorderImage(this, &SDMXControlConsoleEditorMatrixCell::GetBorderImage)
						[
							SNew(SVerticalBox)
							// Matrix Cell Label
							+ SVerticalBox::Slot()
							.Padding(0.f, 1.f, 0.f, 0.f)
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(8.f)
								.Padding(1.f)
								[
									SNew(SImage)
									.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
									.ColorAndOpacity(this, &SDMXControlConsoleEditorMatrixCell::GetLabelBorderColor)
								]
							]

							// Matrix Cell Expand button
							+ SVerticalBox::Slot()
							.Padding(0.f, 4.f, 0.f, 0.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.Text(this, &SDMXControlConsoleEditorMatrixCell::GetMatrixCellLabelText)
								.Justification(ETextJustify::Center)
							]

							// Matrix Cell Text Label
							+ SVerticalBox::Slot()
							.Padding(0.f, 2.f, 0.f, 0.f)
							.AutoHeight()
							[
								SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
									.ToolTipText(LOCTEXT("MatrixCellExpandArrowButton_Tooltip", "Switch expansion state of the cell"))
							]
						]
					]
				]

				// Matrix Cell Faders section
				+ SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SAssignNew(ElementControllersHorizontalBox, SHorizontalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetElementControllersHorizontalBoxVisibility))
				]
			];
	}

	UDMXControlConsoleElementController* SDMXControlConsoleEditorMatrixCell::GetElementController() const
	{
		return ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
	}

	UDMXControlConsoleFixturePatchMatrixCell* SDMXControlConsoleEditorMatrixCell::GetMatrixCell() const
	{
		return ElementControllerModel.IsValid() ? ElementControllerModel->GetMatrixCellElement() : nullptr;
	}

	FReply SDMXControlConsoleEditorMatrixCell::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
			if (!MatrixCell)
			{
				return FReply::Unhandled();
			}

			if (ExpandArrowButton.IsValid())
			{
				ExpandArrowButton->ToggleExpandArrow();
			}

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	void SDMXControlConsoleEditorMatrixCell::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!ensureMsgf(MatrixCell, TEXT("Invalid matrix cell, cannot update matrix cell widget state correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleMatrixCellController*>& MatrixCellControllers = MatrixCell->GetMatrixCellControllers();
		if (MatrixCellControllers.Num() == ElementControllerViews.Num())
		{
			return;
		}

		if (MatrixCellControllers.Num() > ElementControllerViews.Num())
		{
			OnMatrixCellControllerAdded();
		}
		else
		{
			OnMatrixCellControllerRemoved();
		}
	}

	void SDMXControlConsoleEditorMatrixCell::OnMatrixCellControllerAdded()
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!ensureMsgf(MatrixCell, TEXT("Invalid matrix cell, cannot add new controller widget correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleMatrixCellController*>& MatrixCellControllers = MatrixCell->GetMatrixCellControllers();

		for (UDMXControlConsoleMatrixCellController* MatrixCellController : MatrixCellControllers)
		{
			if (!MatrixCellController)
			{
				continue;
			}

			if (ContainsMatrixCellController(MatrixCellController))
			{
				continue;
			}

			AddMatrixCellController(MatrixCellController);
		}
	}

	void SDMXControlConsoleEditorMatrixCell::AddMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot add new matrix cell correctly.")))
		{
			return;
		}

		if (!ensureMsgf(MatrixCellController, TEXT("Invalid matrix cell controller, cannot add new controller widget correctly.")))
		{
			return;
		}

		if (!ElementControllersHorizontalBox.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleElementControllerModel> NewElementControllerModel = MakeShared<FDMXControlConsoleElementControllerModel>(MatrixCellController);
		const TSharedRef<SDMXControlConsoleEditorElementControllerView> ElementControllerView =
			SNew(SDMXControlConsoleEditorElementControllerView, NewElementControllerModel, EditorModel.Get())
			.Padding(FMargin(2.f, 0.f))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetElementControllerWidgetVisibility, NewElementControllerModel.ToSharedPtr()));

		ElementControllerViews.Add(ElementControllerView);

		const int32 Index = MatrixCellController->GetIndex();
		ElementControllersHorizontalBox->InsertSlot(Index)
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				ElementControllerView
			];
	}

	void SDMXControlConsoleEditorMatrixCell::OnMatrixCellControllerRemoved()
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!ensureMsgf(MatrixCell, TEXT("Invalid matrix cell, cannot remove the controller widget correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleMatrixCellController*>& MatrixCellControllers = MatrixCell->GetMatrixCellControllers();

		TArray<TWeakPtr<SDMXControlConsoleEditorElementControllerView>> ElementControllerViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorElementControllerView>& ElementControllerView : ElementControllerViews)
		{
			if (!ElementControllerView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleElementController* ElementController = ElementControllerView.Pin()->GetElementController();
			if (!ElementController || !MatrixCellControllers.Contains(ElementController))
			{
				ElementControllersHorizontalBox->RemoveSlot(ElementControllerView.Pin().ToSharedRef());
				ElementControllerViewsToRemove.Add(ElementControllerView);
			}
		}

		ElementControllerViews.RemoveAll([&ElementControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView)
			{
				return !ElementControllerView.IsValid() || ElementControllerViewsToRemove.Contains(ElementControllerView);
			});
	}

	bool SDMXControlConsoleEditorMatrixCell::ContainsMatrixCellController(UDMXControlConsoleMatrixCellController* MatrixCellController)
	{
		auto IsElementControllerInUseLambda = [MatrixCellController](const TWeakPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView)
			{
				if (!ElementControllerView.IsValid())
				{
					return false;
				}

				const UDMXControlConsoleElementController* ElementController = ElementControllerView.Pin()->GetElementController();
				if (!ElementController)
				{
					return false;
				}

				return ElementController == MatrixCellController;
			};

		return ElementControllerViews.ContainsByPredicate(IsElementControllerInUseLambda);
	}

	bool SDMXControlConsoleEditorMatrixCell::IsAnyElementControllerSelected() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!EditorModel.IsValid() || !MatrixCell)
		{
			return false;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();

		const TArray<UDMXControlConsoleMatrixCellController*>& MatrixCellControllers = MatrixCell->GetMatrixCellControllers();
		const bool bIsAnyMatrixCellControllerSelected = Algo::AnyOf(MatrixCellControllers, 
			[SelectedElementControllers](UDMXControlConsoleMatrixCellController* MatrixCellController)
			{
				return MatrixCellController && SelectedElementControllers.Contains(MatrixCellController);
			});

		return bIsAnyMatrixCellControllerSelected;
	}

	FOptionalSize SDMXControlConsoleEditorMatrixCell::GetMatrixCellHeightByFadersViewMode() const
	{
		using namespace DMXControlConsoleEditorMatrixCell::Private;
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const EDMXControlConsoleEditorViewMode ViewMode = EditorData->GetFadersViewMode();
			return ViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
		}

		return CollapsedViewModeHeight;
	}

	FText SDMXControlConsoleEditorMatrixCell::GetMatrixCellLabelText() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (MatrixCell)
		{
			return FText::FromString(FString::FromInt(MatrixCell->GetCellID()));
		}

		return FText::GetEmpty();
	}

	FSlateColor SDMXControlConsoleEditorMatrixCell::GetLabelBorderColor() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (MatrixCell)
		{
			const UDMXControlConsoleFaderGroup& FaderGroup = MatrixCell->GetOwnerFaderGroupChecked();
			return FaderGroup.GetEditorColor();
		}

		return FSlateColor(FLinearColor::White);
	}

	EVisibility SDMXControlConsoleEditorMatrixCell::GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ControllerModel) const
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		const bool bIsVisible = ElementController && ElementController->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorMatrixCell::GetElementControllersHorizontalBoxVisibility() const
	{
		const bool bIsVisible = ExpandArrowButton.IsValid() && ExpandArrowButton->IsExpanded();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* SDMXControlConsoleEditorMatrixCell::GetBorderImage() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!MatrixCell)
		{
			return nullptr;
		}

		if (IsHovered())
		{
			if (IsAnyElementControllerSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Highlighted");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Hovered");;
			}
		}
		else
		{
			if (IsAnyElementControllerSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Selected");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader");
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
