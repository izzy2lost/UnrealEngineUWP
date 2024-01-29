// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "Widgets/SCompoundWidget.h"

class UDMMaterialComponent;
class UDMMaterialValue;

class SDMMaterialValuePreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMMaterialValuePreview)
		: _DesiredSize(FVector2D(48.f, 48.f))
		{}
		SLATE_ARGUMENT(FVector2D, DesiredSize)
	SLATE_END_ARGS()

public:
	SDMMaterialValuePreview();
	virtual ~SDMMaterialValuePreview();

	void Construct(const FArguments& InArgs, UDMMaterialValue* InValue);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	TWeakObjectPtr<UDMMaterialValue> ValueWeak;
	FSlateMaterialBrush Brush;

	void OnValueUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
};
