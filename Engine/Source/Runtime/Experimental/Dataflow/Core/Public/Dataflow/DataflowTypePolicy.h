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
	using FReturnType = T;
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

template <typename T, typename... TTypes>
struct TDataflowMultiTypePolicy<T, TTypes...>: public TDataflowMultiTypePolicy<TTypes...>
{
	using Super = TDataflowMultiTypePolicy<TTypes...>;
	using FReturnType = T;

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
