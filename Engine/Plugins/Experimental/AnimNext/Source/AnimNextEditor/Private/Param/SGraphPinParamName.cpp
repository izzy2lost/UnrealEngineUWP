// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphPinParamName.h"
#include "EditorUtils.h"
#include "SParameterPickerCombo.h"
#include "ScopedTransaction.h"
#include "UncookedOnlyUtils.h"

#define LOCTEXT_NAMESPACE "SGraphPinParamName"

namespace UE::AnimNext::Editor
{

void SGraphPinParamName::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	ModelPin = InArgs._ModelPin;

	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinParamName::GetDefaultValueWidget()
{
	FParameterPickerArgs Args;
	Args.bShowLibraries = false;
	Args.bShowBlocks = false;
	Args.bMultiSelect = false;
	Args.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		if(ModelPin)
		{
			FScopedTransaction Transaction(LOCTEXT("SelectParameter", "Select Parameter"));
			GraphPinObj->Modify();
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, InParameterBinding.Parameter.ToString());
		}
	});
	
	return SNew(SParameterPickerCombo)
		.PickerArgs(Args)
		.OnGetParameterName_Lambda([this]()
		{
			return FName(*GraphPinObj->DefaultValue);
		})
		.OnGetParameterType_Lambda([this]()
		{
			return UncookedOnly::FUtils::GetParameterTypeFromName(FName(*GraphPinObj->DefaultValue));
		});
}

}

#undef LOCTEXT_NAMESPACE