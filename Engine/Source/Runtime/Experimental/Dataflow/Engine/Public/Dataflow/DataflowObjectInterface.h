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

	/** Simulation flag that could be used across simulation nodes.
	 * The setup and clean modes are game thread only whereas the
	 * update mode could run on game thread and physics thread
	 */
	enum class ESimulationFlags : uint8
	{
		SetupSimulation = 1 << 0,
		CleanSimulation = 1 << 1,
		UpdateSimulation = 1 << 2
	};

	ENUM_CLASS_FLAGS(ESimulationFlags);

	/** Caching mode that could be used across simulation nodes */
	enum class ECachingMode : uint8
	{
		PlayCache = 1 << 0,
		RecordCache = 1 << 1
	};

	/** Simulation context that will be used by all the simulation/cache nodes*/
	template<class Base = FContextSingle>
	class TSimulationContext : public TEngineContext<Base>
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(TEngineContext<Base>, TSimulationContext);

		TSimulationContext(const TObjectPtr<UObject>& InOwner,const TObjectPtr<UDataflow>& InGraph, FTimestamp InTimestamp,
			const TObjectPtr<AActor>& InRootActor, const ECachingMode InCachingMode)
				: Super(InOwner, InGraph, InTimestamp), SimulationWorld(InRootActor->GetWorld()), RootActor(InRootActor),
					SimulationFlags(ESimulationFlags::SetupSimulation), CachingMode(InCachingMode)
		{
			
		}
		
		virtual ~TSimulationContext() override {};
		
		/** Set the timing infos */
		void SetTimingInfos(const float DeltaSeconds, const float TimeSeconds) {DeltaTime = DeltaSeconds; SimulationTime = TimeSeconds;}

		/** Get the delta time in seconds */
		float GetDeltaTime() const {return DeltaTime;}
		
		/** Get the simulation time in seconds */
		float GetSimulationTime() const {return SimulationTime;}
		
		/** Get the simulation world */
		const TObjectPtr<UWorld>& GetSimulationWorld() const {return SimulationWorld;}
		
		/** Get the root actor */
		const TObjectPtr<AActor>& GetRootActor() const {return RootActor;}

		/** Check if the simulation mode is present in the simulation context */
		bool HasSimulationFlag(const ESimulationFlags InSimulationFlag) const
		{
			return (SimulationFlags & InSimulationFlag) == InSimulationFlag;
		}

		/** Set the simulation mode */
		void SetSimulationFlag(const ESimulationFlags InSimulationFlag) {SimulationFlags |= InSimulationFlag;}

		/** Unset the simulation mode */
		void UnsetSimulationFlag(const ESimulationFlags InSimulationFlag) {SimulationFlags &= ~InSimulationFlag;}

		/** Get the caching mode */
		ECachingMode GetCachingMode() const {return CachingMode;}

		/** Set the caching mode */
		void SetCachingMode(const ECachingMode InCachingMode) {CachingMode = InCachingMode;}
		
	private :
		
		/** Evolution world used to register components */
		TObjectPtr<UWorld> SimulationWorld;

		/** Root actor of the world */
		TObjectPtr<AActor> RootActor;

		/** Simulation time */
		float SimulationTime = 0.0f;

		/** Delta time */
		float DeltaTime = 0.0f;

		/** Simulation flags that could be used across simulation nodes */
		ESimulationFlags SimulationFlags;

		/** Caching mode that could be used across cache nodes */
		ECachingMode CachingMode;
	};

	typedef TEngineContext<FContextSingle> FEngineContext;
	typedef TEngineContext<FContextThreaded> FEngineContextThreaded;

	typedef TSimulationContext<FContextSingle> FSimulationContext;
	typedef TSimulationContext<FContextThreaded> FSimulationContextThreaded;

}
