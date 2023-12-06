// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigConnectionRules.h"
#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigConnectionRules)

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRuleStash
////////////////////////////////////////////////////////////////////////////////

FRigConnectionRuleStash::FRigConnectionRuleStash()
{
}

FRigConnectionRuleStash::FRigConnectionRuleStash(const FRigConnectionRule* InRule)
{
	check(InRule);
	ScriptStructPath = InRule->GetScriptStruct()->GetPathName();
	InRule->GetScriptStruct()->ExportText(ExportedText, InRule, InRule, nullptr, PPF_None, nullptr);
}

void FRigConnectionRuleStash::Save(FArchive& Ar)
{
	Ar << ScriptStructPath;
	Ar << ExportedText;
}

void FRigConnectionRuleStash::Load(FArchive& Ar)
{
	Ar << ScriptStructPath;
	Ar << ExportedText;
}

bool FRigConnectionRuleStash::IsValid() const
{
	return !ScriptStructPath.IsEmpty() && !ExportedText.IsEmpty();
}

UScriptStruct* FRigConnectionRuleStash::GetScriptStruct() const
{
	if(!ScriptStructPath.IsEmpty())
	{
		return FindObject<UScriptStruct>(nullptr, *ScriptStructPath);
	}
	return nullptr;
}

TSharedPtr<FStructOnScope> FRigConnectionRuleStash::Get() const
{
	class FErrorPipe : public FOutputDevice
	{
	public:

		int32 NumErrors;

		FErrorPipe()
			: FOutputDevice()
			, NumErrors(0)
		{
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			NumErrors++;
		}
	};

	UScriptStruct* ScriptStruct = GetScriptStruct();
	check(ScriptStruct);
	TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(ScriptStruct));
	FErrorPipe ErrorPipe;
	ScriptStruct->ImportText(*ExportedText, StructOnScope->GetStructMemory(), nullptr, PPF_None, &ErrorPipe, ScriptStruct->GetName());
	return StructOnScope;
}

const FRigConnectionRule* FRigConnectionRuleStash::Get(TSharedPtr<FStructOnScope>& InOutStorage) const
{
	InOutStorage = Get();
	if(InOutStorage.IsValid() && InOutStorage->IsValid())
	{
		check(InOutStorage->GetStruct()->IsChildOf(FRigConnectionRule::StaticStruct()));
		return reinterpret_cast<const FRigConnectionRule*>(InOutStorage->GetStructMemory());
	}
	return nullptr;
}

bool FRigConnectionRuleStash::operator==(const FRigConnectionRuleStash& InOther) const
{
	return ScriptStructPath.Equals(InOther.ScriptStructPath, ESearchCase::CaseSensitive) &&
		ExportedText.Equals(InOther.ExportedText, ESearchCase::CaseSensitive);
}

uint32 GetTypeHash(const FRigConnectionRuleStash& InRuleStash)
{
	return HashCombine(GetTypeHash(InRuleStash.ScriptStructPath), GetTypeHash(InRuleStash.ExportedText));
}

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigConnectionRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetInvalidTarget(FText());
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigAndConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigAndConnectionRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();
	
	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			Result = Rule->Resolve(InTarget, InHierarchy, InRedirector);
			if(!Result.IsValid())
			{
				return Result;
			}
		}
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigOrConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigOrConnectionRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();
	
	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			Result = Rule->Resolve(InTarget, InHierarchy, InRedirector);
			if(Result.IsValid())
			{
				return Result;
			}
		}
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTypeConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigTypeConnectionRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();

	if(!InTarget->GetKey().IsTypeOf(ElementType))
	{
		const FString ExpectedType = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ElementType).ToString();
		static constexpr TCHAR Format[] = TEXT("Element '%s' is not of the expected type (%s).");
		Result.SetInvalidTarget(FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *ExpectedType)));
	}
	
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTagConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigTagConnectionRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();

	if(!InHierarchy->HasTag(InTarget->GetKey(), Tag))
	{
		static constexpr TCHAR Format[] = TEXT("Element '%s' does not contain tag '%s'.");
		Result.SetInvalidTarget(FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *Tag.ToString())));
	}
	
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigOnChainRule
////////////////////////////////////////////////////////////////////////////////

/*
FRigElementResolveResult FRigOnChainRule::Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const
{
	FRigElementResolveResult Result;
	Result.State = ERigElementResolveState::PossibleTarget;

	if(!InHierarchy->HasTag(InTarget->GetKey(), Tag))
	{
		static constexpr TCHAR Format[] = TEXT("Element '%s' does not contain tag '%s'.");
		Result.State = ERigElementResolveState::InvalidTarget;
		Result.Message = FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *Tag.ToString()));
	}
	
	return Result;
}
*/