// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamId.h"
#include "Param/ParamType.h"
#include "AnimNextParam.generated.h"

struct FAnimNextParamInstanceIdentifier;
template<typename T> struct TInstancedStruct;

#if WITH_EDITORONLY_DATA
struct FAnimNextEditorParam;
#endif

USTRUCT()
struct FAnimNextParam
{
	GENERATED_BODY()

	FAnimNextParam() = default;

	ANIMNEXT_API FAnimNextParam(FName InName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId);

#if WITH_EDITORONLY_DATA
	ANIMNEXT_API explicit FAnimNextParam(const FAnimNextEditorParam& InEditorParam);
#endif

	bool IsValid() const
	{
		return !Name.IsNone() && Type.IsValid();
	}

	// Get the param ID for this parameter, lazily calculating the parameter hash
	UE::AnimNext::FParamId GetParamId() const
	{
		if(Hash == 0)
		{
			Hash = UE::AnimNext::FParamId::CalculateHash(Name, InstanceId);
		}

		return UE::AnimNext::FParamId(Name, InstanceId, Hash);
	}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName InstanceId;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<const UScriptStruct> InstanceIdType;
#endif

	UPROPERTY()
	FAnimNextParamType Type;

	// Cached hash
	mutable uint32 Hash = 0;
};