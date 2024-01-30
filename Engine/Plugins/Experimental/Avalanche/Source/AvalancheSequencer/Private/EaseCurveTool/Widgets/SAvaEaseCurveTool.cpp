// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurveTool.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EaseCurveTool/AvaEaseCurveToolCommands.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveEditor.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreset.h"
#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Math/UnrealMathUtility.h"
#include "SCurveEditor.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurveTool"

namespace UE::AvaSequencer
{
	template<typename NumericType>
	void ExtractNumericMetadata(FProperty* InProperty
		, TOptional<NumericType>& OutMinValue
		, TOptional<NumericType>& OutMaxValue
		, TOptional<NumericType>& OutSliderMinValue
		, TOptional<NumericType>& OutSliderMaxValue
		, NumericType& OutSliderExponent
		, NumericType& OutDelta
		, float& OutShiftMultiplier
		, float& OutCtrlMultiplier
		, bool& OutSupportDynamicSliderMaxValue
		, bool& OutSupportDynamicSliderMinValue)
	{
		const FString& MetaUIMinString = InProperty->GetMetaData(TEXT("UIMin"));
		const FString& MetaUIMaxString = InProperty->GetMetaData(TEXT("UIMax"));
		const FString& SliderExponentString = InProperty->GetMetaData(TEXT("SliderExponent"));
		const FString& DeltaString = InProperty->GetMetaData(TEXT("Delta"));
		const FString& ShiftMultiplierString = InProperty->GetMetaData(TEXT("ShiftMultiplier"));
		const FString& CtrlMultiplierString = InProperty->GetMetaData(TEXT("CtrlMultiplier"));
		const FString& SupportDynamicSliderMaxValueString = InProperty->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
		const FString& SupportDynamicSliderMinValueString = InProperty->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
		const FString& ClampMinString = InProperty->GetMetaData(TEXT("ClampMin"));
		const FString& ClampMaxString = InProperty->GetMetaData(TEXT("ClampMax"));

		// If no UIMin/Max was specified then use the clamp string
		const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
		const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

		NumericType ClampMin = TNumericLimits<NumericType>::Lowest();
		NumericType ClampMax = TNumericLimits<NumericType>::Max();

		if (!ClampMinString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMin, *ClampMinString);
		}

		if (!ClampMaxString.IsEmpty())
		{
			TTypeFromString<NumericType>::FromString(ClampMax, *ClampMaxString);
		}

		NumericType UIMin = TNumericLimits<NumericType>::Lowest();
		NumericType UIMax = TNumericLimits<NumericType>::Max();
		TTypeFromString<NumericType>::FromString(UIMin, *UIMinString);
		TTypeFromString<NumericType>::FromString(UIMax, *UIMaxString);

		OutSliderExponent = NumericType(1);

		if (SliderExponentString.Len())
		{
			TTypeFromString<NumericType>::FromString(OutSliderExponent, *SliderExponentString);
		}

		OutDelta = NumericType(0);

		if (DeltaString.Len())
		{
			TTypeFromString<NumericType>::FromString(OutDelta, *DeltaString);
		}

		OutShiftMultiplier = 10.f;
		if (ShiftMultiplierString.Len())
		{
			TTypeFromString<float>::FromString(OutShiftMultiplier, *ShiftMultiplierString);
		}

		OutCtrlMultiplier = 0.1f;
		if (CtrlMultiplierString.Len())
		{
			TTypeFromString<float>::FromString(OutCtrlMultiplier, *CtrlMultiplierString);
		}

		const NumericType ActualUIMin = FMath::Max(UIMin, ClampMin);
		const NumericType ActualUIMax = FMath::Min(UIMax, ClampMax);

		OutMinValue = ClampMinString.Len() ? ClampMin : TOptional<NumericType>();
		OutMaxValue = ClampMaxString.Len() ? ClampMax : TOptional<NumericType>();
		OutSliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<NumericType>();
		OutSliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<NumericType>();

		OutSupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
		OutSupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
	}
}

void SAvaEaseCurveTool::Construct(const FArguments& InArgs, const TSharedRef<FAvaEaseCurveTool>& InCurveEaseTool)
{
	ToolMode = InArgs._ToolMode;
	ToolOperation = InArgs._ToolOperation;

	CurveEaseTool = InCurveEaseTool;

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SAssignNew(CurvePresetWidget, SAvaEaseCurvePreset)
				.OnPresetChanged(this, &SAvaEaseCurveTool::OnPresetChanged)
				.OnGetNewPresetTangents_Lambda([this](FAvaEaseCurveTangents& OutTangents) -> bool
					{
						OutTangents = CurveEaseTool->GetEaseCurveTangents();
						return true;
					})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				ConstructCurveEditorPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				ConstructInputBoxes()
			]
		];

	BindCommands();

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructCurveEditorPanel()
{
	CurrentGraphSize = GetDefault<UAvaEaseCurveToolSettings>()->GetGraphSize();

	const TSharedRef<FAvaEaseCurveTool> EaseCurveToolRef = CurveEaseTool.ToSharedRef();

	return SNew(SBorder)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(CurveEaseEditorWidget, SAvaEaseCurveEditor, CurveEaseTool->GetToolCurve())
				.DisplayRate(EaseCurveToolRef, &FAvaEaseCurveTool::GetDisplayRate)
				.Operation(EaseCurveToolRef, &FAvaEaseCurveTool::GetOperation)
				.DesiredSize_Lambda([this]() -> FVector2D
					{
						return FVector2D(CurrentGraphSize);
					})
				.OnTangentsChanged(this, &SAvaEaseCurveTool::HandleEditorTangentsChanged)
				.GridSnap_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSnap)
				.GridSize_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSize)
				.GetContextMenuContent(this, &SAvaEaseCurveTool::CreateContextMenuContent)
				.OnKeyDown(this, &SAvaEaseCurveTool::OnKeyDown)
				.OnDragStart(this, &SAvaEaseCurveTool::OnEditorDragStart)
				.OnDragEnd(this, &SAvaEaseCurveTool::OnEditorDragEnd)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").TopShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").BottomShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").LeftShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox").RightShadowBrush))
			]
		];
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructInputBoxes()
{
	constexpr float WrapSize = 120.f;

	constexpr float MinTangent = -180.0f;
	constexpr float MaxTangent = 180.0f;
	constexpr float MinWeight = 0.0f;
	constexpr float MaxWeight = 10.0f;

	return SNew(SWrapBox)
		.UseAllottedSize(true)
		.HAlign(HAlign_Center)
		+ SWrapBox::Slot()
		.FillLineWhenSizeLessThan(WrapSize)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(26.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("OutLabel", "Out"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("OutTangentLabel", "T")
					, LOCTEXT("OutTangentToolTip", "Out Tangent")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetStartTangent)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnStartTangentSpinBoxChanged)
					, MinTangent, MaxTangent)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("OutTangentWeightLabel", "W")
					, LOCTEXT("OutTangentWeightToolTip", "Out Tangent Weight")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetStartTangentWeight)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged)
					, MinWeight, MaxWeight)
			]
		]
		+ SWrapBox::Slot()
		.FillLineWhenSizeLessThan(WrapSize)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(26.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("InLabel", "In"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("InTangentLabel", "T")
					, LOCTEXT("InTangentToolTip", "In Tangent")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetEndTangent)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnEndTangentSpinBoxChanged)
					, MinTangent, MaxTangent)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				ConstructTangentNumBox(LOCTEXT("InTangentWeightLabel", "W")
					, LOCTEXT("InTangentWeightToolTip", "In Tangent Weight")
					, TAttribute<float>::CreateSP(this, &SAvaEaseCurveTool::GetEndTangentWeight)
					, SNumericEntryBox<float>::FOnValueChanged::CreateSP(this, &SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged)
					, MinWeight, MaxWeight)
			]
		];
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructTangentNumBox(const FText& InLabel
	, const FText& InToolTip
	, const TAttribute<float>& InValue
	, const SNumericEntryBox<float>::FOnValueChanged& InOnValueChanged
	, const TOptional<float>& InMinSliderValue
	, const TOptional<float>& InMaxSliderValue) const
{
	return SNew(SBox)
		.MaxDesiredWidth(100.f)
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.MinDesiredWidth(8.f)
				.Margin(FMargin(2.f, 5.f, 2.f, 3.f))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(InLabel)
				.ToolTipText(InToolTip)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SSpinBox<float>)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinSliderValue(InMinSliderValue)
				.MaxSliderValue(InMaxSliderValue)
				.Delta(0.00001f)
				.WheelStep(0.001f)
				.MinFractionalDigits(4)
				.MaxFractionalDigits(6)
				.MinDesiredWidth(70.f)
				.Value(InValue)
				.OnBeginSliderMovement_Lambda([this]()
					{
						CurveEaseTool->BeginTransaction(LOCTEXT("SliderDragStartLabel", "Ease Curve Slider Drag"));
					})
				.OnEndSliderMovement_Lambda([this](const float InNewValue)
					{
						CurveEaseTool->EndTransaction();
					})
				.OnValueChanged_Lambda([this, InOnValueChanged](const float InNewValue)
					{
						InOnValueChanged.ExecuteIfBound(InNewValue);
					})
				.OnValueCommitted_Lambda([InOnValueChanged](const float InNewValue, ETextCommit::Type InCommitType)
					{
						InOnValueChanged.ExecuteIfBound(InNewValue);
					})
			]
		];
}

void SAvaEaseCurveTool::HandleEditorTangentsChanged(const FAvaEaseCurveTangents& InTangents) const
{
	SetTangents(InTangents, true, true, true);
}

void SAvaEaseCurveTool::OnEditorDragStart() const
{
	CurveEaseTool->BeginTransaction(LOCTEXT("EditorDragStartLabel", "Ease Curve Graph Drag"));
}

void SAvaEaseCurveTool::OnEditorDragEnd() const
{
	CurveEaseTool->EndTransaction();
}

void SAvaEaseCurveTool::SetTangents(const FAvaEaseCurveTangents& InTangents, const bool bInSetEaseCurve, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents) const
{
	if (CurvePresetWidget.IsValid())
	{
		if (!CurvePresetWidget->SetSelectedItem(InTangents))
		{
			CurvePresetWidget->ClearSelection();
		}
	}

	// To change the graph UI tangents, we need to change the ease curve object tangents and the graph will reflect.
	if (bInSetEaseCurve && CurveEaseTool.IsValid())
	{
		CurveEaseTool->SetEaseCurveTangents(InTangents, bInBroadcastUpdate, bInSetSequencerTangents);
	}

	if (CurveEaseEditorWidget.IsValid())
	{
		CurveEaseEditorWidget->ZoomToFit();
	}
}

float SAvaEaseCurveTool::GetStartTangent() const
{
	return CurveEaseTool->GetEaseCurveTangents().Start;
}

float SAvaEaseCurveTool::GetStartTangentWeight() const
{
	return CurveEaseTool->GetEaseCurveTangents().StartWeight;
}

float SAvaEaseCurveTool::GetEndTangent() const
{
	return CurveEaseTool->GetEaseCurveTangents().End;
}

float SAvaEaseCurveTool::GetEndTangentWeight() const
{
	return CurveEaseTool->GetEaseCurveTangents().EndWeight;
}

void SAvaEaseCurveTool::OnStartTangentSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = CurveEaseTool->GetEaseCurveTangents();
	NewTangents.Start = InNewValue;
	SetTangents(NewTangents, true, true, true);
}

void SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = CurveEaseTool->GetEaseCurveTangents();
	NewTangents.StartWeight = InNewValue;
	SetTangents(NewTangents, true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = CurveEaseTool->GetEaseCurveTangents();
	NewTangents.End = InNewValue;
	SetTangents(NewTangents, true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged(const float InNewValue) const
{
	FAvaEaseCurveTangents NewTangents = CurveEaseTool->GetEaseCurveTangents();
	NewTangents.EndWeight = InNewValue;
	SetTangents(NewTangents, true, true, true);
}

void SAvaEaseCurveTool::OnPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const
{
	SetTangents(InPreset->Tangents, true, true, true);
}

void SAvaEaseCurveTool::BindCommands()
{
	const FAvaEaseCurveToolCommands& AvaCurveEaseToolCommands = FAvaEaseCurveToolCommands::Get();

	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Undo, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::UndoAction));
	CommandList->MapAction(FGenericCommands::Get().Redo, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::RedoAction));

	CommandList->MapAction(AvaCurveEaseToolCommands.OpenToolSettings, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::OpenToolSettings));

	CommandList->MapAction(AvaCurveEaseToolCommands.Refresh, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::UpdateEaseCurveFromSequencerKeySelections));
	CommandList->MapAction(AvaCurveEaseToolCommands.Apply, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::ApplyEaseCurveToSequencerKeySelections));

	CommandList->MapAction(AvaCurveEaseToolCommands.ZoomToFit, FExecuteAction::CreateSP(this, &SAvaEaseCurveTool::ZoomToFit));
	CommandList->MapAction(AvaCurveEaseToolCommands.ToggleGridSnap
		, FExecuteAction::CreateUObject(GetMutableDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::ToggleGridSnap)
		, FCanExecuteAction()
		, FIsActionChecked::CreateUObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSnap));
	CommandList->MapAction(AvaCurveEaseToolCommands.ToggleAutoFlipTangents
		, FExecuteAction::CreateUObject(GetMutableDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::ToggleAutoFlipTangents)
		, FCanExecuteAction()
		, FIsActionChecked::CreateUObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetAutoFlipTangents));

	CommandList->MapAction(AvaCurveEaseToolCommands.SelectNextChannelKey, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SelectNextChannelKey));
	CommandList->MapAction(AvaCurveEaseToolCommands.SelectPreviousChannelKey, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SelectPreviousChannelKey));

	CommandList->MapAction(AvaCurveEaseToolCommands.SetOperationToEaseOut
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::Out)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::Out));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetOperationToEaseInOut
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::InOut)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::InOut));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetOperationToEaseIn
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetToolOperation, FAvaEaseCurveTool::EOperation::In)
		, FCanExecuteAction()
		, FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsToolOperation, FAvaEaseCurveTool::EOperation::In));

	CommandList->MapAction(AvaCurveEaseToolCommands.ResetTangents
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::ResetEaseCurveTangents, true, true));
	CommandList->MapAction(AvaCurveEaseToolCommands.ResetStartTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::ResetEaseCurveTangents, true, false));
	CommandList->MapAction(AvaCurveEaseToolCommands.ResetEndTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::ResetEaseCurveTangents, false, true));

	CommandList->MapAction(AvaCurveEaseToolCommands.FlattenTangents
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, true, true, true));
	CommandList->MapAction(AvaCurveEaseToolCommands.FlattenStartTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, true, false, true));
	CommandList->MapAction(AvaCurveEaseToolCommands.FlattenEndTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, false, true, true));

	CommandList->MapAction(AvaCurveEaseToolCommands.StraightenTangents
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, true, true, false));
	CommandList->MapAction(AvaCurveEaseToolCommands.StraightenStartTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, true, false, false));
	CommandList->MapAction(AvaCurveEaseToolCommands.StraightenEndTangent
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::FlattenOrStraightenTangents, false, true, false));

	CommandList->MapAction(AvaCurveEaseToolCommands.CopyTangents
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::CopyTangentsToClipboard)
		, FCanExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::CanCopyTangentsToClipboard));
	CommandList->MapAction(AvaCurveEaseToolCommands.PasteTangents
		, FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::PasteTangentsFromClipboard)
		, FCanExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::CanPasteTangentsFromClipboard));

	CommandList->MapAction(AvaCurveEaseToolCommands.CreateExternalCurveAsset
		, FExecuteAction::CreateLambda([this]()
			{
				CurveEaseTool->CreateCurveAsset();
			})
		, FCanExecuteAction());

	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpConstant,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Constant, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Constant, RCTM_Auto));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpLinear,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Linear, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Linear, RCTM_Auto));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpCubicAuto,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_Auto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_Auto));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpCubicSmartAuto,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_SmartAuto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_SmartAuto));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpCubicUser,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_User),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_User));
	CommandList->MapAction(AvaCurveEaseToolCommands.SetKeyInterpCubicBreak,
		FExecuteAction::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::SetKeyInterpMode, RCIM_Cubic, RCTM_Break),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(CurveEaseTool.Get(), &FAvaEaseCurveTool::IsKeyInterpMode, RCIM_Cubic, RCTM_Break));
}

TSharedRef<SWidget> SAvaEaseCurveTool::CreateContextMenuContent()
{
	const FAvaEaseCurveToolCommands& AvaCurveEaseToolCommands = FAvaEaseCurveToolCommands::Get();

	TSharedPtr<TArray<TSharedPtr<FCurveViewModel>>> CurvesToAddKeysTo = MakeShareable(new TArray<TSharedPtr<FCurveViewModel>>());
	
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, CommandList);

	MenuBuilder.BeginSection(TEXT("CurveEditorActions"), LOCTEXT("CurveAction", "Curve Ease Tool Actions"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SettingsSubMenuLabel", "Settings"),
			LOCTEXT("SettingsSubMenuToolTip", ""),
			FNewMenuDelegate::CreateSP(this, &SAvaEaseCurveTool::MakeContextMenuSettings),
			false,
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("Icons.Toolbar.Settings")));
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.CreateExternalCurveAsset);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.CopyTangents);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.PasteTangents);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddSubMenu(
			LOCTEXT("StraightenTangentsSubMenuLabel", "Straighten Tangents"),
			LOCTEXT("StraightenTangentsSubMenuToolTip", ""),
			FNewMenuDelegate::CreateLambda([&AvaCurveEaseToolCommands](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.StraightenTangents);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.StraightenStartTangent);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.StraightenEndTangent);
				}),
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("GenericCurveEditor.StraightenTangents")));
		MenuBuilder.AddSubMenu(
			LOCTEXT("FlattenTangentsSubMenuLabel", "Flatten Tangents"),
			LOCTEXT("FlattenTangentsSubMenuToolTip", ""),
			FNewMenuDelegate::CreateLambda([&AvaCurveEaseToolCommands](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.FlattenTangents);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.FlattenStartTangent);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.FlattenEndTangent);
				}), 
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("GenericCurveEditor.FlattenTangents")));
		MenuBuilder.AddSubMenu(
			LOCTEXT("ResetTangentsSubMenuLabel", "Reset Tangents"),
			LOCTEXT("ResetTangentsSubMenuToolTip", ""),
			FNewMenuDelegate::CreateLambda([&AvaCurveEaseToolCommands](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ResetTangents);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ResetStartTangent);
					InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ResetEndTangent);
				}),
			false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), TEXT("PropertyWindow.DiffersFromDefault")));
		MenuBuilder.AddSeparator();
		/** @TODO: Only show these in single edit mode(?)
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpConstant);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpLinear);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpCubicAuto);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpCubicSmartAuto);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpCubicUser);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetKeyInterpCubicBreak);*/
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetOperationToEaseOut);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetOperationToEaseInOut);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SetOperationToEaseIn);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SelectPreviousChannelKey);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.SelectNextChannelKey);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ToggleGridSnap);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ZoomToFit);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.Refresh);
		MenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.Apply);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAvaEaseCurveTool::MakeContextMenuSettings(FMenuBuilder& InMenuBuilder)
{
	const FAvaEaseCurveToolCommands& AvaCurveEaseToolCommands = FAvaEaseCurveToolCommands::Get();

	InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.OpenToolSettings);
	InMenuBuilder.AddSeparator();
	InMenuBuilder.AddMenuEntry(AvaCurveEaseToolCommands.ToggleAutoFlipTangents);
	InMenuBuilder.AddSeparator();
	{
		FProperty* const GraphSizeProperty = UAvaEaseCurveToolSettings::StaticClass()->FindPropertyByName(TEXT("GraphSize"));
		check(GraphSizeProperty);

		TOptional<int32> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		int32 SliderExponent, Delta;
		float ShiftMultiplier = 10.f;
		float CtrlMultiplier = 0.1f;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;
		UE::AvaSequencer::ExtractNumericMetadata(GraphSizeProperty
			, MinValue, MaxValue
			, SliderMinValue, SliderMaxValue
			, SliderExponent, Delta
			, ShiftMultiplier
			, CtrlMultiplier
			, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		const TSharedRef<SNumericEntryBox<int32>> GraphSizeWidget = SNew(SNumericEntryBox<int32>)
			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.AllowSpin(true)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(SliderMinValue)
			.MaxSliderValue(SliderMaxValue)
			.SliderExponent(SliderExponent)
			.Delta(Delta)
			.ShiftMultiplier(ShiftMultiplier)
			.CtrlMultiplier(CtrlMultiplier)
			.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
			.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
			.Value_Lambda([this]()
				{
					return CurrentGraphSize;
				})
			.OnValueChanged_Lambda([this](const int32 InNewValue)
				{
					CurrentGraphSize = InNewValue;
				})
			.OnValueCommitted_Lambda([this](const int32 InNewValue, ETextCommit::Type InCommitType)
				{
					CurrentGraphSize = InNewValue;

					UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetMutableDefault<UAvaEaseCurveToolSettings>();
					check(EaseCurveToolSettings);
					EaseCurveToolSettings->SetGraphSize(CurrentGraphSize);
					EaseCurveToolSettings->SaveConfig();
				});
		InMenuBuilder.AddWidget(GraphSizeWidget, LOCTEXT("ToolSizeLabel", "Tool Size"));
	}
	{
		FProperty* const GridSizeProperty = UAvaEaseCurveToolSettings::StaticClass()->FindPropertyByName(TEXT("GridSize"));
		check(GridSizeProperty);

		TOptional<int32> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
		int32 SliderExponent, Delta;
		float ShiftMultiplier = 10.f;
		float CtrlMultiplier = 0.1f;
		bool SupportDynamicSliderMaxValue = false;
		bool SupportDynamicSliderMinValue = false;
		UE::AvaSequencer::ExtractNumericMetadata(GridSizeProperty
			, MinValue, MaxValue
			, SliderMinValue, SliderMaxValue
			, SliderExponent, Delta
			, ShiftMultiplier
			, CtrlMultiplier
			, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

		auto SetEaseCurveToolGridSize = [](const int32 InNewValue)
			{
				UAvaEaseCurveToolSettings* const EaseCurveToolSettings = GetMutableDefault<UAvaEaseCurveToolSettings>();
				check(EaseCurveToolSettings);
				EaseCurveToolSettings->SetGridSize(InNewValue);
				EaseCurveToolSettings->SaveConfig();
			};

		const TSharedRef<SNumericEntryBox<int32>> GridSizeWidget = SNew(SNumericEntryBox<int32>)
			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.AllowSpin(true)
			.MinValue(MinValue)
			.MaxValue(MaxValue)
			.MinSliderValue(SliderMinValue)
			.MaxSliderValue(SliderMaxValue)
			.SliderExponent(SliderExponent)
			.Delta(Delta)
			.ShiftMultiplier(ShiftMultiplier)
			.CtrlMultiplier(CtrlMultiplier)
			.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
			.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
			.Value_Lambda([this]()
				{
					return GetDefault<UAvaEaseCurveToolSettings>()->GetGridSize();
				})
			.OnValueChanged_Lambda(SetEaseCurveToolGridSize)
			.OnValueCommitted_Lambda([SetEaseCurveToolGridSize](const int32 InNewValue, ETextCommit::Type InCommitType)
				{
					SetEaseCurveToolGridSize(InNewValue);
				});
		InMenuBuilder.AddWidget(GridSizeWidget, LOCTEXT("GridSizeLabel", "Grid Size"));
	}
}

void SAvaEaseCurveTool::UndoAction()
{
	if (GEditor)
	{
		GEditor->UndoTransaction();
	}
}

void SAvaEaseCurveTool::RedoAction()
{
	if (GEditor)
	{
		GEditor->RedoTransaction();
	}
}

void SAvaEaseCurveTool::ZoomToFit() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		CurveEaseEditorWidget->ZoomToFit();
	}
}

FKeyHandle SAvaEaseCurveTool::GetSelectedKeyHandle() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		return CurveEaseEditorWidget->GetSelectedKeyHandle();
	}
	return FKeyHandle::Invalid();
}

FReply SAvaEaseCurveTool::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
