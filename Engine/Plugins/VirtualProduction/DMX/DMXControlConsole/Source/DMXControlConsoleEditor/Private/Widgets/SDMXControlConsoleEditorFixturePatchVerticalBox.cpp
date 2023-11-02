// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFixturePatchVerticalBox"

void SDMXControlConsoleEditorFixturePatchVerticalBox::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
{
	if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct fixture patch vertical box widget correctly.")))
	{
		return;
	}

	EditorModel = InEditorModel;

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	UDMXLibrary* DMXLibrary = ControlConsoleData ? ControlConsoleData->GetDMXLibrary() : nullptr;

	ChildSlot
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateFixturePatchListToolbar()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(FixturePatchList, SDMXControlConsoleFixturePatchList, EditorModel.Get())
				.DMXLibrary(DMXLibrary)
			]
		];
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::ForceRefresh()
{
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (ControlConsoleData && FixturePatchList.IsValid())
	{
		UDMXLibrary* NewLibrary = ControlConsoleData->GetDMXLibrary();
		FixturePatchList->SetDMXLibrary(NewLibrary);
	}
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFixturePatchListToolbar()
{
	const auto GenerateAddButtonContentLambda = [](const FText& AddButtonText, const FText& AddButtonToolTip)
		{
			return
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
					.Text(AddButtonText)
					.ToolTipText(AddButtonToolTip)
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
				];
		};

	const TSharedRef<SWidget> FixturePatchListToolbar =
		SNew(SHorizontalBox)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility))

		// Add All Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(8.f, 0.f, 4.f, 8.f)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled)
			.OnClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked)
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddAllFixturePatchFromList", "Add All Patches"),
					LOCTEXT("AddAllFixturePatchFromList_ToolTip", "Add all Fixture Patches from the list.")
				)
			]
		]

		// Add Combo Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(4.f, 0.f, 8.f, 8.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HasDownArrow(true)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu)
			.ButtonContent()
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddFixturePatchFromList", "Add Patch"),
					LOCTEXT("AddFixturePatchFromList_ToolTip", "Add a Fixture Patch from the list.")
				)
			]
		];

	return FixturePatchListToolbar;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu()
{
	if (!EditorModel.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Show Add Patch buttons only if the current layout is the user layout
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return SNullWidget::NullWidget;
	}

	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
	{
		TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
		TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = FixturePatchList->GetSelectedFixturePatches();
		Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
			{
				return FixturePatch;
			});

		return SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches, EditorModel.Get());
	}

	return SNullWidget::NullWidget;
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (!EditorModel.IsValid() || !FaderGroup || !FixturePatch)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	SelectionHandler->ClearFadersSelection(FaderGroup);

	const FScopedTransaction GenerateFaderGroupFromFixturePatchTransaction(LOCTEXT("GenerateFaderGroupFromFixturePatchTransaction", "Generate Fader Group from Fixture Patch"));
	FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetSoftFixturePatchPtrPropertyName()));
	FaderGroup->GenerateFromFixturePatch(FixturePatch);
	FaderGroup->PostEditChange();
}

FReply SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked()
{
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return FReply::Handled();
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return FReply::Handled();
	}

	const FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Add All Patches"));
	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		// Remove Fader Groups already in the layout and all unpatched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
		FaderGroups.RemoveAll([&ActiveLayout](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && 
					(!FaderGroup->HasFixturePatch() ||
						ActiveLayout->ContainsFaderGroup(FaderGroup));
			});

		ActiveLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->AddNewRowToLayout();
		ActiveLayout->PostEditChange();

		if (LayoutRow)
		{
			LayoutRow->PreEditChange(nullptr);
			LayoutRow->AddToLayoutRow(FaderGroups);
			LayoutRow->PostEditChange();

			Algo::ForEach(FaderGroups,[](UDMXControlConsoleFaderGroup* FaderGroup)
				{
					FaderGroup->Modify();
					FaderGroup->SetIsActive(true);
				});
		}
	}

	return FReply::Handled();
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled() const
{
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	return ControlConsoleData && ControlConsoleData->GetDMXLibrary();
}

EVisibility SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility() const
{
	bool bIsVisible = false;

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (ControlConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		bIsVisible = IsValid(ActiveLayout) && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
