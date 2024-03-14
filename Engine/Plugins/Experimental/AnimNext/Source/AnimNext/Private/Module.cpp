// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IAnimNextModule.h"
#include "AnimNextConfig.h"
#include "Animation/BlendProfile.h"
#include "Curves/CurveFloat.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataRegistry.h"
#include "TraitCore/TraitRegistry.h"
#include "TraitCore/NodeTemplateRegistry.h"
#include "Graph/AnimNextGraph.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Animation/AnimSequence.h"
#include "Scheduler/Scheduler.h"
#include "Param/ExternalParameterRegistry.h"
#include "Param/ObjectProxyFactory.h"

// Enable console commands only in development builds when logging is enabled
#define WITH_ANIMNEXT_CONSOLE_COMMANDS (!UE_BUILD_SHIPPING && !NO_LOGGING)

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

#include "TraitCore/TraitTemplate.h"
#include "TraitCore/NodeDescription.h"
#include "TraitCore/NodeTemplate.h"
#endif

namespace UE::AnimNext
{

class FModule : public IAnimNextModule
{
public:
	virtual void StartupModule() override
	{
		GetMutableDefault<UAnimNextConfig>()->LoadConfig();

		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UAnimSequence::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UScriptStruct::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UBlendProfile::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UCurveFloat::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UAnimNextGraph::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		};

		FRigVMRegistry::Get().RegisterObjectTypes(AllowedObjectTypes);

		FObjectProxyFactory::Init();
		FExternalParameterRegistry::Init();
		FDataRegistry::Init();
		FTraitRegistry::Init();
		FNodeTemplateRegistry::Init();
		FScheduler::Init();
		FRigVMRuntimeDataRegistry::Init();

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
		if (!IsRunningCommandlet())
		{
			ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("AnimNext.ListNodeTemplates"),
				TEXT("Dumps statistics about node templates to the log."),
				FConsoleCommandWithArgsDelegate::CreateRaw(this, &FModule::ListNodeTemplates),
				ECVF_Default
			));
			ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("AnimNext.ListAnimGraphs"),
				TEXT("Dumps statistics about animation graphs to the log."),
				FConsoleCommandWithArgsDelegate::CreateRaw(this, &FModule::ListAnimGraphs),
				ECVF_Default
			));
		}
#endif
	}

	virtual void ShutdownModule() override
	{
		FRigVMRuntimeDataRegistry::Destroy();
		FScheduler::Destroy();
		FNodeTemplateRegistry::Destroy();
		FTraitRegistry::Destroy();
		FDataRegistry::Destroy();
		FObjectProxyFactory::Destroy();
		FExternalParameterRegistry::Destroy();

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
		for (IConsoleObject* Cmd : ConsoleCommands)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Cmd);
		}
		ConsoleCommands.Empty();
#endif
	}

	const IAnimNextAnimGraph* AnimGraphImpl = nullptr;

	virtual void RegisterAnimNextAnimGraph(const IAnimNextAnimGraph& InAnimGraphImpl) override
	{
		AnimGraphImpl = &InAnimGraphImpl;
	}

	virtual void UnregisterAnimNextAnimGraph() override
	{
		AnimGraphImpl = nullptr;
	}

	virtual void UpdateGraph(FAnimNextGraphInstancePtr& GraphInstance, float DeltaTime) override
	{
		if (AnimGraphImpl != nullptr)
		{
			AnimGraphImpl->UpdateGraph(GraphInstance, DeltaTime);
		}
	}

	virtual void EvaluateGraph(FAnimNextGraphInstancePtr& GraphInstance, const UE::AnimNext::FReferencePose& RefPose, int32 GraphLODLevel, FLODPoseHeap& OutputPose) const override
	{
		if (AnimGraphImpl != nullptr)
		{
			AnimGraphImpl->EvaluateGraph(GraphInstance, RefPose, GraphLODLevel, OutputPose);
		}
	}

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	TArray<IConsoleObject*> ConsoleCommands;

	void ListNodeTemplates(const TArray<FString>& Args)
	{
		// Turn off log times to make diff-ing easier
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		// Make sure to log everything
		const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
		LogAnimation.SetVerbosity(ELogVerbosity::All);

		const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();

		UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Node Templates ====="));
		UE_LOG(LogAnimation, Log, TEXT("Template Buffer Size: %u bytes"), NodeTemplateRegistry.TemplateBuffer.GetAllocatedSize());

		for (auto It = NodeTemplateRegistry.TemplateUIDToHandleMap.CreateConstIterator(); It; ++It)
		{
			const FNodeTemplateRegistryHandle Handle = It.Value();
			const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(Handle);

			const uint32 NumTraits = NodeTemplate->GetNumTraits();

			UE_LOG(LogAnimation, Log, TEXT("[%x] has %u traits ..."), NodeTemplate->GetUID(), NumTraits);
			UE_LOG(LogAnimation, Log, TEXT("    Template Size: %u bytes"), NodeTemplate->GetNodeTemplateSize());
			UE_LOG(LogAnimation, Log, TEXT("    Shared Data Size: %u bytes"), NodeTemplate->GetNodeSharedDataSize());
			UE_LOG(LogAnimation, Log, TEXT("    Instance Data Size: %u bytes"), NodeTemplate->GetNodeInstanceDataSize());
			UE_LOG(LogAnimation, Log, TEXT("    Traits ..."));

			const FTraitTemplate* TraitTemplates = NodeTemplate->GetTraits();
			for (uint32 TraitIndex = 0; TraitIndex < NumTraits; ++TraitIndex)
			{
				const FTraitTemplate* TraitTemplate = TraitTemplates + TraitIndex;
				const FTrait* Trait = TraitRegistry.Find(TraitTemplate->GetRegistryHandle());
				const FString TraitName = Trait != nullptr ? Trait->GetTraitName() : TEXT("<Unknown>");

				const uint32 NextTraitIndex = TraitIndex + 1;
				const uint32 EndOfNextTraitSharedData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
				const uint32 TraitSharedDataSize = EndOfNextTraitSharedData - TraitTemplate->GetNodeSharedOffset();

				const uint32 EndOfNextTraitInstanceData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
				const uint32 TraitInstanceDataSize = EndOfNextTraitInstanceData - TraitTemplate->GetNodeInstanceOffset();

				UE_LOG(LogAnimation, Log, TEXT("            %u: [%x] %s (%s)"), TraitIndex, TraitTemplate->GetUID().GetUID(), *TraitName, TraitTemplate->GetMode() == ETraitMode::Base ? TEXT("Base") : TEXT("Additive"));
				UE_LOG(LogAnimation, Log, TEXT("                Shared Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeSharedOffset(), TraitSharedDataSize);
				if (TraitTemplate->HasLatentProperties() && Trait != nullptr)
				{
					UE_LOG(LogAnimation, Log, TEXT("                Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), TraitTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Trait->GetNumLatentTraitProperties());
				}
				UE_LOG(LogAnimation, Log, TEXT("                Instance Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeInstanceOffset(), TraitInstanceDataSize);
			}
		}

		LogAnimation.SetVerbosity(OldVerbosity);
	}

	void ListAnimGraphs(const TArray<FString>& Args)
	{
		// Turn off log times to make diff-ing easier
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		// Make sure to log everything
		const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
		LogAnimation.SetVerbosity(ELogVerbosity::All);

		TArray<const UAnimNextGraph*> AnimGraphs;

		for (TObjectIterator<UAnimNextGraph> It; It; ++It)
		{
			AnimGraphs.Add(*It);
		}

		struct FCompareObjectNames
		{
			FORCEINLINE bool operator()(const UAnimNextGraph& Lhs, const UAnimNextGraph& Rhs) const
			{
				return Lhs.GetPathName().Compare(Rhs.GetPathName()) < 0;
			}
		};
		AnimGraphs.Sort(FCompareObjectNames());

		const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();
		const bool bDetailedOutput = true;

		UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Animation Graphs ====="));
		UE_LOG(LogAnimation, Log, TEXT("Num Graphs: %u"), AnimGraphs.Num());

		for (const UAnimNextGraph* AnimGraph : AnimGraphs)
		{
			uint32 TotalInstanceSize = 0;
			uint32 NumNodes = 0;
			{
				// We always have a node at offset 0
				int32 NodeOffset = 0;

				while (NodeOffset < AnimGraph->SharedDataBuffer.Num())
				{
					const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimGraph->SharedDataBuffer[NodeOffset]);

					TotalInstanceSize += NodeDesc->GetNodeInstanceDataSize();
					NumNodes++;

					const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());
					NodeOffset += NodeTemplate->GetNodeSharedDataSize();
				}
			}

			UE_LOG(LogAnimation, Log, TEXT("    %s ..."), *AnimGraph->GetPathName());
			UE_LOG(LogAnimation, Log, TEXT("        Shared Data Size: %.2f KB"), double(AnimGraph->SharedDataBuffer.Num()) / 1024.0);
			UE_LOG(LogAnimation, Log, TEXT("        Max Instance Data Size: %.2f KB"), double(TotalInstanceSize) / 1024.0);
			UE_LOG(LogAnimation, Log, TEXT("        Num Nodes: %u"), NumNodes);

			if (bDetailedOutput)
			{
				// We always have a node at offset 0
				int32 NodeOffset = 0;

				while (NodeOffset < AnimGraph->SharedDataBuffer.Num())
				{
					const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimGraph->SharedDataBuffer[NodeOffset]);
					const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());

					const uint32 NumTraits = NodeTemplate->GetNumTraits();

					UE_LOG(LogAnimation, Log, TEXT("        Node %u: [Template %x with %u traits]"), NodeDesc->GetUID().GetNodeIndex(), NodeTemplate->GetUID(), NumTraits);
					UE_LOG(LogAnimation, Log, TEXT("            Shared Data: [Offset: %u bytes, Size: %u bytes]"), NodeOffset, NodeTemplate->GetNodeSharedDataSize());
					UE_LOG(LogAnimation, Log, TEXT("            Instance Data Size: %u bytes"), NodeDesc->GetNodeInstanceDataSize());
					UE_LOG(LogAnimation, Log, TEXT("            Traits ..."));

					const FTraitTemplate* TraitTemplates = NodeTemplate->GetTraits();
					for (uint32 TraitIndex = 0; TraitIndex < NumTraits; ++TraitIndex)
					{
						const FTraitTemplate* TraitTemplate = TraitTemplates + TraitIndex;
						const FTrait* Trait = TraitRegistry.Find(TraitTemplate->GetRegistryHandle());
						const FString TraitName = Trait != nullptr ? Trait->GetTraitName() : TEXT("<Unknown>");

						const uint32 NextTraitIndex = TraitIndex + 1;
						const uint32 EndOfNextTraitSharedData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
						const uint32 TraitSharedDataSize = EndOfNextTraitSharedData - TraitTemplate->GetNodeSharedOffset();

						const uint32 EndOfNextTraitInstanceData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
						const uint32 TraitInstanceDataSize = EndOfNextTraitInstanceData - TraitTemplate->GetNodeInstanceOffset();

						UE_LOG(LogAnimation, Log, TEXT("                    %u: [%x] %s (%s)"), TraitIndex, TraitTemplate->GetUID().GetUID(), *TraitName, TraitTemplate->GetMode() == ETraitMode::Base ? TEXT("Base") : TEXT("Additive"));
						UE_LOG(LogAnimation, Log, TEXT("                        Shared Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeSharedOffset(), TraitSharedDataSize);
						if (TraitTemplate->HasLatentProperties() && Trait != nullptr)
						{
							UE_LOG(LogAnimation, Log, TEXT("                        Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), TraitTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Trait->GetNumLatentTraitProperties());
						}
						UE_LOG(LogAnimation, Log, TEXT("                        Instance Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeInstanceOffset(), TraitInstanceDataSize);
					}

					NodeOffset += NodeTemplate->GetNodeSharedDataSize();
				}
			}
		}

		LogAnimation.SetVerbosity(OldVerbosity);
	}
#endif
};

IAnimNextModule& IAnimNextModule::Get()
{
	return FModuleManager::LoadModuleChecked<IAnimNextModule>(TEXT("AnimNext"));
}

}

IMPLEMENT_MODULE(UE::AnimNext::FModule, AnimNext)
