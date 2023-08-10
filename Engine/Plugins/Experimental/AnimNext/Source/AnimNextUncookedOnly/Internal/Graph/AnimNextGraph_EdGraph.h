// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraph.h"
#include "AnimNextGraph_EdGraph.generated.h"

class UAnimNextGraph_EditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/**
  * Wraps UEdGraph which represents the node graph
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_EdGraph : public URigVMEdGraph
{
	GENERATED_BODY()

	friend class UAnimNextGraph_EditorData;

	// URigVMEdGraph interface
	virtual FRigVMClient* GetRigVMClient() const override;
	
	void Initialize(UAnimNextGraph_EditorData* InEditorData);
};