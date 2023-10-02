// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/RigVMDispatch_SetLayerParameter.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVM.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "Param/ParamId.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamStackLayerHandle.h"

class UAnimNextParameter;

const FName FRigVMDispatch_SetLayerParameter::ExecuteContextName = TEXT("ExecuteContext");
const FName FRigVMDispatch_SetLayerParameter::ValueName = TEXT("Value");
const FName FRigVMDispatch_SetLayerParameter::TypeHandleName = TEXT("Type");
const FName FRigVMDispatch_SetLayerParameter::ParameterName = TEXT("Parameter");
const FName FRigVMDispatch_SetLayerParameter::ParameterIdName = TEXT("ParameterId");

FRigVMDispatch_SetLayerParameter::FRigVMDispatch_SetLayerParameter()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_SetLayerParameter::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = 
	{
		ParameterName,
		ValueName,
		ParameterIdName,
		TypeHandleName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

#if WITH_EDITOR
FString FRigVMDispatch_SetLayerParameter::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if ((InArgumentName == TypeHandleName || InArgumentName == ParameterIdName) &&
		InMetaDataKey == FRigVMStruct::SingletonMetaName)
	{
		return TEXT("True");
	}
	else if(InArgumentName == ParameterName && InMetaDataKey == FRigVMStruct::CustomWidgetMetaName)
	{
		return TEXT("ParamName");
	}

	return Super::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

FString FRigVMDispatch_SetLayerParameter::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if (InArgumentName == ParameterIdName)
	{
		return FString::Printf(TEXT("%u"), 0xffffffff);
	}

	return Super::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}
#endif

const TArray<FRigVMTemplateArgument>& FRigVMDispatch_SetLayerParameter::GetArguments() const
{
	static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
	{
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	
	static const TArray<FRigVMTemplateArgument> Arguments = { 
		FRigVMTemplateArgument(ParameterName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName),
		FRigVMTemplateArgument(ValueName, ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(ParameterIdName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
		FRigVMTemplateArgument(TypeHandleName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
	};

	return Arguments;
}

TArray<FRigVMExecuteArgument>& FRigVMDispatch_SetLayerParameter::GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const
{
	static TArray<FRigVMExecuteArgument> Arguments = { { ExecuteContextName, ERigVMPinDirection::IO } };
	return Arguments;
}

FRigVMTemplateTypeMap FRigVMDispatch_SetLayerParameter::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(ParameterName, RigVMTypeUtils::TypeIndex::FName);
	Types.Add(ValueName, InTypeIndex);
	Types.Add(ParameterIdName, RigVMTypeUtils::TypeIndex::UInt32);
	Types.Add(TypeHandleName, RigVMTypeUtils::TypeIndex::UInt32);
	return Types;
}

void FRigVMDispatch_SetLayerParameter::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FName Parameter = *(FName*)Handles[0].GetData();
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty();
	check(ValueProperty);
	const uint8* SourceData =  Handles[1].GetData();

	uint32& RawParameterId = *(uint32*)Handles[2].GetData();
	if (RawParameterId == FParamId::InvalidIndex && Parameter != NAME_None)
	{
		RawParameterId = FParamId(Parameter).ToInt();
	}

	uint32& RawTypeHandle = *(uint32*)Handles[3].GetData();
	if (RawTypeHandle == 0)
	{
		RawTypeHandle = FParamTypeHandle::FromProperty(ValueProperty).ToRaw();
	}

	FParamId ParameterId(RawParameterId);
	FParamTypeHandle TypeHandle = FParamTypeHandle::FromRaw(RawTypeHandle);
	if(ParameterId.IsValid() && TypeHandle.IsValid())
	{
		FAnimNextParameterExecuteContext& ParamContext = InContext.GetPublicData<FAnimNextParameterExecuteContext>();
		TConstArrayView<uint8> SourceDataView(SourceData, ValueProperty->GetSize());
		ParamContext.GetLayerHandle().SetValueRaw(ParameterId, TypeHandle, SourceDataView);
	}
}

