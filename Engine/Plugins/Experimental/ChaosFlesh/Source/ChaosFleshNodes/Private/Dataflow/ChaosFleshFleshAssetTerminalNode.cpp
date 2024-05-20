// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshFleshAssetTerminalNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshFleshAssetTerminalNode)


void FFleshAssetTerminalDataflowNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	if (UFleshAsset* InFleshAsset = Cast<UFleshAsset>(Asset.Get()))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		InFleshAsset->SetCollection(InCollection.NewCopy<FFleshCollection>());
	}
}

void FFleshAssetTerminalDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	SetValue(Context, InCollection, &Collection);
}
