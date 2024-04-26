// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Dataflow/DataflowCollectionAttributeKeyNodes.h"

#include "DataflowCollectionAddScalarVertexPropertyNode.generated.h"

/** Scalar vertex properties. */
USTRUCT(Meta = (DataflowCollection))
struct DATAFLOWNODES_API FDataflowCollectionAddScalarVertexPropertyNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FDataflowCollectionAddScalarVertexPropertyNode, "AddScalarVertexProperty", "Collection", "Add a saved scalar property to a collection")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")


public:

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Vertex Attribute")
	FString Name;

	UPROPERTY(meta = (DisplayName = "AttributeKey", DataflowOutput))
	FCollectionAttributeKey AttributeKey;

	UPROPERTY()
	TArray<float> VertexWeights;

	FDataflowCollectionAddScalarVertexPropertyNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
