// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorCueStackComboBox.h"

#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleCueStack.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXEditorStyle.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorCueList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorCueStackComboBox"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorCueStackComboBox::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create layout toolbar correctly.")))
		{
			return;
		}

		WeakEditorModel = InEditorModel;

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (ControlConsoleCueStack)
		{
			ControlConsoleCueStack->GetOnCueStackChanged().AddSP(this, &SDMXControlConsoleEditorCueStackComboBox::UpdateCueStackComboBoxSource);
		}

		ChildSlot
			[
				SNew(SHorizontalBox)

				// Cue stack combo box section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SAssignNew(CueStackComboBox, SComboBox<TSharedPtr<FDMXControlConsoleEditorCueListItem>>)
					.OnGenerateWidget(this, &SDMXControlConsoleEditorCueStackComboBox::GenerateComboBoxOptionWidget)
					.OptionsSource(&ComboBoxSource)
					.OnSelectionChanged(this, &SDMXControlConsoleEditorCueStackComboBox::OnCueStackComboBoxSelectionChanged)
					.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
					.ItemStyle(&FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DMXControlConsole.FaderGroupToolbar")))
					[
						GenerateComboBoxContentWidget()
					]
				]

				// Add new cue button section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.WidthOverride(28.f)
					.HeightOverride(22.f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "Button")
						.ForegroundColor(FSlateColor::UseForeground())
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SDMXControlConsoleEditorCueStackComboBox::OnAddNewCueClicked)
						.IsEnabled(this, &SDMXControlConsoleEditorCueStackComboBox::IsAddNewCueButtonEnabled)
						.ToolTipText(LOCTEXT("CueStackComboBoxAddNewCueButton_ToolTip", "Add New Cue"))
						.ContentPadding(FMargin(-10.f, 0.f))
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
					]
				]

				// Store cue button section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f)
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.WidthOverride(28.f)
					.HeightOverride(22.f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "Button")
						.ForegroundColor(FSlateColor::UseForeground())
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SDMXControlConsoleEditorCueStackComboBox::OnStoreCueClicked)
						.IsEnabled(this, &SDMXControlConsoleEditorCueStackComboBox::IsStoreCueButtonEnabled)
						.ToolTipText(LOCTEXT("CueStackComboBoxStoreCueButton_ToolTip", "Store Cue"))
						.ContentPadding(FMargin(-10.f, 0.f))
						[
							SNew(SImage)
							.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.StoreCue"))
							.ColorAndOpacity(FStyleColors::White)
						]
					]
				]
			];

		UpdateCueStackComboBoxSource();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueStackComboBox::GenerateComboBoxContentWidget()
	{
		const TSharedRef<SWidget> ComboBoxContentWidget =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(4.f)
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.MinDesiredWidth(4.f)
				.MinDesiredHeight(14.f)
				[
					SNew(SImage)
					.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.WhiteRoundedPropertyBorder"))
					.ColorAndOpacity(this, &SDMXControlConsoleEditorCueStackComboBox::GetLoadedCueColor)
				]
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(70.f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(this, &SDMXControlConsoleEditorCueStackComboBox::GetLoadedCueNameAsText)
				]
			];

		return ComboBoxContentWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueStackComboBox::GenerateComboBoxOptionWidget(const TSharedPtr<FDMXControlConsoleEditorCueListItem> CueItem)
	{
		if (!CueItem.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		const FSlateColor CueColor = CueItem->GetCueColor();
		const FText CueNameAsText =  CueItem->GetCueNameText();

		const TSharedRef<SWidget> ComboBoxOptionWidget =
			SNew(SHorizontalBox)

			// Row color tag
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.MinDesiredWidth(4.f)
				.MinDesiredHeight(14.f)
				[
					SNew(SImage)
					.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.WhiteRoundedPropertyBorder"))
					.ColorAndOpacity(CueColor)
				]
			]

			// Row name label
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.MaxWidth(140.f)
			.Padding(6.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(CueNameAsText)
				.ToolTipText(CueNameAsText)
			];

			return ComboBoxOptionWidget;
	}

	void SDMXControlConsoleEditorCueStackComboBox::UpdateCueStackComboBoxSource()
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleEditorData || !ControlConsoleCueStack || !CueStackComboBox.IsValid())
		{
			return;
		}

		ComboBoxSource.Reset();

		TSharedPtr<FDMXControlConsoleEditorCueListItem> LastLoadedCueItem;
		const TArray<FDMXControlConsoleCue>& CuesArray = ControlConsoleCueStack->GetCuesArray();
		for (const FDMXControlConsoleCue& Cue : CuesArray)
		{
			const TSharedRef<FDMXControlConsoleEditorCueListItem> CueListItem = MakeShared<FDMXControlConsoleEditorCueListItem>(Cue);
			if (Cue == ControlConsoleEditorData->LoadedCue)
			{
				LastLoadedCueItem = CueListItem;
			}

			ComboBoxSource.Add(CueListItem);
		}

		CueStackComboBox->RefreshOptions();
		if (LastLoadedCueItem.IsValid())
		{
			CueStackComboBox->SetSelectedItem(LastLoadedCueItem);
		}
	}

	void SDMXControlConsoleEditorCueStackComboBox::OnCueStackComboBoxSelectionChanged(const TSharedPtr<FDMXControlConsoleEditorCueListItem> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (!NewSelection.IsValid() || (SelectInfo != ESelectInfo::OnMouseClick && SelectInfo != ESelectInfo::OnKeyPress))
		{
			return;
		}

		UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleEditorData || !ControlConsoleCueStack)
		{
			return;
		}

		const FScopedTransaction RecallCueTransaction(LOCTEXT("RecallCueTransaction", "Recall Cue"));
		const FDMXControlConsoleCue& SelectedCue = NewSelection->GetCue();

		// Update the loaded cue
		ControlConsoleEditorData->PreEditChange(nullptr);
		ControlConsoleEditorData->LoadedCue = SelectedCue;
		ControlConsoleEditorData->PostEditChange();

		// Synch controllers to the new fader values
		const TMap<TWeakObjectPtr<UDMXControlConsoleFaderBase>, uint32>& FaderToValueMap = SelectedCue.FaderToValueMap;
		for (const TTuple<TWeakObjectPtr<UDMXControlConsoleFaderBase>, uint32>& FaderToValue : FaderToValueMap)
		{
			const UDMXControlConsoleFaderBase* Fader = FaderToValue.Key.Get();
			if (!Fader)
			{
				continue;
			}

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const TArray<UDMXControlConsoleFaderBase*> Faders = ElementController->GetFaders();
			if (Faders.IsEmpty())
			{
				continue;
			}

			const UDMXControlConsoleFaderBase* FirstFader = Faders[0];
			const bool bHasUniformDataType = Algo::AllOf(Faders,
				[FirstFader](const UDMXControlConsoleFaderBase* Fader)
				{
					return Fader && Fader->GetDataType() == FirstFader->GetDataType();
				});

			// Synch only if all faders in the controller have the same data type
			if (bHasUniformDataType)
			{
				const uint32 Value = FaderToValue.Value;
				const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
				const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
				const float NormalizedValue = static_cast<float>(Value) / ValueRange;

				ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetValuePropertyName()));
				constexpr bool bSynchElements = false;
				ElementController->SetValue(NormalizedValue, bSynchElements);
				ElementController->PostEditChange();
			}
		}

		// Recall the selected cue
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->Recall(SelectedCue);
		ControlConsoleCueStack->PostEditChange();
	}

	bool SDMXControlConsoleEditorCueStackComboBox::IsAddNewCueButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		return ControlConsoleData && !ControlConsoleData->GetAllFaderGroups().IsEmpty();
	}

	bool SDMXControlConsoleEditorCueStackComboBox::IsStoreCueButtonEnabled() const
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleEditorData || !ControlConsoleCueStack || !CueStackComboBox.IsValid())
		{
			return false;
		}

		const TSharedPtr<FDMXControlConsoleEditorCueListItem> SelectedCueItem = CueStackComboBox->GetSelectedItem();
		if (!SelectedCueItem.IsValid())
		{
			return false;
		}

		if (SelectedCueItem->GetCue() == ControlConsoleEditorData->LoadedCue)
		{
			return ControlConsoleCueStack->CanStore();
		}

		return true;
	}

	FReply SDMXControlConsoleEditorCueStackComboBox::OnAddNewCueClicked()
	{
		UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleEditorData || !ControlConsoleCueStack || !CueStackComboBox.IsValid())
		{
			return FReply::Unhandled();
		}

		TArray<UDMXControlConsoleFaderBase*> FadersToCue;
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FadersToCue.Append(FaderGroup->GetAllFaders());
			}
		}

		if (FadersToCue.IsEmpty())
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction AddNewCueTransaction(LOCTEXT("AddNewCueTransaction", "Add Cue"));

		// Add a new cue with faders data
		ControlConsoleCueStack->PreEditChange(nullptr);
		const FDMXControlConsoleCue* NewCue = ControlConsoleCueStack->AddNewCue(FadersToCue);
		ControlConsoleCueStack->PostEditChange();
		if (NewCue)
		{
			// Update the last recalled cue
			ControlConsoleEditorData->PreEditChange(nullptr);
			ControlConsoleEditorData->LoadedCue = *NewCue;
			ControlConsoleEditorData->PostEditChange();

			UpdateCueStackComboBoxSource();
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorCueStackComboBox::OnStoreCueClicked()
	{
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack || !CueStackComboBox.IsValid())
		{
			return FReply::Unhandled();
		}

		const TSharedPtr<FDMXControlConsoleEditorCueListItem> SelectedItem = CueStackComboBox->GetSelectedItem();
		if (!SelectedItem.IsValid())
		{
			return FReply::Unhandled();
		}

		TArray<UDMXControlConsoleFaderBase*> FadersToCue;
		const FDMXControlConsoleCue& SelectedCue = SelectedItem->GetCue();
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FadersToCue.Append(FaderGroup->GetAllFaders());
			}
		}

		ControlConsoleCueStack->UpdateCueData(SelectedCue.CueID, FadersToCue);

		return FReply::Handled();
	}

	FSlateColor SDMXControlConsoleEditorCueStackComboBox::GetLoadedCueColor() const
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		const bool bHasValidCueData =
			ControlConsoleEditorData &&
			ControlConsoleCueStack &&
			ControlConsoleCueStack->FindCue(ControlConsoleEditorData->LoadedCue.CueID);

		return bHasValidCueData ? ControlConsoleEditorData->LoadedCue.CueColor : FLinearColor::White;
	}

	FText SDMXControlConsoleEditorCueStackComboBox::GetLoadedCueNameAsText() const
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		const bool bHasValidCueData =
			ControlConsoleEditorData &&
			ControlConsoleCueStack &&
			ControlConsoleCueStack->FindCue(ControlConsoleEditorData->LoadedCue.CueID);

		if (!bHasValidCueData)
		{
			return LOCTEXT("NoValidCueText", "No Cue");
		}

		FString LastRecalledCueName = ControlConsoleEditorData->LoadedCue.CueLabel;

		// Add 'edited' tag if the control console data are not synched to the loaded cue
		if (ControlConsoleCueStack->CanStore())
		{
			LastRecalledCueName += TEXT("  [edited]");
		}

		return FText::FromString(LastRecalledCueName);
	}
}

#undef LOCTEXT_NAMESPACE
