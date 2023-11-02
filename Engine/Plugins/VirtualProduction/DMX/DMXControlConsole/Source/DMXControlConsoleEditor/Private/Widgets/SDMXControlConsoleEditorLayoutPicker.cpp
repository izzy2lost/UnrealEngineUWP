// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutPicker.h"

#include "DMXControlConsoleEditorSelection.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorLayoutPicker"

void SDMXControlConsoleEditorLayoutPicker::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
{
	if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct layout picker correctly.")))
	{
		return;
	}

	EditorModel = InEditorModel;

	ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.Padding(0.f, 8.f, 0.f, 4.f)
			.AutoHeight()
			[
				GenerateLayoutCheckBoxWidget()
			]

			+SVerticalBox::Slot()
			.Padding(0.f, 4.f, 0.f, 8.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutPicker::GetComboBoxVisibility))

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SAssignNew(UserLayoutsComboBox, SComboBox<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>>)
					.OptionsSource(&ComboBoxSource)
					.OnGenerateWidget(this, &SDMXControlConsoleEditorLayoutPicker::GenerateLayoutComboBoxWidget)
					.OnComboBoxOpening(this, &SDMXControlConsoleEditorLayoutPicker::UpdateComboBoxSource)
					.OnSelectionChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnComboBoxSelectionChanged)
					.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
					.ItemStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row")))
					.Content()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.MaxWidth(80.f)
						.Padding(4.f, 0.f)
						.AutoWidth()
						[
							SAssignNew(LayoutNameEditableBox, SEditableTextBox)
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text_Lambda([this](){ return LayoutNameText; })
							.OverflowPolicy(ETextOverflowPolicy::Clip)
							.OnTextChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextChanged)
							.OnTextCommitted(this, &SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextCommitted)
						]
					]
				]

				// Add Layout button
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Plus"))
						.ColorAndOpacity(FStyleColors::AccentGreen)
					]
				]

				// Rename Layout button
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnRenameLayoutClicked)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Edit"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Delete Layout button
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton")))
					.OnClicked(this, &SDMXControlConsoleEditorLayoutPicker::OnDeleteLayoutClicked)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
		];

	UpdateComboBoxSource();

	// Sync to control console's active layout
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
	{
		UserLayoutsComboBox->SetSelectedItem(ActiveLayout);
		LastSelectedItem = ActiveLayout;
		LayoutNameText = FText::FromString(ActiveLayout->LayoutName);
	}
}

TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateLayoutCheckBoxWidget()
{
	const TSharedRef<SWidget> LayoutCheckBoxWidget =
		SNew(SHorizontalBox)

		// Default Layout Mode
		+ SHorizontalBox::Slot()
		.Padding(2.f, 2.f, 4.f, 2.f)
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutChecked)
				.IsChecked_Lambda([this]() { return IsDefaultLayoutActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("DefaultLayoutModeLabel", "Default"))
			]
		]

		// User Layout Mode
		+ SHorizontalBox::Slot()
		.Padding(4.f, 2.f)
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.OnCheckStateChanged(this, &SDMXControlConsoleEditorLayoutPicker::OnUserLayoutChecked)
				.IsChecked_Lambda([this]() { return IsDefaultLayoutActive() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked; })
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("UserLayoutModeLabel", "User"))
			]
		];

	return LayoutCheckBoxWidget;
}

TSharedRef<SWidget> SDMXControlConsoleEditorLayoutPicker::GenerateLayoutComboBoxWidget(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout)
{
	if (InLayout.IsValid())
	{
		const TSharedRef<SWidget> ComboBoxWidget =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.MaxWidth(80.f)
			.Padding(6.f, 0.f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([InLayout](){ return InLayout.IsValid() ? FText::FromString(InLayout->LayoutName) : FText::GetEmpty(); })
			];

		return ComboBoxWidget;
	}

	return SNullWidget::NullWidget;
}

bool SDMXControlConsoleEditorLayoutPicker::IsDefaultLayoutActive() const
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (ControlConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		return ActiveLayout && ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	return false;
}

void SDMXControlConsoleEditorLayoutPicker::OnDefaultLayoutChecked(ECheckBoxState CheckBoxState)
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (ControlConsoleLayouts)
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		UDMXControlConsoleEditorGlobalLayoutBase* DefaultLayout = &ControlConsoleLayouts->GetDefaultLayoutChecked();

		const FScopedTransaction SetActiveDefaultLayoutTransaction(LOCTEXT("SetActiveDefaultLayoutTransaction", "Change Layout"));
		ControlConsoleLayouts->Modify();
		ControlConsoleLayouts->SetActiveLayout(DefaultLayout);
	}
}

void SDMXControlConsoleEditorLayoutPicker::OnUserLayoutChecked(ECheckBoxState CheckBoxState)
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const FScopedTransaction SetActiveUserLayoutTransaction(LOCTEXT("SetActiveUserLayoutTransaction", "Change Layout"));
	ControlConsoleLayouts->Modify();

	if (LastSelectedItem.IsValid())
	{
		ControlConsoleLayouts->SetActiveLayout(LastSelectedItem.Get());
		LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
	}
	else
	{
		const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* NewSelectedLayout = nullptr;
		if (UserLayouts.IsEmpty())
		{
			NewSelectedLayout = ControlConsoleLayouts->AddUserLayout("");
		}
		else
		{
			NewSelectedLayout = UserLayouts.Last();
		}

		if (NewSelectedLayout)
		{
			ControlConsoleLayouts->SetActiveLayout(NewSelectedLayout);

			UpdateComboBoxSource();
			UserLayoutsComboBox->SetSelectedItem(NewSelectedLayout);
			LastSelectedItem = NewSelectedLayout;
			LayoutNameText = FText::FromString(NewSelectedLayout->LayoutName);
		}
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	SelectionHandler->ClearSelection();
}

void SDMXControlConsoleEditorLayoutPicker::UpdateComboBoxSource()
{
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (!ControlConsoleLayouts)
	{
		return;
	}
		
	const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
	ComboBoxSource.Reset(UserLayouts.Num());
	ComboBoxSource.Append(UserLayouts);

	if (UserLayoutsComboBox.IsValid())
	{
		UserLayoutsComboBox->RefreshOptions();
	}
}

void SDMXControlConsoleEditorLayoutPicker::OnComboBoxSelectionChanged(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (!InLayout.IsValid() || !EditorModel.IsValid() || !UserLayoutsComboBox.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	SelectionHandler->ClearSelection();

	const FScopedTransaction UserLayoutSelectionChangedTransaction(LOCTEXT("UserLayoutSelectionChangedTransaction", "Change Layout"));
	ControlConsoleLayouts->Modify();
	ControlConsoleLayouts->SetActiveLayout(InLayout.Get());

	UserLayoutsComboBox->SetSelectedItem(InLayout);
	LastSelectedItem = InLayout;
	LayoutNameText = FText::FromString(InLayout->LayoutName);
}

void SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextChanged(const FText& NewText)
{
	LayoutNameText = NewText;
}

void SDMXControlConsoleEditorLayoutPicker::OnLayoutNameTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter && !LayoutNameText.IsEmpty())
	{
		const FString& NewLayoutName = LayoutNameText.ToString();
		OnRenameLayout(NewLayoutName);
	}
}

void SDMXControlConsoleEditorLayoutPicker::OnRenameLayout(const FString& NewName)
{
	if (LastSelectedItem.IsValid())
	{
		const FScopedTransaction LayoutNameEditedTransaction(LOCTEXT("LayoutNameEditedTransaction", "Edit Layout Name"));
		LastSelectedItem->PreEditChange(UDMXControlConsoleEditorGlobalLayoutBase::StaticClass()->FindPropertyByName(UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutNamePropertyName()));
		LastSelectedItem->LayoutName = NewName;
		LastSelectedItem->PostEditChange();

		LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
	}
}

FReply SDMXControlConsoleEditorLayoutPicker::OnAddLayoutClicked()
{
	if (!EditorModel.IsValid())
	{
		return FReply::Handled();
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (ControlConsoleLayouts)
	{
		const FString& NewLayoutName = LayoutNameText.ToString();

		const FScopedTransaction AddUserLayoutTransaction(LOCTEXT("AddUserLayoutTransaction", "Add New Layout"));
		ControlConsoleLayouts->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutBase* NewUserLayout = ControlConsoleLayouts->AddUserLayout(NewLayoutName);
		ControlConsoleLayouts->SetActiveLayout(NewUserLayout);
		ControlConsoleLayouts->PostEditChange();

		UpdateComboBoxSource();
		UserLayoutsComboBox->SetSelectedItem(NewUserLayout);
		LastSelectedItem = NewUserLayout;
		LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();
	}

	return FReply::Handled();
}

FReply SDMXControlConsoleEditorLayoutPicker::OnRenameLayoutClicked()
{
	if (!LayoutNameText.IsEmpty())
	{
		const FString& NewLayoutName = LayoutNameText.ToString();
		OnRenameLayout(NewLayoutName);
	}

	return FReply::Handled();
}

FReply SDMXControlConsoleEditorLayoutPicker::OnDeleteLayoutClicked()
{
	if (!EditorModel.IsValid())
	{
		return FReply::Handled();
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return FReply::Handled();
	}

	const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
	if (LastSelectedItem.IsValid() && UserLayouts.Contains(LastSelectedItem))
	{
		const int32 LayoutIndex = UserLayouts.IndexOfByKey(LastSelectedItem);

		const FScopedTransaction DeleteUserLayoutTransaction(LOCTEXT("DeleteUserLayoutTransaction", "Delete Layout"));
		ControlConsoleLayouts->Modify();
		ControlConsoleLayouts->DeleteUserLayout(LastSelectedItem.Get());

		if (LayoutIndex > 0)
		{
			// Select the previous user layout in the array
			UDMXControlConsoleEditorGlobalLayoutBase* LayoutToSelect = UserLayouts[LayoutIndex - 1];
			ControlConsoleLayouts->SetActiveLayout(LayoutToSelect);

			UpdateComboBoxSource();
			UserLayoutsComboBox->SetSelectedItem(LayoutToSelect);
			LastSelectedItem = LayoutToSelect;
			LayoutNameText = FText::FromString(LastSelectedItem->LayoutName);
		}
		else
		{
			// If there are no more user layouts switch to default layout
			UDMXControlConsoleEditorGlobalLayoutBase* DefaultLayout = &ControlConsoleLayouts->GetDefaultLayoutChecked();
			ControlConsoleLayouts->SetActiveLayout(DefaultLayout);

			UserLayoutsComboBox->ClearSelection();
			LastSelectedItem = nullptr;
			LayoutNameText = FText::GetEmpty();
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();
	}

	return FReply::Handled();
}

EVisibility SDMXControlConsoleEditorLayoutPicker::GetComboBoxVisibility() const
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
