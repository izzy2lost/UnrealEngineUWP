// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"
#include "Logging/LogMacros.h"

NETCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogNetToken, Log, All);

namespace UE::Net
{
	class FNetTokenStore;
	class FNetTokenStoreState;
}

namespace UE::Net
{

class FNetToken
{
public:
	typedef uint32 FTypeId;

	enum : uint32 { Invalid = 0U };

	enum : uint32 { InvalidTokenTypeId = ~FTypeId(0) };
	enum : uint32 { InvalidTokenIndex = 0U };

	/** How many bits we use to express the TypeId for NetTokens, Increasing this value will break network compatibility and might need versioning for replays. */
	enum : uint32 { TokenTypeIdBits = 3U };

	/** How many bits we use to express the index part of NetTokens */
	enum : uint32 { TokenBits = 20U };

	enum : uint32 { MaxTypeIdCount = 1U << TokenTypeIdBits };
	enum : uint32 { MaxNetTokenCount = 1U << TokenBits };

	enum class ENetTokenAuthority
	{
		None,
		Authority,
	};

public:	
	FNetToken() : Value(Invalid) {}
	inline bool IsValid() const { return Value != Invalid; }
	bool IsAssignedByAuthority() const { return bIsAssignedByAuthority != 0U; }
	uint32 GetIndex() const { return Index; }
	bool operator==(const FNetToken& Other) const { return IsAssignedByAuthority() == Other.IsAssignedByAuthority() && Index == Other.Index; }
	FString ToString() const;

	friend uint32 GetTypeHash(const FNetToken& Token)
	{
		return ::GetTypeHash(Token.Value);
	}
	
	static FNetToken MakeNetToken(uint32 Index, ENetTokenAuthority Authority) { check(Index < MaxNetTokenCount); return FNetToken(Index, Authority); }
	
private:
	explicit FNetToken(uint32 InIndex, ENetTokenAuthority Authority) { Padding = 0U, Index = InIndex, bIsAssignedByAuthority = Authority == ENetTokenAuthority::Authority ? 1U : 0U; }

private:

	union 
	{
		struct
		{
			uint32 Index : TokenBits;
			uint32 bIsAssignedByAuthority : 1U;
			uint32 Padding : 32 - TokenBits - 1U;
		};
		uint32 Value;
	};
};

class FNetTokenStoreKey
{
public:
	enum { Invalid = 0U };

	FNetTokenStoreKey() : Value(Invalid) {}
	bool IsValid() const { return Value != Invalid; }
	uint32 GetKeyValue() const { return Key; }
	FNetToken::FTypeId GetTypeId() const { return TypeId; }
	bool operator==(const FNetTokenStoreKey& Other) const { return Value == Other.Value; }

private:
	friend class FNetTokenStore;
	friend class FNetTokenDataStore;

private:
	union
	{
		struct
		{
			uint32 TypeId : FNetToken::TokenTypeIdBits;
			uint32 Key : FNetToken::TokenBits;
		};
		uint32 Value;
	};
};

static_assert(sizeof(FNetTokenStoreKey) == sizeof(uint32), "FNetTokenKey should fit in a uint32");

// Contains necessary context to resolve NetTokens
class FNetTokenResolveContext
{
public:
	FNetTokenStore* NetTokenStore = nullptr;
	const FNetTokenStoreState* RemoteNetTokenStoreState = nullptr;
};

inline FString FNetToken::ToString() const
{
	FString Result;
	Result = FString::Printf(TEXT("NetToken (Auth:%u Index=%u)"), IsAssignedByAuthority(), Index);
	return Result;
}

}

template <> struct TIsPODType<UE::Net::FNetToken> { enum { Value = true }; };
