// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowEngine.h"

#include "DataflowCollectionAttributeKeyNodes.generated.h"

class USkeletalMesh;


USTRUCT(BlueprintType)
struct DATAFLOWNODES_API FCollectionAttributeKey
{
	GENERATED_USTRUCT_BODY()
public:
	FCollectionAttributeKey() : Attribute(""), Group("") {}
	FCollectionAttributeKey(FString InAttribute, FString InGroup)
		: Attribute(InAttribute), Group(InGroup) {}
	TPair<FName, FName> GetNamedKey() { return { FName(Attribute), FName(Group)}; }

	UPROPERTY(EditAnywhere, Category = "Collection Key")
	FString Attribute;

	UPROPERTY(EditAnywhere, Category = "Collection Key")
	FString Group;

};


USTRUCT(meta = (Dataflow))
struct FMakeAttributeKeyDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FMakeAttributeKeyDataflowNode, "MakeAttributeKey", "GeometryCollection", "")

public:

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowInput, DisplayName = "Group"))
	FString GroupIn = "";

	UPROPERTY(EditAnywhere, Category = "Collection Key", meta = (DataflowInput, DisplayName = "Attribute"))
	FString AttributeIn = "";

	UPROPERTY(meta = (DataflowOutput, DisplayName = "AttributeKey"))
	FCollectionAttributeKey AttributeKeyOut;

	FMakeAttributeKeyDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&GroupIn);
		RegisterInputConnection(&AttributeIn);
		RegisterOutputConnection(&AttributeKeyOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

USTRUCT(meta = (DataflowGeometryCollection))
struct FBreakAttributeKeyDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FBreakAttributeKeyDataflowNode, "BreakAttributeKey", "Dataflow", "")

public:

	UPROPERTY(meta = (DataflowInput, DisplayName = "AttributeKey"))
	FCollectionAttributeKey AttributeKeyIn;

	UPROPERTY(EditAnywhere, Category = "Attribute Key", meta = (DataflowOutput, DisplayName = "Attribute"))
	FString AttributeOut = "";

	UPROPERTY(EditAnywhere, Category = "Attribute Key", meta = (DataflowOutput, DisplayName = "Group"))
	FString GroupOut = "";



	FBreakAttributeKeyDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterInputConnection(&AttributeKeyIn);
		RegisterOutputConnection(&AttributeOut);
		RegisterOutputConnection(&GroupOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};

namespace Dataflow
{
	void DataflowCollectionAttributeKeyNodes();
}

