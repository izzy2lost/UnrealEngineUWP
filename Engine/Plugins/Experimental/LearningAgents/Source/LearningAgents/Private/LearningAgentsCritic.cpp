// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCritic.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsHelpers.h"
#include "LearningAgentsNeuralNetworkData.h"
#include "LearningFeatureObject.h"
#include "LearningNeuralNetwork.h"
#include "LearningNeuralNetworkObject.h"
#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"

ULearningAgentsCritic::ULearningAgentsCritic() : Super(FObjectInitializer::Get()) {}
ULearningAgentsCritic::ULearningAgentsCritic(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsCritic::~ULearningAgentsCritic() = default;

void ULearningAgentsCritic::SetupCritic(
	ULearningAgentsInteractor* InInteractor, 
	ULearningAgentsPolicy* InPolicy,
	const FLearningAgentsCriticSettings& CriticSettings,
	ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already run!"), *GetName());
		return;
	}

	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		return;
	}

	if (!InInteractor)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InInteractor is nullptr."), *GetName());
		return;
	}

	if (!InInteractor->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InInteractor->GetName());
		return;
	}

	Interactor = InInteractor;

	if (!InPolicy)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InPolicy is nullptr."), *GetName());
		return;
	}

	if (!InPolicy->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InPolicy->GetName());
		return;
	}

	Policy = InPolicy;

	const int32 NetworkInputNum = Interactor->GetObservationFeature().DimNum() + Policy->GetMemoryStateSize();
	const int32 NetworkOutputNum = 1;

	if (NeuralNetworkAsset)
	{
		// Use Existing Neural Network Asset

		if (NeuralNetworkAsset->NeuralNetworkData)
		{
			if (NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetInputNum() != NetworkInputNum ||
				NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetOutputNum() != NetworkOutputNum)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Neural Network Asset provided during Setup is incorrect size: Inputs and outputs don't match."), *GetName());
				return;
			}

			Network = NeuralNetworkAsset;
		}
		else
		{
			Network = NeuralNetworkAsset;
			Network->NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(Network);
			Network->NeuralNetworkData->CreateMLP(
				NetworkInputNum,
				NetworkOutputNum,
				CriticSettings.HiddenLayerSize,
				CriticSettings.LayerNum,
				CriticSettings.ActivationFunction);
		}
	}
	else
	{
		// Create New Neural Network Asset

		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("CriticNetwork"), EUniqueObjectNameOptions::GloballyUnique);

		Network = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
		Network->NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(Network);
		Network->NeuralNetworkData->CreateMLP(
			NetworkInputNum,
			NetworkOutputNum,
			CriticSettings.HiddenLayerSize,
			CriticSettings.LayerNum,
			CriticSettings.ActivationFunction);
	}

	// Create Critic Object
	CriticObject = MakeShared<UE::Learning::FNeuralNetworkCriticFunction>(
		TEXT("CriticObject"),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum(),
		Interactor->GetObservationFeature().DimNum(),
		Policy->GetMemoryStateSize(),
		Network->NeuralNetworkData->GetNetworkInterface());

	CriticAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(CriticAgentIteration, INDEX_NONE);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsCritic::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(CriticAgentIteration, 0, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsCritic::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(CriticAgentIteration, INDEX_NONE, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsCritic::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(CriticAgentIteration, 0, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

ULearningAgentsNeuralNetwork* ULearningAgentsCritic::GetNetworkAsset()
{
	return Network;
}

UE::Learning::INeuralNetwork& ULearningAgentsCritic::GetCriticNetwork()
{
	return *Network->NeuralNetworkData->GetNetworkInterface();
}

UE::Learning::FNeuralNetworkCriticFunction& ULearningAgentsCritic::GetCriticObject()
{
	return *CriticObject;
}

void ULearningAgentsCritic::LoadCriticFromSnapshot(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromSnapshot(File);
}

void ULearningAgentsCritic::SaveCriticToSnapshot(const FFilePath& File) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToSnapshot(File);
}

void ULearningAgentsCritic::UseCriticFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!NeuralNetworkAsset || !NeuralNetworkAsset->NeuralNetworkData)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == Network)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetInputNum() != Network->NeuralNetworkData->GetNetworkInterface()->GetInputNum() ||
		NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetOutputNum() != Network->NeuralNetworkData->GetNetworkInterface()->GetOutputNum())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to use asset as network settings don't match."), *GetName());
		return;
	}

	Network = NeuralNetworkAsset;
	CriticObject->UpdateNeuralNetwork(Network->NeuralNetworkData->GetNetworkInterface());
}

void ULearningAgentsCritic::LoadCriticFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromAsset(NeuralNetworkAsset);
}

void ULearningAgentsCritic::SaveCriticToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToAsset(NeuralNetworkAsset);
}

void ULearningAgentsCritic::EvaluateCritic()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsCritic::EvaluateCritic);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check Agents actually have encoded observations.
	// All added agents should already have some memory state even if it is zero.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not made observations so critic will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Get views of Observations and Memory State and copy into network input buffers

	TLearningArrayView<2, const float> MemoryStateView = Policy->GetMemoryStateView();
	TLearningArrayView<2, const float> ObservationsView = Manager->GetInstanceData()->ConstView(Interactor->GetObservationFeature().FeatureHandle);
	TLearningArrayView<2, float> InputObservationView = Manager->GetInstanceData()->View(CriticObject->InputObservationHandle);
	TLearningArrayView<2, float> InputMemoryStateView = Manager->GetInstanceData()->View(CriticObject->InputMemoryStateHandle);

	UE::Learning::Array::Copy<2, float>(InputObservationView, ObservationsView, ValidAgentSet);
	UE::Learning::Array::Copy<2, float>(InputMemoryStateView, MemoryStateView, ValidAgentSet);

	// Evaluate Critic

	CriticObject->Evaluate(ValidAgentSet);

	// Increment Discounted Return Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		CriticAgentIteration[AgentId]++;
	}
		
	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	VisualLog(ValidAgentSet);
#endif
}

float ULearningAgentsCritic::GetEstimatedDiscountedReturn(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (CriticAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not yet computed the estimated discounted return. Did you run EvaluateCritic?"), *GetName(), AgentId);
		return 0.0f;
	}

	return CriticObject->InstanceData->ConstView(CriticObject->OutputHandle)[AgentId];
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void ULearningAgentsCritic::VisualLog(const UE::Learning::FIndexSet AgentSet) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsCritic::VisualLog);

	const TLearningArrayView<2, const float> InputObservationView = CriticObject->InstanceData->ConstView(CriticObject->InputObservationHandle);
	const TLearningArrayView<2, const float> InputMemoryStateView = CriticObject->InstanceData->ConstView(CriticObject->InputMemoryStateHandle);
	const TLearningArrayView<1, const float> OutputView = CriticObject->InstanceData->ConstView(CriticObject->OutputHandle);

	for (const int32 AgentId : AgentSet)
	{
		if (const AActor* Actor = Cast<AActor>(GetAgent(AgentId)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nObservation Input: %s\nObservation Input Stats (Min/Max/Mean/Std): %s\nMemory State Input: %s\nMemory State Input Stats (Min/Max/Mean/Std): %s\nOutput: [% 6.3f]"),
				AgentId,
				*UE::Learning::Array::FormatFloat(InputObservationView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputObservationView[AgentId]),
				*UE::Learning::Array::FormatFloat(InputMemoryStateView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputMemoryStateView[AgentId]),
				OutputView[AgentId]);
		}
	}
}
#endif
