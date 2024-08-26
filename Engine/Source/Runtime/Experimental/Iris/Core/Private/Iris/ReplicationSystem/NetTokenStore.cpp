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
		if (ensureAlwaysMsgf(GetTypeId() == Key.GetTypeId(), TEXT("Cannot resolve NetToken %s with TypeId: %u in DataStore with TypeId: %u"), *Token.ToString(), Key.GetTypeId(), GetTypeId()))
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
	UE_NET_TRACE_NAMED_DYNAMIC_NAME_SCOPE(TokenScope, FName(), *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	const uint32 Index = ReadPackedUint32(Reader);
	const bool bIsAssignedByAuthority = Reader->ReadBool();

	FNetToken ReadToken;
	
	if (!Reader->IsOverflown())
	{
		ReadToken = FNetToken::MakeNetToken(Index, bIsAssignedByAuthority ? FNetToken::ENetTokenAuthority::Authority : FNetToken::ENetTokenAuthority::None);
		UE_NET_TRACE_SET_SCOPE_NAME(TokenScope, *ReadToken.ToString());
	}

	return ReadToken;
}

void WriteNetToken(UE::Net::FNetSerializationContext& Context, FNetToken Token)
{
	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(*Token.ToString(), *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	WritePackedUint32(Writer, Token.GetIndex());
	Writer->WriteBool(Token.IsAssignedByAuthority());
}

FNetTokenStore::FNetTokenStore()
: LocalNetTokenStoreState(new FNetTokenStoreState)
{
}

FNetTokenStore::~FNetTokenStore()
{
	delete LocalNetTokenStoreState;
}

void FNetTokenStore::Init(FNetTokenStore::FInitParams& InParams)
{
	Params = InParams;
}

const FNetTokenDataStore* FNetTokenStore::GetDataStore(FName Name) const
{
	const TTuple<FName, FNetTokenDataStore*>* Entry = TokenDataStores.FindByPredicate([Name](const TTuple<FName, FNetTokenDataStore*>& Entry){ return Name == Entry.Get<0>(); });
	return Entry ? Entry->Get<1>() : nullptr;
}

 FNetTokenDataStore* FNetTokenStore::GetDataStore(FName Name)
{
	TTuple<FName, FNetTokenDataStore*>* Entry = TokenDataStores.FindByPredicate([Name](TTuple<FName, FNetTokenDataStore*>& Entry){ return Name == Entry.Get<0>(); });
	return Entry ? Entry->Get<1>() : nullptr;
}

bool FNetTokenStore::RegisterDataStore(FNetTokenDataStore* DataStore, FName TokenStoreName)
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
	TokenDataStores.Emplace(TokenStoreName, DataStore);

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
		TokenDataStores[TokenKey.TypeId].Get<1>()->WriteTokenData(Context, TokenKey);
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
