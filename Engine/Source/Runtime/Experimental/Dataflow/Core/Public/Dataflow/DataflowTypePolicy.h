// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"

struct IDataflowTypePolicy
{
	virtual bool SupportsType(FName Type) const = 0;
};

struct FDataflowAllTypesPolicy : public IDataflowTypePolicy
{
	virtual bool SupportsType(FName InType) const override
	{
		return true;
	}

	static IDataflowTypePolicy* GetInterface()
	{
		static FDataflowAllTypesPolicy Instance;
		return &Instance;
	}
};

template <typename T>
struct TDataflowSingleTypePolicy : public IDataflowTypePolicy
{
	using FType = T;

	virtual bool SupportsType(FName InType) const override
	{
		return SupportsTypeStatic(InType);
	}

	static bool SupportsTypeStatic(FName InType)
	{
		return (InType == TypeName);
	}

	template <typename TVisitor>
	static bool VisitPolicyByType(FName RequestedType, TVisitor Visitor)
	{
		if (RequestedType == TypeName)
		{
			TDataflowSingleTypePolicy<T> SingleTypePolicy;
			Visitor(SingleTypePolicy);
			return true;
		}
		return false;
	}

	static IDataflowTypePolicy* GetInterface()
	{
		static TDataflowSingleTypePolicy Instance;
		return &Instance;
	}

	inline static const FName TypeName = FName(TNameOf<T>::GetName());
};

template <typename... TTypes>
struct TDataflowMultiTypePolicy;

template <>
struct TDataflowMultiTypePolicy<>: public IDataflowTypePolicy
{
	virtual bool SupportsType(FName InType) const override
	{
		return false;
	}

	static bool SupportsTypeStatic(FName InType)
	{
		return false;
	}

	template <typename TVisitor>
	static bool VisitPolicyByType(FName RequestedType, TVisitor Visitor)
	{
		return false;
	}
};

Expose_TNameOf(bool);

template <typename T, typename... TTypes>
struct TDataflowMultiTypePolicy<T, TTypes...>: public TDataflowMultiTypePolicy<TTypes...>
{
	using Super = TDataflowMultiTypePolicy<TTypes...>;

	virtual bool SupportsType(FName InType) const override
	{
		return SupportsTypeStatic(InType);
	}

	static bool SupportsTypeStatic(FName InType)
	{
		return TDataflowSingleTypePolicy<T>::SupportsTypeStatic(InType)
			|| Super::SupportsTypeStatic(InType);
	}

	template <typename TVisitor>
	static bool VisitPolicyByType(FName RequestedType, TVisitor Visitor)
	{
		if (TDataflowSingleTypePolicy<T>::VisitPolicyByType(RequestedType, Visitor))
		{
			return true;
		}
		return Super::VisitPolicyByType(RequestedType, Visitor);
	}

	static IDataflowTypePolicy* GetInterface()
	{
		static TDataflowMultiTypePolicy<T, TTypes...> Instance;
		return &Instance;
	}
};

struct FDataflowNumericTypePolicy : 
	public TDataflowMultiTypePolicy<double, float, int64, uint64, int32, uint32, int16, uint16, int8, uint8>
{
};

struct FDataflowStringTypePolicy :
	public TDataflowMultiTypePolicy<FString, FName>
{
};

/**
* string comvertible types
* - FString / Fname
* - Numeric types ( see FDataflowNumericTypePolicy )
* - bool
*/
struct FDataflowStringConvertibleTypePolicy : IDataflowTypePolicy
{
	virtual bool SupportsType(FName InType) const override
	{
		return SupportsTypeStatic(InType);
	}

	static bool SupportsTypeStatic(FName InType)
	{
		return FDataflowStringTypePolicy::SupportsTypeStatic(InType)
			|| FDataflowNumericTypePolicy::SupportsTypeStatic(InType)
			|| TDataflowSingleTypePolicy<bool>::SupportsTypeStatic(InType)
			;
	}

	template <typename TVisitor>
	static bool VisitPolicyByType(FName RequestedType, TVisitor Visitor)
	{
		return FDataflowStringTypePolicy::VisitPolicyByType(RequestedType, Visitor)
			|| FDataflowNumericTypePolicy::VisitPolicyByType(RequestedType, Visitor)
			|| TDataflowSingleTypePolicy<bool>::VisitPolicyByType(RequestedType, Visitor)
			;
	}

	static IDataflowTypePolicy* GetInterface()
	{
		static FDataflowStringConvertibleTypePolicy Instance;
		return &Instance;
	}
};

// type Converters

template <typename T>
struct FDataflowConverter
{
	template <typename TFromType>
	static void From(const TFromType& From, T& To) { To = From; }

	template <typename TToType>
	static void To(const T& From, TToType& To) { To = From; }
};

template <>
struct FDataflowConverter<FString>
{
	template <typename TFromType>
	static void From(const TFromType& From, FString& To)
	{
		if constexpr (std::is_same_v<TFromType, FName>)
		{
			To = From.ToString();
		}
		else if constexpr (std::is_same_v<TFromType, bool>)
		{
			To = FString((From == true) ? "True" : "False");
		}
		else if constexpr (std::is_convertible_v<TFromType, double>)
		{
			To = FString::SanitizeFloat(double(From), 0);
		}
		else
		{
			To = From;
		}
	}

	template <typename TToType>
	static void To(const FString& From, TToType& To)
	{
		if constexpr (std::is_same_v<TToType, FName>)
		{
			To = FName(From);
		}
		else if constexpr (std::is_same_v<TToType, bool>)
		{
			To = From.ToBool();
		}
		else if constexpr (std::is_convertible_v<double, TToType>)
		{
			double Result = {};
			LexTryParseString(Result, *From);
			To = Result;
		}
		else
		{
			To = From;
		}
	}
};