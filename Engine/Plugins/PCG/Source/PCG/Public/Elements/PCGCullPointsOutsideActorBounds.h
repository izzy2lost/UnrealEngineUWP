// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGCullPointsOutsideActorBounds.generated.h"

/**
 * Removes points that lie outside the current actor bounds.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCullPointsOutsideActorBoundsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("CullPointsOutsideActorBounds"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
	

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float BoundsExpansion = 0.0;
};

class FPCGCullPointsOutsideActorBoundsElement : public IPCGElement
{
public:
	// Returns true when Context is nullptr which happens in FPCGGraphExecutor::OnTaskInputsReady
	// We can't know for sure at that point if we need to call UPCGComponent::GetActorPCGData() in our GetDependenciesCrc so we need to make sure it happens on MainThread
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return Context == nullptr; }
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override { return EPCGElementExecutionLoopMode::SinglePrimaryPin; }
};
