// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/NiagaraDistributionEditorUtilities.h"

#include "NiagaraEditorStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/INiagaraDistributionAdapter.h"

#define LOCTEXT_NAMESPACE "NiagaraDistributionEditorUtilities"

FText FNiagaraDistributionEditorUtilities::DistributionModeToDisplayName(ENiagaraDistributionEditorMode InMode)
{
	switch (InMode)
	{
	case ENiagaraDistributionEditorMode::Constant:
		return LOCTEXT("ConstantDisplayName", "Constant");
	case ENiagaraDistributionEditorMode::UniformConstant:
		return LOCTEXT("UniformContstantDisplayName", "Uniform Constant");
	case ENiagaraDistributionEditorMode::NonUniformConstant:
		return LOCTEXT("NonUniformConstantDisplayName", "Non-uniform Constant");
	case ENiagaraDistributionEditorMode::Range:
		return LOCTEXT("RangeDisplayName", "Range");
	case ENiagaraDistributionEditorMode::UniformRange:
		return LOCTEXT("UniformRangeDisplayName", "Uniform Range");
	case ENiagaraDistributionEditorMode::NonUniformRange:
		return LOCTEXT("NonUniformRangeDisplayName", "Non-uniform Range");
	case ENiagaraDistributionEditorMode::Curve:
		return LOCTEXT("CurveDisplayName", "Curve");
	case ENiagaraDistributionEditorMode::UniformCurve:
		return LOCTEXT("UniformCurveDisplayName", "Uniform Curve");
	case ENiagaraDistributionEditorMode::NonUniformCurve:
		return LOCTEXT("NonUniformCurveDisplayName", "Non-uniform Curve");
	default:
		return LOCTEXT("UnknownDisplayName", "Unknown");
	}
}

FText FNiagaraDistributionEditorUtilities::DistributionModeToToolTipText(ENiagaraDistributionEditorMode InMode)
{
	switch (InMode)
	{
	case ENiagaraDistributionEditorMode::Constant:
		return LOCTEXT("ConstantToolTip", "A constant single value.");
	case ENiagaraDistributionEditorMode::UniformConstant:
		return LOCTEXT("UniformContstantToolTip", "A constant value applied to all value components.");
	case ENiagaraDistributionEditorMode::NonUniformConstant:
		return LOCTEXT("NonUniformConstantToolTip", "Constant values which can be different for each value component.");
	case ENiagaraDistributionEditorMode::Range:
		return LOCTEXT("RangeToolTip", "A single min/max range.");
	case ENiagaraDistributionEditorMode::UniformRange:
		return LOCTEXT("UniformRangeToolTip", "A min/max range applied to all value components.");
	case ENiagaraDistributionEditorMode::NonUniformRange:
		return LOCTEXT("NonUniformRangeToolTip", "Min/max ranges which can be different for each value component.");
	case ENiagaraDistributionEditorMode::Curve:
		return LOCTEXT("CurveToolTip", "This value is driven by a curve.");
	case ENiagaraDistributionEditorMode::UniformCurve:
		return LOCTEXT("UniformCurveToolTip", "All components of this value are driven by the same curve.");
	case ENiagaraDistributionEditorMode::NonUniformCurve:
		return LOCTEXT("NonUniformCurveToolTip", "Each component of this value is driven by its own curve.");
	default:
		return LOCTEXT("UnknownToolTip", "Unknown");
	}
}

FName FNiagaraDistributionEditorUtilities::DistributionModeToIconBrushName(ENiagaraDistributionEditorMode InMode)
{
	switch (InMode)
	{
	case ENiagaraDistributionEditorMode::Constant:
	case ENiagaraDistributionEditorMode::UniformConstant:
		return "NiagaraEditor.DistributionEditor.UniformConstant";
	case ENiagaraDistributionEditorMode::NonUniformConstant:
		return "NiagaraEditor.DistributionEditor.NonUniformConstant";
	case ENiagaraDistributionEditorMode::Range:
	case ENiagaraDistributionEditorMode::UniformRange:
		return "NiagaraEditor.DistributionEditor.UniformRange";
	case ENiagaraDistributionEditorMode::NonUniformRange:
		return "NiagaraEditor.DistributionEditor.NonUniformRange";
	case ENiagaraDistributionEditorMode::Curve:
	case ENiagaraDistributionEditorMode::UniformCurve:
		return "NiagaraEditor.DistributionEditor.UniformCurve";
	case ENiagaraDistributionEditorMode::NonUniformCurve:
		return "NiagaraEditor.DistributionEditor.NonUniformCurve";
	default:
		return NAME_None;
	}
}

const FSlateBrush* FNiagaraDistributionEditorUtilities::DistributionModeToIconBrush(ENiagaraDistributionEditorMode InMode)
{
	return FNiagaraEditorStyle::Get().GetBrush(DistributionModeToIconBrushName(InMode));
}

FSlateIcon FNiagaraDistributionEditorUtilities::DistributionModeToIcon(ENiagaraDistributionEditorMode InMode)
{
	return FSlateIcon(FNiagaraEditorStyle::Get().GetStyleSetName(), DistributionModeToIconBrushName(InMode));
}

bool FNiagaraDistributionEditorUtilities::IsUniform(ENiagaraDistributionEditorMode InMode)
{
	return
		InMode == ENiagaraDistributionEditorMode::Constant ||
		InMode == ENiagaraDistributionEditorMode::UniformConstant ||
		InMode == ENiagaraDistributionEditorMode::Range ||
		InMode == ENiagaraDistributionEditorMode::UniformRange ||
		InMode == ENiagaraDistributionEditorMode::Curve ||
		InMode == ENiagaraDistributionEditorMode::UniformCurve;
}

bool FNiagaraDistributionEditorUtilities::IsConstant(ENiagaraDistributionEditorMode InMode)
{
	return
		InMode == ENiagaraDistributionEditorMode::Constant ||
		InMode == ENiagaraDistributionEditorMode::UniformConstant ||
		InMode == ENiagaraDistributionEditorMode::NonUniformConstant;
}

bool FNiagaraDistributionEditorUtilities::IsRange(ENiagaraDistributionEditorMode InMode)
{
	return
		InMode == ENiagaraDistributionEditorMode::Range ||
		InMode == ENiagaraDistributionEditorMode::UniformRange ||
		InMode == ENiagaraDistributionEditorMode::NonUniformRange;
}

bool FNiagaraDistributionEditorUtilities::IsCurve(ENiagaraDistributionEditorMode InMode)
{
	return
		InMode == ENiagaraDistributionEditorMode::Curve ||
		InMode == ENiagaraDistributionEditorMode::UniformCurve ||
		InMode == ENiagaraDistributionEditorMode::NonUniformCurve;
}

#undef LOCTEXT_NAMESPACE