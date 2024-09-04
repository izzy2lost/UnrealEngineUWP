// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeReference.h"
#include "Chimera/ChimeraLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChimeraAnimNodeLibrary.generated.h"

struct FAnimNode_Chimera;

USTRUCT(BlueprintType)
struct FChimeraAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_Chimera FInternalNodeType;
};

UCLASS()
class CHIMERA_API UChimeraAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Animation|Chimera", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FChimeraAnimNodeReference ConvertToChimeraNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	UFUNCTION(BlueprintPure, Category = "Animation|Chimera", meta = (BlueprintThreadSafe, DisplayName = "Convert to Chimera Node"))
	static void ConvertToChimeraNodePure(const FAnimNodeReference& Node, FChimeraAnimNodeReference& ChimeraNode, bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		ChimeraNode = ConvertToChimeraNode(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	UFUNCTION(BlueprintCallable, Category = "Animation|Chimera", meta = (BlueprintThreadSafe))
	static void SetAvailabilities(const FChimeraAnimNodeReference& ChimeraNode, const TArray<FChimeraAvailability>& Availabilities);
};