// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "SelectionNode.generated.h"

/** What type of element is selected in the Selection */
UENUM()
enum class UE_DEPRECATED(5.4, "Use FChaosClothAssetNodeSelectionGroup instead") EChaosClothAssetSelectionType : uint8
{
	/** 2D simulation vertices */
	SimVertex2D,

	/** 3D simulation vertices */
	SimVertex3D,

	/** Render vertices */
	RenderVertex,

	/** Simulation faces (2D/3D are the same) */
	SimFace,

	/** Render faces */
	RenderFace,

	/** Deprecated marker */
	Deprecated UMETA(Hidden)
};

/**
 * The managed array collection group used in the selection.
 * This separate structure is required to allow for customization of the UI.
 */
USTRUCT()
struct FChaosClothAssetNodeSelectionGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Selection Group")
	FString Name;
};

/** Integer index set selection node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSelectionNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSelectionNode, "Selection", "Cloth", "Cloth Selection")

public:

	UPROPERTY(Meta = (Dataflowinput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/** The name to give the selection attribute */
	UPROPERTY(EditAnywhere, Category = "Selection", Meta = (DataflowOutput))
	FString Name;

	/** The type of element the selection refers to */
	UE_DEPRECATED(5.4, "Use Group instead")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	EChaosClothAssetSelectionType Type_DEPRECATED = EChaosClothAssetSelectionType::Deprecated;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** The type of element the selection refers to */
	UPROPERTY(EditAnywhere, Category = "Selection")
	FChaosClothAssetNodeSelectionGroup Group;

	/** Selected element indices */
	UPROPERTY(EditAnywhere, Category = "Selection", Meta = (ClampMin = "0"))
	TSet<int32> Indices;

	FChaosClothAssetSelectionNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	/** Return a cached array of all the groups used by the input collection during at the time of the latest evaluation. */
	const TArray<FName>& GetCachedCollectionGroupNames() const { return CachedCollectionGroupNames; }

private:

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	virtual void OnSelected(Dataflow::FContext& Context) override;
	virtual void OnDeselected() override;
	virtual void Serialize(FArchive& Ar);

	TArray<FName> CachedCollectionGroupNames;
};

