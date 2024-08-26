// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "NetToken.h"
#include "UObject/NameTypes.h"
#include "Containers/Array.h"
#include "Templates/Tuple.h"

class UNetTokenDataStream;
namespace UE::Net
{
	class FNetTokenDataStore;
	class FNetTokenStore;
	class FNetTokenStoreState;
	class FNetSerializationContext;

	namespace Private
	{
		class FNetExportContext;
	}
}

namespace UE::Net
{

// virtual interface for NetTokenDataStores
class FNetTokenDataStore
{
public:
	virtual ~FNetTokenDataStore();

protected:
	FNetTokenDataStore();

	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey Key) const = 0;
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) = 0;

	FNetTokenStoreKey GetTokenKey(FNetToken Token, const FNetTokenStoreState& TokenStoreState) const;
	inline FNetToken::FTypeId GetTypeId() const { return TypeId; }

	// Create new NetToken
	FNetToken CreateToken(FNetToken::ENetTokenAuthority Authority, FNetTokenStoreKey Key, FNetTokenStoreState& TokenStoreState);

	// Make NetTokenStoreKey
	static FNetTokenStoreKey MakeNetTokenStoreKey(FNetToken::FTypeId TokenTypeId, uint32 TokenKeyValue);

private:
	friend FNetTokenStore;
	FNetToken::FTypeId TypeId;
};

// This is the token store, we currently have one per ReplicationSystem but it is possible we will share this across game instance.
class FNetTokenStore
{
	UE_NONCOPYABLE(FNetTokenStore);
public:
	FNetTokenStore();
	~FNetTokenStore();

	/** External configuration variables used to initialize the NetTokenStore */
	struct FInitParams
	{
		FNetToken::ENetTokenAuthority Authority;
	};
	void Init(FInitParams& InitParams);

	bool IsAuthority() const { return Params.Authority == FNetToken::ENetTokenAuthority::Authority; }

	// A token is local if the authority of the NetTokenStore and the token matches, Invalid tokens are always local.
	bool IsLocalToken(const FNetToken NetToken) const { return !NetToken.IsValid() || IsAuthority() == NetToken.IsAssignedByAuthority(); }

	// Register DataStore and return true if it was registered.
	bool RegisterDataStore(FNetTokenDataStore* DataStore, FName TokenStoreName);

	IRISCORE_API const FNetTokenDataStore* GetDataStore(FName Name) const;
	IRISCORE_API FNetTokenDataStore* GetDataStore(FName Name);

	/** Return data store of specified type. */
	template<typename T>
	T* GetDataStore() { return static_cast<T*>(GetDataStore(T::GetTokenStoreName())); }

	/** Return data store of specified type. */
	template<typename T>
	const T* GetDataStore() const { return static_cast<const T*>(GetDataStore(T::GetTokenStoreName())); }

	const FNetTokenStoreState* GetLocalNetTokenStoreState() const { return LocalNetTokenStoreState; }
	FNetTokenStoreState* GetLocalNetTokenStoreState() { return LocalNetTokenStoreState; }

	// Write data associated with the NetToken
	void WriteTokenData(FNetSerializationContext& Context, const FNetToken NetToken) const;

	// Read data associated with the NetToken
	void ReadTokenData(FNetSerializationContext& Context, const FNetToken NetToken, FNetTokenStoreState& RemoteNetTokenStoreState);

	// Conditionally write NetTokenData unless already exported
	void ConditionalWriteNetTokenData(FNetSerializationContext& Context, Private::FNetExportContext* ExportContext, const FNetToken NetToken) const;

	// Conditionally read NetTokenData if exported
	void ConditionalReadNetTokenData(FNetSerializationContext& Context, const FNetToken NetToken);

	// Utility methods, consolidate with other changes to NetTokenStore as next step.
	IRISCORE_API static void AppendExportOrWriteInlinedExportData(FNetSerializationContext&, FNetToken NetToken);
	IRISCORE_API static void ReadInlinedExportData(FNetSerializationContext&, FNetToken NetToken);

private:
	friend UNetTokenDataStream;
	friend FNetTokenDataStore;

	FNetTokenStoreState* LocalNetTokenStoreState;
	TArray<TTuple<FName, FNetTokenDataStore*> > TokenDataStores;
	FInitParams Params;
};

inline FNetTokenStoreKey FNetTokenDataStore::MakeNetTokenStoreKey(FNetToken::FTypeId TokenTypeId, uint32 TokenKeyValue)
{
	check(TokenTypeId < FNetToken::MaxTypeIdCount);
	check(TokenKeyValue < FNetToken::MaxNetTokenCount)

	FNetTokenStoreKey TokenKey;
	TokenKey.TypeId = TokenTypeId;
	TokenKey.Key = TokenKeyValue;

	return TokenKey;
}


}
