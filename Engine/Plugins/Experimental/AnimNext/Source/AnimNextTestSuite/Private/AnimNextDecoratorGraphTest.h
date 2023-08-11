// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorSharedData.h"
#include "Graph/AnimNextGraph.h"

#include "AnimNextDecoratorGraphTest.generated.h"

UCLASS(BlueprintType)
class UAnimNextGraphTest : public UAnimNextGraph
{
	GENERATED_BODY()

public:
	void SetEditorData(class UAnimNextGraph_EditorData* InEditorData);
};

USTRUCT()
struct FTestDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	int32 SomeInt32 = 3;

	UPROPERTY(meta = (Input))
	float SomeFloat = 34.0f;
};
