// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

namespace UE::Net
{
	class FNetSerializationContext;
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
	enum : uint32 { TokenTypeIdBits = 2U };
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
	explicit FNetToken(uint32 InIndex, ENetTokenAuthority Authority) { Index = InIndex, bIsAssignedByAuthority = Authority == ENetTokenAuthority::Authority ? 1U : 0U; }

private:
	friend IRISCORE_API FNetToken ReadNetToken(UE::Net::FNetSerializationContext&);

	union 
	{
		struct
		{
			uint32 Index : TokenBits;
			uint32 bIsAssignedByAuthority : 1U;
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

inline FString FNetToken::ToString() const
{
	FString Result;
	Result = FString::Printf(TEXT("NetToken (Auth:%u Index=%u)"), IsAssignedByAuthority(), Index);
	return Result;
}

// Read and write tokens
IRISCORE_API FNetToken ReadNetToken(UE::Net::FNetSerializationContext& Context);
IRISCORE_API void WriteNetToken(UE::Net::FNetSerializationContext& Context, FNetToken Token);

}

template <> struct TIsPODType<UE::Net::FNetToken> { enum { Value = true }; };
