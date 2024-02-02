// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Styling/SlateColor.h"

struct FRichCurve;

enum class ENiagaraDistributionEditorMode
{
	Constant,
	UniformConstant,
	NonUniformConstant,
	Range,
	UniformRange,
	NonUniformRange,
	Curve,
	UniformCurve,
	NonUniformCurve
};

class INiagaraDistributionAdapter
{
public:
	virtual ~INiagaraDistributionAdapter() { }

	virtual bool IsValid() const = 0;

	virtual int32 GetNumChannels() const = 0;
	virtual FText GetChannelDisplayName(int32 ChannelIndex) const = 0;
	virtual FSlateColor GetChannelColor(int32 ChannelIndex) const = 0;

	virtual void GetSupportedDistributionModes(TArray<ENiagaraDistributionEditorMode>& OutSupportedModes) const = 0;

	virtual ENiagaraDistributionEditorMode GetDistributionMode() const = 0;
	virtual void SetDistributionMode(ENiagaraDistributionEditorMode InMode) = 0;

	virtual float GetConstantOrRangeValue(int32 ChannelIndex, int32 ValueIndex) const = 0;
	virtual void SetConstantOrRangeValue(int32 ChannelIndex, int32 ValueIndex, float InValue) = 0;

	virtual FRichCurve* GetCurveValue(int32 ChannelIndex) const = 0;
	virtual void SetCurveValue(int32 ChannelIndex, const FRichCurve& InValue) = 0;

	virtual void BeginContinuousChange() = 0;
	virtual void EndContinuousChange() = 0;
};