// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "BoneContainer.h"
#include "PoseSearchFeatureChannel_Position.generated.h"

UCLASS(EditInlineNew, meta = (DisplayName = "Position Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_Position : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference OriginBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	// the data relative to the sampling time associated to this channel will be offsetted by SampleTimeOffset seconds.
	// For example, if Bone is the head bone, and SampleTimeOffset is 0.5, this channel will try to match the future position of the character head bone 0.5 seconds ahead
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTimeOffset = 0.f;

	// index referencing the associated bone in UPoseSearchSchema::BoneReferences
	UPROPERTY(Transient)
	int8 SchemaBoneIdx = 0;

	UPROPERTY(Transient)
	int8 SchemaOriginBoneIdx = 0;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ExcludeFromHash, DisplayPriority = 0))
	FLinearColor DebugColor = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EComponentStrippingVector ComponentStripping = EComponentStrippingVector::None;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime;

	// UPoseSearchFeatureChannel interface
	virtual void Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const override;

	virtual EPermutationTimeType GetPermutationTimeType() const override { return PermutationTimeType; }
	virtual void AddDependentChannels(UPoseSearchSchema* Schema) const override;

#if ENABLE_DRAW_DEBUG
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual FString GetLabel() const override;
#endif

	static void FindOrAddToSchema(UPoseSearchSchema* Schema, float SampleTimeOffset, const FName& BoneName = NAME_None, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
};
