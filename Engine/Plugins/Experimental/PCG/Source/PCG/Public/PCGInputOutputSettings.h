// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGInputOutputSettings.generated.h"

namespace PCGInputOutputConstants
{
	const FName DefaultInputLabel = TEXT("Input");
	const FName DefaultActorLabel = TEXT("Actor");
	const FName DefaultOriginalActorLabel = TEXT("OriginalActor");
	const FName DefaultLandscapeLabel = TEXT("Landscape");
	const FName DefaultLandscapeHeightLabel = TEXT("Landscape Height");
	const FName DefaultNewCustomPinName = TEXT("NewPin");
}

class PCG_API FPCGInputOutputElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

UCLASS(NotBlueprintable, Hidden, ClassGroup = (Procedural))
class PCG_API UPCGGraphInputOutputSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return bIsInput ? FName(TEXT("InputNode")) : FName(TEXT("OutputNode")); }
	virtual FText GetDefaultNodeTitle() const override { return bIsInput ? NSLOCTEXT("PCGGraphInputOutputSettings", "InputNodeTitle", "Input Node") : NSLOCTEXT("PCGGraphInputOutputSettings", "OutputNodeTitle", "Output Node"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::InputOutput; }
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
	// The output node does not emit a task (input node does however). This prevents it displaying as culled when debugging.
	virtual bool EmitsTaskForExecution() const override { return bIsInput; }
#endif

	TArray<FPCGPinProperties> InputPinProperties() const override;
	TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual TArray<FPCGPinProperties> DefaultInputPinProperties() const override;
	virtual TArray<FPCGPinProperties> DefaultOutputPinProperties() const override;

	void SetInput(bool bInIsInput);

	// Add a new custom pin
	// Note that you should use the return value of this function, since it can be different from
	// the one passed as argument. It will change if its label collides with existing pins.
	[[nodiscard]] const FPCGPinProperties& AddPin(const FPCGPinProperties& NewCustomPinProperties);

protected:
	virtual FPCGElementPtr CreateElement() const override { return MakeShared<FPCGInputOutputElement>(); }
	// ~End UPCGSettings interface

	void FixPinProperties();

protected:
	UPROPERTY()
	TSet<FName> PinLabels_DEPRECATED;

	UPROPERTY()
	TArray<FPCGPinProperties> CustomPins_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGPinProperties> Pins;

	UPROPERTY()
	bool bHasAddedDefaultPin = false;

	bool bIsInput = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
