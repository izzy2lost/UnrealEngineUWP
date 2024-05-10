// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "StaticMeshImportNode.generated.h"

class UStaticMesh;

/** Import a static mesh asset into the cloth collection simulation and/or render mesh containers. */
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetStaticMeshImportNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetStaticMeshImportNode, "StaticMeshImport", "Cloth", "Cloth Static Mesh Import")

public:
	UPROPERTY(Meta = (DataflowOutput))
	FManagedArrayCollection Collection;

	/* The Static Mesh to import from. */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import")
	TObjectPtr<const UStaticMesh> StaticMesh;

	/* Which static mesh Lod to import. */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import", Meta = (ClampMin = "0", DisplayName = "LOD Index"))
	int32 LODIndex = 0;

	/* Import static mesh data as a simulation mesh data. */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import")
	bool bImportSimMesh = true;

	/* Import static mesh data as render mesh data. */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import")
	bool bImportRenderMesh = true;

	/**
	 * UV channel of the static mesh to import the 2D simulation mesh patterns from.
	 * If set to -1, or the specified UVChannel doesn't exist then the import will unwrap the 3D simulation mesh into 2D simulation mesh patterns.
	 */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import", meta = (ClampMin = "-1", EditCondition = "bImportSimMesh"))
	int32 UVChannel = 0;

	/* Apply this scale to the UVs when populating Sim Mesh positions. */
	UPROPERTY(EditAnywhere, Category = "Static Mesh Import", Meta = (AllowPreserveRatio, EditCondition = "bImportSimMesh && (UVChannel >= 0)"))
	FVector2f UVScale = { 1.f, 1.f };

	FChaosClothAssetStaticMeshImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};
