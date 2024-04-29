// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGElevationIsolines.generated.h"

/**
* Compute the elevation isolines of a surface, can output either points or splines.
* Currently only work for Z-up surfaces.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGElevationIsolinesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Minimum elevation of the isolines. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta = (PCG_Overridable))
	double ElevationStart = 0.0;

	/** Maximum elevation of the isolines. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double ElevationEnd = 1000.0;

	/** Increment elevation between each isolines. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double ElevationIncrement = 100.0;

	/** Resolution of the grid for the discretization of the surface. This is the size of one cell, in cm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Resolution = 100.0;

	/** Will output splines rather than points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOutputAsSpline = false;
};

class FPCGElevationIsolinesElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

