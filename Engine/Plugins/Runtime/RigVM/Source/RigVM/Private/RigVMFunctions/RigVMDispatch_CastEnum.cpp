// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_CastEnum.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMFunctions/RigVMFunctionDefines.h"

#define LOCTEXT_NAMESPACE "RigVMDispatch_CastEnum"

const FName FRigVMDispatch_CastEnum::ValueName = TEXT("Value");
const FName FRigVMDispatch_CastEnum::ResultName = TEXT("Result");

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CastEnum::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> OutInfos;
	if (OutInfos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ElementCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleEnumValue
		};
		
		static TArray<FRigVMTemplateArgumentInfo> Infos;
		Infos.Emplace(ValueName, ERigVMPinDirection::Input, ElementCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Int32);
		
		OutInfos = BuildArgumentListFromPrimaryArgument(Infos, ValueName);
	}

	return OutInfos;
}

bool FRigVMDispatch_CastEnum::GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const
{
	if (InArgumentName == ValueName)
	{
		OutPermutations.Add(
	{
			{ ValueName, InTypeIndex },
			{ ResultName, RigVMTypeUtils::TypeIndex::Int32 }
		});
	}
	else if (InArgumentName == ResultName && InTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		const TArray<TRigVMTypeIndex>& EnumTypes = FRigVMRegistry::Get().GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue);
		for (const TRigVMTypeIndex& Type : EnumTypes)
		{
			OutPermutations.Add(
	{
				{ ValueName, Type },
				{ ResultName, InTypeIndex }
			});
		}
	}
	return !OutPermutations.IsEmpty();
}

#if WITH_EDITOR

FString FRigVMDispatch_CastEnum::GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const
{
	return TEXT("Cast to int");
}

FText FRigVMDispatch_CastEnum::GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const
{
	return LOCTEXT("CastToolTip", "Casts from enum to int");
}

#endif

void FRigVMDispatch_CastEnum::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	const FEnumProperty* ValueProperty = CastFieldChecked<FEnumProperty>(Handles[0].GetProperty());
	const FProperty* ResultProperty = CastFieldChecked<FProperty>(Handles[1].GetProperty());

	if (!ResultProperty || !ValueProperty)
	{
		return;
	}

	uint8* ValuePtr = Handles[0].GetData();
	int32* ResultPtr = (int32*)Handles[1].GetData();
	if(ValuePtr && ResultPtr)
	{
		*ResultPtr = 0;
		ValueProperty->CopyCompleteValue(ResultPtr, ValuePtr);
	}
	else
	{
		*ResultPtr = INDEX_NONE;
	}

#if WITH_EDITOR
	if (*ResultPtr == INDEX_NONE)
	{
		const FRigVMExecuteContext& ExecuteContext = InContext.GetPublicData<>();
		if(ExecuteContext.GetLog() != nullptr)
		{
			ExecuteContext.Report(EMessageSeverity::Error, InContext.GetPublicData<>().GetFunctionName(), InContext.GetPublicData<>().GetInstructionIndex(), TEXT("Enum value invalid"));
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
