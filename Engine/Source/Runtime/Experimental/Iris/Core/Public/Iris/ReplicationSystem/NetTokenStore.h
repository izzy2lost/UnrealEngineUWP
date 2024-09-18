// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Net/Core/NetToken/NetToken.h"
#include "UObject/NameTypes.h"
#include "Containers/Array.h"
#include "Templates/Tuple.h"

/** 
* The idea with NetTokens is to allow export of "stable"-pieces of data such as string and names by replacing them with a NetToken during quantization/serialization.
* The data associated with a NetToken is then exported separately from the data. As soon as the exported data has been acknowledged the data will only be serialized using the NetToken.
* 
* As both Client and Server can communicate NetTokens, each side can end up assigning different tokens that will differe from each other.
* Here is a high-level overview of the algorithm which works slightly differently based on if using Iris replication or the old replication system.
*
* The sending side looks-up or creates a NetToken for the Data being serialized. Servers will mark assigned NetTokens as authoritive, while clients generate a temporary NetToken.
* When a network bunch/batch that contains NetToken is being sent, there is a per-connection look-up to see if we need to append and serialize exports or not, if the data associated with the token
* has been acknowledged the token will not be exported again.
*
* On the receiving side, imported exports are always guaranteed to have been processed before we attempt to read received data containing NetTokens which allows
* the receiving side to resolve the NetToken to get the actual data.
*
* The implmentaion details differs a bit depending on if we are using iris replication or old style replication.
*
* A current example that us used by both systems is GameplayTags, For Iris: See GameplayTagNetSerializer.cpp, for the old replication system: See: GameplayTagContainer.cpp.
*
* Note: this is still an experimental feature and is currently only available if compiling UE_WITH_IRIS. 
* Implementation datails is subject to change.
*/

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
	IRISCORE_API virtual ~FNetTokenDataStore();

protected:
	IRISCORE_API FNetTokenDataStore();

	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey Key) const = 0;
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) = 0;

	virtual void WriteTokenData(FArchive& Ar, FNetTokenStoreKey Key) const = 0;
	virtual FNetTokenStoreKey ReadTokenData(FArchive& Ar, const FNetToken& NetToken) = 0;

	FNetTokenStoreKey GetTokenKey(FNetToken Token, const FNetTokenStoreState& TokenStoreState) const;
	inline FNetToken::FTypeId GetTypeId() const { return TypeId; }

	// Create new NetToken
	IRISCORE_API FNetToken CreateToken(FNetToken::ENetTokenAuthority Authority, FNetTokenStoreKey Key, FNetTokenStoreState& TokenStoreState);

	// Make NetTokenStoreKey
	IRISCORE_API static FNetTokenStoreKey MakeNetTokenStoreKey(FNetToken::FTypeId TokenTypeId, uint32 TokenKeyValue);

private:
	friend FNetTokenStore;
	FNetToken::FTypeId TypeId;
};

// This is the token store, we currently have one per ReplicationSystem but it is possible we will share this across game instance.
class FNetTokenStore
{
	UE_NONCOPYABLE(FNetTokenStore);
public:
	IRISCORE_API FNetTokenStore();
	IRISCORE_API ~FNetTokenStore();

	/** External configuration variables used to initialize the NetTokenStore */
	struct FInitParams
	{
		FNetToken::ENetTokenAuthority Authority;
		uint32 MaxConnections = 256;
	};
	IRISCORE_API void Init(FInitParams& InitParams);

	bool IsAuthority() const { return Params.Authority == FNetToken::ENetTokenAuthority::Authority; }

	// A token is local if the authority of the NetTokenStore and the token matches, Invalid tokens are always local.
	bool IsLocalToken(const FNetToken NetToken) const { return !NetToken.IsValid() || IsAuthority() == NetToken.IsAssignedByAuthority(); }

	// Register DataStore and return true if it was registered, 
	IRISCORE_API bool RegisterDataStore(TUniquePtr<FNetTokenDataStore> DataStore, FName TokenStoreName);

	IRISCORE_API const FNetTokenDataStore* GetDataStore(FName Name) const;
	IRISCORE_API FNetTokenDataStore* GetDataStore(FName Name);

	/** Create and return data store of specified type, it will be owned by NetTokenStore */
	template<typename T>
	bool CreateAndRegisterDataStore()
	{
		TUniquePtr<FNetTokenDataStore> NetTokenDataStore(MakeUnique<T>(*this));
		
		return RegisterDataStore(MakeUnique<T>(*this), T::GetTokenStoreName());
	}

	/** Return data store of specified type. */
	template<typename T>
	T* GetDataStore() { return static_cast<T*>(GetDataStore(T::GetTokenStoreName())); }

	/** Return data store of specified type. */
	template<typename T>
	const T* GetDataStore() const { return static_cast<const T*>(GetDataStore(T::GetTokenStoreName())); }

	const FNetTokenStoreState* GetLocalNetTokenStoreState() const { return LocalNetTokenStoreState.Get(); }
	FNetTokenStoreState* GetLocalNetTokenStoreState() { return LocalNetTokenStoreState.Get();; }

	/** Init RemoteNetTokenStoreState for given ConnectionId, if it already exists it will be reset. */
	IRISCORE_API void InitRemoteNetTokenStoreState(uint32 ConnectionId);

	/** Get RemoteNetTokenStoreState for given ConnectionId. */
	IRISCORE_API const FNetTokenStoreState* GetRemoteNetTokenStoreState(uint32 ConnectionId) const;

	/** Get RemoteNetTokenStoreState for given ConnectionId. */
	IRISCORE_API FNetTokenStoreState* GetRemoteNetTokenStoreState(uint32 ConnectionId);

	// Write data associated with the NetToken
	IRISCORE_API void WriteTokenData(FNetSerializationContext& Context, const FNetToken NetToken) const;

	// Read data associated with the NetToken
	IRISCORE_API void ReadTokenData(FNetSerializationContext& Context, const FNetToken NetToken, FNetTokenStoreState& RemoteNetTokenStoreState);

	// Write data associated with the NetToken
	IRISCORE_API void WriteTokenData(FArchive& Ar, const FNetToken NetToken) const;

	// Read data associated with the NetToken
	IRISCORE_API void ReadTokenData(FArchive& Ar, const FNetToken NetToken, FNetTokenStoreState& RemoteNetTokenStoreState);

	// Conditionally write NetTokenData unless already exported
	IRISCORE_API void ConditionalWriteNetTokenData(FNetSerializationContext& Context, Private::FNetExportContext* ExportContext, const FNetToken NetToken) const;

	// Conditionally read NetTokenData if exported
	IRISCORE_API void ConditionalReadNetTokenData(FNetSerializationContext& Context, const FNetToken NetToken);

	// Utility methods, consolidate with other changes to NetTokenStore as next step.
	IRISCORE_API static void AppendExportOrWriteInlinedExportData(FNetSerializationContext&, FNetToken NetToken);
	IRISCORE_API static void ReadInlinedExportData(FNetSerializationContext&, FNetToken NetToken);

private:
	friend UNetTokenDataStream;
	friend FNetTokenDataStore;

	TUniquePtr<FNetTokenStoreState> LocalNetTokenStoreState;
	TArray<TUniquePtr<FNetTokenStoreState>> RemoteNetTokenStoreStates;
	TArray<TTuple<FName, TUniquePtr<FNetTokenDataStore>>> TokenDataStores;
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

// Read and write tokens
IRISCORE_API FNetToken ReadNetToken(UE::Net::FNetSerializationContext& Context);
IRISCORE_API void WriteNetToken(UE::Net::FNetSerializationContext& Context, FNetToken Token);

IRISCORE_API FNetToken ReadNetToken(FArchive& Ar);
IRISCORE_API void WriteNetToken(FArchive& Ar, FNetToken Token);

}
