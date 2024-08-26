// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NameTokenStore.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Hash/CityHash.h"
#include "Net/Core/Trace/NetTrace.h"

#define UE_NET_ENABLE_FNAME_TOKEN_LOG 1

#if UE_NET_ENABLE_FNAME_TOKEN_LOG
#	define UE_LOG_FNAMETOKEN(Format, ...)  UE_LOG(LogIris, Verbose, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_FNAMETOKEN(...)
#endif

#define UE_LOG_FNAMETOKEN_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)

namespace UE::Net
{

FNetToken FNameTokenStore::GetOrCreateToken(FName Name)
{
	FNetTokenStoreKey Key = GetOrCreateTokenStoreKey(Name);
	if (Key.IsValid())
	{
		const uint32 NameIndex = Key.GetKeyValue();
		if (StoredTokens[NameIndex].IsValid())
		{
			return StoredTokens[NameIndex];
		}
		else
		{
			const FNetToken NewToken = CreateToken(TokenStore.IsAuthority() ? FNetToken::ENetTokenAuthority::Authority : FNetToken::ENetTokenAuthority::None, Key, *TokenStore.GetLocalNetTokenStoreState());
			StoredTokens[NameIndex] = NewToken;

			UE_LOG_FNAMETOKEN(TEXT("FNameTokenStore::GetOrCreateToken - Created %s for %s"), *NewToken.ToString(), *Name.ToString());

			return NewToken;
		}
	}

	return FNetToken();
}

FNetTokenStoreKey FNameTokenStore::GetOrCreateTokenStoreKey(FName Name)
{
	// Lock if we have to.. make that a policy
	//FScopeLock Lock(&CriticalSection);
	if (const FNetTokenStoreKey* ExistingKey = FNameToKey.Find(Name))
	{
		return *ExistingKey;
	}
	else if (FNameToKey.Num() < FNetToken::MaxNetTokenCount)
	{
		// Store bookkeeping data
		const FNetTokenStoreKey NewKey = MakeNetTokenStoreKey(GetTypeId(), StoredFNames.Num());

		FNameToKey.Add(Name, NewKey);
		StoredFNames.Add(Name);
		StoredTokens.Add(FNetToken());

		return NewKey;
	}

	return FNetTokenStoreKey();
}

FNameTokenStore::FNameTokenStore(FNetTokenStore& InTokenStore)
: TokenStore(InTokenStore)
{
	// Reserve 0
	StoredFNames.Add(FName());
	StoredTokens.Add(FNetToken());

	// Register
	TokenStore.RegisterDataStore(this, FNameTokenStore::GetTokenStoreName());
}

FName FNameTokenStore::ResolveToken(FNetToken Token, const FNetTokenStoreState* NetTokenStoreState) const
{
	const FNetTokenStoreState* TokenStoreState = TokenStore.IsLocalToken(Token) ? TokenStore.GetLocalNetTokenStoreState() : NetTokenStoreState;
	if (Token.IsValid() && ensureMsgf(TokenStoreState, TEXT("FNameTokenStore::ResolveToken Needs valid remote NetTokenStoreState to resolve remote %s"), *Token.ToString()))
	{
		const FNetTokenStoreKey StoreKey = GetTokenKey(Token, *TokenStoreState);
		if (StoreKey.IsValid() && StoreKey.GetKeyValue() < (uint32)StoredFNames.Num())
		{
			return StoredFNames[StoreKey.GetKeyValue()];
		}
	}

	return FName();
}

void FNameTokenStore::WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey TokenStoreKey) const
{
	UE_NET_TRACE_DYNAMIC_NAME_SCOPE(StoredFNames[TokenStoreKey.GetKeyValue()], *Context.GetBitStreamWriter(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);
	// $TODO: $IRIS: We can be a bit smarter here and utilize the string-number split of FNames to export less data.. JIRA: UE-221753
	WriteString(Context.GetBitStreamWriter(), StoredFNames[TokenStoreKey.GetKeyValue()].ToString());
}

FNetTokenStoreKey FNameTokenStore::ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken)
{
	UE_NET_TRACE_NAMED_DYNAMIC_NAME_SCOPE(TokenScope, FName(), *Context.GetBitStreamReader(), Context.GetTraceCollector(), ENetTraceVerbosity::VeryVerbose);

	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	// Read the token data and add it to the store without assigning LocalToken
	FString Temp;
	ReadString(Reader, Temp);

	if (!Reader->IsOverflown())
	{
		FName Name(Temp);
		UE_NET_TRACE_SET_SCOPE_NAME(TokenScope, Name);

		FNetTokenStoreKey Key = GetOrCreateTokenStoreKey(Name);

		// If this is an authtoken and we are not the authority, update the stored key
		if (Key.IsValid() && NetToken.IsAssignedByAuthority() && !TokenStore.IsAuthority())
		{
			UE_LOG_FNAMETOKEN(TEXT("NameTokenStore::ReadTokenData - Replaced local key %s with %s for name %s"), *StoredTokens[Key.GetKeyValue()].ToString(), *NetToken.ToString(), *Temp);	

			// This way the next time we lookup this key during assignment use the imported authoritative key which we do not have to export
			StoredTokens[Key.GetKeyValue()] = NetToken;
		}

		return Key;
	}
	else
	{
		return FNetTokenStoreKey();
	}
}

}
