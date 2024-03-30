// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GeometryCollectionSkeletalMeshToCollectionNode.generated.h"

class USkeletalMesh;

UENUM()
enum EDF_ConversionMethod : int
{
	Skinned				UMETA(DisplayName = "Skinned"),
	StaticComponents	UMETA(DisplayName = "Static Components"),
};

USTRUCT(meta = (DataflowGeometryCollection))
struct FSkeletalMeshToCollectionDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FSkeletalMeshToCollectionDataflowNode, "SkeletalMeshToCollection", "GeometryCollection", "")
	DATAFLOW_NODE_RENDER_TYPE(FGeometryCollection::StaticType(), "Collection")

public:
	typedef FManagedArrayCollection DataType;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TEnumAsByte<EDF_ConversionMethod> Method = EDF_ConversionMethod::StaticComponents;

	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (DataflowInput, DisplayName = "SkeletalMesh"))
	TObjectPtr<const USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Collection"))
	FManagedArrayCollection Collection;

	FSkeletalMeshToCollectionDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&SkeletalMesh);
		RegisterOutputConnection(&Collection);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;

private:
	void AppendSkeletalMeshComponentsToGeometryCollection(const USkeletalMesh* InSkeletalMesh, FGeometryCollection& OutCollection) const;

};

