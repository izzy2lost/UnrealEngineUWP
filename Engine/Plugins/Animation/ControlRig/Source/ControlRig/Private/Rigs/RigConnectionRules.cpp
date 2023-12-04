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

/*

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	for(const TPair<FRigElementKey, FRigElementKey>& Pair : InConnectionInfo->ConnectionMap)
	{
		const FRigElementKey& ConnectorKey = Pair.Key;
		if(!ConnectorKey.IsValid())
		{
			if(OutFailureReason)
			{
				static constexpr TCHAR Format[] = TEXT("Connector element '%s' is not valid.");
				OutFailureReason->Appendf(Format, *ConnectorKey.ToString());
			}
			return false;
		}
		if(ConnectorKey.Type != ERigElementType::Connector)
		{
			if(OutFailureReason)
			{
				static constexpr TCHAR Format[] = TEXT("Connector element '%s' is not a Connector.");
				OutFailureReason->Appendf(Format, *ConnectorKey.ToString());
			}
			return false;
		}
		if(InConnectionInfo->SourceHierarchy->Find<FRigConnectorElement>(ConnectorKey) == nullptr)
		{
			if(OutFailureReason)
			{
				static constexpr TCHAR Format[] = TEXT("Connector element '%s' does not exist.");
				OutFailureReason->Appendf(Format, *ConnectorKey.ToString());
			}
			return false;
		}
		const FRigElementKey& TargetKey = Pair.Value;
		if(!TargetKey.IsValid())
		{
			if(OutFailureReason)
			{
				static constexpr TCHAR Format[] = TEXT("Target element '%s' is not valid.");
				OutFailureReason->Appendf(Format, *TargetKey.ToString());
			}
			return false;
		}
		if(InConnectionInfo->TargetHierarchy->Find(TargetKey) == nullptr)
		{
			if(OutFailureReason)
			{
				static constexpr TCHAR Format[] = TEXT("Target element '%s' does not exist.");
				OutFailureReason->Appendf(Format, *TargetKey.ToString());
			}
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FRigAndConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigAndConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	if(!Super::CanConnect(InConnectionInfo, OutFailureReason))
	{
		return false;
	}

	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			if(!Rule->CanConnect(InConnectionInfo, OutFailureReason))
			{
				return false;
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FRigOrConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigOrConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	if(!Super::CanConnect(InConnectionInfo, OutFailureReason))
	{
		return false;
	}

	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			if(Rule->CanConnect(InConnectionInfo, OutFailureReason))
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTypeConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigTypeConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	if(!Super::CanConnect(InConnectionInfo, OutFailureReason))
	{
		return false;
	}
	
	for(const TPair<FRigElementKey, FRigElementKey>& Pair : InConnectionInfo->ConnectionMap)
	{
		const FRigElementKey& TargetKey = Pair.Value;
		if(!TargetKey.IsTypeOf(ElementType))
		{
			if(OutFailureReason)
			{
				const FString ExpectedType = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ElementType).ToString();
				static constexpr TCHAR Format[] = TEXT("Element '%s' is not of the expected type (%s).");
				OutFailureReason->Appendf(Format, *TargetKey.ToString(), *ExpectedType);
			}
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTagConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigTagConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	if(!Super::CanConnect(InConnectionInfo, OutFailureReason))
	{
		return false;
	}
	const FRigElementKey TargetKey = InConnectionInfo->ConnectionMap.begin().Value();
	if(!InConnectionInfo->TargetHierarchy->HasTag(TargetKey, Tag))
	{
		if(OutFailureReason)
		{
			static constexpr TCHAR Format[] = TEXT("Element '%s' does not contain tag '%s'.");
			OutFailureReason->Appendf(Format, *TargetKey.ToString(), *Tag.ToString());
		}
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// FRigChainConnectionRule
////////////////////////////////////////////////////////////////////////////////

bool FRigChainConnectionRule::CanConnect(const FRigConnectionInfo* InConnectionInfo, FString* OutFailureReason) const
{
	if(!Super::CanConnect(InConnectionInfo, OutFailureReason))
	{
		return false;
	}

	TArray<FRigElementKey> Targets;
	Targets.Reserve(InConnectionInfo->ConnectionMap.Num());
	for(const TPair<FRigElementKey, FRigElementKey>& Pair : InConnectionInfo->ConnectionMap)
	{
		Targets.Add(Pair.Value);
	}
	if(Targets.IsEmpty())
	{
		return false;
	}

	const FRigElementKey EndKey = Targets[0];
	const FRigElementKey StartKey = Targets.Last();

	FRigElementKey NextKey = EndKey;
	TArray<FRigElementKey> ReversedChain;

	while(NextKey.IsValid())
	{
		ReversedChain.Add(NextKey);
		if(NextKey == StartKey)
		{
			break;
		}
		NextKey = InConnectionInfo->TargetHierarchy->GetFirstParent(NextKey);
	}

	if(ReversedChain.Last() != StartKey)
	{
		if(OutFailureReason)
		{
			static constexpr TCHAR Format[] = TEXT("Elements '%s' and '%s' are not on a chain.");
			OutFailureReason->Appendf(Format, *StartKey.ToString(), *EndKey.ToString());
		}
		return false;
	}

	if(ReversedChain.Num() < MinNumBones)
	{
		if(OutFailureReason)
		{
			static constexpr TCHAR Format[] = TEXT("Chain from '%s' to '%s' only has %d elements (minimum of %d required).");
			OutFailureReason->Appendf(Format, *StartKey.ToString(), *EndKey.ToString(), ReversedChain.Num(), MinNumBones);
		}
		return false;
	}
	
	if(ReversedChain.Num() > MaxNumBones && MaxNumBones > 2)
	{
		if(OutFailureReason)
		{
			static constexpr TCHAR Format[] = TEXT("Chain from '%s' to '%s' has %d elements (maximum of %d allowed).");
			OutFailureReason->Appendf(Format, *StartKey.ToString(), *EndKey.ToString(), ReversedChain.Num(), MaxNumBones);
		}
		return false;
	}

	if(!bAllowBranches)
	{
		for(int32 Index = 1; Index < ReversedChain.Num() - 1; Index++)
		{
			NextKey = ReversedChain[Index];
			if(InConnectionInfo->TargetHierarchy->GetChildren(NextKey).Num() > 1)
			{
				if(OutFailureReason)
				{
					static constexpr TCHAR Format[] = TEXT("Chain element '%s' contains a branch.");
					OutFailureReason->Appendf(Format, *NextKey.ToString());
				}
				return false;
			}
		}
	}

	return true;
}

*/
