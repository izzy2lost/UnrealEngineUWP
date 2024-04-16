// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/RigVMDispatch_GetScopedParameter.h"
#include "Param/ParamStack.h"
#include "RigVMCore/RigVMStruct.h"
#include "Param/AnimNextParam.h"

const FName FRigVMDispatch_GetScopedParameter::ParameterName = TEXT("Parameter");
const FName FRigVMDispatch_GetScopedParameter::ValueName = TEXT("Value");
const FName FRigVMDispatch_GetScopedParameter::ParameterIdName = TEXT("ParameterId");
const FName FRigVMDispatch_GetScopedParameter::TypeHandleName = TEXT("Type");

FRigVMDispatch_GetScopedParameter::FRigVMDispatch_GetScopedParameter()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_GetScopedParameter::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] =
	{
		ParameterName,
		ValueName,
		ParameterIdName,
		TypeHandleName,
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

#if WITH_EDITOR
FString FRigVMDispatch_GetScopedParameter::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if ((InArgumentName == TypeHandleName || InArgumentName == ParameterIdName) &&
		InMetaDataKey == FRigVMStruct::SingletonMetaName)
	{
		return TEXT("True");
	}
	else if(InArgumentName == ParameterName && InMetaDataKey == FRigVMStruct::HideSubPinsMetaName)
	{
		return TEXT("True");
	}
	return Super::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}
#endif

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_GetScopedParameter::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if(Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};

		const FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForRead();
		Infos.Emplace(ParameterName, ERigVMPinDirection::Input, Registry.GetTypeIndex_NoLock<FAnimNextParam>());
		Infos.Emplace(ValueName, ERigVMPinDirection::Output, ValueCategories);
		Infos.Emplace(ParameterIdName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32);
		Infos.Emplace(TypeHandleName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32);
	}

	return Infos;
}
 
FRigVMTemplateTypeMap FRigVMDispatch_GetScopedParameter::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	const FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForRead();
	Types.Add(ParameterName, Registry.GetTypeIndex_NoLock<FAnimNextParam>());
	Types.Add(ValueName, InTypeIndex);
	Types.Add(ParameterIdName, RigVMTypeUtils::TypeIndex::UInt32);
	Types.Add(TypeHandleName, RigVMTypeUtils::TypeIndex::UInt32);
	return Types;
}

void FRigVMDispatch_GetScopedParameter::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FAnimNextParam& Parameter = *(FAnimNextParam*)Handles[0].GetData();
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty();
	check(ValueProperty);
	uint8* TargetDataPtr = Handles[1].GetData();

	uint32& ParameterHash = *(uint32*)Handles[2].GetData();
	if (ParameterHash == 0 && Parameter.Name != NAME_None)
	{
		ParameterHash = FParamId::CalculateHash(Parameter.Name, Parameter.InstanceId);
	}

	uint32& TypeHandle = *(uint32*)Handles[3].GetData();
	if (TypeHandle == 0)
	{
		TypeHandle = FParamTypeHandle::FromProperty(ValueProperty).ToRaw();
	}

	TConstArrayView<uint8> SourceData;
	if (FParamStack::Get().GetParamData(FParamId(Parameter.Name, Parameter.InstanceId, ParameterHash), FParamTypeHandle::FromRaw(TypeHandle), SourceData).IsSuccessful())
	{
		ValueProperty->CopyCompleteValue(TargetDataPtr, SourceData.GetData());
	}
}
