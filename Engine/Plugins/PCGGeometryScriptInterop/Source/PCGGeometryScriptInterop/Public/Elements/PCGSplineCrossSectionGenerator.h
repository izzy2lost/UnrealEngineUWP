// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGSplineCrossSectionGenerator.generated.h"

/**
 * Creates a spline cross-section of one more primitives based on vertex features, slice resolution, or both.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGSplineCrossSectionGeneratorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGSplineCrossSectionGeneratorSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SplineCrossSectionGenerator")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSplineCrossSectionGeneratorElement", "NodeTitle", "Spline Cross Section Generator"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGSplineCrossSectionGeneratorElement", "NodeTooltip", "Creates a spline cross-section of one more primitives based on vertex features, slice resolution, or both."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Slicing will happen from the minimum vertex along this direction vector (normalized). Useful primarily with the Tier Slicing Resolution. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector SliceDirection = FVector::UpVector;

	/** The attribute that will be populated with each cross-section's extrusion vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyOutputSelector ExtrusionVectorAttribute;

	/** Find primitive tier "features", which consist of a number of co-planar vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEnableFindFeatures = true;

	/** The minimum required number of vertices that must be co-planar in order to be considered a tier "feature". */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bEnableFindFeatures", EditConditionHides))
	int32 MinimumCoplanarVertices = 3;

	/** Create tiers at a specified resolution. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bEnableTierSlicing = false;

	/** A tier will be created at this interval (in cm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bEnableTierSlicing", ClampMin = "100"))
	double TierSlicingResolution = 500.0;

	/** Cull tiers that are within a specified threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bEnableTierMerging = false;

	/** If a tier is within this distance (in cm) of the previous tier, it will be culled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bEnableTierMerging"))
	double TierMergingThreshold = 100.0;

	/** Cull tiers that have a surface area smaller than a specified threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bEnableMinAreaCulling = false;

	/** If a tier is smaller in area than this threshold, it will be culled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bEnableMinAreaCulling"))
	double MinAreaCullingThreshold = 100.0;

	/** If multiple tiers can be combined into a single tier without affecting the contour, remove the redundant one. Note: This will currently cull even if there are other unique tiers in between. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bRemoveRedundantSections = true;

	/** A safeguard to prevent finding features on an overly complex mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced", meta = (PCG_Overridable, EditCondition = "bEnableFindFeatures", EditConditionHides))
	int32 MaxMeshVertexCount = 2048;
};

class FPCGSplineCrossSectionGeneratorElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
