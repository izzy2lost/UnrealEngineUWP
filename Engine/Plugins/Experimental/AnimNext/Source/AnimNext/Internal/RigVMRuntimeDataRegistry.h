// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"


namespace UE::AnimNext
{

struct FRigVMRuntimeData
{
	FRigVMExtendedExecuteContext Context;
};

using FRigVMRuntimeDataID = TObjectKey<URigVM>;

/**
	* RigVMRuntimeDataRegistry
	* 
	* A global registry of all existing VMs that require TLS data instantiation
	* 
	*/
struct ANIMNEXT_API FRigVMRuntimeDataRegistry final
{
	/**
	 * Finds the VM runtime data ID in the TLS storage
	 *
	 * @param RigVMRuntimeDataID The key to associate the value with.
	 * @return A pointer to the data as stored in the map. The data is only valid until the next change to any key in the map.
	 */
	static FRigVMRuntimeData* FindRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID);

	/**
	 * Adds VM runtime data for the passed ID
	 *
	 * @param RigVMRuntimeDataID The key to associate the value with.
	 * @param ReferenceContext Reference context to copy data from
	 * @return A pointer to the data as stored in the map. The data is only valid until the next change to any key in the map.
	 */
	static FRigVMRuntimeData* AddRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID, const FRigVMExtendedExecuteContext& ReferenceContext);

	
	/**
	 * Finds or adds the VM runtime data ID
	 *
	 * @param RigVMRuntimeDataID The key to associate the value with.
	 * @param ReferenceContext Reference context to copy data from (if not found)
	 * @return A pointer to the data as stored in the map. The data is only valid until the next change to any key in the map.
	 */
	static FRigVMRuntimeData* FindOrAddRuntimeData(const FRigVMRuntimeDataID& RigVMRuntimeDataID, const FRigVMExtendedExecuteContext& ReferenceContext);

private:

	// Post GC callback to signal a compactation has to be done (and perform it on game thread)
	static void HandlePostGarbageCollect();

	// Checks if any of the stored VM datas have been deleted and removes deleted elements.
	static void PerformStorageCompaction();

	// Module lifetime functions
	static void Init();
	static void Destroy();

	friend class FModule;
};

} // end namespace UE::AnimNext
