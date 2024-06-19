// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "UniversalObjectLocator.h"
#include "StructUtils/InstancedStruct.h"
#include "AnimNextEditorParam.generated.h"

struct FAnimNextParam;
struct FAnimNextParamInstanceIdentifier;

// TODO: Move this to UncookedOnly module once schedules are refactored - this should be an editor-only type

USTRUCT()
struct FAnimNextEditorParam
{
	GENERATED_BODY()

	FAnimNextEditorParam() = default;

	ANIMNEXT_API FAnimNextEditorParam(FName InName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId);
#if WITH_EDITORONLY_DATA
	ANIMNEXT_API explicit FAnimNextEditorParam(const FAnimNextParam& InAnimNextParam);
#endif
	bool IsValid() const
	{
		return !Name.IsNone() && Type.IsValid();
	}

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FAnimNextParamType Type;

	UPROPERTY(EditAnywhere, Category = "Parameter", NoClear, meta=(ExcludeBaseStruct))
	TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;
};

