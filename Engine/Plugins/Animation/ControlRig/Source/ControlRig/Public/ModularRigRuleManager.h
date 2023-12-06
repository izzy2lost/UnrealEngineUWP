// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularRig.h"
#include "ModularRigRuleManager.generated.h"

// A management class to validate rules and pattern match
UCLASS(BlueprintType)
class CONTROLRIG_API UModularRigRuleManager : public UObject
{
public:

	GENERATED_BODY()

	/***
	 * Returns the possible targets for the given connector in the current resolval stage
	 * @param InConnector The connector to resolve
	 * @param InResolvedConnectors A redirect map of the already resolved connectors
	 * @return The resolval result including a list of matches
	 */
	FModularRigResolveResult Resolve(
		const FRigConnectorElement* InConnector,
		const FRigModuleInstance* InModule,
		const FRigElementKeyRedirector& InResolvedConnectors = FRigElementKeyRedirector()
	) const;

private:

	struct FWorkData
	{
		FWorkData()
		: Hierarchy(nullptr)
		, Connector(nullptr)
		, Module(nullptr)
		, ResolvedConnectors(nullptr)
		, Result(nullptr)
		{
		}

		void Filter(TFunction<void(FRigElementResolveResult&)> PerMatchFunction);

		const URigHierarchy* Hierarchy;
		const FRigConnectorElement* Connector;
		const FRigModuleInstance* Module;
		const FRigElementKeyRedirector* ResolvedConnectors;
		FModularRigResolveResult* Result;
	};

	void SetHierarchy(const URigHierarchy* InHierarchy);
	static void ResolveConnector(FWorkData& InOutWorkData);
	static void FilterIncompatibleTypes(FWorkData& InOutWorkData);
	static void FilterInvalidNameSpaces(FWorkData& InOutWorkData);
	static void FilterByConnectorRules(FWorkData& InOutWorkData);

	TWeakObjectPtr<const URigHierarchy> Hierarchy;

	friend class URigHierarchy;
};