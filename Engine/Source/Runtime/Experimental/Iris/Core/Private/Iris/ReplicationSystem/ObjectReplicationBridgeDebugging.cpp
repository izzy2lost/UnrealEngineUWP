// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

#include "HAL/IConsoleManager.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisDebugging.h"

#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"

#include "Iris/ReplicationSystem/Filtering/ReplicationFiltering.h"

#include "Net/Core/NetBitArrayPrinter.h"
#include "Net/Core/Trace/NetDebugName.h"

namespace UE::Net::Private::ObjectBridgeDebugging
{

void PrintDefaultNetObjectState(UReplicationSystem* ReplicationSystem, uint32 ConnectionId, const FReplicationFragments& RegisteredFragments, FStringBuilderBase& StringBuilder)
{
	FReplicationSystemInternal* ReplicationSystemInternal = ReplicationSystem->GetReplicationSystemInternal();

	// In order to be able to output object references we need the TokenStoreState, for the server we just use the local one but if we are a client we must use the remote token store state
	FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();
	FNetTokenStoreState* TokenStoreState = ReplicationSystem->IsServer() ? ReplicationSystemInternal->GetNetTokenStore().GetLocalNetTokenStoreState() : &Connections.GetRemoteNetTokenStoreState(ConnectionId);

	// Setup Context
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = ReplicationSystem;
	InternalContextInitParams.PackageMap = ReplicationSystemInternal->GetIrisObjectReferencePackageMap();
	InternalContextInitParams.ObjectResolveContext.RemoteNetTokenStoreState = TokenStoreState;
	InternalContextInitParams.ObjectResolveContext.ConnectionId = ConnectionId;
	InternalContext.Init(InternalContextInitParams);

	FNetSerializationContext NetSerializationContext;
	NetSerializationContext.SetInternalContext(&InternalContext);
	NetSerializationContext.SetLocalConnectionId(ConnectionId);

	FReplicationInstanceOperations::OutputInternalDefaultStateToString(NetSerializationContext, StringBuilder, RegisteredFragments);
}

void RemoteProtocolMismatchDetected(UReplicationSystem* ReplicationSystem, uint32 ConnectionId, const FReplicationFragments& RegisteredFragments, const UObject* ArchetypeOrCDOKey, const UObject* InstancePtr)
{
	if (UE_LOG_ACTIVE(LogIris, Error))
	{
		static TMap<FObjectKey, bool> ArchetypesAlreadyPrinted;

		// Only print the CDO state once
		if (ArchetypesAlreadyPrinted.Find(FObjectKey(ArchetypeOrCDOKey)) == nullptr)
		{
			ArchetypesAlreadyPrinted.Add(FObjectKey(ArchetypeOrCDOKey), true);

			TStringBuilder<4096> StringBuilder;
			PrintDefaultNetObjectState(ReplicationSystem, ConnectionId, RegisteredFragments, StringBuilder);
			UE_LOG(LogIris, Error, TEXT("Printing replication state of CDO %s used for %s:\n%s"), *GetNameSafe(ArchetypeOrCDOKey), *GetNameSafe(InstancePtr), StringBuilder.ToString());
		}
	}
}

UReplicationSystem* FindReplicationSystemFromArg(const TArray<FString>& Args)
{
	uint32 RepSystemId = 0;

	// If the ReplicationSystemId was specified
	if (const FString* ArgRepSystemId = Args.FindByPredicate([](const FString& Str) { return Str.Contains(TEXT("RepSystemId=")); }))
	{
		FParse::Value(**ArgRepSystemId, TEXT("RepSystemId="), RepSystemId);
	}

	return UE::Net::GetReplicationSystem(RepSystemId);
}


FString PrintNetObject(FNetRefHandleManager* NetRefHandleManager, FInternalNetRefIndex ObjectIndex)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	FNetRefHandle NetRefHandle = NetRefHandleManager->GetNetRefHandleFromInternalIndex(ObjectIndex);
	const FNetRefHandleManager::FReplicatedObjectData& NetObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
	UObject* ObjectPtr = NetRefHandleManager->GetReplicatedObjectInstance(ObjectIndex);

	return FString::Printf(TEXT("%s %s (InternalIndex: %u) (%s)"), 
		(NetObjectData.SubObjectRootIndex == FNetRefHandleManager::InvalidInternalIndex) ? TEXT("RootObject"):TEXT("SubObject"),
		*GetNameSafe(ObjectPtr), ObjectIndex, *NetRefHandle.ToString()
	);
}

void LogRootObjectListWithSubObjects(FNetRefHandleManager* NetRefHandleManager, const FNetBitArrayView RootObjectList, TTuple<uint32, uint32>& OutStats, TFunction<FString(FInternalNetRefIndex ObjectIndex)> OptionalObjectPrint)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;

	auto PrintRootObjectInfo = [&](FInternalNetRefIndex RootObjectIndex)
	{
		UE_LOG(LogIrisBridge, Display, TEXT("%s %s"), *PrintNetObject(NetRefHandleManager, RootObjectIndex), OptionalObjectPrint ? *OptionalObjectPrint(RootObjectIndex) : TEXT(""));

		// TotalRootObjects
		OutStats.Get<0>()++;

		TArrayView<const FInternalNetRefIndex> SubObjects = NetRefHandleManager->GetSubObjects(RootObjectIndex);
		for (FInternalNetRefIndex SubObjectIndex : SubObjects)
		{
			UE_LOG(LogIrisBridge, Display, TEXT("\t%s %s"), *PrintNetObject(NetRefHandleManager, SubObjectIndex), OptionalObjectPrint ? *OptionalObjectPrint(RootObjectIndex) : TEXT(""));

			// TotalSubObjects
			OutStats.Get<1>()++;
		}
	};

	RootObjectList.ForAllSetBits(PrintRootObjectInfo);
}


} // end namespace UE::Net::Private::ObjectBridgeDebugging

// --------------------------------------------------------------------------------------------------------------------------------------------
// Debug commands
// --------------------------------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintDynamicFilter(
	TEXT("Net.Iris.PrintDynamicFilterClassConfig"), 
	TEXT("Prints the dynamic filter configured to be assigned to specific classes."), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
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
		TMap<FName, FClassFilterInfo> SortedClassConfig = ClassesWithDynamicFilter;

		SortedClassConfig.KeyStableSort([](FName lhs, FName rhs){return lhs.Compare(rhs) < 0;});
		for (auto MapIt = SortedClassConfig.CreateConstIterator(); MapIt; ++MapIt)
		{
			const FName ClassName = MapIt.Key();
			const FClassFilterInfo FilterInfo = MapIt.Value();

			UE_LOG(LogIrisFilterConfig, Display, TEXT("\t%s -> %s"), *ClassName.ToString(), *RepSystem->GetFilterName(FilterInfo.FilterHandle).ToString());
		}
	}
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintReplicatedObjects(
	TEXT("Net.Iris.PrintReplicatedObjects"), 
	TEXT("Prints the list of replicated objects registered for replication in Iris"), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (RepSystem)
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			ObjectBridge->PrintReplicatedObjects();
		}
	}
}));

void UObjectReplicationBridge::PrintReplicatedObjects() const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing ALL Replicated Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	uint32 TotalRootObjects = 0;
	uint32 TotalSubObjects = 0;

	FNetBitArray RootObjects;
	RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	MakeNetBitArrayView(RootObjects).Set(NetRefHandleManager->GetGlobalScopableInternalIndices(), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());

	auto PrintClassOrProtocol = [&](FInternalNetRefIndex ObjectIndex) -> FString
	{
		const FNetRefHandleManager::FReplicatedObjectData& NetObjectData = NetRefHandleManager->GetReplicatedObjectDataNoCheck(ObjectIndex);
		UObject* ObjectPtr = NetRefHandleManager->GetReplicatedObjectInstance(ObjectIndex);

		return FString::Printf(TEXT("Class %s"), ObjectPtr ? *(ObjectPtr->GetClass()->GetName()) : NetObjectData.Protocol->DebugName->Name);
	};
	
	TTuple<uint32, uint32> Stats(0,0);
	LogRootObjectListWithSubObjects(NetRefHandleManager, MakeNetBitArrayView(RootObjects), Stats, PrintClassOrProtocol);

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u sub objects"), Stats.Get<0>(), Stats.Get<1>());
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing ALL Replicated Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintRelevantObjects(
	TEXT("Net.Iris.PrintRelevantObjects"), 
	TEXT("Prints the list of netobjects currently relevant to any connection"), 
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	if (UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args))
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			ObjectBridge->PrintRelevantObjects();
		}
	}
}));

void UObjectReplicationBridge::PrintRelevantObjects() const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Relevant Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	uint32 TotalRootObjects = 0;
	uint32 TotalSubObjects = 0;

	FNetBitArray RootObjects;
	RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	MakeNetBitArrayView(RootObjects).Set(NetRefHandleManager->GetRelevantObjectsInternalIndices(), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());

	TTuple<uint32, uint32> Stats(0, 0);
	LogRootObjectListWithSubObjects(NetRefHandleManager, MakeNetBitArrayView(RootObjects), Stats, nullptr);
	

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u sub objects"), Stats.Get<0>(), Stats.Get<1>());
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Relevant Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintAlwaysRelevantObjects(
	TEXT("Net.Iris.PrintAlwaysRelevantObjects"),
	TEXT("Prints the list of netobjects always relevant to every connection"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	if (UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args))
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			ObjectBridge->PrintAlwaysRelevantObjects();
		}
	}
}));

void UObjectReplicationBridge::PrintAlwaysRelevantObjects() const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Always Relevant Objects ################"));
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	FNetBitArray AlwaysRelevantList;
	AlwaysRelevantList.Init(NetRefHandleManager->GetMaxActiveObjectCount());
	
	ReplicationSystemInternal->GetFiltering().BuildAlwaysRelevantList(MakeNetBitArrayView(AlwaysRelevantList), ReplicationSystemInternal->GetNetRefHandleManager().GetGlobalScopableInternalIndices());

	// Remove subobjects from the list.
	MakeNetBitArrayView(AlwaysRelevantList).Combine(NetRefHandleManager->GetSubObjectInternalIndicesView(), FNetBitArrayView::AndNotOp);

	TTuple<uint32, uint32> Stats(0, 0);
	LogRootObjectListWithSubObjects(NetRefHandleManager, MakeNetBitArrayView(AlwaysRelevantList), Stats, nullptr);

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("Printed %u root objects and %u sub objects"), Stats.Get<0>(), Stats.Get<1>());
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Always Relevant Objects ################"));
}

//-----------------------------------------------
FAutoConsoleCommand ObjectBridgePrintRelevantObjectsToConnection(
TEXT("Net.Iris.PrintRelevantObjectsToConnection"),
TEXT("Prints the list of replicated objects relevant to a specific connection"),
FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray< FString >& Args)
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UReplicationSystem* RepSystem = FindReplicationSystemFromArg(Args);
	if (RepSystem)
	{
		if (UObjectReplicationBridge* ObjectBridge = CastChecked<UObjectReplicationBridge>(RepSystem->GetReplicationBridge()))
		{
			FReplicationSystemInternal* ReplicationSystemInternal = RepSystem->GetReplicationSystemInternal();

			ObjectBridge->PrintRelevantObjectsForConnections(Args);
		}
	}
}));

void UObjectReplicationBridge::PrintRelevantObjectsForConnections(const TArray<FString>& Args) const
{
	using namespace UE::Net;
	using namespace UE::Net::Private;
	using namespace UE::Net::Private::ObjectBridgeDebugging;

	UE_LOG(LogIrisBridge, Display, TEXT("################ Start Printing Relevant Objects of %d Connections ################"), 1);
	UE_LOG(LogIrisBridge, Display, TEXT(""));

	FReplicationSystemInternal* ReplicationSystemInternal = GetReplicationSystem()->GetReplicationSystemInternal();
	FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();
	const FNetBitArray& ValidConnections = Connections.GetValidConnections();

	// Default to all connections
	FNetBitArray ConnectionsToPrint;
	ConnectionsToPrint.InitAndCopy(ValidConnections);

	// Filter down the list if users wanted specific connections
	TArray<uint32> RequestedConnectionList = FindConnectionsFromArgs(Args);
	if (RequestedConnectionList.Num())
	{
		ConnectionsToPrint.Reset();
		for (uint32 ConnectionId : RequestedConnectionList)
		{
			if (ValidConnections.IsBitSet(ConnectionId))
			{
				ConnectionsToPrint.SetBit(ConnectionId);
			}
			else
			{
				UE_LOG(LogIris, Warning, TEXT("UObjectReplicationBridge::PrintRelevantObjectsForConnections ConnectionId: %u is not valid"), ConnectionId);
			}
		}
	}

	ConnectionsToPrint.ForAllSetBits([&](uint32 ConnectionId)
	{
		UE_LOG(LogIrisBridge, Display, TEXT(""));
		UE_LOG(LogIrisBridge, Display, TEXT("###### Begin Relevant list of Connection:%u ######"), ConnectionId);
		UE_LOG(LogIrisBridge, Display, TEXT(""));

		struct FRootObjectData
		{
			FInternalNetRefIndex ObjectIndex = 0;
			UObject* Instance = nullptr;
			UClass* Class = nullptr;

			// Sort by class names
			bool operator<(const FRootObjectData& rhs) const 
			{ 
				if (Class==rhs.Class) { return false; }
				if (!Class) { return false; }
				if (!rhs.Class) { return true; }
				return Class->GetName() < rhs.Class->GetName();
			}
		};
		TArray<FRootObjectData> RelevantObjects;

		FNetBitArray RootObjects;
		RootObjects.Init(NetRefHandleManager->GetMaxActiveObjectCount());
		MakeNetBitArrayView(RootObjects).Set(GetReplicationSystem()->GetReplicationSystemInternal()->GetFiltering().GetRelevantObjectsInScope(ConnectionId), FNetBitArrayView::AndNotOp, NetRefHandleManager->GetSubObjectInternalIndicesView());
		
		RootObjects.ForAllSetBits([&](uint32 RootObjectIndex)
		{
			FRootObjectData Data;
			Data.ObjectIndex = RootObjectIndex;
			Data.Instance = NetRefHandleManager->GetReplicatedObjectInstance(RootObjectIndex);
			Data.Class = Data.Instance ? Data.Instance->GetClass() : nullptr;

			RelevantObjects.Emplace(MoveTemp(Data));
		});

		RelevantObjects.Sort();

		for (const FRootObjectData& ObjectData : RelevantObjects)
		{
			UE_LOG(LogIrisBridge, Display, TEXT("%s"), *PrintNetObject(NetRefHandleManager, ObjectData.ObjectIndex));

			// TODO: Output the subobjects and tell if they are relevant or not to the connection.
		}

		UE_LOG(LogIrisBridge, Display, TEXT(""));
		UE_LOG(LogIrisBridge, Display, TEXT("###### Stop Relevant list of Connection:%u | Total: %u root objects relevant ######"), ConnectionId, RelevantObjects.Num());
		UE_LOG(LogIrisBridge, Display, TEXT(""));
	});

	UE_LOG(LogIrisBridge, Display, TEXT(""));
	UE_LOG(LogIrisBridge, Display, TEXT("################ Stop Printing Relevant Objects of %d Connections ################"), 1);
}