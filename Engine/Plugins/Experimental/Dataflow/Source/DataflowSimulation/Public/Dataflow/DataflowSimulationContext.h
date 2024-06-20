// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Dataflow/DataflowObjectInterface.h"

struct FDataflowSimulationProxy;

namespace Dataflow
{
	/** Simulation context that will be used by all the simulation nodes*/
	template<typename Base = FContextSingle>
	class DATAFLOWSIMULATION_API TSimulationContext : public TEngineContext<Base>
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(TEngineContext<Base>, TSimulationContext);

		TSimulationContext(const TObjectPtr<UObject>& InOwner, FTimestamp InTimestamp)
				: Super(InOwner, InTimestamp)
		{}
		
		virtual ~TSimulationContext() override {};
		
		/** Set the timing infos */
		void SetTimingInfos(const float DeltaSeconds, const float TimeSeconds) {DeltaTime = DeltaSeconds; SimulationTime = TimeSeconds;}

		/** Get the delta time in seconds */
		float GetDeltaTime() const {return DeltaTime;}
		
		/** Get the simulation time in seconds */
		float GetSimulationTime() const {return SimulationTime;}

		/** Filter the physics solvers matching the groups */
		void GetSimulationProxies(const FString& ProxyType, const TArray<FString>& SimulationGroups, TArray<FDataflowSimulationProxy*>& FilteredProxies) const;

		/** Add simulation proxy to the context */
		void AddSimulationProxy(const FString& ProxyType, FDataflowSimulationProxy* SimulationProxy);
		
		/** Remove simulation proxy from the context */
    	void RemoveSimulationProxy(const FString& ProxyType, const FDataflowSimulationProxy* SimulationProxy);

		/** Reset all the simulation proxies */
		void ResetSimulationProxies();
		
		/** Return the number of physics solvers */
		int32 NumSimulationProxies(const FString& ProxyType) const;

		/** Register all the proxy groups used in the proxy */
		void RegisterProxyGroups();

		/** Build group bits based on the string array */
		void BuildGroupBits(const TArray<FString>& SimulationGroups, TBitArray<>& GroupBits) const;
		
	private :

		/** Simulation time */
		float SimulationTime = 0.0f;

		/** Delta time */
		float DeltaTime = 0.0f;

		/** List of all the simulation proxies within the context sorted by type */
		TMap<FString, TSet<FDataflowSimulationProxy*>> SimulationProxies;

		/** List of all the simulation groups indices */
		TMap<FString, uint32> GroupIndices;
	};

	typedef TSimulationContext<FContextSingle> FDataflowSimulationContext;
	typedef TSimulationContext<FContextThreaded> FDataflowSimulationContextThreaded;
	
}
