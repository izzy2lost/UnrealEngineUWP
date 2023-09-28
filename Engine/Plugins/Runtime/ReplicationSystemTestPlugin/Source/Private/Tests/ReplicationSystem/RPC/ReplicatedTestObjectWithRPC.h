// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tests/ReplicationSystem/ReplicatedTestObject.h"

#include "ReplicatedTestObjectWithRPC.generated.h"

/**
 *  A test class for testing RPC replication
 */
UCLASS()
class UTestReplicatedObjectWithRPC : public UReplicatedTestObject
{
	GENERATED_BODY()

public:
	UTestReplicatedObjectWithRPC();

	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;


	UReplicationSystem* ReplicationSystem = nullptr;

	// Network data only for test
	TArray<UE::Net::FReplicationFragment*> ReplicationFragments;

	bool bIsServerObject = false;

	// RPC test functions
	UFUNCTION(Reliable, Client)
	void ClientRPC();
	bool bClientRPCCalled = false;

	UFUNCTION(Reliable, Client)
	void ClientRPCWithParam(int32 IntParam);
	int32 ClientRPCWithParamCalled = 0;

	UFUNCTION(Reliable, Server)
	void ServerRPC();
	bool bServerRPCCalled = false;

	UFUNCTION(Reliable, Server)
	void ServerRPCWithParam(int32 IntParam);
	int32 ServerRPCWithParamCalled = 0;

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_MultiCastRPCSendImmediate();
	int32 NetMulticast_MultiCastRPCSendImmediateCallOrder = 0;

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_MultiCastRPC();
	int32 NetMulticast_MultiCastRPCCallOrder = 0;

	int32 CallOrder = 0;

};

