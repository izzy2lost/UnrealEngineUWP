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

	// Create a string token for the provided string
	IRISCORE_API FNetToken GetOrCreateToken(FName Name);

	// Resolve a local token
	IRISCORE_API FName ResolveToken(FNetToken Token) const;

	// Resolve a token received from remote
	IRISCORE_API FName ResolveRemoteToken(FNetToken Token, const FNetTokenStoreState& NetTokenStoreState) const;

protected:
	// Serialize data for a token, note there is not validation in this function
	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey TokenStoreKey) const override;

	// Read data for a token, returns a valid StoreKey if successful read
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context) override;

	// Create a persistent string
	FNetTokenStoreKey GetOrCreateTokenStoreKey(FName Name);

private:
	FNetTokenStore& TokenStore;
	TMap<FName, FNetTokenStoreKey> FNameToKey;
	TArray<FName> StoredFNames;
	TArray<FNetToken> StoredTokens;
};

}
