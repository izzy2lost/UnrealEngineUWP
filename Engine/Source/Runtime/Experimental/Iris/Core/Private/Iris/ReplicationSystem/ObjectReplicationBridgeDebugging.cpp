// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

#include "HAL/IConsoleManager.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisDebugging.h"

#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"

#include "Net/Core/NetBitArrayPrinter.h"

namespace UE::Net::Private::ObjectBridgeDebugging
{

UReplicationSystem* FindReplicationSystem(const TArray<FString>& Args)
{
	uint32 RepSystemId = 0;

	// If the ReplicationSystemId was specified
	if (const FString* ArgRepSystemId = Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("RepSystemId=")); }))
	{
		FParse::Value(**ArgRepSystemId, TEXT("RepSystemId="), RepSystemId);
	}

	return UE::Net::GetReplicationSystem(RepSystemId);
}


} // end namespace UE::Net::Private::ObjectBridgeDebugging

// --------------------------------------------------------------------------------------------------------------------------------------------
// Debug commands
// --------------------------------------------------------------------------------------------------------------------------------------------

FAutoConsoleCommand ObjectBridgePrintDynamicFilter(TEXT("Net.Iris.PrintDynamicFilterClassConfig"), TEXT("Prints the dynamic filter configured to be assigned to specific classes."), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystem(Args);
	if (!RepSystem)
	{
		UE_LOG(LogIrisBridge, Error, TEXT("Could not find ReplicationSystem."));
		return;
	}

	UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge());
	if (!ObjectBridge)
	{
		UE_LOG(LogIrisBridge, Error, TEXT("Could not find ObjectReplicationBridge."));
		return;
	}

	ObjectBridge->PrintDynamicFilterClassConfig();
}));

void UObjectReplicationBridge::PrintDynamicFilterClassConfig() const
{
	const UReplicationSystem* RepSystem = GetReplicationSystem();

	UE_LOG(LogIrisFilterConfig, Display, TEXT(""));
	UE_LOG(LogIrisFilterConfig, Display, TEXT("Default Dynamic Filter Class Config:"));
	{
		TMap<FName, UE::Net::FNetObjectFilterHandle> SortedClassConfig = ClassesWithDynamicFilter;

		SortedClassConfig.KeyStableSort([](FName lhs, FName rhs){return lhs.Compare(rhs) < 0;});
		for (auto MapIt = SortedClassConfig.CreateConstIterator(); MapIt; ++MapIt)
		{
			const FName ClassName = MapIt.Key();
			const UE::Net::FNetObjectFilterHandle FilterHandle = MapIt.Value();

			UE_LOG(LogIrisFilterConfig, Display, TEXT("\t%s -> %s"), *ClassName.ToString(), *RepSystem->GetFilterName(FilterHandle).ToString());
		}
	}
}