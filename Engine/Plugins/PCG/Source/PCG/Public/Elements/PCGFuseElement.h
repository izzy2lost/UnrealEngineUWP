// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "Elements/PCGTimeSlicedElementBase.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/PCGPropertyAccessor.h"

#include "PCGFuseElement.generated.h"

class UPCGPointData;

UENUM(BlueprintType)
enum class EPCGFuseAlgorithm : uint8
{
	Sequential UMETA(Tooltip = "Evaluate the points sequentially in index order."),
	Random UMETA(Tooltip = "Evaluate the points in a random order.")
};

UENUM(BlueprintType)
enum class EPCGFusePriority : uint8
{
	Target UMETA(Tooltip = "Fuse source to target directly."),
	MinAttribute UMETA(Tooltip = "Fuse source or target, depending on which has the lower value for a selected attribute."),
	MaxAttribute UMETA(Tooltip = "Fuse source or target, depending on which has the higher value for a selected attribute."),
	Weighted UMETA(Tooltip = "The source point will be fused between target and source based on a directly selected weight or weight attribute."),
	WeightedAverage UMETA(Tooltip = "The source point will be fused between target and source based on a normalized and weighted attribute on each of the two."),
};

/** Fuse transforms or attributes of two 'co-located' points--based on a minimum distance and other criteria in world space. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGFuseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FuseElement")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGFuseElement", "NodeTitle", "Fuse"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
	virtual FText GetNodeTooltipText() const override;
#endif
	virtual bool UseSeed() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Will be used to determine which points to fuse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0.01", PCG_Overridable))
	double Distance = 100.0;

	/** Dictates the process and order of points as they are considered for fusion. They can differ in their results and efficiency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	EPCGFuseAlgorithm Algorithm = EPCGFuseAlgorithm::Sequential;

	/** Further control of the fusion allows for interpolation by giving 'priority' between the source and target point. Includes attribute-wise comparison or weighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	EPCGFusePriority Priority = EPCGFusePriority::Target;

	/** The source attribute used to determine fusion priority. I.e. The fusion can choose between the point with the minimum or maximum value in compared attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Priority == EPCGFusePriority::MinAttribute || Priority == EPCGFusePriority::MaxAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector SourcePriorityAttribute;

	/** The target attribute used to determine fusion priority. I.e. The fusion can choose between the point with the minimum or maximum value in compared attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Priority == EPCGFusePriority::MinAttribute || Priority == EPCGFusePriority::MaxAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector TargetPriorityAttribute;

	/** Use an attribute on the point directly as the weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Priority == EPCGFusePriority::Weighted || Priority == EPCGFusePriority::WeightedAverage", EditConditionHides, PCG_Overridable))
	bool bWeightAsAttribute = false;

	/** The normalized [0..1] weight when determining the interpolated fusion result between the two points--0.0 equates to the source point and 1.0 equates to the target point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "(Priority == EPCGFusePriority::Weighted || Priority == EPCGFusePriority::WeightedAverage) && !bWeightAsAttribute", EditConditionHides, PCG_Overridable))
	double Weight = 0.5;

	/** This attribute will determine the weight of the fusion result for the source point. It will be normalized to the range of [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "(Priority == EPCGFusePriority::Weighted || Priority == EPCGFusePriority::WeightedAverage) && bWeightAsAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector SourceWeightAttribute;

	/** This attribute will determine the weight of the fusion result for the target point. It will be normalized to the range of [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "Priority == EPCGFusePriority::WeightedAverage", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector TargetWeightAttribute;

	/** Will remove the points to be fused, rather than fusing them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	bool bRemoveFusedPoints = true;

	/** Fuse the two points' location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints", EditConditionHides, PCG_Overridable))
	bool bFuseLocation = true;

	/** Fuse the two points' rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints", EditConditionHides, PCG_Overridable))
	bool bFuseRotation = true;

	/** Fuse the two points' scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints", EditConditionHides, PCG_Overridable))
	bool bFuseScale = true;

	/** Fuse a pair of specified attributes on both the target and source points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints", EditConditionHides, PCG_Overridable))
	bool bFuseAttribute = false;

	/** The source attribute to be compared during the fusion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints && bFuseAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector SourceFuseAttribute;

	/** The target attribute to be compared during the fusion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints && bFuseAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector TargetFuseAttribute;

	/** The output attribute to store the interpolated result of the fusion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "!bRemoveFusedPoints && bFuseAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyOutputSelector OutputFuseAttribute;
};

namespace PCGFuseElement
{
	struct FFuseState
	{
		const UPCGPointData* TargetPointData = nullptr;
		const UPCGPointData* SourcePointData = nullptr;
		UPCGPointData* OutPointData = nullptr;

		int32 IterationIndex = INDEX_NONE;
		bool bTargetIsSource = false;

		TBitArray<TInlineAllocator<512>> CheckedIndices;
		TBitArray<TInlineAllocator<512>> RemovedIndices;

		TUniquePtr<const IPCGAttributeAccessor> SourcePriorityAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> SourcePriorityKeys;
		TUniquePtr<const IPCGAttributeAccessor> TargetPriorityAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> TargetPriorityKeys;

		TUniquePtr<const IPCGAttributeAccessor> SourceFuseAttributeAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> SourceFuseAttributeKeys;
		TUniquePtr<const IPCGAttributeAccessor> TargetFuseAttributeAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> TargetFuseAttributeKeys;

		TUniquePtr<IPCGAttributeAccessor> OutputFuseAttributeAccessor;
		TUniquePtr<IPCGAttributeAccessorKeys> OutputFuseAttributeKeys;

		TArray<double> SourceWeights;
		TArray<double> TargetWeights;

		// For weighted and normalized weighted selections. This stores the maximum weight of the point data to be used in normalization.
		double WeightMax = 1.0;

		FRandomStream RandomStream;
		TArray<int32> RandomIndices;
		// For an iterative approach to fusing.
		bool bPointWasFusedThisCycle = false;
	};

	struct FExecState
	{
		using FuseSignature = bool(*)(const FPCGContext* InContext, FFuseState& FuseData, const UPCGFuseSettings& FuseSettings);
		FuseSignature FuseFunction = nullptr;
	};
}

class FPCGFuseElement : public TPCGTimeSlicedElementBase<PCGFuseElement::FExecState, PCGFuseElement::FFuseState>
{
protected:
	virtual bool PrepareDataInternal(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
