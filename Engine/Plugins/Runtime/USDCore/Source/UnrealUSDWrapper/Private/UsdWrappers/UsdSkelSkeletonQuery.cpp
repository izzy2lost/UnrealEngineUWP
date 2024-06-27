// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/UsdSkelSkeletonQuery.h"

#include "USDMemory.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdSkelAnimQuery.h"
#include "UsdWrappers/UsdSkelSkeletonQuery.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"
#include "USDIncludesEnd.h"
#endif	  //  #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FUsdSkelSkeletonQueryImpl
		{
		public:
			FUsdSkelSkeletonQueryImpl()
#if USE_USD_SDK
				: PxrUsdSkelSkeletonQuery()
#endif	  // #if USE_USD_SDK
			{
			}

#if USE_USD_SDK
			explicit FUsdSkelSkeletonQueryImpl(const pxr::UsdSkelSkeletonQuery& InUsdSkelSkeletonQuery)
				: PxrUsdSkelSkeletonQuery(InUsdSkelSkeletonQuery)
			{
			}

			explicit FUsdSkelSkeletonQueryImpl(pxr::UsdSkelSkeletonQuery&& InUsdSkelSkeletonQuery)
				: PxrUsdSkelSkeletonQuery(MoveTemp(InUsdSkelSkeletonQuery))
			{
			}

			TUsdStore<pxr::UsdSkelSkeletonQuery> PxrUsdSkelSkeletonQuery;
#endif	  // #if USE_USD_SDK
		};
	}	  // namespace Internal

	FUsdSkelSkeletonQuery::FUsdSkelSkeletonQuery()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>();
	}

	FUsdSkelSkeletonQuery::FUsdSkelSkeletonQuery(const FUsdSkelSkeletonQuery& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>(Other.Impl->PxrUsdSkelSkeletonQuery.Get());
#endif	  // #if USE_USD_SDK
	}

	FUsdSkelSkeletonQuery::FUsdSkelSkeletonQuery(FUsdSkelSkeletonQuery&& Other)
		: Impl(MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>())
	{
		if (this != &Other)
		{
			Impl = MoveTemp(Other.Impl);
		}
	}

	FUsdSkelSkeletonQuery::~FUsdSkelSkeletonQuery()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	FUsdSkelSkeletonQuery& FUsdSkelSkeletonQuery::operator=(const FUsdSkelSkeletonQuery& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>(Other.Impl->PxrUsdSkelSkeletonQuery.Get());
#endif	  // #if USE_USD_SDK

		return *this;
	}

	FUsdSkelSkeletonQuery& FUsdSkelSkeletonQuery::operator=(FUsdSkelSkeletonQuery&& Other)
	{
		if (this != &Other)
		{
			FScopedUnrealAllocs UnrealAllocs;
			Impl = MoveTemp(Other.Impl);
		}

		return *this;
	}

	FUsdSkelSkeletonQuery::operator bool() const
	{
#if USE_USD_SDK
		return static_cast<bool>(Impl->PxrUsdSkelSkeletonQuery.Get());
#else
		return false;
#endif
	}

#if USE_USD_SDK
	FUsdSkelSkeletonQuery::FUsdSkelSkeletonQuery(const pxr::UsdSkelSkeletonQuery& InUsdSkelSkeletonQuery)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>(InUsdSkelSkeletonQuery);
	}

	FUsdSkelSkeletonQuery::FUsdSkelSkeletonQuery(pxr::UsdSkelSkeletonQuery&& InUsdSkelSkeletonQuery)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdSkelSkeletonQueryImpl>(MoveTemp(InUsdSkelSkeletonQuery));
	}

	FUsdSkelSkeletonQuery::operator pxr::UsdSkelSkeletonQuery&()
	{
		return Impl->PxrUsdSkelSkeletonQuery.Get();
	}

	FUsdSkelSkeletonQuery::operator const pxr::UsdSkelSkeletonQuery&() const
	{
		return Impl->PxrUsdSkelSkeletonQuery.Get();
	}
#endif	  // #if USE_USD_SDK

	bool FUsdSkelSkeletonQuery::IsValid() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdSkelSkeletonQuery.Get().IsValid();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FUsdPrim FUsdSkelSkeletonQuery::GetPrim() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return FUsdPrim{Impl->PxrUsdSkelSkeletonQuery.Get().GetPrim()};
#else
		return {};
#endif	  // #if USE_USD_SDK
	}

	FUsdPrim FUsdSkelSkeletonQuery::GetSkeleton() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return FUsdPrim{Impl->PxrUsdSkelSkeletonQuery.Get().GetSkeleton().GetPrim()};
#else
		return {};
#endif	  // #if USE_USD_SDK
	}

	FUsdSkelAnimQuery FUsdSkelSkeletonQuery::GetAnimQuery() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return FUsdSkelAnimQuery{Impl->PxrUsdSkelSkeletonQuery.Get().GetAnimQuery()};
#else
		return {};
#endif	  // #if USE_USD_SDK
	}
}	 // namespace UE
