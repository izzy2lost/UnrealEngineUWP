// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextEditorParam.h"

#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamInstanceIdentifier.h"

FAnimNextEditorParam::FAnimNextEditorParam(FName InName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
	: Name(InName)
	, Type(InType)
	, InstanceId(InInstanceId)
{
}

#if WITH_EDITORONLY_DATA

FAnimNextEditorParam::FAnimNextEditorParam(const FAnimNextParam& InAnimNextParam)
	: Name(InAnimNextParam.Name)
	, Type(InAnimNextParam.Type)
{
	if(InAnimNextParam.InstanceIdType != nullptr)
	{
		InstanceId.InitializeAsScriptStruct(InAnimNextParam.InstanceIdType);
		InstanceId.GetMutable().FromName(InAnimNextParam.InstanceId);
	}
}

#endif