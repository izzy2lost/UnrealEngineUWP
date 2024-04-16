// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextObjectFunctionLocatorFragment.h"
#include "UniversalObjectLocatorFragmentTypeHandle.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "UniversalObjectLocatorStringParams.h"
#include "UniversalObjectLocatorInitializeParams.h"
#include "UniversalObjectLocatorInitializeResult.h"

UE::UniversalObjectLocator::TFragmentTypeHandle<FAnimNextObjectFunctionLocatorFragment> FAnimNextObjectFunctionLocatorFragment::FragmentType;

FAnimNextObjectFunctionLocatorFragment::FAnimNextObjectFunctionLocatorFragment(UFunction* InFunction)
{
	Path = InFunction;
}

UE::UniversalObjectLocator::FResolveResult FAnimNextObjectFunctionLocatorFragment::Resolve(const UE::UniversalObjectLocator::FResolveParams& Params) const
{
	using namespace UE::UniversalObjectLocator;

	UObject* Result = nullptr;

	if(UFunction* Function = Cast<UFunction>(Path.ResolveObject()))
	{
		if(Params.Context && Function->NumParms == 1 && Params.Context->GetClass()->IsChildOf(Function->GetOuterUClass()))
		{
			if(FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Function->GetReturnProperty()))
			{
				Params.Context->ProcessEvent(Function, &Result);
			}
		}
	}

	return FResolveResultData(Result);
}

void FAnimNextObjectFunctionLocatorFragment::ToString(FStringBuilderBase& OutStringBuilder) const
{
	Path.AppendString(OutStringBuilder);
}

UE::UniversalObjectLocator::FParseStringResult FAnimNextObjectFunctionLocatorFragment::TryParseString(FStringView InString, const UE::UniversalObjectLocator::FParseStringParams& Params)
{
	Path = InString;
	return UE::UniversalObjectLocator::FParseStringResult().Success();
}

UE::UniversalObjectLocator::FInitializeResult FAnimNextObjectFunctionLocatorFragment::Initialize(const UE::UniversalObjectLocator::FInitializeParams& InParams)
{
	using namespace UE::UniversalObjectLocator;

	if(const UFunction* Function = Cast<UFunction>(InParams.Object))
	{
		if(Function->NumParms == 1)
		{
			if(FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Function->GetReturnProperty()))
			{
				Path = Function;
			}
		}
	}

	return FInitializeResult::Relative(InParams.Context);
}

uint32 FAnimNextObjectFunctionLocatorFragment::ComputePriority(const UObject* ObjectToReference, const UObject* Context)
{
	// We can't use this at all unless explicitly added by code
	return 0;
}
