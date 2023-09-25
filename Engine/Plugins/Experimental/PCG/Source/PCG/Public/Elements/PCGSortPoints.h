// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGSortPoints.generated.h"

/**
 * Sorts points based on an attribute.
 */

UENUM()
enum class EPCGSortMethod : uint8
{
	Ascending,
	Descending
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGSortPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SortPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSortPointsElement", "NodeTitle", "Sort Points"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGSortPointsElement", "NodeTooltip", "Sorts points based on an attribute."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGSortMethod SortMethod = EPCGSortMethod::Ascending;
};

class FPCGSortPointsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};