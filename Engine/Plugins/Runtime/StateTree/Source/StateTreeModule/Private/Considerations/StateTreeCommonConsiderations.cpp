// Copyright Epic Games, Inc. All Rights Reserved.

#include "Considerations/StateTreeCommonConsiderations.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeCommonConsiderations)

#define LOCTEXT_NAMESPACE "StateTree"

#if WITH_EDITOR
FText FStateTreeFloatConsideration::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText OptionalConstantKeywordText = FText::GetEmpty();
	FText RawScoreOrPropertyPathText = BindingLookup.GetBindingSourceDisplayName(FStateTreePropertyPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, RawScore)), Formatting);
	if (RawScoreOrPropertyPathText.IsEmpty())
	{
		//using a constant value for the float param
		OptionalConstantKeywordText = (Formatting == EStateTreeNodeFormatting::RichText)
			? LOCTEXT("ConstantConsiderationRich", "<s>Constant</>")
			: LOCTEXT("ConstantConsideration", "Constant");

		RawScoreOrPropertyPathText = FText::AsNumber(InstanceData->RawScore);
	}

	FText WithinValueRangeText = UE::StateTree::DescHelpers::GetWithinValueRangeText(ResponseCurve.RawScoreLowerBound, ResponseCurve.RawScoreUpperBound, Formatting);
	
	return FText::Format(LOCTEXT("FloatParam", "{OptionalConstantKeyword} {RawScoreOrPropertyPath} {WithinValueRange}"),
		OptionalConstantKeywordText,
		RawScoreOrPropertyPathText,
		WithinValueRangeText
		);
}
#endif //WITH_EDITOR

float FStateTreeFloatConsideration::ComputeRawScore(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return InstanceData.RawScore;
}

#undef LOCTEXT_NAMESPACE
