// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigRuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigRuleManager)

#define LOCTEXT_NAMESPACE "ModularRigRuleManager"

FModularRigResolveResult UModularRigRuleManager::Resolve(
	const FRigConnectorElement* InConnector,
	const FRigModuleInstance* InModule,
	const FRigElementKeyRedirector& InResolvedConnectors) const
{
	FModularRigResolveResult Result;
	
	if(!Hierarchy.IsValid())
	{
		static const FText MissingHierarchyMessage = LOCTEXT("MissingHierarchyMessage", "The rule manager is missing the hierarchy.");
		Result.Message = MissingHierarchyMessage;
		Result.State = EModularRigResolveState::Error;
		return Result;
	}

	// start with a full set of possible targets
	TArray<bool> VisitedElement;
	VisitedElement.AddZeroed(Hierarchy->Num());
	Result.Matches.Reserve(Hierarchy->Num());
	Hierarchy->Traverse([&VisitedElement, &Result](const FRigBaseElement* Element, bool& bContinue)
	{
		if(!VisitedElement[Element->GetIndex()])
		{
			VisitedElement[Element->GetIndex()] = true;
			Result.Matches.Emplace(Element->GetKey(), ERigElementResolveState::PossibleTarget, FText());
		}
		bContinue = true;
	}, true);

	FWorkData WorkData;
	WorkData.Hierarchy = Hierarchy.Get();
	WorkData.Connector = InConnector;
	WorkData.Module = InModule;
	WorkData.ResolvedConnectors = &InResolvedConnectors;
	WorkData.Result = &Result;
	ResolveConnector(WorkData);
	return Result;;
}

void UModularRigRuleManager::FWorkData::Filter(TFunction<void(FRigElementResolveResult&)> PerMatchFunction)
{
	const TArray<FRigElementResolveResult> PreviousMatches = Result->Matches;;
	Result->Matches.Reset();
	for(const FRigElementResolveResult& PreviousMatch : PreviousMatches)
	{
		FRigElementResolveResult Match = PreviousMatch;
		PerMatchFunction(Match);
		if(Match.IsValid())
		{
			Result->Matches.Add(Match);
		}
		else
		{
			Result->Excluded.Add(Match);
		}
	}
}

void UModularRigRuleManager::SetHierarchy(const URigHierarchy* InHierarchy)
{
	check(InHierarchy);
	Hierarchy = InHierarchy;
}

void UModularRigRuleManager::ResolveConnector(FWorkData& InOutWorkData)
{
	FilterIncompatibleTypes(InOutWorkData);
	FilterInvalidNameSpaces(InOutWorkData);
	FilterByConnectorRules(InOutWorkData);

	if(InOutWorkData.Result->Matches.IsEmpty())
	{
		InOutWorkData.Result->State = EModularRigResolveState::Error;
	}
	else
	{
		InOutWorkData.Result->State = EModularRigResolveState::Success;
	}
}

void UModularRigRuleManager::FilterIncompatibleTypes(FWorkData& InOutWorkData)
{
	InOutWorkData.Filter([](FRigElementResolveResult& Result)
	{
		if(Result.GetKey().Type == ERigElementType::Curve)
		{
			static const FText CurveInvalidTargetMessage = LOCTEXT("CannotConnectToCurves", "Cannot connect to curves.");
			Result.SetInvalidTarget(CurveInvalidTargetMessage);
		}
		if(Result.GetKey().Type == ERigElementType::Connector)
		{
			static const FText CurveInvalidTargetMessage = LOCTEXT("CannotConnectToConnectors", "Cannot connect to connectors.");
			Result.SetInvalidTarget(CurveInvalidTargetMessage);
		}
	});
}

void UModularRigRuleManager::FilterInvalidNameSpaces(FWorkData& InOutWorkData)
{
	const FName NameSpace = InOutWorkData.Hierarchy->GetNameMetadata(InOutWorkData.Connector->GetKey(), URigHierarchy::NameSpaceMetadataName, NAME_None);
	if(NameSpace.IsNone())
	{
		return;
	}
	
	const FString NameSpaceString = NameSpace.ToString(); 
	const FString NameSpacePrefix = NameSpaceString + UModularRig::NamespaceSeparator; 
	InOutWorkData.Filter([NameSpaceString, NameSpacePrefix, InOutWorkData](FRigElementResolveResult& Result)
	{
		const FName MatchNameSpace = InOutWorkData.Hierarchy->GetNameMetadata(Result.GetKey(), URigHierarchy::NameSpaceMetadataName, NAME_None);
		if(!MatchNameSpace.IsNone())
		{
			const FString MatchNameSpaceString = MatchNameSpace.ToString();
			if(MatchNameSpaceString.Equals(NameSpaceString, ESearchCase::CaseSensitive))
			{
				static const FText CannotConnectWithinNameSpaceMessage = LOCTEXT("CannotConnectWithinNameSpace", "Cannot connect within the same namespace.");
				Result.SetInvalidTarget(CannotConnectWithinNameSpaceMessage);
			}
			else if(MatchNameSpaceString.StartsWith(NameSpacePrefix, ESearchCase::CaseSensitive))
			{
				static const FText CannotConnectBelowNameSpaceMessage = LOCTEXT("CannotConnectBelowNameSpace", "Cannot connect to element below the connector's namespace.");
				Result.SetInvalidTarget(CannotConnectBelowNameSpaceMessage);
			}
		}
	});
}

void UModularRigRuleManager::FilterByConnectorRules(FWorkData& InOutWorkData)
{
	const FRigConnectorElement* Connector = InOutWorkData.Connector;
	for(const FRigConnectionRuleStash& Stash : Connector->Settings.Rules)
	{
		TSharedPtr<FStructOnScope> Storage;
		const FRigConnectionRule* Rule = Stash.Get(Storage);

		InOutWorkData.Filter([Rule, InOutWorkData](FRigElementResolveResult& Result)
		{
			const FRigBaseElement* Target = InOutWorkData.Hierarchy->Find(Result.GetKey());
			check(Target);
			Result = Rule->Resolve(Target, InOutWorkData.Hierarchy, InOutWorkData.ResolvedConnectors);
		});
	}
}

#undef LOCTEXT_NAMESPACE
