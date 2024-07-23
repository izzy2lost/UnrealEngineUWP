// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/AddWeightMapNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/ConnectableValue.h"
#include "WeightMapNode.generated.h"

/** Which mesh to update with the node's weight map */
UENUM()
enum class EChaosClothAssetWeightMapMeshTarget : uint8
{
	Simulation,
	Render
};

/** Painted weight map attributes node. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetWeightMapNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetWeightMapNode, "WeightMap", "Cloth", "Cloth Weight Map")
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:
	static constexpr float ReplaceChangedPassthroughValue = UE_BIG_NUMBER;

	UPROPERTY(Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "Collection"))
	FManagedArrayCollection Collection;

	/**
	 * The collection used to transfer weight map from.
	 * Connecting a collection containing a weight map with Input Name (or Name if Input Name is empty)
	 * will transfer the weights to the input collection vertices.
	 * Note this operation only happens once when the TransferCollection is first connected, or updated.
	 * Changing the InputName or the TransferType will also redo the transfer operation.
	 */
	UPROPERTY(Meta = (DataflowInput))
	FManagedArrayCollection TransferCollection;

	/** The name to be set as a weight map attribute. */
	UPROPERTY(EditAnywhere, Category = "Weight Map", Meta = (DataflowOutput))
	FString Name;

	/** The name to populate this map from and override based on Map Override Type. Name will be used if Input Name is empty.*/
	UPROPERTY(EditAnywhere, Category = "Weight Map")
	FChaosClothAssetConnectableIStringValue InputName = { TEXT("") };

	/** How to apply this node's weight values onto existing maps. Changing this value will change the output map.
	 *  To change how the node's stored weights are calculated, change the equivalent value on the Weight Map Paint Tool context.*/
	UPROPERTY(EditAnywhere, Category = "Weight Map")
	EChaosClothAssetWeightMapOverrideType MapOverrideType = EChaosClothAssetWeightMapOverrideType::ReplaceChanged;

	/**
	 * The type of transfer used to transfer the weight map when a TransferCollection is connected.
	 * This property is disabled when no TransferCollection input has been conencted.
	 */
	UPROPERTY(EditAnywhere, Category = "Weight Map", Meta = (EditCondition = "TransferCollectionHash != 0"))
	EChaosClothAssetWeightMapTransferType TransferType = EChaosClothAssetWeightMapTransferType::Use2DSimMesh;

	UPROPERTY(EditAnywhere, Category = "Weight Map")
	EChaosClothAssetWeightMapMeshTarget MeshTarget = EChaosClothAssetWeightMapMeshTarget::Simulation;

	FChaosClothAssetWeightMapNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	friend class UClothEditorWeightMapPaintTool;

	class FWeightMapNodeChange;
	static TUniquePtr<class FToolCommandChange> CHAOSCLOTHASSETDATAFLOWNODES_API MakeWeightMapNodeChange(const FChaosClothAssetWeightMapNode& Node);

	const TArray<float>& GetVertexWeights() const { return VertexWeights; }
	TArray<float>& GetVertexWeights() { return VertexWeights; }

	UPROPERTY()
	TArray<float> VertexWeights;

	// These methods are exported for UClothEditorWeightMapPaintTool which lives in a different module.
	FName CHAOSCLOTHASSETDATAFLOWNODES_API GetInputName(Dataflow::FContext& Context) const;

	void CHAOSCLOTHASSETDATAFLOWNODES_API SetVertexWeights(const TConstArrayView<float> InputMap, const TArray<float>& FinalValues);

	// Input and FinalOutputMap can be the same array, but should not otherwise be interleaved.
	void CHAOSCLOTHASSETDATAFLOWNODES_API CalculateFinalVertexWeightValues(const TConstArrayView<float> InputMap, TArrayView<float> FinalOutputMap) const;

	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
	//~ End FDataflowNode interface

	UPROPERTY()
	uint32 TransferCollectionHash = 0;
};
