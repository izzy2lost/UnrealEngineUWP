// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/CoreNet.h"
#include "Iris/ReplicationSystem/NetToken.h"
#include "IrisObjectReferencePackageMap.generated.h"

// Forward declarations
class FNetworkGUID;

namespace UE::Net
{
	class FNetTokenStoreState;
	class FNetTokenStore;

	struct FNetTokenExportContext
	{
		FNetTokenStore* TokenStore = nullptr;
		FNetTokenStoreState* RemoteState = nullptr;		
	};

	// In order to properly capture exported data when calling in to old style NetSerialize methods
	// we need to capture and inject certain types.
	struct FIrisPackageMapExports
	{
		typedef TArray<TObjectPtr<UObject>, TInlineAllocator<4>> FObjectReferenceArray;
		typedef TArray<FName, TInlineAllocator<4>> FNameArray;
		typedef TArray<UE::Net::FNetToken, TInlineAllocator<4>> FNetTokensArray;

		void Reset()
		{
			References.Reset();
			Names.Reset();
			NetTokens.Reset();
		}

		FObjectReferenceArray References;
		FNameArray Names;
		FNetTokensArray NetTokens;
	};
}

/**
 * Custom packagemap implementation used to be able to capture UObject* references from external serialization.
 * Any object references written when using this packagemap will be added to the References array and serialized as an index.
 * When reading using this packagemap references will be read as an index and resolved by picking the corresponding entry from the provided array containing the references.
 */
UCLASS(transient, MinimalAPI)
class UIrisObjectReferencePackageMap : public UPackageMap
{
public:
	GENERATED_BODY()

	typedef TArray<TObjectPtr<UObject>, TInlineAllocator<4>> FObjectReferenceArray;
	typedef TArray<FName, TInlineAllocator<4>> FNameArray;
	typedef TArray<UE::Net::FNetToken, TInlineAllocator<4>> FNetTokensArray;

	// We override SerializeObject in order to be able to capture object references
	virtual bool SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID) override;

	// Override SerializeName in order to be able to capture name and serialize them with iris instead.
	virtual bool SerializeName(FArchive& Ar, FName& InName);

	// Allow serialization of NetTokens to be able to support export of custom token types from existing NetSerialize implementations.
	IRISCORE_API bool SerializeNetToken(FArchive& Ar, UE::Net::FNetToken& InNetToken);

	// Init for read, we need a reference array to be able to resolve references
	IRISCORE_API void InitForRead(const UE::Net::FIrisPackageMapExports* PackageMapExports);

	// Init for write, all captured exports will be serialized as in index and added to the PackageMapExports for later export using iris.
	IRISCORE_API void InitForWrite(UE::Net::FIrisPackageMapExports* PackageMapExports);

	// Gets NetTokenExportContext to allow access to NetTokenStore
	UE::Net::FNetTokenExportContext* GetNetTokenExportContext() { return &NetTokenExportContext; }

	// Gets NetTokenExportContext to allow access to NetTokenStore
	const UE::Net::FNetTokenExportContext* GetNetTokenExportContext() const { return &NetTokenExportContext; }

protected:
	UE::Net::FIrisPackageMapExports* PackageMapExports = nullptr;
	UE::Net::FNetTokenExportContext NetTokenExportContext;
};
