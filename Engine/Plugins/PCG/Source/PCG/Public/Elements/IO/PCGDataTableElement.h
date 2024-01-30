// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGExternalData.h"

#include "PCGCommon.h"

class UDataTable;

#include "PCGDataTableElement.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGLoadDataTableSettings : public UPCGExternalDataSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("LoadDataTable")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual bool HasDynamicPins() const override { return true; }
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual bool CanDynamicalyTrackKeys() const override { return true; }
#endif

	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UDataTable> DataTable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ValidEnumValues = "Point, Param"))
	EPCGExclusiveDataType OutputType = EPCGExclusiveDataType::Point;
};

class FPCGLoadDataTableElement : public FPCGExternalDataElement
{
protected:
	virtual bool PrepareLoad(FPCGExternalDataContext* Context) const override;
	virtual bool ExecuteLoad(FPCGExternalDataContext* Context) const override;
};