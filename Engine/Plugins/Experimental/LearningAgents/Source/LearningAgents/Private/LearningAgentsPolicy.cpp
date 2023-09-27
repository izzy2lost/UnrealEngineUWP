// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsPolicy.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
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

ULearningAgentsPolicy::ULearningAgentsPolicy() : Super(FObjectInitializer::Get()) {}
ULearningAgentsPolicy::ULearningAgentsPolicy(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsPolicy::~ULearningAgentsPolicy() = default;

void ULearningAgentsPolicy::SetupPolicy(
	ULearningAgentsInteractor* InInteractor, 
	const FLearningAgentsPolicySettings& PolicySettings,
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

	// Setup Neural Network

	const int32 NetworkInputNum = Interactor->GetObservationFeature().DimNum() + PolicySettings.MemoryStateSize;
	const int32 NetworkOutputNum = 2 * Interactor->GetActionFeature().DimNum() + PolicySettings.MemoryStateSize;

	if (NeuralNetworkAsset)
	{
		// Use Existing Neural Network Asset

		if (NeuralNetworkAsset->NeuralNetworkData)
		{
			if (NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetInputNum() != NetworkInputNum ||
				NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface()->GetOutputNum() != NetworkOutputNum)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Neural Network Asset provided during Setup is incorrect size: Inputs and outputs don't match what is required."), *GetName());
				return;
			}

			Network = NeuralNetworkAsset;
		}
		else
		{
			Network = NeuralNetworkAsset;
			Network->NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(Network);
			
			Network->NeuralNetworkData->CreateMemoryBackbone(
				Interactor->GetObservationFeature().DimNum(),
				2 * Interactor->GetActionFeature().DimNum(),
				PolicySettings.MemoryStateSize,
				PolicySettings.HiddenLayerSize,
				FMath::Max(PolicySettings.LayerNum / 2, 1),
				FMath::Max(PolicySettings.LayerNum / 2, 1));

		}
	}
	else
	{
		// Create New Neural Network Asset

		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("PolicyNetwork"), EUniqueObjectNameOptions::GloballyUnique);

		Network = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
		Network->NeuralNetworkData = NewObject<ULearningAgentsNeuralNetworkData>(Network);
		
		Network->NeuralNetworkData->CreateMemoryBackbone(
			Interactor->GetObservationFeature().DimNum(),
			2 * Interactor->GetActionFeature().DimNum(),
			PolicySettings.MemoryStateSize,
			PolicySettings.HiddenLayerSize,
			FMath::Max(PolicySettings.LayerNum / 2, 1),
			FMath::Max(PolicySettings.LayerNum / 2, 1));

	}

	// Create Policy Object
	UE::Learning::FNeuralNetworkPolicyFunctionSettings PolicyFunctionSettings;
	PolicyFunctionSettings.ActionNoiseMin = PolicySettings.ActionNoiseMin;
	PolicyFunctionSettings.ActionNoiseMax = PolicySettings.ActionNoiseMax;
	PolicyFunctionSettings.ActionNoiseScale = PolicySettings.ActionNoiseScale;

	PolicyObject = MakeShared<UE::Learning::FNeuralNetworkPolicyFunction>(
		TEXT("PolicyObject"),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum(),
		Interactor->GetObservationFeature().DimNum(),
		Interactor->GetActionFeature().DimNum(),
		PolicySettings.MemoryStateSize,
		Network->NeuralNetworkData->GetNetworkInterface(),
		PolicySettings.ActionNoiseSeed,
		UE::Learning::FNeuralNetworkInferenceSettings(),
		PolicyFunctionSettings);

	PolicyAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(PolicyAgentIteration, INDEX_NONE);

	PreEvaluationMemoryStateHandle = Manager->GetInstanceData()->Add<2, float>({ GetFName(), TEXT("PreEvaluationMemoryState") }, { Manager->GetMaxAgentNum(), PolicySettings.MemoryStateSize });
	UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle), FLT_MAX);

	MemoryStateHandle = Manager->GetInstanceData()->Add<2, float>({ GetFName(), TEXT("MemoryState") }, { Manager->GetMaxAgentNum(), PolicySettings.MemoryStateSize });
	UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(MemoryStateHandle), FLT_MAX);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsPolicy::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(PolicyAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle), 0.0f, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(MemoryStateHandle), 0.0f, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsPolicy::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(PolicyAgentIteration, INDEX_NONE, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle), FLT_MAX, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(MemoryStateHandle), FLT_MAX, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsPolicy::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(PolicyAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle), 0.0f, AgentIds);
		UE::Learning::Array::Set<2, float>(Manager->GetInstanceData()->View(MemoryStateHandle), 0.0f, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

ULearningAgentsNeuralNetwork* ULearningAgentsPolicy::GetNetworkAsset()
{
	return Network;
}

UE::Learning::INeuralNetwork& ULearningAgentsPolicy::GetPolicyNetwork()
{
	return *Network->NeuralNetworkData->GetNetworkInterface();
}

UE::Learning::FNeuralNetworkPolicyFunction& ULearningAgentsPolicy::GetPolicyObject()
{
	return *PolicyObject;
}

void ULearningAgentsPolicy::LoadPolicyFromSnapshot(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromSnapshot(File);
}

void ULearningAgentsPolicy::SavePolicyToSnapshot(const FFilePath& File) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToSnapshot(File);
}

void ULearningAgentsPolicy::UsePolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!NeuralNetworkAsset || !NeuralNetworkAsset->NeuralNetworkData->GetNetworkInterface())
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
	PolicyObject->UpdateNeuralNetwork(Network->NeuralNetworkData->GetNetworkInterface());
}

void ULearningAgentsPolicy::LoadPolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromAsset(NeuralNetworkAsset);
}

void ULearningAgentsPolicy::SavePolicyToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToAsset(NeuralNetworkAsset);
}

void ULearningAgentsPolicy::EvaluatePolicy()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::EvaluatePolicy);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check Agents actually have encoded observations.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not made observations so policy will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Record pre-evaluation state

	TLearningArrayView<2, float> PreEvaluationMemoryStateView = Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle);
	TLearningArrayView<2, float> MemoryStateView = Manager->GetInstanceData()->View(MemoryStateHandle);

	UE::Learning::Array::Copy<2, float>(PreEvaluationMemoryStateView, MemoryStateView, ValidAgentSet);

	// Copy Observations and Memory State into input buffer

	TLearningArrayView<2, const float> ObservationsView = Manager->GetInstanceData()->ConstView(Interactor->GetObservationFeature().FeatureHandle);
	TLearningArrayView<2, float> InputObservationView = Manager->GetInstanceData()->View(PolicyObject->InputObservationHandle);
	TLearningArrayView<2, float> InputMemoryStateView = Manager->GetInstanceData()->View(PolicyObject->InputMemoryStateHandle);

	UE::Learning::Array::Copy<2, float>(InputObservationView, ObservationsView, ValidAgentSet);
	UE::Learning::Array::Copy<2, float>(InputMemoryStateView, MemoryStateView, ValidAgentSet);

	// Evaluate Policy

	PolicyObject->Evaluate(ValidAgentSet);

	// Increment Policy Evaluation Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		PolicyAgentIteration[AgentId]++;
	}

	// Copy Actions and Memory State out of output buffer

	TLearningArrayView<2, float> ActionsView = Manager->GetInstanceData()->View(Interactor->GetActionFeature().FeatureHandle);
	TLearningArrayView<2, const float> OutputActionView = Manager->GetInstanceData()->ConstView(PolicyObject->OutputActionHandle);
	TLearningArrayView<2, const float> OutputMemoryStateView = Manager->GetInstanceData()->ConstView(PolicyObject->OutputMemoryStateHandle);

	UE::Learning::Array::Copy<2, float>(ActionsView, OutputActionView, ValidAgentSet);
	UE::Learning::Array::Copy<2, float>(MemoryStateView, OutputMemoryStateView, ValidAgentSet);

	// Increment Action Encoding Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		Interactor->GetActionEncodingAgentIteration()[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	VisualLog(ValidAgentSet);
#endif
}

void ULearningAgentsPolicy::RunInference()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::RunInference);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Interactor->EncodeObservations();
	EvaluatePolicy();
	Interactor->DecodeActions();
}
float ULearningAgentsPolicy::GetActionNoiseScale() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	return PolicyObject->InstanceData->ConstView(PolicyObject->ActionNoiseScaleHandle)[0];
}

void ULearningAgentsPolicy::SetActionNoiseScale(const float ActionNoiseScale)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Array::Set(PolicyObject->InstanceData->View(PolicyObject->ActionNoiseScaleHandle), ActionNoiseScale);
}

void ULearningAgentsPolicy::GetMemoryState(TArray<float>& OutMemoryState, const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutMemoryState.Empty();
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutMemoryState.Empty();
		return;
	}

	TLearningArrayView<2, const float> MemoryStateView = Manager->GetInstanceData()->ConstView(MemoryStateHandle);

	OutMemoryState.SetNumUninitialized(MemoryStateView.Num<1>());
	UE::Learning::Array::Copy<1, float>(OutMemoryState, MemoryStateView[AgentId]);
}

void ULearningAgentsPolicy::SetMemoryState(const int32 AgentId, const TArray<float>& InMemoryState)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	TLearningArrayView<2, float> MemoryStateView = Manager->GetInstanceData()->View(MemoryStateHandle);

	if (InMemoryState.Num() != MemoryStateView.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Memory State is incorrect size. Expected %i, got %i."), *GetName(), MemoryStateView.Num<1>(), InMemoryState.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(MemoryStateView[AgentId], InMemoryState);
}

TLearningArrayView<2, const float> ULearningAgentsPolicy::GetPreEvaluationMemoryStateView() const
{
	return Manager->GetInstanceData()->ConstView(PreEvaluationMemoryStateHandle);
}

TLearningArrayView<2, float> ULearningAgentsPolicy::GetPreEvaluationMemoryStateView()
{
	return Manager->GetInstanceData()->View(PreEvaluationMemoryStateHandle);
}

TLearningArrayView<2, const float> ULearningAgentsPolicy::GetMemoryStateView() const
{
	return Manager->GetInstanceData()->ConstView(MemoryStateHandle);
}

TLearningArrayView<2, float> ULearningAgentsPolicy::GetMemoryStateView()
{
	return Manager->GetInstanceData()->View(MemoryStateHandle);
}

int32 ULearningAgentsPolicy::GetMemoryStateSize() const
{
	return Manager->GetInstanceData()->ConstView(MemoryStateHandle).Num<1>();
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void ULearningAgentsPolicy::VisualLog(const UE::Learning::FIndexSet AgentSet) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::VisualLog);

	const TLearningArrayView<2, const float> InputObservationView = PolicyObject->InstanceData->ConstView(PolicyObject->InputObservationHandle);
	const TLearningArrayView<2, const float> InputMemoryStateView = PolicyObject->InstanceData->ConstView(PolicyObject->InputMemoryStateHandle);
	const TLearningArrayView<2, const float> OutputActionView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputActionHandle);
	const TLearningArrayView<2, const float> OutputActionMeanView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputActionMeanHandle);
	const TLearningArrayView<2, const float> OutputActionStdView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputActionStdHandle);
	const TLearningArrayView<1, const float> ActionNoiseScaleView = PolicyObject->InstanceData->ConstView(PolicyObject->ActionNoiseScaleHandle);

	for (const int32 AgentId : AgentSet)
	{
		if (const AActor* Actor = Cast<AActor>(GetAgent(AgentId)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nAction Noise Scale: [% 6.3f]\nObservation Input: %s\nObservation Input Stats (Min/Max/Mean/Std): %s\nMemory State Input: %s\nMemory State Input Stats (Min/Max/Mean/Std): %s\nOutput Mean: %s\nOutput Std: %s\nOutput Sample: %s\nOutput Stats (Min/Max/Mean/Std): %s"),
				AgentId,
				ActionNoiseScaleView[AgentId],
				*UE::Learning::Array::FormatFloat(InputObservationView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputObservationView[AgentId]),
				*UE::Learning::Array::FormatFloat(InputMemoryStateView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputMemoryStateView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputActionMeanView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputActionStdView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputActionView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(OutputActionView[AgentId]));

		}
	}
}
#endif
