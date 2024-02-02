// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNiagaraDistributionEditor.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "NiagaraEditorStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/NiagaraDistributionEditorUtilities.h"
#include "Widgets/SNiagaraDistributionCurveEditor.h"

#define LOCTEXT_NAMESPACE "NiagaraDistributionEditor"

class SNiagaraDistributionModeSelector : public SComboButton
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDistributionModeSelector) { }
		SLATE_EVENT(FSimpleDelegate, OnDistributionModeChanged)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<INiagaraDistributionAdapter> InDistributionAdapter)
	{
		
		DistributionAdapter = InDistributionAdapter;
		OnDistributionChangedDelegate = InArgs._OnDistributionModeChanged;

		SComboButton::Construct(SComboButton::FArguments()
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(FMargin(0))
			.OnGetMenuContent(this, &SNiagaraDistributionModeSelector::OnGetMenuContent)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(this, &SNiagaraDistributionModeSelector::GetModeIcon)
				.ToolTipText(this, &SNiagaraDistributionModeSelector::GetModeToolTip)
			]);
	}

private:
	const FSlateBrush* GetModeIcon() const
	{
		UpdateCachedValues();
		return ModeIconCache;
	}

	FText GetModeToolTip() const
	{
		UpdateCachedValues();
		return ModeToolTipCache;
	}

	TSharedRef<SWidget> OnGetMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		TArray<ENiagaraDistributionEditorMode> SupportedModes;
		DistributionAdapter->GetSupportedDistributionModes(SupportedModes);
		for (ENiagaraDistributionEditorMode SupportedMode : SupportedModes)
		{
			MenuBuilder.AddMenuEntry(
				FNiagaraDistributionEditorUtilities::DistributionModeToDisplayName(SupportedMode),
				FNiagaraDistributionEditorUtilities::DistributionModeToToolTipText(SupportedMode),
				FNiagaraDistributionEditorUtilities::DistributionModeToIcon(SupportedMode),
				FUIAction(
					FExecuteAction::CreateSP(this, &SNiagaraDistributionModeSelector::DistributionModeSelected, SupportedMode),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SNiagaraDistributionModeSelector::IsDistributionModeSelected, SupportedMode)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		return MenuBuilder.MakeWidget();
	}

	void UpdateCachedValues() const
	{
		ENiagaraDistributionEditorMode CurrentMode = DistributionAdapter->GetDistributionMode();
		if (ModeCache.IsSet() == false || ModeCache.GetValue() != CurrentMode)
		{
			ModeCache = CurrentMode;
			ModeIconCache = FNiagaraDistributionEditorUtilities::DistributionModeToIconBrush(CurrentMode);
			ModeToolTipCache = FText::Format(LOCTEXT("ModeSelectorToolTip", "{0} - {1}"),
				FNiagaraDistributionEditorUtilities::DistributionModeToDisplayName(CurrentMode),
				FNiagaraDistributionEditorUtilities::DistributionModeToToolTipText(CurrentMode));
		}
	}

	bool IsDistributionModeSelected(ENiagaraDistributionEditorMode InSelectedMode) const
	{
		UpdateCachedValues();
		return ModeCache == InSelectedMode;
	}

	void DistributionModeSelected(ENiagaraDistributionEditorMode InSelectedMode)
	{
		DistributionAdapter->SetDistributionMode(InSelectedMode);
		UpdateCachedValues();
		OnDistributionChangedDelegate.ExecuteIfBound();
	}

private:
	TSharedPtr<INiagaraDistributionAdapter> DistributionAdapter;
	FSimpleDelegate OnDistributionChangedDelegate;

	mutable TOptional<ENiagaraDistributionEditorMode> ModeCache;
	mutable const FSlateBrush* ModeIconCache = nullptr;
	mutable FText ModeToolTipCache;
};


class SNiagaraDistributionValueEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDistributionValueEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<INiagaraDistributionAdapter> InDistributionAdapter)
	{
		DistributionAdapter = InDistributionAdapter;
		bool bIsUniform = FNiagaraDistributionEditorUtilities::IsUniform(DistributionAdapter->GetDistributionMode());
		bool bIsConstant = FNiagaraDistributionEditorUtilities::IsConstant(DistributionAdapter->GetDistributionMode());

		FText MinLabelText = LOCTEXT("MinLabel", "Min");
		FText MaxLabelText = LOCTEXT("MaxLabel", "Max");

		TSharedPtr<SWidget> ChildWidget;
		if (bIsUniform)
		{
			if (bIsConstant)
			{
				ChildWidget = ConstructFloatWidget(0, 0, FText());
			}
			else
			{
				ChildWidget = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						ConstructFloatWidget(0, 0, MinLabelText)
					]
					+ SHorizontalBox::Slot()
					[
						ConstructFloatWidget(0, 1, MaxLabelText)
					];
			}
		}
		else
		{
			if (bIsConstant)
			{
				ChildWidget = ConstructVectorWidget(DistributionAdapter->GetNumChannels(), 0);
			}
			else
			{
				ChildWidget = SNew(SGridPanel)
					.FillColumn(1, 1)
					+ SGridPanel::Slot(0, 0)
					.VAlign(VAlign_Center)
					.Padding(0, 0, 5, 3)
					[
						SNew(STextBlock)
						.Text(MinLabelText)
					]
					+ SGridPanel::Slot(1, 0)
					.Padding(0, 0, 0, 3)
					[
						ConstructVectorWidget(DistributionAdapter->GetNumChannels(), 0)
					]
					+ SGridPanel::Slot(0, 1)
					.VAlign(VAlign_Center)
					.Padding(0, 0, 5, 0)
					[
						SNew(STextBlock)
						.Text(MaxLabelText)
					]
					+ SGridPanel::Slot(1, 1)
					[
						ConstructVectorWidget(DistributionAdapter->GetNumChannels(), 1)
					];
			}
		}

		ChildSlot
		[
			ChildWidget.ToSharedRef()
		];
	}

private:
	TSharedRef<SWidget> ConstructFloatWidget(int32 ChannelIndex, int32 ValueIndex, FText LabelText)
	{
		TSharedRef<SWidget> LabelWidget = LabelText.IsEmpty()
			? SNullWidget::NullWidget
			: SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(LabelText);

		return SNew(SNumericEntryBox<float>)
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			.Value(this, &SNiagaraDistributionValueEditor::GetValue, ChannelIndex, ValueIndex)
			.OnValueChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, ChannelIndex, ValueIndex)
			.OnValueCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, ChannelIndex, ValueIndex)
			.OnBeginSliderMovement(this, &SNiagaraDistributionValueEditor::BeginValueSliderMovement)
			.OnEndSliderMovement(this, &SNiagaraDistributionValueEditor::EndValueSliderMovement)
			.AllowSpin(true)
			.MinValue(TOptional<float>())
			.MaxValue(TOptional<float>())
			.MinSliderValue(TOptional<float>())
			.MaxSliderValue(TOptional<float>())
			.BroadcastValueChangesPerKey(false)
			.LabelVAlign(EVerticalAlignment::VAlign_Center)
			.MinDesiredValueWidth(30)
			.Label()
			[
				LabelWidget
			];
	}

	typedef SNumericVectorInputBox<float, UE::Math::TVector2<float>, 2> SNumericVectorInputBox2;
	typedef SNumericVectorInputBox<float, UE::Math::TVector<float>, 3> SNumericVectorInputBox3;
	typedef SNumericVectorInputBox<float, UE::Math::TVector4<float>, 4> SNumericVectorInputBox4;

	TSharedRef<SWidget> ConstructVectorWidget(int32 ChannelCount, int32 ValueIndex)
	{
		if (ChannelCount == 2)
		{
			return SNew(SNumericVectorInputBox2)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.X(this, &SNiagaraDistributionValueEditor::GetValue, 0, ValueIndex)
				.OnXChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 0, ValueIndex)
				.OnXCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 0, ValueIndex)
				.Y(this, &SNiagaraDistributionValueEditor::GetValue, 1, ValueIndex)
				.OnYChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 1, ValueIndex)
				.OnYCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 1, ValueIndex)
				.OnBeginSliderMovement(this, &SNiagaraDistributionValueEditor::BeginValueSliderMovement)
				.OnEndSliderMovement(this, &SNiagaraDistributionValueEditor::EndValueSliderMovement);
		}
		if (ChannelCount == 3)
		{
			return SNew(SNumericVectorInputBox3)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.X(this, &SNiagaraDistributionValueEditor::GetValue, 0, ValueIndex)
				.OnXChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 0, ValueIndex)
				.OnXCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 0, ValueIndex)
				.Y(this, &SNiagaraDistributionValueEditor::GetValue, 1, ValueIndex)
				.OnYChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 1, ValueIndex)
				.OnYCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 1, ValueIndex)
				.Z(this, &SNiagaraDistributionValueEditor::GetValue, 2, ValueIndex)
				.OnZChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 2, ValueIndex)
				.OnZCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 2, ValueIndex)
				.OnBeginSliderMovement(this, &SNiagaraDistributionValueEditor::BeginValueSliderMovement)
				.OnEndSliderMovement(this, &SNiagaraDistributionValueEditor::EndValueSliderMovement);
		}
		if (ChannelCount == 4)
		{
			return SNew(SNumericVectorInputBox4)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.AllowSpin(true)
				.bColorAxisLabels(true)
				.X(this, &SNiagaraDistributionValueEditor::GetValue, 0, ValueIndex)
				.OnXChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 0, ValueIndex)
				.OnXCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 0, ValueIndex)
				.Y(this, &SNiagaraDistributionValueEditor::GetValue, 1, ValueIndex)
				.OnYChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 1, ValueIndex)
				.OnYCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 1, ValueIndex)
				.Z(this, &SNiagaraDistributionValueEditor::GetValue, 2, ValueIndex)
				.OnZChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 2, ValueIndex)
				.OnZCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 2, ValueIndex)
				.W(this, &SNiagaraDistributionValueEditor::GetValue, 3, ValueIndex)
				.OnWChanged(this, &SNiagaraDistributionValueEditor::ValueChanged, 3, ValueIndex)
				.OnWCommitted(this, &SNiagaraDistributionValueEditor::ValueCommitted, 3, ValueIndex)
				.OnBeginSliderMovement(this, &SNiagaraDistributionValueEditor::BeginValueSliderMovement)
				.OnEndSliderMovement(this, &SNiagaraDistributionValueEditor::EndValueSliderMovement);
		}
		return SNullWidget::NullWidget;
	}

	TOptional<float> GetValue(int32 ChannelIndex, int32 ValueIndex) const
	{
		return DistributionAdapter->GetConstantOrRangeValue(ChannelIndex, ValueIndex);
	}

	void ValueChanged(float Value, int32 ChannelIndex, int32 ValueIndex)
	{
		DistributionAdapter->SetConstantOrRangeValue(ChannelIndex, ValueIndex, Value);
	}
	
	void ValueCommitted(float Value, ETextCommit::Type CommitInfo, int32 ChannelIndex, int32 ValueIndex)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			DistributionAdapter->SetConstantOrRangeValue(ChannelIndex, ValueIndex, Value);
		}
	}

	void BeginValueSliderMovement()
	{
		DistributionAdapter->BeginContinuousChange();
	}

	void EndValueSliderMovement(float Value)
	{
		DistributionAdapter->EndContinuousChange();
	}

private:
	TSharedPtr<INiagaraDistributionAdapter> DistributionAdapter;
};

void SNiagaraDistributionEditor::Construct(const FArguments& InArgs, TSharedRef<INiagaraDistributionAdapter> InDistributionAdapter)
{
	DistributionAdapter = InDistributionAdapter;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 10, 0)
		[
			SNew(SNiagaraDistributionModeSelector, InDistributionAdapter)
			.OnDistributionModeChanged(this, &SNiagaraDistributionEditor::OnDistributionModeChanged)
		]
		+ SHorizontalBox::Slot()
		[
			SAssignNew(ContentBox, SBox)
			[
				ConstructContentForMode()
			]
		]
	];
}

void  SNiagaraDistributionEditor::OnDistributionModeChanged()
{
	ContentBox->SetContent(ConstructContentForMode());
}

TSharedRef<SWidget> SNiagaraDistributionEditor::ConstructContentForMode()
{
	ENiagaraDistributionEditorMode Mode = DistributionAdapter->GetDistributionMode();
	if (FNiagaraDistributionEditorUtilities::IsConstant(Mode) || FNiagaraDistributionEditorUtilities::IsRange(Mode))
	{
		return SNew(SNiagaraDistributionValueEditor, DistributionAdapter.ToSharedRef());
	}
	else if (FNiagaraDistributionEditorUtilities::IsCurve(Mode))
	{
		TSharedRef<SVerticalBox> CurveBox = SNew(SVerticalBox);
		int32 NumberOfChannels = FNiagaraDistributionEditorUtilities::IsUniform(Mode) ? 1 : DistributionAdapter->GetNumChannels();
		for (int32 ChannelIndex = 0; ChannelIndex < NumberOfChannels; ChannelIndex++)
		{
			CurveBox->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 3)
			[
				SNew(SNiagaraDistributionCurveEditor, DistributionAdapter.ToSharedRef(), ChannelIndex)
				.CurveColor(DistributionAdapter->GetChannelColor(ChannelIndex))
			];
		}
		return CurveBox;
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

#undef LOCTEXT_NAMESPACE