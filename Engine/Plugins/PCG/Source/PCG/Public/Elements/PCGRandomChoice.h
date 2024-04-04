// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGRandomChoice.generated.h"

namespace PCGRandomChoiceConstants
{
	const FName ChosenPointsLabel = TEXT("Chosen");
	const FName DiscardedPointsLabel = TEXT("Discarded");
}

/**
* Chooses points randomly through ratio or a fixed number of points.
* Chosen/Discarded Points will be in the same order than they appear in the input data.
*/
UCLASS(BlueprintType)
class UPCGRandomChoiceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGRandomChoiceSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

	virtual bool HasDynamicPins() const override { return true; }
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface


public:
	// Either choose a fixed number of points, or a ratio of points.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFixedMode = true;

	// Defines the number of points to keep.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bFixedMode", ClampMin = "0"))
	int FixedNumber = 1;

	// Defines the ratio of points to keep.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bFixedMode", ClampMin = "0", ClampMax = "1"))
	float Ratio = 0.5;

	// By default, we output discarded points. If you don't need them, disable this option.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOutputDiscardedPoints = true;
};

class FPCGRandomChoiceElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
