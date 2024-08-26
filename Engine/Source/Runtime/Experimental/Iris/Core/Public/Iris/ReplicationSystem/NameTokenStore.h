// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Misc/MemStack.h"

namespace UE::Net
{

// Simple token store used to store string tokens
// When the PackageMapRefactor is complete we will most likely rely on NetTagManager for persistent storage
class FNameTokenStore : public FNetTokenDataStore
{
	UE_NONCOPYABLE(FNameTokenStore);
public:
	explicit FNameTokenStore(FNetTokenStore& TokenStore);

	// Create a NetToken for the provided name
	IRISCORE_API FNetToken GetOrCreateToken(FName Name);

	// Resolve NetToken, to resolve remote tokens RemoteTokenStoreState must be valid
	IRISCORE_API FName ResolveToken(FNetToken Token, const FNetTokenStoreState* RemoteTokenStoreState = nullptr) const;	

	static FName GetTokenStoreName() { return NameTokenStoreName; }

protected:
	// Serialize data for a token, note there is not validation in this function
	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey TokenStoreKey) const override;

	// Read data for a token, returns a valid StoreKey if successful read
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) override;

	// Create a persistent string
	FNetTokenStoreKey GetOrCreateTokenStoreKey(FName Name);

private:
	inline static FName NameTokenStoreName = TEXT("NameTokenStore");

	FNetTokenStore& TokenStore;
	TMap<FName, FNetTokenStoreKey> FNameToKey;
	TArray<FName> StoredFNames;
	TArray<FNetToken> StoredTokens;
};

}
