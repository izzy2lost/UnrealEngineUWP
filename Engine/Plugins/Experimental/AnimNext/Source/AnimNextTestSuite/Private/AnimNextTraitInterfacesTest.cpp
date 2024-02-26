// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextTraitInterfacesTest.h"
#include "AnimNextRuntimeTest.h"
#include "AnimNextTest.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Serialization/MemoryReader.h"

#include "TraitCore/Trait.h"
#include "TraitCore/TraitReader.h"
#include "TraitCore/TraitRegistry.h"
#include "TraitCore/TraitWriter.h"
#include "TraitCore/ExecutionContext.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/NodeInstance.h"
#include "TraitCore/NodeTemplateBuilder.h"
#include "TraitCore/NodeTemplateRegistry.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/IHierarchy.h"
#include "TraitInterfaces/IUpdate.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/GraphFactory.h"

//****************************************************************************
// AnimNext Runtime TraitInterfaces Tests
//****************************************************************************

namespace UE::AnimNext
{
	namespace Private
	{
		static TArray<FTraitUID>* UpdatedTraits = nullptr;
		static TArray<FTraitUID>* EvaluatedTraits = nullptr;
	}

	struct FTraitWithNoChildren : FBaseTrait, IUpdate, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FTraitWithNoChildren, 0x79080a53, FBaseTrait)

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override
		{
			if (Private::UpdatedTraits != nullptr)
			{
				Private::UpdatedTraits->Add(FTraitWithNoChildren::TraitUID);
			}

			IUpdate::PreUpdate(Context, Binding, TraitState);
		}

		virtual void PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override
		{
			if (Private::UpdatedTraits != nullptr)
			{
				Private::UpdatedTraits->Add(FTraitWithNoChildren::TraitUID);
			}

			IUpdate::PostUpdate(Context, Binding, TraitState);
		}

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedTraits != nullptr)
			{
				Private::EvaluatedTraits->Add(FTraitWithNoChildren::TraitUID);
			}

			IEvaluate::PreEvaluate(Context, Binding);
		}

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedTraits != nullptr)
			{
				Private::EvaluatedTraits->Add(FTraitWithNoChildren::TraitUID);
			}

			IEvaluate::PostEvaluate(Context, Binding);
		}
	};

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FTraitWithNoChildren, TRAIT_INTERFACE_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	// This trait does not update or evaluate
	struct FTraitWithOneChild : FBaseTrait, IHierarchy
	{
		DECLARE_ANIM_TRAIT(FTraitWithOneChild, 0xab680b35, FBaseTrait)

		using FSharedData = FTraitWithOneChildSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			FTraitPtr Child;

			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding)
			{
				Child = Context.AllocateNodeInstance(Binding.GetTraitPtr(), Binding.GetSharedData<FSharedData>()->Child);
			}
		};

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const override
		{
			return 1;
		}

		virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			Children.Add(InstanceData->Child);
		}
	};

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IHierarchy) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FTraitWithOneChild, TRAIT_INTERFACE_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR

	struct FTraitWithChildren : FBaseTrait, IHierarchy, IUpdate, IUpdateTraversal, IEvaluate
	{
		DECLARE_ANIM_TRAIT(FTraitWithChildren, 0x4b296948, FBaseTrait)

		using FSharedData = FTraitWithChildrenSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
			FTraitPtr Children[2];

			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding)
			{
				Children[0] = Context.AllocateNodeInstance(Binding.GetTraitPtr(), Binding.GetSharedData<FSharedData>()->Children[0]);
				Children[1] = Context.AllocateNodeInstance(Binding.GetTraitPtr(), Binding.GetSharedData<FSharedData>()->Children[1]);
			}
		};

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const override
		{
			return 2;
		}

		virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			Children.Add(InstanceData->Children[0]);
			Children.Add(InstanceData->Children[1]);
		}

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override
		{
			if (Private::UpdatedTraits != nullptr)
			{
				Private::UpdatedTraits->Add(FTraitWithChildren::TraitUID);
			}

			IUpdate::PreUpdate(Context, Binding, TraitState);
		}

		virtual void PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override
		{
			if (Private::UpdatedTraits != nullptr)
			{
				Private::UpdatedTraits->Add(FTraitWithChildren::TraitUID);
			}

			IUpdate::PostUpdate(Context, Binding, TraitState);
		}

		// IUpdateTraversal impl
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TTraitBinding<IUpdateTraversal>& Binding, const FTraitUpdateState& TraitState, FUpdateTraversalQueue& TraversalQueue) const override
		{
			const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

			TraversalQueue.Push(InstanceData->Children[0], TraitState);
			TraversalQueue.Push(InstanceData->Children[1], TraitState);
		}

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedTraits != nullptr)
			{
				Private::EvaluatedTraits->Add(FTraitWithChildren::TraitUID);
			}

			IEvaluate::PreEvaluate(Context, Binding);
		}

		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const override
		{
			if (Private::EvaluatedTraits != nullptr)
			{
				Private::EvaluatedTraits->Add(FTraitWithChildren::TraitUID);
			}

			IEvaluate::PostEvaluate(Context, Binding);
		}
	};

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IEvaluate) \
		GeneratorMacro(IHierarchy) \
		GeneratorMacro(IUpdate) \
		GeneratorMacro(IUpdateTraversal) \

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FTraitWithChildren, TRAIT_INTERFACE_ENUMERATOR)
	#undef TRAIT_INTERFACE_ENUMERATOR
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IHierarchy, "Animation.AnimNext.Runtime.IHierarchy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IHierarchy::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	{
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithNoChildren)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithOneChild)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single trait with no children
		TArray<FTraitUID> NodeTemplateTraitListA;
		NodeTemplateTraitListA.Add(FTraitWithNoChildren::TraitUID);

		// Template B has a single trait with one child
		TArray<FTraitUID> NodeTemplateTraitListB;
		NodeTemplateTraitListB.Add(FTraitWithOneChild::TraitUID);

		// Template C has two traits, each with one child
		TArray<FTraitUID> NodeTemplateTraitListC;
		NodeTemplateTraitListC.Add(FTraitWithOneChild::TraitUID);
		NodeTemplateTraitListC.Add(FTraitWithOneChild::TraitUID);

		// Template D has a single trait with children
		TArray<FTraitUID> NodeTemplateTraitListD;
		NodeTemplateTraitListD.Add(FTraitWithChildren::TraitUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC, NodeTemplateBufferD;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListC, NodeTemplateBufferC);
		const FNodeTemplate* NodeTemplateD = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListD, NodeTemplateBufferD);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA
		// NodeC has two children: NodeA and NodeB (but both traits are base, only NodeB will be referenced)
		// NodeD has two children: NodeA and NodeC

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FTraitWriter TraitWriter;

			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateB));
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateC));
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateD));

			// We don't have trait properties
			TArray<TMap<FName, FString>> TraitPropertiesA;
			TraitPropertiesA.AddDefaulted(NodeTemplateTraitListA.Num());

			TArray<TMap<FName, FString>> TraitPropertiesB;
			TraitPropertiesB.AddDefaulted(NodeTemplateTraitListB.Num());
			TraitPropertiesB[0].Add(TEXT("Child"), ToString<FTraitWithOneChild::FSharedData>(TEXT("Child"), FAnimNextTraitHandle(NodeHandles[0])));

			TArray<TMap<FName, FString>> TraitPropertiesC;
			TraitPropertiesC.AddDefaulted(NodeTemplateTraitListC.Num());
			TraitPropertiesC[0].Add(TEXT("Child"), ToString<FTraitWithOneChild::FSharedData>(TEXT("Child"), FAnimNextTraitHandle(NodeHandles[0])));
			TraitPropertiesC[1].Add(TEXT("Child"), ToString<FTraitWithOneChild::FSharedData>(TEXT("Child"), FAnimNextTraitHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> TraitPropertiesD;
			TraitPropertiesD.AddDefaulted(NodeTemplateTraitListD.Num());
			FAnimNextTraitHandle ChildrenHandlesD[2] = { FAnimNextTraitHandle(NodeHandles[0]), FAnimNextTraitHandle(NodeHandles[2], 1)};
			TraitPropertiesD[0].Add(TEXT("Children"), ToString<FTraitWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesD));

			TraitWriter.BeginNodeWriting();
			TraitWriter.WriteNode(NodeHandles[0],
				[&TraitPropertiesA](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesA[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[1],
				[&TraitPropertiesB](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesB[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[2],
				[&TraitPropertiesC](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesC[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[3],
				[&TraitPropertiesD](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesD[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.EndNodeWriting();

			AddErrorIfFalse(TraitWriter.GetErrorState() == FTraitWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to write traits");
			GraphSharedDataArchiveBuffer = TraitWriter.GetGraphSharedData();
			GraphReferencedObjects = TraitWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		{
			FMemMark Mark(FMemStack::Get());

			FAnimNextTraitHandle RootHandle(NodeHandles[3]);	// Point to NodeD, first base trait

			FTraitPtr NodeDPtr = Context.AllocateNodeInstance(*GraphInstance.GetImpl(), RootHandle);
			AddErrorIfFalse(NodeDPtr.IsValid(), "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to allocate root node instance");

			FTraitStackBinding StackNodeD;
			AddErrorIfFalse(Context.GetStack(NodeDPtr, StackNodeD), "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to bind to trait stack");

			TTraitBinding<IHierarchy> HierarchyBindingNodeD;
			AddErrorIfFalse(StackNodeD.GetInterface(HierarchyBindingNodeD), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

			FChildrenArray ChildrenNodeD;
			HierarchyBindingNodeD.GetChildren(Context, ChildrenNodeD);

			AddErrorIfFalse(ChildrenNodeD.Num() == 2, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
			AddErrorIfFalse(HierarchyBindingNodeD.GetNumChildren(Context) == 2, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
			AddErrorIfFalse(ChildrenNodeD[0].IsValid() && ChildrenNodeD[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");
			AddErrorIfFalse(ChildrenNodeD[1].IsValid() && ChildrenNodeD[1].GetNodeInstance()->GetNodeHandle() == NodeHandles[2], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeC");

			ChildrenNodeD.Reset();
			IHierarchy::GetStackChildren(Context, StackNodeD, ChildrenNodeD);

			AddErrorIfFalse(ChildrenNodeD.Num() == 2, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
			AddErrorIfFalse(IHierarchy::GetNumStackChildren(Context, StackNodeD) == 2, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 2 children");
			AddErrorIfFalse(ChildrenNodeD[0].IsValid() && ChildrenNodeD[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");
			AddErrorIfFalse(ChildrenNodeD[1].IsValid() && ChildrenNodeD[1].GetNodeInstance()->GetNodeHandle() == NodeHandles[2], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeC");

			{
				FTraitStackBinding StackNodeC;
				AddErrorIfFalse(Context.GetStack(ChildrenNodeD[1], StackNodeC), "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to bind to trait stack");

				TTraitBinding<IHierarchy> HierarchyBindingNodeC;
				AddErrorIfFalse(StackNodeC.GetInterface(HierarchyBindingNodeC), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

				FChildrenArray ChildrenNodeC;
				HierarchyBindingNodeC.GetChildren(Context, ChildrenNodeC);

				AddErrorIfFalse(ChildrenNodeC.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
				AddErrorIfFalse(HierarchyBindingNodeC.GetNumChildren(Context) == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
				AddErrorIfFalse(ChildrenNodeC[0].IsValid() && ChildrenNodeC[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[1], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeB");

				ChildrenNodeC.Reset();
				IHierarchy::GetStackChildren(Context, StackNodeC, ChildrenNodeC);

				AddErrorIfFalse(ChildrenNodeC.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
				AddErrorIfFalse(IHierarchy::GetNumStackChildren(Context, StackNodeC) == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
				AddErrorIfFalse(ChildrenNodeC[0].IsValid() && ChildrenNodeC[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[1], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeB");

				{
					FTraitStackBinding StackNodeB;
					AddErrorIfFalse(Context.GetStack(ChildrenNodeC[0], StackNodeB), "FAnimationAnimNextRuntimeTest_IHierarchy -> Failed to bind to trait stack");

					TTraitBinding<IHierarchy> HierarchyBindingNodeB;
					AddErrorIfFalse(StackNodeB.GetInterface(HierarchyBindingNodeB), "FAnimationAnimNextRuntimeTest_IHierarchy -> IHierarchy not found");

					FChildrenArray ChildrenNodeB;
					HierarchyBindingNodeB.GetChildren(Context, ChildrenNodeB);

					AddErrorIfFalse(ChildrenNodeB.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
					AddErrorIfFalse(HierarchyBindingNodeB.GetNumChildren(Context) == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
					AddErrorIfFalse(ChildrenNodeB[0].IsValid() && ChildrenNodeB[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");

					ChildrenNodeB.Reset();
					IHierarchy::GetStackChildren(Context, StackNodeB, ChildrenNodeB);

					AddErrorIfFalse(ChildrenNodeB.Num() == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
					AddErrorIfFalse(IHierarchy::GetNumStackChildren(Context, StackNodeB) == 1, "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected 1 child");
					AddErrorIfFalse(ChildrenNodeB[0].IsValid() && ChildrenNodeB[0].GetNodeInstance()->GetNodeHandle() == NodeHandles[0], "FAnimationAnimNextRuntimeTest_IHierarchy -> Expected child: NodeA");
				}
			}
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);
		Registry.Unregister(NodeTemplateD);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IHierarchy -> Registry should contain 0 templates");
	}

	Tests::FUtils::CleanupAfterTests();
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IUpdate, "Animation.AnimNext.Runtime.IUpdate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IUpdate::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;
	{
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithNoChildren)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithOneChild)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IUpdate -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single trait with no children
		TArray<FTraitUID> NodeTemplateTraitListA;
		NodeTemplateTraitListA.Add(FTraitWithNoChildren::TraitUID);

		// Template B has a single trait with one child, it doesn't update
		TArray<FTraitUID> NodeTemplateTraitListB;
		NodeTemplateTraitListB.Add(FTraitWithOneChild::TraitUID);

		// Template C has a single trait with children
		TArray<FTraitUID> NodeTemplateTraitListC;
		NodeTemplateTraitListC.Add(FTraitWithChildren::TraitUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListC, NodeTemplateBufferC);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA (it doesn't update)
		// NodeC (root) has two children: NodeA and NodeB

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FTraitWriter TraitWriter;

			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateC));	// Root node
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateB));

			// We don't have trait properties
			TArray<TMap<FName, FString>> TraitPropertiesA;
			TraitPropertiesA.AddDefaulted(NodeTemplateTraitListA.Num());

			TArray<TMap<FName, FString>> TraitPropertiesB;
			TraitPropertiesB.AddDefaulted(NodeTemplateTraitListB.Num());
			TraitPropertiesB[0].Add(TEXT("Child"), ToString<FTraitWithOneChild::FSharedData>(TEXT("Child"), FAnimNextTraitHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> TraitPropertiesC;
			TraitPropertiesC.AddDefaulted(NodeTemplateTraitListC.Num());
			FAnimNextTraitHandle ChildrenHandlesC[2] = { FAnimNextTraitHandle(NodeHandles[1]), FAnimNextTraitHandle(NodeHandles[2])};
			TraitPropertiesC[0].Add(TEXT("Children"), ToString<FTraitWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesC));

			TraitWriter.BeginNodeWriting();
			TraitWriter.WriteNode(NodeHandles[0],
				[&TraitPropertiesC](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesC[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[1],
				[&TraitPropertiesA](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesA[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[2],
				[&TraitPropertiesB](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesB[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.EndNodeWriting();

			AddErrorIfFalse(TraitWriter.GetErrorState() == FTraitWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IUpdate -> Failed to write traits");
			GraphSharedDataArchiveBuffer = TraitWriter.GetGraphSharedData();
			GraphReferencedObjects = TraitWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		FExecutionContext Context(GraphInstance);

		{
			TArray<FTraitUID> UpdatedTraits;
			Private::UpdatedTraits = &UpdatedTraits;

			// Call pre/post update on our graph
			UpdateGraph(GraphInstance, 0.0333f);

			AddErrorIfFalse(UpdatedTraits.Num() == 6, "FAnimationAnimNextRuntimeTest_IUpdate -> Expected 6 nodes to have been visited during the update traversal");
			AddErrorIfFalse(UpdatedTraits[0] == FTraitWithChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeC
			AddErrorIfFalse(UpdatedTraits[1] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeA
			AddErrorIfFalse(UpdatedTraits[2] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeA
			AddErrorIfFalse(UpdatedTraits[3] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeB -> NodeA
			AddErrorIfFalse(UpdatedTraits[4] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeB -> NodeA
			AddErrorIfFalse(UpdatedTraits[5] == FTraitWithChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IUpdate -> Unexpected update order");		// NodeC

			Private::UpdatedTraits = nullptr;
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IUpdate -> Registry should contain 0 templates");
	}
	Tests::FUtils::CleanupAfterTests();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextRuntimeTest_IEvaluate, "Animation.AnimNext.Runtime.IEvaluate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextRuntimeTest_IEvaluate::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;
	{
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithNoChildren)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithOneChild)
		AUTO_REGISTER_ANIM_TRAIT(FTraitWithChildren)

		UFactory* GraphFactory = NewObject<UAnimNextGraphFactory>();
		UAnimNextGraph* AnimNextGraph = CastChecked<UAnimNextGraph>(GraphFactory->FactoryCreateNew(UAnimNextGraph::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextGraph"), RF_Transient, nullptr, nullptr, NAME_None));
		UE_RETURN_ON_ERROR(AnimNextGraph != nullptr, "FAnimationAnimNextRuntimeTest_IEvaluate -> Failed to create animation graph");

		FScopedClearNodeTemplateRegistry ScopedClearNodeTemplateRegistry;
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();

		// We create a few node templates
		// Template A has a single trait with no children
		TArray<FTraitUID> NodeTemplateTraitListA;
		NodeTemplateTraitListA.Add(FTraitWithNoChildren::TraitUID);

		// Template B has a single trait with one child, it doesn't evaluate
		TArray<FTraitUID> NodeTemplateTraitListB;
		NodeTemplateTraitListB.Add(FTraitWithOneChild::TraitUID);

		// Template C has a single trait with children
		TArray<FTraitUID> NodeTemplateTraitListC;
		NodeTemplateTraitListC.Add(FTraitWithChildren::TraitUID);

		// Populate our node template registry
		TArray<uint8> NodeTemplateBufferA, NodeTemplateBufferB, NodeTemplateBufferC;
		const FNodeTemplate* NodeTemplateA = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListA, NodeTemplateBufferA);
		const FNodeTemplate* NodeTemplateB = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListB, NodeTemplateBufferB);
		const FNodeTemplate* NodeTemplateC = FNodeTemplateBuilder::BuildNodeTemplate(NodeTemplateTraitListC, NodeTemplateBufferC);

		// Build our graph, it as follow (each node template has a single node instance):
		// NodeA has no children
		// NodeB has one child: NodeA (it doesn't evaluate)
		// NodeC (root) has two children: NodeA and NodeB

		TArray<FNodeHandle> NodeHandles;

		// Write our graph
		TArray<uint8> GraphSharedDataArchiveBuffer;
		TArray<TObjectPtr<UObject>> GraphReferencedObjects;
		{
			FTraitWriter TraitWriter;

			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateC));	// Root node
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateA));
			NodeHandles.Add(TraitWriter.RegisterNode(*NodeTemplateB));

			// We don't have trait properties
			TArray<TMap<FName, FString>> TraitPropertiesA;
			TraitPropertiesA.AddDefaulted(NodeTemplateTraitListA.Num());

			TArray<TMap<FName, FString>> TraitPropertiesB;
			TraitPropertiesB.AddDefaulted(NodeTemplateTraitListB.Num());
			TraitPropertiesB[0].Add(TEXT("Child"), ToString<FTraitWithOneChild::FSharedData>(TEXT("Child"), FAnimNextTraitHandle(NodeHandles[1])));

			TArray<TMap<FName, FString>> TraitPropertiesC;
			TraitPropertiesC.AddDefaulted(NodeTemplateTraitListC.Num());

			FAnimNextTraitHandle ChildrenHandlesC[2] = { FAnimNextTraitHandle(NodeHandles[1]), FAnimNextTraitHandle(NodeHandles[2])};
			TraitPropertiesC[0].Add(TEXT("Children"), ToString<FTraitWithChildren::FSharedData>(TEXT("Children"), ChildrenHandlesC));

			TraitWriter.BeginNodeWriting();
			TraitWriter.WriteNode(NodeHandles[0],
				[&TraitPropertiesC](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesC[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[1],
				[&TraitPropertiesA](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesA[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.WriteNode(NodeHandles[2],
				[&TraitPropertiesB](uint32 TraitIndex, FName PropertyName)
				{
					return TraitPropertiesB[TraitIndex][PropertyName];
				},
				[](uint32 TraitIndex, FName PropertyName)
				{
					return MAX_uint16;
				});
			TraitWriter.EndNodeWriting();

			AddErrorIfFalse(TraitWriter.GetErrorState() == FTraitWriter::EErrorState::None, "FAnimationAnimNextRuntimeTest_IEvaluate -> Failed to write traits");
			GraphSharedDataArchiveBuffer = TraitWriter.GetGraphSharedData();
			GraphReferencedObjects = TraitWriter.GetGraphReferencedObjects();
		}

		// Read our graph
		FTestUtils::LoadFromArchiveBuffer(*AnimNextGraph, NodeHandles, GraphSharedDataArchiveBuffer);

		FAnimNextGraphInstancePtr GraphInstance;
		AnimNextGraph->AllocateInstance(GraphInstance);

		{
			TArray<FTraitUID> EvaluatedTraits;
			Private::EvaluatedTraits = &EvaluatedTraits;

			// Call pre/post evaluate on our graph
			(void)EvaluateGraph(GraphInstance);

			AddErrorIfFalse(EvaluatedTraits.Num() == 6, "FAnimationAnimNextRuntimeTest_IEvaluate -> Expected 6 nodes to have been visited during the evaluate traversal");
			AddErrorIfFalse(EvaluatedTraits[0] == FTraitWithChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");		// NodeC
			AddErrorIfFalse(EvaluatedTraits[1] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeA
			AddErrorIfFalse(EvaluatedTraits[2] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeA
			AddErrorIfFalse(EvaluatedTraits[3] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeB -> NodeA
			AddErrorIfFalse(EvaluatedTraits[4] == FTraitWithNoChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");			// NodeB -> NodeA
			AddErrorIfFalse(EvaluatedTraits[5] == FTraitWithChildren::TraitUID, "FAnimationAnimNextRuntimeTest_IEvaluate -> Unexpected evaluate order");		// NodeC

			Private::EvaluatedTraits = nullptr;
		}

		Registry.Unregister(NodeTemplateA);
		Registry.Unregister(NodeTemplateB);
		Registry.Unregister(NodeTemplateC);

		AddErrorIfFalse(Registry.GetNum() == 0, "FAnimationAnimNextRuntimeTest_IEvaluate -> Registry should contain 0 templates");
	}
	Tests::FUtils::CleanupAfterTests();

	return true;
}
#endif
