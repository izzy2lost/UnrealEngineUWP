// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParam.h"

#include "StructUtils/InstancedStruct.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParamInstanceIdentifier.h"

FAnimNextParam::FAnimNextParam(FName InName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
	: Name(InName)
	, InstanceId(InInstanceId.IsValid() ? InInstanceId.Get().ToName() : NAME_None)
#if WITH_EDITORONLY_DATA
	, InstanceIdType(InInstanceId.GetScriptStruct())
#endif
	, Type(InType)
{
}

#if WITH_EDITORONLY_DATA

FAnimNextParam::FAnimNextParam(const FAnimNextEditorParam& InEditorParam)
	: FAnimNextParam(InEditorParam.Name, InEditorParam.Type, InEditorParam.InstanceId)
{
}

#endif