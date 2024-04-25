// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCollectionAttributeKeyNodes.h"
#include "GeometryCollection/GeometryCollection.h"

#include "Dataflow/DataflowCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowCollectionAttributeKeyNodes)

namespace Dataflow
{
	void DataflowCollectionAttributeKeyNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMakeCollectionKeyDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBreakCollectionKeyDataflowNode);
	}
}


void FMakeCollectionKeyDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FString GroupName = GetValue<FString>(Context, &GroupIn);
	FString AttributeName = GetValue<FString>(Context, &AttributeIn);
	SetValue(Context, FCollectionKey(GroupName, AttributeName), &CollectionKeyOut);
}

void FBreakCollectionKeyDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FCollectionKey CollectionKey = GetValue<FCollectionKey>(Context, &CollectionKeyIn);
	SetValue(Context, CollectionKey.Group, &GroupOut);
	SetValue(Context, CollectionKey.Attribute, &AttributeOut);
}