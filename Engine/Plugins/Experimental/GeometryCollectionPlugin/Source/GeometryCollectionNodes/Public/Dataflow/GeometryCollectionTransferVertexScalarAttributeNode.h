// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollectionTransferVertexScalarAttributeNode.generated.h"

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

	/** The name of the vertex attribute to generate indices from. */
	UPROPERTY(EditAnywhere, Category = "Dataflow", Meta = (DataflowInput, DataflowOutput, DisplayName = "Name", DataflowPassthrough = "Name"))
	FString Name = FString("");

	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float VertexRadiusRatio = .001f;

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

	TArray<FIntVector2> FindSourceToTargetGeometryMap(const FManagedArrayCollection& SourceCollection, const FManagedArrayCollection& TargetCollection) const;
	void PairedGeometryTransfer(FString AttributName, const TArray<FIntVector2>& PairedGeometry, const FManagedArrayCollection& AttributeCollectionVal, const FManagedArrayCollection& CollectionVal, TManagedArray<float>* FloatArray) const;
	void NearestVertexTransfer(FString AttributName, const FManagedArrayCollection& AttributeCollectionVal, const FManagedArrayCollection& CollectionVal, TManagedArray<float>* FloatArray) const;

};
