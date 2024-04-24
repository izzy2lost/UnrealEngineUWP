// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Chaos/BoundingVolumeHierarchy.h"
#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollectionTransferVertexScalarAttributeNode.generated.h"


namespace UE::Private {

	class FTransferFacade;
	typedef Chaos::TBoundingVolumeHierarchy<TArray<Chaos::TSphere<Chaos::FReal, 3>*>, TArray<int32>, Chaos::FReal, 3> BVH;
};


/** Convert an vertex float array to a list of indices */
USTRUCT(meta = (DataflowGeometryCollection))
struct FGeometryCollectionTransferVertexScalarAttributeNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FGeometryCollectionTransferVertexScalarAttributeNode, "TransferVertexScalarAttribute", "GeometryCollection", "Transfer a named scalar attribute from the Source Collection to the Target Collection")

public:

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DisplayName = "Collection", DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	UPROPERTY(Meta = (DataflowInput, DisplayName = "FromCollection"))
	FManagedArrayCollection FromCollection;

	// The name of the vertex attribute to generate indices from.
	UPROPERTY(EditAnywhere, Category = "Dataflow", Meta = (DataflowInput, DataflowOutput, DisplayName = "Name", DataflowPassthrough = "Name"))
	FString Name = FString("");

	// Threshold based on distance from sample triangle. Anything past the threshold is not sampled. [Defualts to 1 percent (0.01)]
	UPROPERTY(EditAnywhere, Category = "Dataflow|Scales")
	float  FalloffThreshold = 0.01f;

	// Scalar for the Bounding Volume Hierarchy (BVH) target's particle search radius.
	UPROPERTY(EditAnywhere, Category = "Dataflow|Scales")
	float  EdgeMultiplier = 1.0f;


	FGeometryCollectionTransferVertexScalarAttributeNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&Collection);
		RegisterInputConnection(&FromCollection);
		RegisterInputConnection(&Name);

		RegisterOutputConnection(&Collection, &Collection);
		RegisterOutputConnection(&Name, &Name);
	}

private:

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;


	//
	// private helpers
	//
	TArray<FIntVector2> FindSourceToTargetGeometryMap(const FManagedArrayCollection& SourceCollection, const FManagedArrayCollection& TargetCollection) const;
	void PairedGeometryTransfer(FString AttributName, const TArray<FIntVector2>& PairedGeometry,const UE::Private::FTransferFacade& Sample, UE::Private::FTransferFacade& Target, TManagedArray<float>* TargetFloatArray) const;
	void NearestVertexTransfer(FString AttributName, const UE::Private::FTransferFacade& Sample, UE::Private::FTransferFacade& Target, TManagedArray<float>* TargetFloatArray) const;
	static float MaxEdgeLength(TArray<FVector>& Vert, const TManagedArray<FIntVector3>& Tri, int VertexOFfset, int TriStart, int TriCount);
	static void BuildComponentSpaceVertices(const TManagedArray<FTransform3f>* LocalSpaceTransform, const TManagedArray<int32>* Parent, const TManagedArray<int32>* BoneMapArray, const TManagedArray<FVector3f>* VertexArray, int32 Start, int32 Count, TArray<FVector>& ComponentSpaceVertices);
	static UE::Private::BVH* BuildParticleSphereBVH(const TArray<FVector>& Vertices, float Radius);
	static bool TriangleHasWeightsToTransfer(const FIntVector3& T, const TManagedArray<float>& F);
	static void TriangleToVertexIntersections(UE::Private::BVH& VertexBVH, const TArray<FVector>& ComponentSpaceVertices, const FIntVector3& Triangle, TArray<int32>& OutTargetVertexIntersection);


};


