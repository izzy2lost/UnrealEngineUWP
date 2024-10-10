// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowConnectionTypes.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Dataflow/DataflowSelection.h"

#include "GeometryCollectionTransferVertexAttributeNode.generated.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
namespace Dataflow = UE::Dataflow;
#else
namespace UE_DEPRECATED(5.5, "Use UE::Dataflow instead.") Dataflow {}
#endif

namespace UE::Private 
{
	class FTransferFacade;
	struct FTransferData;
};


UENUM(BlueprintType)
enum class EDataflowTransferVertexAttributeNodeFalloff : uint8
{
	/** Squared falloff based on distance from triangle*/
	Squared  UMETA(DisplayName = "Squared"),

	/** Linear falloff based on distance from triangle*/
	Linear UMETA(DisplayName = "Linear"),

	/** No distance falloff */
	None UMETA(DisplayName = "None"),
	//~~~
	//256th entry
	Dataflow_Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDataflowTransferVertexAttributeNodeSampleScale : uint8
{
	/** Bounding volume hierarchy cell size based on max edge length of each geometry group*/
	Component_Edge  UMETA(DisplayName = "Component Max Edge"),

	/** Bounding volume hierarchy cell size based on max edge length of the whole asset*/
	Asset_Edge UMETA(DisplayName = "Asset Max Edge"),

	/** Bounding volume hierarchy cell size based on max length of the bounding box of the whole asset*/
	Asset_Bound UMETA(DisplayName = "Asset Max Bound"),

	//~~~
	//256th entry
	Dataflow_Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDataflowTransferVertexAttributeNodeBoundingVolume : uint8
{
	/** Bounding volume on vertices of the source triangle mesh*/
	Vertex UMETA(DisplayName = "Vertex"),

	/** Bounding volume on triangles of the source triangle mesh*/
	Triangle UMETA(DisplayName = "Triangle"),

	//~~~
	//256th entry
	Dataflow_Max UMETA(Hidden)
};

/**
 * Transfer float properties from a sample collection to a target collection.
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGeometryCollectionTransferVertexAttributeNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGeometryCollectionTransferVertexAttributeNode, "TransferVertexAttribute", "GeometryCollection", "Transfer a named vertex attribute from the Source Collection to the Target Collection")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceWeightsRender", FGeometryCollection::StaticType(), "Collection", "AttributeKey")

public:

	/* Target collection to transfer vertex attribute to. */
	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/* Sample collection to transfer vertex attribute from. */
	UPROPERTY(Meta = (DataflowInput, DisplayName = "FromCollection"))
	FManagedArrayCollection FromCollection;

	/* The name of the vertex attribute to generate indices from. */
	UPROPERTY(EditAnywhere, Category = "Dataflow", Meta = (DataflowInput, DataflowOutput, DisplayName = "AttributeKey", DataflowPassthrough = "AttributeKey",
		EditCondition = "!bTransferSkinWeights", EditConditionHides))
	FCollectionAttributeKey AttributeKey = FCollectionAttributeKey(FString(""), FString("Vertices"));

	/* If kinematic skin weight attribute is transferred. */
	UPROPERTY(EditAnywhere, Category = "Dataflow", Meta = (DisplayName = "TransferSkinWeights"))
	bool bTransferSkinWeights = false;
	
	/* Bounding volume type for source assets[default: Triangle] */
	UPROPERTY(EditAnywhere, Category = "Thresholds")
	EDataflowTransferVertexAttributeNodeBoundingVolume BoundingVolumeType = EDataflowTransferVertexAttributeNodeBoundingVolume::Triangle;

	/* Bounding volume hierarchy cell size for neighboring vertices to transfer into[default: Asset] */
	UPROPERTY(EditAnywhere, Category = "Thresholds")
	EDataflowTransferVertexAttributeNodeSampleScale SampleScale = EDataflowTransferVertexAttributeNodeSampleScale::Asset_Bound;

	/* Falloff of sample value based on distance from sample triangle[default: Squared] */
	UPROPERTY(EditAnywhere, Category = "Thresholds")
	EDataflowTransferVertexAttributeNodeFalloff Falloff = EDataflowTransferVertexAttributeNodeFalloff::None;

	/* Threshold based on distance from sample triangle.Values sampled past the threshold will falloff.[Defaults to 1 percent of triangle size(0.01)] */
	UPROPERTY(EditAnywhere, Category = "Thresholds")
	float FalloffThreshold = 0.01f;

	/* Edge multiplier for the Bounding Volume Hierarchy(BVH) target's particle search radius. */
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (EditCondition = "SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Edge || SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Component_Edge", EditConditionHides))
	float EdgeMultiplier = 0.5f;

	/* Max bound multiplier for the Bounding Volume Hierarchy(BVH) target's particle search radius. */
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (
		EditCondition = "SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Bound",
		EditConditionHides))
	float BoundMultiplier = 0.01f;

	FGeometryCollectionTransferVertexAttributeNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&FromCollection);
		RegisterInputConnection(&AttributeKey);

		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&AttributeKey, &AttributeKey);
	}

private:

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

	typedef Chaos::TSphere<Chaos::FReal, 3> SphereType;
	typedef Chaos::TBoundingVolumeHierarchy<TArray<SphereType*>, TArray<int32>, Chaos::FReal, 3> BVH;

	//
	// private helpers
	//
	TArray<FIntVector2> FindSourceToTargetGeometryMap(const FManagedArrayCollection& SourceCollection, const FManagedArrayCollection& TargetCollection) const;
	TMap<int32, int32> FindSourceToTargetTransformMap(const FManagedArrayCollection& SourceCollection, const FManagedArrayCollection& TargetCollection) const;

	TArray<UE::Private::FTransferData> PairedGeometryTransfer(FCollectionAttributeKey Key, const TArray<FIntVector2>& PairedGeometry,
		const UE::Private::FTransferFacade& Sample, const UE::Private::FTransferFacade& Target) const;
	TArray<UE::Private::FTransferData> NearestVertexTransfer(FCollectionAttributeKey Key, const UE::Private::FTransferFacade& Sample, const UE::Private::FTransferFacade& Target) const;
	static float MaxEdgeLength(TArray<FVector3f>& Vert, const TManagedArray<FIntVector3>& Tri, int VertexOFfset, int TriStart, int TriCount);
	static void BuildComponentSpaceVertices(const TManagedArray<FTransform3f>& LocalSpaceTransform, const TManagedArray<int32>& Parent, const TManagedArray<int32>& BoneMapArray, const TManagedArray<FVector3f>& VertexArray, int32 Start, int32 Count, TArray<FVector3f>& ComponentSpaceVertices);
	static BVH* BuildParticleSphereBVH(const TArray<FVector3f>& Vertices, float Radius);
	static void TriangleToVertexIntersections(const BVH& VertexBVH, const TArray<FVector3f>& ComponentSpaceVertices, const FIntVector3& Triangle, TArray<int32>& OutTargetVertexIntersection);
	static float CalculateFalloffScale(EDataflowTransferVertexAttributeNodeFalloff FalloffSetting, float Threshold, float Distance);

};

/**
 * Set VertexSelection to be kinematic. Note that kinematic particles need skin weights.
 */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGeometryCollectionSetKinematicVertexSelectionNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGeometryCollectionSetKinematicVertexSelectionNode, "SetKinematicVertexSelection", "GeometryCollection", "Set Vertex Collection to be kinematic")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FGeometryCollection::StaticType(), "Collection")

public:

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** Vertex Selection set to be kinematic */
	UPROPERTY(meta = (DataflowInput, DisplayName = "VertexSelection"))
	FDataflowVertexSelection VertexSelection;

	FGeometryCollectionSetKinematicVertexSelectionNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&VertexSelection);
		RegisterOutputConnection(&Collection, &Collection);
	}

private:

	virtual void Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};