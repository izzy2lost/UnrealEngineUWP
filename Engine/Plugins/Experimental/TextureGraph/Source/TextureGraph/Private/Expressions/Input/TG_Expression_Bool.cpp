// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Input/TG_Expression_Bool.h"


FTG_SignaturePtr UTG_Expression_Bool::BuildInputParameterSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	return MakeShared<FTG_Signature>(SignatureInit);
};

FTG_SignaturePtr UTG_Expression_Bool::BuildInputConstantSignature() const
{
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();
	for (auto& arg : SignatureInit.Arguments)
	{
		if (arg.IsInput() && arg.IsParam())
		{
			arg.ArgumentType = arg.ArgumentType.Unparamed();
			arg.ArgumentType.SetNotConnectable();
		}
	}
	return MakeShared<FTG_Signature>(SignatureInit);
};

void UTG_Expression_Bool::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	// The Value is updated either as an input or as a setting and then becomes the output for this expression
	// The pin out is named "ValueOut"
	ValueOut = bValue;
}

void UTG_Expression_Bool::SetTitleName(FName NewName)
{
	GetParentNode()->GetInputPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Bool, bValue))->SetAliasName(NewName);
}

FName UTG_Expression_Bool::GetTitleName() const
{
	return GetParentNode()->GetInputPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_Bool, bValue))->GetAliasName();
}