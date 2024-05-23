// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeBooleanAlgebraPropertyFunctions.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeNodeDescriptionHelpers.h"

#define LOCTEXT_NAMESPACE "StateTree"

namespace UE::StateTree::BooleanPropertyFunctions::Internal
{
#if WITH_EDITOR
	FText GetDescriptionForOperation(FText OperationText, const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting)
	{
		const FStateTreeBooleanOperationPropertyFunctionInstanceData& InstanceData = InstanceDataView.Get<FStateTreeBooleanOperationPropertyFunctionInstanceData>();

		FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FStateTreePropertyPath(ID, GET_MEMBER_NAME_CHECKED(FStateTreeBooleanOperationPropertyFunctionInstanceData, bLeft)), Formatting);
		if (LeftValue.IsEmpty())
		{
			LeftValue = UE::StateTree::DescHelpers::GetBoolText(InstanceData.bLeft, Formatting);
		}

		FText RightValue = BindingLookup.GetBindingSourceDisplayName(FStateTreePropertyPath(ID, GET_MEMBER_NAME_CHECKED(FStateTreeBooleanOperationPropertyFunctionInstanceData, bRight)), Formatting);
		if (RightValue.IsEmpty())
		{
			RightValue = UE::StateTree::DescHelpers::GetBoolText(InstanceData.bRight, Formatting);
		}

		const FText Format = (Formatting == EStateTreeNodeFormatting::RichText)
			? LOCTEXT("BoolFuncRich", "({Left} <s>{Operation}</> {Right})")
			: LOCTEXT("BoolFunc", "({Left} {Operation} {Right})");

		return FText::FormatNamed(Format,
			TEXT("Left"), LeftValue,
			TEXT("Operation"), OperationText,
			TEXT("Right"), RightValue);
	}
#endif // WITH_EDITOR
}

void FStateTreeBooleanAndPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FStateTreeBooleanOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = InstanceData.bLeft && InstanceData.bRight;
}

#if WITH_EDITOR
FText FStateTreeBooleanAndPropertyFunction::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return UE::StateTree::BooleanPropertyFunctions::Internal::GetDescriptionForOperation(LOCTEXT("BoolAnd", "and"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

void FStateTreeBooleanOrPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FStateTreeBooleanOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = InstanceData.bLeft || InstanceData.bRight;
}

#if WITH_EDITOR
FText FStateTreeBooleanOrPropertyFunction::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return UE::StateTree::BooleanPropertyFunctions::Internal::GetDescriptionForOperation(LOCTEXT("BoolOr", "or"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
