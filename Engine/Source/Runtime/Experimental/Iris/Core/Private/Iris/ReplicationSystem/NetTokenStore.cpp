// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Iris/ReplicationSystem/NetTokenStoreState.h"
#include "Iris/ReplicationSystem/ObjectReferenceCacheFwd.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetExportContext.h"
#include "Net/Core/Trace/NetTrace.h"

namespace UE::Net
{

FNetTokenDataStore::FNetTokenDataStore()
: TypeId(FNetToken::InvalidTokenTypeId)
{
}

FNetTokenDataStore::~FNetTokenDataStore()
{
}

FNetTokenStoreKey FNetTokenDataStore::GetTokenKey(FNetToken Token, const FNetTokenStoreState& TokenStoreState) const
{
	if (Token.GetIndex() < (uint32)TokenStoreState.TokenInfos.Num())
	{
		const FNetTokenStoreKey& Key = TokenStoreState.TokenInfos[Token.GetIndex()];
		//if (ensureAlwaysMsgf(Key.IsValid() && GetTypeId() == Key.GetTypeId(), TEXT("Cannot resolve NetToken %s with TypeId: %u in DataStore with TypeId: %u"), *Token.ToString(), Key.GetTypeId(), GetTypeId()))
		if (ensureMsgf(Key.IsValid() && GetTypeId() == Key.GetTypeId(), TEXT("Cannot resolve NetToken %s with TypeId: %u in DataStore with TypeId: %u"), *Token.ToString(), Key.GetTypeId(), GetTypeId()))
		{
			return Key;
		}
	}

	return FNetTokenStoreKey();
}

FNetToken FNetTokenDataStore::CreateToken(FNetToken::ENetTokenAuthority Authority, FNetTokenStoreKey Key, FNetTokenStoreState& TokenStoreState)
{
	const uint32 NextTokenIndex = TokenStoreState.TokenInfos.Num();

	if (!ensure(NextTokenIndex < FNetToken::MaxNetTokenCount))
	{
		return FNetToken();
	}

	// Store token info
	TokenStoreState.TokenInfos.Add(Key);

	return FNetToken::MakeNetToken(NextTokenIndex, Authority);
}

FNetToken ReadNetToken(UE::Net::FNetSerializationContext& Context)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	FNetToken ReadToken;

	UE_NET_TRACE_NAMED_DYNAMIC_NAME_SCOPE(TokenScope, FName(), *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);
	const uint32 TokenIndex = ReadPackedUint32(Reader);
	if (const bool bIsValid = (TokenIndex != FNetToken::InvalidTokenIndex))
	{
		const bool bIsAssignedByAuthority = Reader->ReadBool();
		if (!Reader->IsOverflown())
		{
			ReadToken = FNetToken::MakeNetToken(TokenIndex, bIsAssignedByAuthority ? FNetToken::ENetTokenAuthority::Authority : FNetToken::ENetTokenAuthority::None);
			UE_NET_TRACE_SET_SCOPE_NAME(TokenScope, *ReadToken.ToString());
		}
	}

	return ReadToken;
}

// Note: Be cereful when modifying this methods to not affect replay compatibility.
void WriteNetToken(UE::Net::FNetSerializationContext& Context, FNetToken Token)
{
	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(*Token.ToString(), *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	const uint32 TokenIndex = Token.GetIndex();
	WritePackedUint32(Writer, TokenIndex);
	if (TokenIndex != FNetToken::InvalidTokenIndex)
	{
		Writer->WriteBool(Token.IsAssignedByAuthority());
	}
}

FNetToken ReadNetToken(FArchive& Ar)
{
	FNetToken ReadToken;

	uint32 TokenIndex = 0;
	Ar.SerializeIntPacked(TokenIndex);

	if (const bool bIsValid = (TokenIndex != FNetToken::InvalidTokenIndex))
	{
		bool bIsAssignedByAuthority = false;
		Ar.SerializeBits(&bIsAssignedByAuthority, 1);

		if (!Ar.IsError())
		{
			ReadToken = FNetToken::MakeNetToken(TokenIndex, bIsAssignedByAuthority ? FNetToken::ENetTokenAuthority::Authority : FNetToken::ENetTokenAuthority::None);
		}
	}

	return ReadToken;
}

// Note: Be cereful when modifying this methods to not affect replay compatibility.
void WriteNetToken(FArchive& Ar, FNetToken Token)
{
	uint32 TokenIndex = Token.GetIndex();
	Ar.SerializeIntPacked(TokenIndex);
	if (TokenIndex != FNetToken::InvalidTokenIndex)
	{
		bool bIsAssignedByAuthority = Token.IsAssignedByAuthority();
		Ar.SerializeBits(&bIsAssignedByAuthority, 1);
	}
}

FNetTokenStore::FNetTokenStore()
: LocalNetTokenStoreState(new FNetTokenStoreState)
{
}

FNetTokenStore::~FNetTokenStore()
{
}

void FNetTokenStore::Init(FNetTokenStore::FInitParams& InParams)
{
	Params = InParams;
	RemoteNetTokenStoreStates.SetNum(InParams.MaxConnections);
}

void FNetTokenStore::InitRemoteNetTokenStoreState(uint32 ConnectionId)
{
	if (ensureMsgf((ConnectionId != InvalidConnectionId) && (ConnectionId < (uint32)RemoteNetTokenStoreStates.Num()), TEXT("Trying to init RemoteNetTokenStoreState for invalid connection %u"), ConnectionId))
	{
		if (FNetTokenStoreState* ExistingState = RemoteNetTokenStoreStates[ConnectionId].Get())
		{
			ExistingState->ReserveTokenCount(0U);

		}
		else
		{
			RemoteNetTokenStoreStates[ConnectionId] = MakeUnique<FNetTokenStoreState>();
		}
	}
}

const FNetTokenStoreState* FNetTokenStore::GetRemoteNetTokenStoreState(uint32 ConnectionId) const
{
	if (!ensureMsgf((ConnectionId != InvalidConnectionId) && (ConnectionId < (uint32)RemoteNetTokenStoreStates.Num()), TEXT("Trying to access RemoteNetTokenStoreState for ConnectionID: %u"), ConnectionId))
	{
		return nullptr;
	}
	return RemoteNetTokenStoreStates[ConnectionId].Get();
}

FNetTokenStoreState* FNetTokenStore::GetRemoteNetTokenStoreState(uint32 ConnectionId)
{
	if (!ensureMsgf((ConnectionId != InvalidConnectionId) && (ConnectionId < (uint32)RemoteNetTokenStoreStates.Num()), TEXT("Trying to access non existing RemoteNetTokenStoreState for ConnectionId: %u"), ConnectionId))
	{
		return nullptr;
	}
	return RemoteNetTokenStoreStates[ConnectionId].Get();
}

const FNetTokenDataStore* FNetTokenStore::GetDataStore(FName Name) const
{
	const TTuple<FName, TUniquePtr<FNetTokenDataStore>>* Entry = TokenDataStores.FindByPredicate([Name](const TTuple<FName, TUniquePtr<FNetTokenDataStore>>& Entry){ return Name == Entry.Get<0>(); });
	return Entry ? Entry->Get<1>().Get() : nullptr;
}

 FNetTokenDataStore* FNetTokenStore::GetDataStore(FName Name)
{
	TTuple<FName, TUniquePtr<FNetTokenDataStore>>* Entry = TokenDataStores.FindByPredicate([Name](TTuple<FName, TUniquePtr<FNetTokenDataStore>>& Entry){ return Name == Entry.Get<0>(); });
	return Entry ? Entry->Get<1>().Get() : nullptr;
}

bool FNetTokenStore::RegisterDataStore(TUniquePtr<FNetTokenDataStore> DataStore, FName TokenStoreName)
{
	if (TokenDataStores.Num() >= FNetToken::MaxTypeIdCount)
	{
		return false;
	}

	if (!DataStore)
	{
		return false;
	}

	if (!ensure(GetDataStore(TokenStoreName) == nullptr))
	{
		// Already registered
		return false;
	}

	if (!ensure(DataStore->GetTypeId() == FNetToken::InvalidTokenTypeId))
	{
		// Already registered
		return false;
	}

	DataStore->TypeId = TokenDataStores.Num();
	TokenDataStores.Emplace(TokenStoreName, MoveTemp(DataStore));

	return true;
}

void FNetTokenStore::WriteTokenData(FNetSerializationContext& Context, const FNetToken NetToken) const
{
	if (NetToken.IsValid())
	{
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		// Write type
		const FNetTokenStoreKey& TokenKey = LocalNetTokenStoreState->TokenInfos[NetToken.GetIndex()];
		Writer->WriteBits(TokenKey.TypeId, FNetToken::TokenTypeIdBits);

		// Write token data
		TokenDataStores[TokenKey.TypeId].Get<1>().Get()->WriteTokenData(Context, TokenKey);
	}
}

void FNetTokenStore::WriteTokenData(FArchive& Ar, const FNetToken NetToken) const
{
	if (NetToken.IsValid())
	{
		// Write type
		const FNetTokenStoreKey TokenKey = LocalNetTokenStoreState->TokenInfos[NetToken.GetIndex()];
		uint32 TypeId = TokenKey.TypeId;

		Ar.SerializeBits(&TypeId, FNetToken::TokenTypeIdBits);

		// Write token data
		TokenDataStores[TokenKey.TypeId].Get<1>()->WriteTokenData(Ar, TokenKey);
	}
}

void FNetTokenStore::ReadTokenData(FNetSerializationContext& Context, const FNetToken NetToken, FNetTokenStoreState& RemoteNetTokenStoreState)
{
	if (NetToken.IsValid())
	{
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		// TODO: Guard this better, a map might be better after-all!
		// Also need to protect this if we process inbound data in parallel.
		if (!RemoteNetTokenStoreState.ReserveTokenCount(NetToken.GetIndex() + 1))
		{
			Reader->DoOverflow();
			return;
		}

		// Read type
		FNetToken::FTypeId TokenTypeId = (FNetToken::FTypeId)Reader->ReadBits(FNetToken::TokenTypeIdBits);

		// Validate that we managed to read and verify the type
		if (Reader->IsOverflown() || TokenTypeId >= (uint32)TokenDataStores.Num() )
		{
			Reader->DoOverflow();
			return;
		}

		const FNetTokenStoreKey StoreKey = TokenDataStores[TokenTypeId].Get<1>()->ReadTokenData(Context, NetToken);

		// Validate that we managed to read the data for the key
		if (Reader->IsOverflown() || !StoreKey.IsValid())
		{
			Reader->DoOverflow();
			return;
		}

		// Since the same tokendata might be exported multiple times we better validate that it is the same data
		const FNetTokenStoreKey& ExistingStoreKey = RemoteNetTokenStoreState.TokenInfos[NetToken.GetIndex()];
		if (!ensureAlways(!ExistingStoreKey.IsValid() || StoreKey == ExistingStoreKey))
		{
			Reader->DoOverflow();
			return;
		}
	
		// Store
		RemoteNetTokenStoreState.TokenInfos[NetToken.GetIndex()] = StoreKey;
	}
}

void FNetTokenStore::ReadTokenData(FArchive& Ar, const FNetToken NetToken, FNetTokenStoreState& RemoteNetTokenStoreState)
{
	if (NetToken.IsValid())
	{
		// TODO: Guard this better, a map might be better after-all!
		// Also need to protect this if we process inbound data in parallel.
		if (!RemoteNetTokenStoreState.ReserveTokenCount(NetToken.GetIndex() + 1))
		{
			Ar.SetError();
			return;
		}

		// Read type
		FNetToken::FTypeId TokenTypeId = 0U;
		Ar.SerializeBits(&TokenTypeId, FNetToken::TokenTypeIdBits);

		// Validate that we managed to read and verify the type
		if (Ar.IsError() || TokenTypeId >= (uint32)TokenDataStores.Num() )
		{
			Ar.SetError();
			return;
		}

		const FNetTokenStoreKey StoreKey = TokenDataStores[TokenTypeId].Get<1>()->ReadTokenData(Ar, NetToken);

		// Validate that we managed to read the data for the key
		if (Ar.IsError() || !StoreKey.IsValid())
		{
			Ar.SetError();
			return;
		}

		// Since the same tokendata might be exported multiple times we better validate that it is the same data
		const FNetTokenStoreKey& ExistingStoreKey = RemoteNetTokenStoreState.TokenInfos[NetToken.GetIndex()];
		if (!ensureAlways(!ExistingStoreKey.IsValid() || StoreKey == ExistingStoreKey))
		{
			Ar.SetError();
			return;
		}
	
		// Store
		RemoteNetTokenStoreState.TokenInfos[NetToken.GetIndex()] = StoreKey;
	}
}


void FNetTokenStore::ConditionalWriteNetTokenData(FNetSerializationContext& Context, Private::FNetExportContext* ExportContext, const FNetToken NetToken) const
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	// We should not try to export tokens received from remote
	if (IsAuthority() != NetToken.IsAssignedByAuthority())
	{
		Writer->WriteBool(false);
		return;
	}

	if (ExportContext)
	{
		if (Writer->WriteBool(!ExportContext->IsExported(NetToken)))
		{
			WriteTokenData(Context, NetToken);
			ExportContext->AddExported(NetToken);			
		}
	}
	else
	{
		Writer->WriteBool(true);
		WriteTokenData(Context, NetToken);
	}
}

void FNetTokenStore::ConditionalReadNetTokenData(FNetSerializationContext& Context, const FNetToken NetToken)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	const bool bIsExportToken = Reader->ReadBool();
	if (bIsExportToken)
	{
		if (Reader->IsOverflown())
		{
			return;
		}

		FNetObjectResolveContext& ResolveContext = Context.GetInternalContext()->ResolveContext;
	
		ReadTokenData(Context, NetToken, *ResolveContext.RemoteNetTokenStoreState);
	}
}

void FNetTokenStore::AppendExportOrWriteInlinedExportData(FNetSerializationContext& Context, FNetToken NetToken)
{
	using namespace UE::Net::Private;

	FNetExportContext* ExportContext = Context.GetExportContext();
	const FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();
	
	if (InternalContext->bInlineObjectReferenceExports == 0U)
	{
		ExportContext->AddPendingExport(NetToken);
	}
	else
	{
		FNetTokenStore* NetTokenStore = Context.GetNetTokenStore();
		NetTokenStore->ConditionalWriteNetTokenData(Context, ExportContext, NetToken);
	}
}

void FNetTokenStore::ReadInlinedExportData(FNetSerializationContext& Context, FNetToken NetToken)
{
	using namespace UE::Net::Private;

	const FInternalNetSerializationContext* InternalContext = Context.GetInternalContext();

	// Conditionally import if using inlined exports
	if (InternalContext->bInlineObjectReferenceExports == 1U)
	{
		FNetTokenStore* NetTokenStore = Context.GetNetTokenStore();
		NetTokenStore->ConditionalReadNetTokenData(Context, NetToken);
	}
}

}
