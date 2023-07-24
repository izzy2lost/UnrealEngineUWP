// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SimModuleTree.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "Chaos/DebugDrawQueue.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

int FSimModuleTree::AddRoot(ISimulationModuleBase* SimModule)
{
	check(!bIsSimulating);
	return AddNodeBelow(-1, SimModule);
}

void FSimModuleTree::Reparent(int AtIndex, int ParentIndex)
{
	check(!bIsSimulating);
	check(AtIndex < SimulationModuleTree.Num());
	check(ParentIndex < SimulationModuleTree.Num());

	UE_LOG(LogSimulationModule, Log, TEXT("Reparent %s To %s")
		, *SimulationModuleTree[AtIndex].SimModule->GetDebugName()
		, *SimulationModuleTree[ParentIndex].SimModule->GetDebugName());

	int OrginalParent = SimulationModuleTree[AtIndex].Parent;
	if (OrginalParent != ParentIndex)
	{
		SimulationModuleTree[AtIndex].Parent = ParentIndex;
		SimulationModuleTree[ParentIndex].Children.Add(AtIndex);

		// if had a parent and wasn't a root
		if (OrginalParent != -1)
		{
			SimulationModuleTree[OrginalParent].Children.Remove(AtIndex);
		}
	}
}

int FSimModuleTree::AddNodeBelow(int AtIndex, ISimulationModuleBase* SimModule)
{
	check(!bIsSimulating);
	int NewIndex = GetNextIndex();
	FSimModuleNode& Node = SimulationModuleTree[NewIndex];
	SimModule->SetTreeIndex(NewIndex);
	Node.SimModule = SimModule;
	Node.Parent = AtIndex;
	if (AtIndex >= 0)
	{
		SimulationModuleTree[AtIndex].Children.Add(NewIndex);
	}
	else
	{
		Node.Parent = FSimModuleNode::INVALID_IDX;
	}

	return NewIndex;
}

void FSimModuleTree::AppendTreeUpdates(const FSimTreeUpdates& TreeUpdates)
{
	ensure(!IsSimulating());

	int TreeIndex = -1;
	TMap<int, int> SimTreeMapping;

	//if (GetNumNodes() == 0) //Always add a null node, so there is a parent when the root chassis is removed
	//{
	//	// add a single chassis root component
	//	Chaos::FChassisSettings Settings;
	//	Chaos::ISimulationModuleBase* Chassis = new Chaos::FChassisSimModule(Settings);
	//	ParentIndex = AddRoot(Chassis);
	//	Chassis->SetTransformIndex(-1);
	//}

	int LocalIndex = 0;
	for (const FPendingModuleAdds& TreeUpdate : TreeUpdates.GetNewModules())
	{
		int AddIndex = -1;
		if (int* AddIndexPtr = SimTreeMapping.Find(TreeUpdate.ParentIndex))
		{
			AddIndex = *AddIndexPtr;
		}

		ensure(!IsSimulating());

		TreeIndex = AddNodeBelow(AddIndex, TreeUpdate.NewSimModule);
		SimTreeMapping.Add(LocalIndex, TreeIndex);
		LocalIndex++;
	}

	TArray<int> ComponentIndices;
	for (const FPendingModuleDeletions& TreeUpdate : TreeUpdates.GetDeletedModules())
	{
		for (int Index = 0; Index < SimulationModuleTree.Num(); Index++)
		{
			if (Chaos::ISimulationModuleBase* SimModule = GetNode(Index).SimModule)
			{ 
				if (SimModule->GetGuid() == TreeUpdate.Guid)
				{
					ComponentIndices.AddUnique(SimModule->GetTransformIndex());
					DeleteNode(Index);
					break;
				}
			}
		}				
	}

	//// when the particle is removed the references will be out by one unless we fix them up
	//// this may only apply to ClusterUnion and not GeometryCollection
	//for (int ComponentIndex : ComponentIndices)
	//{
	//	for (int I = 0; I < GetNumNodes(); I++)
	//	{
	//		if (Chaos::ISimulationModuleBase* SimModule = GetNode(I).SimModule)
	//		{
	//			if (SimModule->GetTransformIndex() > ComponentIndex)
	//			{
	//				SimModule->SetTransformIndex(SimModule->GetTransformIndex() - 1);
	//			}

	//		}
	//	}
	//}

	for (int ComponentIndex : ComponentIndices)
	{
		// find the largest transform index
		int LargestIndex = -1;
		for (int I = 0; I < GetNumNodes(); I++)
		{
			if (Chaos::ISimulationModuleBase* SimModule = GetNode(I).SimModule)
			{
				if (SimModule->GetTransformIndex() > LargestIndex)
				{
					LargestIndex = SimModule->GetTransformIndex();
				}
			}
		}

		// swap with the one that has just been deleted 
		// - there can be more than one SimModule referencing the same component index
		for (int I = 0; I < GetNumNodes(); I++)
		{
			if (Chaos::ISimulationModuleBase* SimModule = GetNode(I).SimModule)
			{
				if (SimModule->GetTransformIndex() == LargestIndex)
				{
					SimModule->SetTransformIndex(ComponentIndex);
				}
			}
		}
	}

}


int FSimModuleTree::GetNextIndex()
{
	int NewIndex = FSimModuleNode::INVALID_IDX;
	if (FreeList.IsEmpty())
	{
		NewIndex = SimulationModuleTree.Num();
		SimulationModuleTree.AddZeroed(1);
		SimulationModuleTree[NewIndex].Parent = FSimModuleNode::INVALID_IDX;
		SimulationModuleTree[NewIndex].SimModule = nullptr;
	}
	else
	{
		NewIndex = FreeList.Pop();
	}

	return NewIndex;
}

int FSimModuleTree::InsertNodeAbove(int AtIndex, ISimulationModuleBase* SimModule)
{
	check(!bIsSimulating);
	int NewIndex = FSimModuleNode::INVALID_IDX;

	if (ensure(AtIndex < SimulationModuleTree.Num()))
	{
		NewIndex = GetNextIndex();
		FSimModuleNode& Node = SimulationModuleTree[NewIndex];

		int OriginalParentIdx = SimulationModuleTree[AtIndex].Parent;

		// remove current idx from children of parent & add new index in its place
		SimulationModuleTree[OriginalParentIdx].Children.Remove(AtIndex);
		SimulationModuleTree[OriginalParentIdx].Children.Add(NewIndex);

		SimulationModuleTree[AtIndex].Parent = NewIndex;

		Node.SimModule = SimModule;
		Node.Parent = OriginalParentIdx;	// new node takes parent from existing node
		Node.Children.Add(AtIndex);			// existing node becomes child of new node
		SimModule->SetTreeIndex(NewIndex);
	}

	return NewIndex;
}

void FSimModuleTree::DeleteNode(int AtIndex)
{
	check(!bIsSimulating);
	// multiple children might become equal parents?
	
	int ParentIndex = SimulationModuleTree[AtIndex].Parent;
	
	if (ParentIndex >= 0)
	{
		// remove from parents children list
		SimulationModuleTree[ParentIndex].Children.Remove(AtIndex);
	}

	// move deleted nodes children to parent and these children need new parent
	for (int ChildIndex : SimulationModuleTree[AtIndex].Children)
	{
		if (ParentIndex >= 0)
		{
			SimulationModuleTree[ParentIndex].Children.Add(ChildIndex);
		}
		SimulationModuleTree[ChildIndex].Parent = ParentIndex;
	}	

	SimulationModuleTree[AtIndex].Parent = FSimModuleNode::INVALID_IDX;
	SimulationModuleTree[AtIndex].Children.Empty();
	delete SimulationModuleTree[AtIndex].SimModule;
	SimulationModuleTree[AtIndex].SimModule = nullptr;

	FreeList.Push(AtIndex);

}


void FSimModuleTree::Simulate(float DeltaTime, FAllInputs& Inputs, FClusterUnionPhysicsProxy* PhysicsProxy)
{
	if (PhysicsProxy)
	{
		UpdateModuleVelocites(PhysicsProxy);
	}

	TArray<int> RootNodes;
	GetRootNodes(RootNodes);

	for (int RootIndex : RootNodes)
	{
		SimulateNode(DeltaTime, Inputs, RootIndex);
	}
}


void FSimModuleTree::Simulate(float DeltaTime, FAllInputs& Inputs, FGeometryCollectionPhysicsProxy* PhysicsProxy)
{
	if (PhysicsProxy)
	{
		UpdateModuleVelocites(PhysicsProxy);
	}

	TArray<int> RootNodes;
	GetRootNodes(RootNodes);

	for (int RootIndex : RootNodes)
	{
		SimulateNode(DeltaTime, Inputs, RootIndex);
	}

}

void FSimModuleTree::SimulateNode(float DeltaTime, FAllInputs& Inputs, int NodeIndex)
{
	if (ISimulationModuleBase* Module = AccessSimModule(NodeIndex))
	{
		if (Module->IsEnabled())
		{
			Module->Simulate(DeltaTime, Inputs, *this);
		}

		for (int ChildIdx : GetChildren(NodeIndex))
		{
			SimulateNode(DeltaTime, Inputs, ChildIdx);
		}
	}
}


void FSimModuleTree::DeleteNodesBelow(int AtIndex)
{
	if (IsValidNode(AtIndex))
	{
		for (int ChildIdx : GetChildren(AtIndex))
		{
			DeleteNodesBelow(ChildIdx);
		}

		delete SimulationModuleTree[AtIndex].SimModule;
		SimulationModuleTree[AtIndex].SimModule = nullptr;
		SimulationModuleTree[AtIndex].Children.Empty();
		SimulationModuleTree[AtIndex].Parent = FSimModuleNode ::INVALID_IDX;

		FreeList.Push(AtIndex);

	}
}


void FSimModuleTree::GetRootNodes(TArray<int>& RootNodesOut)
{
	RootNodesOut.Empty();

	// never assume the root bone is always index 0
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (SimulationModuleTree[i].SimModule != nullptr && SimulationModuleTree[i].Parent == FSimModuleNode::INVALID_IDX)
		{
			RootNodesOut.Add(i);
		}
	}
}

void FSimModuleTree::UpdateModuleVelocites(FGeometryCollectionPhysicsProxy* PhysicsProxy)
{
	check(PhysicsProxy);

	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = PhysicsProxy->GetSolverClusterHandles();
	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles = PhysicsProxy->GetSolverParticleHandles();

	// capture the velocities at the start of each sim iteration
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (ISimulationModuleBase* Module = SimulationModuleTree[i].SimModule)
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ParentParticle = Clusters[Module->GetTransformIndex()])
			{
				const FTransform BodyTransform(ParentParticle->R(), ParentParticle->X());

				if (Module->IsBehaviourType(eSimModuleTypeFlags::Velocity))
				{
					Chaos::FPBDRigidClusteredParticleHandle* Particle = nullptr;
					if (Module->IsClustered())
					{
						Particle = Clusters[Module->GetTransformIndex()];
					}
					else
					{
						Particle = Particles[Module->GetTransformIndex()];				
					}

					if (Particle)
					{
						FVector WorldLocation = BodyTransform.TransformPosition(Module->GetParentRelativeTransform().GetLocation());
						const Chaos::FVec3 Arm = WorldLocation - Particle->X();

						FVector WorldVelocity = Particle->V() - Chaos::FVec3::CrossProduct(Arm, Particle->W());
						FVector LocalVelocity = BodyTransform.InverseTransformVector(WorldVelocity);
						LocalVelocity = Module->GetClusteredTransform().InverseTransformVector(LocalVelocity);

						Module->SetLocalVelocity(LocalVelocity);
					}
			
				}
			}
		}
	}

}

void FSimModuleTree::UpdateModuleVelocites(FClusterUnionPhysicsProxy* PhysicsProxy)
{
	check(PhysicsProxy);

	// capture the velocities at the start of each sim iteration
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (ISimulationModuleBase* Module = SimulationModuleTree[i].SimModule)
		{
			if (const Chaos::FClusterUnionPhysicsProxy::FInternalParticle* ParentParticle = PhysicsProxy->GetParticle_Internal())
			{
				const FTransform BodyTransform(ParentParticle->R(), ParentParticle->X());

				if (Module->IsBehaviourType(eSimModuleTypeFlags::Velocity))
				{
					const Chaos::FClusterUnionPhysicsProxy::FInternalParticle* Particle = nullptr;
					if (Module->IsClustered())
					{
						Particle = ParentParticle;
					}
					else
					{
		//				Particle = Particles[Module->GetTransformIndex()];
					}

					if (Particle)
					{
						const FTransform& OffsetTransform = DeferredForces.GetOffsetTransform();
						FVector LocalPos = Module->GetParentRelativeTransform().GetLocation();
						FVector WorldLocation = BodyTransform.TransformPosition(OffsetTransform.TransformVector(LocalPos));
						const Chaos::FVec3 Arm = WorldLocation - Particle->X();

						//Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Particle->X(), Particle->X() + Arm, FColor::Yellow, false, -1.f, 0, 2.f);

						FVector WorldVelocity = Particle->V() - Chaos::FVec3::CrossProduct(Arm, Particle->W());
						FVector LocalVelocity = OffsetTransform.InverseTransformVector(BodyTransform.InverseTransformVector(WorldVelocity));
						Module->SetLocalVelocity(LocalVelocity);
					}

				}
			}
		}
	}

}

void FSimModuleTree::GenerateReplicationStructure(Chaos::FModuleNetDataArray& NetData)
{
	const TArray<FSimModuleNode>& Tree = SimulationModuleTree;
	NetData.Reserve(Tree.Num());
	for (int Index = 0; Index < Tree.Num(); Index++)
	{
		TSharedPtr<FModuleNetData>&& Data = Tree[Index].SimModule->GenerateNetData(Index);
		// not all modules will have net replication data - nullptr is a valid response
		if (Data)
		{
			NetData.Emplace(Data);
		}
	}
}

void FSimModuleTree::SetNetState(Chaos::FModuleNetDataArray& ModuleDatas)
{
	if (ModuleDatas.IsEmpty())
	{
		GenerateReplicationStructure(ModuleDatas);
	}

	for (TSharedPtr<FModuleNetData>& DataElement : ModuleDatas)
	{
		if (!SimulationModuleTree.IsEmpty() && SimulationModuleTree[DataElement->SimArrayIndex].SimModule)
		{
			DataElement->FillNetState(SimulationModuleTree[DataElement->SimArrayIndex].SimModule);
		}
	}
}

void FSimModuleTree::SetSimState(const Chaos::FModuleNetDataArray& ModuleDatas)
{
	for (const TSharedPtr<FModuleNetData>& DataElement : ModuleDatas)
	{
		if (!SimulationModuleTree.IsEmpty() && SimulationModuleTree[DataElement->SimArrayIndex].SimModule)
		{
			DataElement->FillSimState(SimulationModuleTree[DataElement->SimArrayIndex].SimModule);
		}
	}
}


void FSimModuleTree::InterpolateState(const float LerpFactor, Chaos::FModuleNetDataArray& LerpDatas, const Chaos::FModuleNetDataArray& MinDatas, const Chaos::FModuleNetDataArray& MaxDatas)
{

}

} // namespace Chaos


#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif
