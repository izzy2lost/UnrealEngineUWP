// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupToolbar.h"

#include "Controllers/DMXControlConsoleElementController.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/Filter/FilterModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupComboBox.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroup"

namespace UE::DMX::Private
{ 
	void SDMXControlConsoleEditorFaderGroupToolbar::Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create fader group toolbar widget correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot create fader group toolbar widget correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		FaderGroupView = InFaderGroupView;

		OnAddFaderGroupDelegate = InArgs._OnAddFaderGroup;
		OnAddFaderGroupRowDelegate = InArgs._OnAddFaderGroupRow;

		ChildSlot
		[
			SNew(SHorizontalBox)
			
			// Fader Group tag section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 4.f, 0.f)
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.MinDesiredWidth(8.f)
				.MinDesiredHeight(24.f)
				[
					SNew(SImage)
					.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
					.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupEditorColor)
				]
			]

			// Fixture Patch ComboBox section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.MaxWidth(120.f)
			.Padding(4.f, 8.f)
			.AutoWidth()
			[
				SAssignNew(FaderGroupComboBox, SDMXControlConsoleEditorFaderGroupComboBox, FaderGroupView, EditorModel.Get())
			]

			// Expand Arrow button section
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.MaxWidth(20.f)
			.AutoWidth()
			[
				SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
				.OnExpandClicked(InArgs._OnExpanded)
				.ToolTipText(LOCTEXT("FaderGroupExpandArrowButton_Tooltip", "Switch between Collapsed/Expanded view mode"))
			]

			// Info Combo button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 8.f, 0.f, 8.f)
			[
				SNew(SComboButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
				.ContentPadding(2.f)
				.ForegroundColor(FSlateColor::UseStyle())
				.HasDownArrow(false)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateFaderGroupInfoMenuWidget)
				.ToolTipText(LOCTEXT("InfoButtonToolTipText", "Info"))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
				.ButtonContent()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.Info"))
				]
			]

			//Searchbox section
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.MaxWidth(250.f)
			.Padding(4.f, 8.f)
			[
				SAssignNew(ToolbarSearchBox, SSearchBox)
				.MinDesiredWidth(100.f)
				.OnTextChanged(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnSearchTextChanged)
				.ToolTipText(LOCTEXT("SearchBarTooltip", "Searches for Fader Name, Attributes, Fixture ID, Universe or Patch. Examples:\n\n* FaderName\n* Dimmer\n* Pan, Tilt\n* 1\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
			]

			// Add New button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 8.f, 0.f, 8.f)
			[
				SNew(SComboButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HasDownArrow(true)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateAddNewFaderGroupMenuWidget)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FStyleColors::AccentGreen)
						.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(3.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.Text(LOCTEXT("AddFaderGroupComboButton", "Add New"))
						.ToolTipText(LOCTEXT("AddFaderGroupComboButton_ToolTip", "Add a new Fader Group to the Control Console."))
						.TextStyle(FAppStyle::Get(), "SmallButtonText")
					]
				]
			]

			//Settings section
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.Padding(4.f, 8.f)
			[
				SNew(SComboButton)
				.ContentPadding(0.f)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupToolbar::GenerateSettingsMenuWidget)
				.HasDownArrow(true)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility))
				.ButtonContent()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
				]
			]
		];

		RestoreFaderGroupFilter();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateSettingsMenuWidget()
	{
		constexpr bool bShouldCloseWindowAfterClosing = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("Options", LOCTEXT("FaderGroupViewOptionsCategory", "Options"));
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("InfoLabel", "Info"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnGetInfoPanel)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("SelectAllLabel", "Select All"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnSelectAllFaders)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("DuplicateLabel", "Duplicate"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnDuplicateFaderGroup)
					, FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup)
					, FIsActionChecked()
					, FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("RemoveLabel", "Remove"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnRemoveFaderGroup),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanRemoveFaderGroup)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Controls", LOCTEXT("FaderGroupViewControlsCategory", "Controls"));
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ResetToDefaultLabel", "Reset To Default"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.ResetToDefault"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnResetFaderGroup)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("LockLabel", "Lock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup, true),
					FCanExecuteAction::CreateLambda([this]() { return GetFaderGroup() && !GetFaderGroup()->IsLocked(); }),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroup() && !GetFaderGroup()->IsLocked(); })
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("UnlockLabel", "Unlock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup, false),
					FCanExecuteAction::CreateLambda([this]() { return GetFaderGroup() && GetFaderGroup()->IsLocked(); }),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroup() && GetFaderGroup()->IsLocked(); })
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	UDMXControlConsoleFaderGroup* SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroup() const
	{
		return FaderGroupView.IsValid() ? FaderGroupView.Pin()->GetFaderGroup() : nullptr;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateFaderGroupInfoMenuWidget()
	{
		if (FaderGroupView.IsValid())
		{
			constexpr bool bShouldCloseWindowAfterClosing = false;
			FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

			MenuBuilder.BeginSection("Info", LOCTEXT("FaderGroupInfoMenuCategory", "Info"));
			{
				const TSharedRef<SWidget> FaderGroupInfoPanel =
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(8.f)
					.MaxWidth(116.f)
					[
						SNew(SDMXControlConsoleEditorFaderGroupPanel, FaderGroupView.Pin())
					];

				MenuBuilder.AddWidget(FaderGroupInfoPanel, FText::GetEmpty());
			}
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}

		return SNullWidget::NullWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupToolbar::GenerateAddNewFaderGroupMenuWidget()
	{
		constexpr bool bShouldCloseWindowAfterClosing = false;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("Options", LOCTEXT("AddNewFaderGroupMenuCategory", "New Fader Group"));
		{
			MenuBuilder.AddMenuEntry
			(
				FText::FromString(TEXT("Next"))
				, FText::FromString(TEXT("Add new Fader Group next"))
				, FSlateIcon()
				, FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroup),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroup)
				)
				, NAME_None
				, EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				FText::FromString(TEXT("Next Row"))
				, FText::FromString(TEXT("Add new Fader Group to next row"))
				, FSlateIcon()
				, FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroupRow),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroupRow)
				)
				, NAME_None
				, EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::RestoreFaderGroupFilter()
	{
		if (const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			const FString& FilterString = FaderGroup->FilterString;
			const FText FilterText = FText::FromString(FilterString);
			ToolbarSearchBox->SetText(FilterText);
		}
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnSearchTextChanged(const FText& SearchText)
	{
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!EditorData)
		{
			return;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (!FaderGroup)
		{
			return;
		}

		const TSharedRef<FFilterModel> FilterModel = EditorModel->GetFilterModel();
		FilterModel->SetFaderGroupFilter(FaderGroup, SearchText.ToString());

		if (!SearchText.IsEmpty() && EditorData->GetAutoSelectFilteredElements())
		{
			TArray<UObject*> ElementControllersToSelect;
			TArray<UObject*> ElementControllersToUnselect;
			const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroup->GetAllElementControllers();
			for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
			{
				if (!ElementController)
				{
					continue;
				}

				if (ElementController->IsMatchingFilter())
				{
					ElementControllersToSelect.Add(ElementController);
				}
				else
				{
					ElementControllersToUnselect.Add(ElementController);
				}
			}

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			constexpr bool bNotifySelection = false;
			SelectionHandler->AddToSelection(ElementControllersToSelect, bNotifySelection);
			SelectionHandler->RemoveFromSelection(ElementControllersToUnselect);
		}
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroup() const
	{
		OnAddFaderGroupDelegate.ExecuteIfBound();
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnAddFaderGroupRow() const
	{
		OnAddFaderGroupRowDelegate.ExecuteIfBound();
	}

	bool SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroup() const
	{
		return FaderGroupView.IsValid() ? FaderGroupView.Pin()->CanAddFaderGroup() : false;
	}

	bool SDMXControlConsoleEditorFaderGroupToolbar::CanAddFaderGroupRow() const
	{
		return FaderGroupView.IsValid() ? FaderGroupView.Pin()->CanAddFaderGroupRow() : false;
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnGetInfoPanel()
	{
		if (FaderGroupView.IsValid())
		{
			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked(FaderGroupView.Pin().ToSharedRef(), WidgetPath);

			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateFaderGroupInfoMenuWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnSelectAllFaders() const
	{
		UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (EditorModel.IsValid() && FaderGroup)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->AddAllFadersFromFaderGroupToSelection(FaderGroup, true);
		}
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnDuplicateFaderGroup()  const
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			const FScopedTransaction DuplicateFaderGroupOptionTransaction(LOCTEXT("DuplicateFaderGroupOptionTransaction", "Fader Group duplicated"));
			FaderGroup->PreEditChange(nullptr);
			FaderGroup->Duplicate();
			FaderGroup->PostEditChange();
		}
	}

	bool SDMXControlConsoleEditorFaderGroupToolbar::CanDuplicateFaderGroup() const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		return IsValid(FaderGroup) ? !FaderGroup->HasFixturePatch() : false;
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnRemoveFaderGroup() const
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (!FaderGroup)
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const FScopedTransaction RemoveFaderGroupOptionTransaction(LOCTEXT("RemoveFaderGroupOptionTransaction", "Remove Fader Group"));
		ActiveLayout->PreEditChange(nullptr);
		ActiveLayout->RemoveFromLayout(FaderGroup);
		ActiveLayout->RemoveFromActiveFaderGroups(FaderGroup);
		ActiveLayout->ClearEmptyLayoutRows();
		ActiveLayout->PostEditChange();

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->RemoveFromSelection(FaderGroup);

		if (!FaderGroup->HasFixturePatch())
		{
			FaderGroup->Destroy();
		}
	}

	bool SDMXControlConsoleEditorFaderGroupToolbar::CanRemoveFaderGroup() const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (!EditorModel.IsValid() || !IsValid(FaderGroup))
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return false;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		return IsValid(ActiveLayout) && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnResetFaderGroup() const
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			const FScopedTransaction ResetFaderGroupOptionTransaction(LOCTEXT("ResetFaderGroupOptionTransaction", "Fader Group reset to default"));
			FaderGroup->PreEditChange(nullptr);
			FaderGroup->ResetToDefault();
			FaderGroup->PostEditChange();
		}
	}

	void SDMXControlConsoleEditorFaderGroupToolbar::OnLockFaderGroup(bool bLock) const
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			const FScopedTransaction LockFaderGroupOptionTransaction(LOCTEXT("LockFaderGroupOptionTransaction", "Edit Fader Group lock state"));

			const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroup->GetElementControllers();
			for (UDMXControlConsoleElementController* ElementController : ElementControllers)
			{
				if (ElementController)
				{
					ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetIsLockedPropertyName()));
					ElementController->SetLock(bLock);
					ElementController->PostEditChange();
				}
			}
		}
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupToolbar::GetFaderGroupEditorColor() const
	{
		if (const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			return FaderGroup->GetEditorColor();	
		}

		return FLinearColor::White;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupToolbar::GetExpandedViewModeVisibility() const
	{
		const bool bIsVisible =
			FaderGroupView.IsValid() &&
			FaderGroupView.Pin()->GetViewMode() == EDMXControlConsoleEditorViewMode::Expanded;

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
