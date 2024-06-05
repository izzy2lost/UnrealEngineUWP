// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNode.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"

namespace Dataflow
{
	class FContext;
	typedef TPair<FString, FName> FRenderKey;

	struct FGraphRenderingState {
		FGraphRenderingState(const FGuid InGuid, const FDataflowNode* InNode, const FRenderingParameter& InParameters, Dataflow::FContext& InContext)
			: NodeGuid(InGuid)
			, Node(InNode)
			, RenderName(InParameters.Name)
			, RenderType(InParameters.Type)
			, RenderOutputs(InParameters.Outputs)
			, Context(InContext)
		{}

		const FGuid& GetGuid() const { return NodeGuid; }
		FName GetNodeName() const { return Node?Node->GetName():FName(); }
		FRenderKey GetRenderKey() const { return { RenderName,RenderType }; }
		const TArray<FName>& GetRenderOutputs() const { return RenderOutputs; }

		template<class T>
		const T& GetValue(FName OutputName, const T& Default) const
		{
			if (Node)
			{
				if (const FDataflowOutput* Output = Node->FindOutput(OutputName))
				{
					return Output->GetValue<T>(Context, Default);
				}
			}
			return Default;
		}

	private:
		const FGuid NodeGuid;
		const FDataflowNode* Node = nullptr;

		FString RenderName;
		FName RenderType;
		TArray<FName> RenderOutputs;

		Dataflow::FContext& Context;
	};

	//
	//
	//
	class FRenderingFactory
	{
		typedef TFunction<void(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State)> FOutputRenderingFunction;

		// All Maps indexed by TypeName
		TMap<FRenderKey, FOutputRenderingFunction > RenderMap;		// [TypeName] -> NewNodeFunction
		DATAFLOWENGINE_API static FRenderingFactory* Instance;
		FRenderingFactory() {}

	public:
		~FRenderingFactory() { delete Instance; }

		static FRenderingFactory* GetInstance()
		{
			if (!Instance)
			{
				Instance = new FRenderingFactory();
			}
			return Instance;
		}

		void RegisterOutput(const FRenderKey& Key, FOutputRenderingFunction InFunction)
		{
			if (RenderMap.Contains(Key))
			{
				UE_LOG(LogChaos, Warning,
					TEXT("Warning : Dataflow output rendering registration conflicts with "
						"existing renderer(<%s,%s>)"), 
					*Key.Get<0>(),
					*Key.Get<1>().ToString());
			}
			else
			{
				RenderMap.Add(Key, InFunction);
			}
		}

		DATAFLOWENGINE_API void RenderNodeOutput(GeometryCollection::Facades::FRenderingFacade& RenderData, const FGraphRenderingState& State);

		bool Contains(const FRenderKey& InKey) const { return RenderMap.Contains(InKey); }

	};

}

