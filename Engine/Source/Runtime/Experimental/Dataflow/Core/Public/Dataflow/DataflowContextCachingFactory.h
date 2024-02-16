// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"

class FArchive;

namespace Dataflow
{
	struct FContextCacheData {

		FContextCacheData(FName InType, FGuid InNodeGuid, FContextCacheElementBase* InData, uint32 InNodeHash, FTimestamp InTimestamp)
			: Type(InType), NodeGuid(InNodeGuid), Data(InData), NodeHash(InNodeHash), Timestamp(InTimestamp) {}

		FName Type;
		FGuid NodeGuid;
		FContextCacheElementBase* Data = nullptr;
		uint32 NodeHash;
		FTimestamp Timestamp = FTimestamp::Invalid;
	};

	//
	//
	//
	class DATAFLOWCORE_API FContextCachingFactory
	{
		typedef TFunction<FContextCacheElementBase*(FArchive& Ar, FContextCacheElementBase* InData)> FSerializeFunction;

		// All Maps indexed by TypeName
		TMap<FName, FSerializeFunction > CachingMap;		// [TypeName] -> Caching Funcitons
		static FContextCachingFactory* Instance;
		FContextCachingFactory() {}

	public:
		~FContextCachingFactory() { delete Instance; }

		static FContextCachingFactory* GetInstance()
		{
			if (!Instance)
			{
				Instance = new FContextCachingFactory();
			}
			return Instance;
		}

		void RegisterSerializeFunction(const FName& Type, FSerializeFunction InSerializeFunc)
		{
			if (CachingMap.Contains(Type))
			{
				UE_LOG(LogChaos, Warning,
					TEXT("Warning : Dataflow output caching registration conflicts with "
						"existing type(%s)"), *Type.ToString());
			}
			else
			{
				CachingMap.Add(Type, InSerializeFunc);
			}
		}

		template<class T>
		static T& GetTypedElement(FContextCacheElementBase* InElement)
		{
			return (T&)InElement->GetTypedData<T>(InElement->Property);
		}

		template<class T>
		static FContextCacheElementBase* NewTypedElement(T&& Data)
		{
			return new FContextCacheElement<T>(FGuid(), (FProperty*)nullptr, MoveTemp(Data), (uint32)0, FTimestamp::Invalid);
		}


		FContextCacheElementBase* Serialize(FArchive& Ar, FContextCacheData&& Data);

		bool Contains(FName InType) const { return CachingMap.Contains(InType); }

	};

}

