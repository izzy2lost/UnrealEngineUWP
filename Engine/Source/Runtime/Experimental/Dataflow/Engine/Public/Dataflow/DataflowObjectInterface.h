// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Dataflow/DataflowGraph.h"
#include "GameFramework/Actor.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "GeometryCollection/ManagedArrayCollection.h"

class UDataflow;
class UObject;
class FArchive;

namespace Dataflow
{
	template<class Base = FContextSingle>
	class TEngineContext : public Base
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(Base, TEngineContext);

		TEngineContext(const TObjectPtr<UObject>& InOwner,const TObjectPtr<UDataflow>& InGraph, FTimestamp InTimestamp)
				: Base(InTimestamp)
				, Owner(InOwner)
				, Graph(InGraph)
		{}
	
		TObjectPtr<UObject> Owner = nullptr;
		TObjectPtr<UDataflow> Graph = nullptr;

		virtual ~TEngineContext() {}

		int32 GetKeys(TSet<FContextCacheKey>& InKeys) const { return Base::GetKeys(InKeys); }

		TUniquePtr<FContextCacheElementBase>* GetBaseData(FContextCacheKey Key) { return Base::GetDataImpl(Key); }

		virtual void Serialize(FArchive& Ar) { Base::Serialize(Ar); }

	};

	typedef TEngineContext<FContextSingle> FEngineContext;
	typedef TEngineContext<FContextThreaded> FEngineContextThreaded;
}
