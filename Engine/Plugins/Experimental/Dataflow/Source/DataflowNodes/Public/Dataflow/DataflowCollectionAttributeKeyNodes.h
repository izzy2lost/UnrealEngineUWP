// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"

#include "DataflowCollectionAttributeKeyNodes.generated.h"

class USkeletalMesh;


USTRUCT(BlueprintType)
struct DATAFLOWNODES_API FCollectionKey
{
	GENERATED_USTRUCT_BODY()
public:
	FCollectionKey() : Group(""), Attribute("") {}
	FCollectionKey(FString InGroup, FString InAttribute)
		:Group(InGroup), Attribute(InAttribute) {}
	TPair<FName, FName> GetNamedKey() { return { FName(Group), FName(Attribute)}; }
	FString Group;
	FString Attribute;
};


USTRUCT(meta = (Dataflow))
struct FMakeCollectionKeyDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeCollectionKeyDataflowNode, "MakeCollectionKey", "GeometryCollection", "")

public:

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowInput, DisplayName = "Group"))
	FString GroupIn = "";

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowInput, DisplayName = "Attribute"))
	FString AttributeIn = "";

	UPROPERTY(meta = (DataflowOutput, DisplayName = "CollectionKey"))
	FCollectionKey CollectionKeyOut;

	FMakeCollectionKeyDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&GroupIn);
		RegisterInputConnection(&AttributeIn);
		RegisterOutputConnection(&CollectionKeyOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowGeometryCollection))
struct FBreakCollectionKeyDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBreakCollectionKeyDataflowNode, "BreakCollectionKey", "GeometryCollection", "")

public:

	UPROPERTY(meta = (DataflowInput, DisplayName = "CollectionKey"))
	FCollectionKey CollectionKeyIn;

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowOutput, DisplayName = "Group"))
	FString GroupOut = "";

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowOutput, DisplayName = "Attribute"))
	FString AttributeOut = "";


	FBreakCollectionKeyDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&CollectionKeyIn);
		RegisterOutputConnection(&GroupOut);
		RegisterOutputConnection(&AttributeOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

namespace Dataflow
{
	void DataflowCollectionAttributeKeyNodes();
}

