// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/MultiUserReplicationSubsystem.h"

#if WITH_CONCERT
#include "UObjectAdapterReplicationDiscoverer.h"
#include "IMultiUserClientModule.h"
#include "Replication/Data/ObjectReplicationMap.h"
#include "Replication/IMultiUserReplication.h"

#include "Algo/Transform.h"
#endif

bool UMultiUserReplicationSubsystem::IsReplicatingObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const
{
#if WITH_CONCERT
	const UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
	if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
	{
		return ReplicationInterface->IsReplicatingObject(ClientId, ObjectPath);
	}
#endif
	return false;
}

TArray<FConcertPropertyChainWrapper> UMultiUserReplicationSubsystem::GetPropertiesRegisteredToObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const
{
#if WITH_CONCERT  
	const UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
	if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
	{
		const FObjectReplicationMap* ObjectMap = ReplicationInterface->FindReplicationMapForClient(ClientId);
		if (!ObjectMap)
		{
			return {};
		}

		const FReplicatedObjectInfo* ObjectInfo = ObjectMap->ReplicatedObjects.Find(ObjectPath);
		if (!ObjectInfo)
		{
			return {};
		}
		
		TArray<FConcertPropertyChainWrapper> Result;
		Algo::Transform(ObjectInfo->PropertySelection.ReplicatedProperties, Result, [](const FConcertPropertyChain& PropertyChain)
		{
			return FConcertPropertyChainWrapper{ PropertyChain }; 
		});
		return Result;
	}
#endif
	return {};
}

TArray<FSoftObjectPath> UMultiUserReplicationSubsystem::GetRegisteredObjects(const FGuid& ClientId) const
{
#if WITH_CONCERT
	const UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
	if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
	{
		const FObjectReplicationMap* ObjectMap = ReplicationInterface->FindReplicationMapForClient(ClientId);
		if (!ObjectMap)
		{
			return {};
		}
		
		TArray<FSoftObjectPath> Result;
		ObjectMap->ReplicatedObjects.GenerateKeyArray(Result);
		return Result;
	}
#endif
	return {};
}

TArray<FSoftObjectPath> UMultiUserReplicationSubsystem::GetReplicatedObjects(const FGuid& ClientId) const
{
#if WITH_CONCERT
	const UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
	if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
	{
		const FObjectReplicationMap* ObjectMap = ReplicationInterface->FindReplicationMapForClient(ClientId);
		if (!ObjectMap)
		{
			return {};
		}

		TArray<FSoftObjectPath> Result;
		for (const TPair<FSoftObjectPath, FReplicatedObjectInfo>& Pair : ObjectMap->ReplicatedObjects)
		{
			if (ReplicationInterface->IsReplicatingObject(ClientId, Pair.Key))
			{
				Result.Add(Pair.Key);
			}
		}
		return Result;
	}
#endif
	return {};
}

void UMultiUserReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);  

#if WITH_CONCERT
	UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
	if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
	{
		UObjectAdapter = MakeShared<UE::MultiUserClientLibrary::FUObjectAdapterReplicationDiscoverer>();
		ReplicationInterface->RegisterReplicationDiscoverer(UObjectAdapter.ToSharedRef());
	}
#endif
}

void UMultiUserReplicationSubsystem::Deinitialize()
{
	Super::Deinitialize();

#if WITH_CONCERT
	if (IMultiUserClientModule::IsAvailable())
	{
		UE::MultiUserClient::IMultiUserReplication* ReplicationInterface = IMultiUserClientModule::Get().GetReplication();
		if (ensureMsgf(ReplicationInterface, TEXT("We expected it to always be valid.")))
		{
			ReplicationInterface->RemoveReplicationDiscoverer(UObjectAdapter.ToSharedRef());
			UObjectAdapter.Reset();
		}
	}
#endif
}
