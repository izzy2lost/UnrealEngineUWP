// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "Elements/Grammar/PCGSlicingBase.h"

#include "Elements/PCGSplitPoints.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGSegmentSlicer.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSegmentSlicerSettings : public UPCGSlicingBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Slicing direction in point local space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSplitAxis SlicingAxis = EPCGSplitAxis::X;

	/** Use an attribute to determine whether we should flip axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bFlipAxisAsAttribute = false;

	/** If we need to flip axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bFlipAxisAsAttribute", EditConditionHides, ShowAfter = "bFlipAxisAsAttribute"))
	bool bShouldFlipAxis = false;

	/** Name of the attribute to know if we need to flip axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bFlipAxisAsAttribute", EditConditionHides, ShowAfter="bFlipAxisAsAttribute"))
	FPCGAttributePropertyInputSelector FlipAxisAttribute;

	/** If the slicing with a given grammar doesn't fill the entire segment, setting it to true makes it a valid case. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bAcceptIncompleteSlicing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputModuleIndexAttribute = false;

	/** Name of the module index output attribute name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, EditCondition = "bOutputModuleIndexAttribute"))
	FName ModuleIndexAttributeName = TEXT("ModuleIndex");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputExtremityAttribute = false;

	/** Name of the Extremity output attribute name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, EditCondition = "bOutputExtremityAttribute"))
	FName ExtremityAttributeName = TEXT("Extremity");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputExtremityNeighborIndexAttribute = false;

	/** Name of the extremity neighbor index output attribute name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|ExtraAttributes", meta = (PCG_Overridable, EditCondition = "bOutputExtremityNeighborIndexAttribute"))
	FName ExtremityNeighborIndexAttributeName = TEXT("ExtremityNeighborIndex");

};

class PCGSegmentSlicerHelpers;

class FPCGSegmentSlicerElement : public FPCGSlicingBaseElement
{
public:
	friend class PCGSegmentSlicerHelpers;
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
