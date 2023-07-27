// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsEngine/ClusterUnionComponent.h"

#include "Chaos/ImplicitObject.h"
#include "Chaos/Serializable.h"
#include "Chaos/ShapeInstance.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/GenericPhysicsInterface.h"
#include "PhysicsEngine/ClusterUnionReplicatedProxyComponent.h"
#include "PhysicsEngine/ExternalSpatialAccelerationPayload.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "TimerManager.h"
#include "PhysicsEngine/PhysicsSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClusterUnionComponent)

DEFINE_LOG_CATEGORY(LogClusterUnion);

namespace
{
	bool bUseClusterUnionAccelerationStructure = true;
	FAutoConsoleVariableRef CVarUseClusterUnionAccelerationStructure(TEXT("ClusterUnion.UseAccelerationStructure"), bUseClusterUnionAccelerationStructure, TEXT("Whether component level sweeps and overlaps against cluster unions should use an acceleration structure instead."));

	// TODO: Should this be exposed in Chaos instead?
	using FAccelerationStructure = Chaos::TAABBTree<FExternalSpatialAccelerationPayload, Chaos::TAABBTreeLeafArray<FExternalSpatialAccelerationPayload>>;

	TUniquePtr<Chaos::ISpatialAcceleration<FExternalSpatialAccelerationPayload, Chaos::FReal, 3>> CreateEmptyAccelerationStructure()
	{
		FAccelerationStructure* Structure = new FAccelerationStructure(
			nullptr,
			FAccelerationStructure::DefaultMaxChildrenInLeaf,
			FAccelerationStructure::DefaultMaxTreeDepth,
			FAccelerationStructure::DefaultMaxPayloadBounds,
			FAccelerationStructure::DefaultMaxNumToProcess,
			true,
			false
		);

		check(Structure != nullptr);
		return TUniquePtr<Chaos::ISpatialAcceleration<FExternalSpatialAccelerationPayload, Chaos::FReal, 3>>(Structure);
	}
}

UClusterUnionComponent::UClusterUnionComponent(const FObjectInitializer& ObjectInitializer)
	: UPrimitiveComponent(ObjectInitializer)
{
	PhysicsProxy = nullptr;
	SetIsReplicatedByDefault(true);
	bComputeBoundsOnceForGame = false;
	bHasReceivedTransform = false;
	bHasCachedLocalBounds = false;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
}

FPhysScene_Chaos* UClusterUnionComponent::GetChaosScene() const
{
	if (AActor* Owner = GetOwner(); Owner && Owner->GetWorld())
	{
		return Owner->GetWorld()->GetPhysicsScene();
	}

	check(GWorld);
	return GWorld->GetPhysicsScene();
}

void UClusterUnionComponent::AddComponentToCluster(UPrimitiveComponent* InComponent, const TArray<int32>& BoneIds, bool bRebuildGeometry)
{
	if (!InComponent || !PhysicsProxy || !InComponent->GetWorld())
	{
		return;
	}

	FClusterUnionPendingAddData PendingData;
	PendingData.BoneIds = BoneIds;

	if (!InComponent->HasValidPhysicsState())
	{
		if (!PendingComponentsToAdd.Contains(InComponent))
		{
			// Early out - defer adding the component to the cluster until the component has a valid physics state.
			PendingComponentsToAdd.Add(InComponent, PendingData);
			InComponent->OnComponentPhysicsStateChanged.AddDynamic(this, &UClusterUnionComponent::HandleComponentPhysicsStateChange);
		}
		return;
	}

	PendingComponentsToAdd.Remove(InComponent);

	TArray<Chaos::FPhysicsObjectHandle> AllObjects = InComponent->GetAllPhysicsObjects();
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(AllObjects);

	TArray<Chaos::FPhysicsObjectHandle> Objects;
	if (BoneIds.IsEmpty())
	{
		Objects = AllObjects;
	}
	else
	{
		Objects.Reserve(BoneIds.Num());
		for (int32 Id : BoneIds)
		{
			Objects.Add(InComponent->GetPhysicsObjectById(Id));
		}
	}

	if (BoneIds.IsEmpty())
	{
		Objects = Objects.FilterByPredicate(
			[&Interface](Chaos::FPhysicsObjectHandle Handle)
			{
				return !Interface->AreAllDisabled({ &Handle, 1 });
			}
		);
	}

	if (Objects.IsEmpty())
	{
		UE_LOG(LogClusterUnion, Warning, TEXT("Trying to add a component [%p] with no physics objects to a cluster union...ignoring"), InComponent)
		return;
	}

	PendingData.BoneIds.Empty(Objects.Num());
	PendingData.AccelerationPayloads.Empty(Objects.Num());

	for (Chaos::FPhysicsObjectHandle Object : Objects)
	{
		const int32 BoneId = Chaos::FPhysicsObjectInterface::GetId(Object);
		PendingData.BoneIds.Add(BoneId);

		if (AccelerationStructure && Object)
		{
			const FBox HandleBounds = Interface->GetWorldBounds({ &Object, 1 });
			FExternalSpatialAccelerationPayload Handle;
			Handle.Initialize(InComponent, BoneId);
			AccelerationStructure->UpdateElement(Handle, Chaos::TAABB<Chaos::FReal, 3>{HandleBounds.Min, HandleBounds.Max}, HandleBounds.IsValid != 0);
			PendingData.AccelerationPayloads.Add(Handle);
		}
	}

	// Need to listen to changes in the component's physics state. If it gets destroyed it should be removed from the cluster union as well.
	// This technically is giving us a false sense of security because when we get this notification, the primitive component will already have no physics proxy on it so we can't actually grab physics objects/particles from it.
	InComponent->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &UClusterUnionComponent::HandleComponentPhysicsStateChangePostAddIntoClusterUnion);

	PendingComponentSync.Add(InComponent, PendingData);

	PhysicsProxy->AddPhysicsObjects_External(Objects);

	if (bRebuildGeometry)
	{
		ForceRebuildGTParticleGeometry();
	}
}

void UClusterUnionComponent::RemoveComponentFromCluster(UPrimitiveComponent* InComponent)
{
	if (!InComponent || !PhysicsProxy)
	{
		return;
	}

	const int32 NumRemoved = PendingComponentsToAdd.Remove(InComponent);
	if (NumRemoved > 0)
	{
		// We haven't actually added yet so we can early out.
		return;
	}

	// If we're still waiting on the physics sync for the added component, we still need to
	// remove its acceleration handles from the acceleration structure since the payloads have
	// already been added and will not yet be stored in PerComponentData.
	if (FClusterUnionPendingAddData* PendingData = PendingComponentSync.Find(InComponent))
	{
		if (AccelerationStructure)
		{
			for (const FExternalSpatialAccelerationPayload& Payload : PendingData->AccelerationPayloads)
			{
				AccelerationStructure->RemoveElement(Payload);
			}
		}

		PendingComponentSync.Remove(InComponent);
	}

	TSet<Chaos::FPhysicsObjectHandle> PhysicsObjectsToRemove;

	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(GetWorld()->GetPhysicsScene());

	if (FClusteredComponentData* ComponentData = PerComponentData.Find(InComponent))
	{
		// We need to mark the replicated proxy as pending deletion.
		// This way anyone who tries to use the replicated proxy component knows that it
		// doesn't actually denote a meaningful cluster union relationship.
		ComponentData->bPendingDeletion = true;

		if (InComponent->HasValidPhysicsState())
		{
			PhysicsObjectsToRemove = TSet<Chaos::FPhysicsObjectHandle>{ GetAllPhysicsObjectsById(InComponent, ComponentData->BoneIds.Array()) };
		}

		if (IsAuthority())
		{
			if (UClusterUnionReplicatedProxyComponent* Component = ComponentData->ReplicatedProxyComponent.Get())
			{
				Component->MarkPendingDeletion();
			}
		}

		if (AccelerationStructure)
		{
			for (const FExternalSpatialAccelerationPayload& Payload : ComponentData->CachedAccelerationPayloads)
			{
				AccelerationStructure->RemoveElement(Payload);
			}
			ComponentData->CachedAccelerationPayloads.Reset();
		}
	}

	// If PhysicsObjectsToRemove is empty, it either means that BoneIds is empty OR the component's physics state is already destroyed.
	// In the case of the latter, we rely on the physics thread to cleanup the cluster union manager properly and to sync back.
	if (!PhysicsObjectsToRemove.IsEmpty())
	{
		PhysicsProxy->RemovePhysicsObjects_External(PhysicsObjectsToRemove);
		ForceRebuildGTParticleGeometry();
	}
}

// TODO: Can this merge with RemoveComponentFromCluster?
void UClusterUnionComponent::RemoveComponentBonesFromCluster(UPrimitiveComponent* InComponent, const TArray<int32>& BoneIds)
{
	if (!InComponent || !PhysicsProxy)
	{
		return;
	}
	TSet<int32> RemoveBoneIdsSet{ BoneIds };
	// If we're removing bones from a component and it hasn't been added into the component yet, then we want to make sure 
	// the set we actually want to add doesn't have the remove bones in it.
	if (FClusterUnionPendingAddData* PendingAdd = PendingComponentsToAdd.Find(InComponent))
	{
		for (int32 BoneId : RemoveBoneIdsSet)
		{
			PendingAdd->BoneIds.Remove(BoneId);
		}

		if (PendingAdd->BoneIds.IsEmpty())
		{
			PendingComponentsToAdd.Remove(InComponent);
		}

		// Component is pending add so we can be confident we don't need to do any more work.
		return;
	}

	// If we're still waiting on the physics sync for the added component, we still need to
	// remove its acceleration handles from the acceleration structure since the payloads have
	// already been added and will not yet be stored in PerComponentData.
	if (FClusterUnionPendingAddData* PendingData = PendingComponentSync.Find(InComponent))
	{
		if (AccelerationStructure)
		{
			for (int32 BoneId : RemoveBoneIdsSet)
			{
				FExternalSpatialAccelerationPayload Payload;
				Payload.Initialize(InComponent, BoneId);
				PendingData->AccelerationPayloads.Remove(Payload);
				AccelerationStructure->RemoveElement(Payload);
			}
		}

		for (int32 BoneId : RemoveBoneIdsSet)
		{
			PendingData->BoneIds.Remove(BoneId);
		}

		if (PendingData->BoneIds.IsEmpty())
		{
			PendingComponentSync.Remove(InComponent);
		}
	}

	TSet<Chaos::FPhysicsObjectHandle> PhysicsObjectsToRemove;
	if (FClusteredComponentData* ComponentData = PerComponentData.Find(InComponent))
	{
		for (int32 BoneId : RemoveBoneIdsSet)
		{
			if (AccelerationStructure)
			{
				FExternalSpatialAccelerationPayload Payload;
				Payload.Initialize(InComponent, BoneId);
				ComponentData->CachedAccelerationPayloads.Remove(Payload);
				AccelerationStructure->RemoveElement(Payload);
			}
			ComponentData->BoneIds.Remove(BoneId);
		}

		if (InComponent->HasValidPhysicsState())
		{
			PhysicsObjectsToRemove = TSet<Chaos::FPhysicsObjectHandle>{ GetAllPhysicsObjectsById(InComponent, RemoveBoneIdsSet.Array()) };
		}

		if (ComponentData->BoneIds.IsEmpty())
		{
			// We need to mark the replicated proxy as pending deletion.
			// This way anyone who tries to use the replicated proxy component knows that it
			// doesn't actually denote a meaningful cluster union relationship.
			ComponentData->bPendingDeletion = true;

			if (IsAuthority())
			{
				if (UClusterUnionReplicatedProxyComponent* Component = ComponentData->ReplicatedProxyComponent.Get())
				{
					Component->MarkPendingDeletion();
				}
			}

			PerComponentData.Remove(InComponent);
		}
	}

	// If PhysicsObjectsToRemove is empty, it either means that BoneIds is empty OR the component's physics state is already destroyed.
	// In the case of the latter, we rely on the physics thread to cleanup the cluster union manager properly and to sync back.
	if (!PhysicsObjectsToRemove.IsEmpty())
	{
		PhysicsProxy->RemovePhysicsObjects_External(PhysicsObjectsToRemove);
		ForceRebuildGTParticleGeometry();
	}
}

void UClusterUnionComponent::ForceRebuildGTParticleGeometry()
{
	if (!PhysicsProxy)
	{
		return;
	}

	// This is an assumption that all the physics objects are part of this scene and thus this is the right thing to lock.
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(GetWorld()->GetPhysicsScene());

	const FTransform ClusterWorldTM = GetComponentTransform();
	TArray<UPrimitiveComponent*> Components = GetAllCurrentChildComponents();
	// This code is similar-ish to FClusterUnionManager::ForceRecreateClusterUnionSharedGeometry but not enough to make it necessary
	// to reshare exactly the same code.
	TArray<TUniquePtr<Chaos::FImplicitObject>> Objects;
	TArray<Chaos::FPBDRigidParticle*> Particles;

	// Should be a good number to reserve for now since it's a generally safe assumption we'll be working with 1 particle per component added.
	Objects.Reserve(Components.Num());
	Particles.Reserve(Components.Num());

	for (UPrimitiveComponent* Child : Components)
	{
		check(Child != nullptr);
		TArray<int32> BoneIds = GetAddedBoneIdsForComponent(Child);
		TArray<Chaos::FPhysicsObjectHandle> PhysicsObjects;
		PhysicsObjects.Reserve(BoneIds.Num());
		for (int32 Id : BoneIds)
		{
			PhysicsObjects.Add(Child->GetPhysicsObjectById(Id));
		}

		for (Chaos::FPBDRigidParticle* Particle : Interface->GetAllRigidParticles(PhysicsObjects))
		{
			if (Particle && Particle->Geometry())
			{
				const FTransform ChildWorldTM{ Particle->R(), Particle->X() };
				const FTransform Frame = ChildWorldTM.GetRelativeTransform(ClusterWorldTM);
				Objects.Add(TUniquePtr<Chaos::FImplicitObject>(Chaos::FClusterUnionManager::CreateTransformGeometryForClusterUnion<Chaos::EThreadContext::External>(Particle, Frame)));
				Particles.Add(Particle);
			}
		}
	}

	Chaos::FImplicitObjectUnion* NewGeometry = Objects.IsEmpty() ? new Chaos::FImplicitObjectUnionClustered() : new Chaos::FImplicitObjectUnion(MoveTemp(Objects));
	NewGeometry->SetAllowBVH(true);

	PhysicsProxy->SetSharedGeometry_External(TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe>(NewGeometry), Particles);
}

TArray<UPrimitiveComponent*> UClusterUnionComponent::GetPrimitiveComponents()
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PrimitiveComponents.Reserve(PerComponentData.Num());

	for (auto Iter = PerComponentData.CreateIterator(); Iter; ++Iter)
	{
		PrimitiveComponents.Add(Iter.Key().ResolveObjectPtr());
	}

	return PrimitiveComponents;
}

TArray<AActor*> UClusterUnionComponent::GetActors()
{
	TArray<AActor*> Actors;
	Actors.Reserve(ActorToComponents.Num());

	for (auto Iter = ActorToComponents.CreateIterator(); Iter; ++Iter)
	{
		Actors.Add(Iter.Key().ResolveObjectPtr());
	}

	return Actors;
}

void UClusterUnionComponent::SetIsAnchored(bool bIsAnchored)
{
	if (!PhysicsProxy)
	{
		return;
	}

	PhysicsProxy->SetIsAnchored_External(bIsAnchored);
}

bool UClusterUnionComponent::IsAuthority() const
{
	ENetMode Mode = GetNetMode();
	if (Mode == ENetMode::NM_Standalone)
	{
		return true;
	}

	if (AActor* Owner = GetOwner())
	{
		return Owner->GetLocalRole() == ROLE_Authority && Mode != NM_Client;
	}

	return false;
}

void UClusterUnionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState();

	// If we've already created the physics proxy we shouldn't do this again.
	if (PhysicsProxy)
	{
		return;
	}

	// If we're not actually playing/needing this to simulate (e.g. in the editor) there should be no reason to create this proxy.
	const bool bValidWorld = GetWorld() && (GetWorld()->IsGameWorld() || GetWorld()->IsPreviewWorld());
	if (!bValidWorld)
	{
		return;
	}

	// TODO: Expose these parameters via the component.
	Chaos::FClusterCreationParameters Parameters{ 0.3f, 100, false, false };
	Parameters.ConnectionMethod = Chaos::FClusterCreationParameters::EConnectionMethod::PointImplicit;

	FChaosUserData::Set<UPrimitiveComponent>(&PhysicsUserData, this);

	const bool bHasAuthority = GetOwner()->HasAuthority();

	Chaos::FClusterUnionInitData InitData;
	InitData.UserData = static_cast<void*>(&PhysicsUserData);
	InitData.ActorId = GetOwner()->GetUniqueID();
	InitData.ComponentId = GetUniqueID();
	InitData.bNeedsClusterXRInitialization = bHasAuthority;

	// Client needs to be set to unbreakable so the server is authoritative.
	InitData.bUnbreakable = !bHasAuthority;

	// Only need to check connectivity on the server and have the client rely on replication to get the memo on when to release from cluster union.
	InitData.bCheckConnectivity = bHasAuthority;

	bHasReceivedTransform = false;
	PhysicsProxy = new Chaos::FClusterUnionPhysicsProxy{ this, Parameters, InitData };
	PhysicsProxy->Initialize_External();
	if (FPhysScene_Chaos* Scene = GetChaosScene())
	{
		Scene->AddObject(this, PhysicsProxy);
	}
	
	if (bUseClusterUnionAccelerationStructure)
	{
		AccelerationStructure = CreateEmptyAccelerationStructure();
		check(AccelerationStructure != nullptr);
	}

	// It's just logically easier to be consistent on the client to go through the replication route.
	if (IsAuthority())
	{
		for (const FComponentReference& ComponentReference : ClusteredComponentsReferences)
		{
			if (!ComponentReference.OtherActor.IsValid())
			{
				continue;
			}

			AddComponentToCluster(Cast<UPrimitiveComponent>(ComponentReference.GetComponent(ComponentReference.OtherActor.Get())), {});
		}
	}
	else
	{
		// we should not rely on the OnTransformUpdated callback to set the initial position of the external particle
		// if it is not set and the callback does not get called then the cluster union component will be moved to the origin of the world
		const FTransform Transform = GetComponentTransform();
		PhysicsProxy->SetXR_External(Transform.GetLocation(), Transform.GetRotation());
	}
}

void UClusterUnionComponent::OnDestroyPhysicsState()
{
	USceneComponent::OnDestroyPhysicsState();

	if (!PhysicsProxy)
	{
		return;
	}

	// We need to make sure we *immediately* disconnect on the GT side since there's no guarantee the normal flow
	// will happen once we've destroyed things.
	TArray<TObjectKey<UPrimitiveComponent>> ComponentsToRemove;
	PerComponentData.GetKeys(ComponentsToRemove);

	for (const TObjectKey<UPrimitiveComponent>& Component : ComponentsToRemove)
	{
		HandleRemovedClusteredComponent(Component);
	}

	if (FPhysScene_Chaos* Scene = GetChaosScene())
	{
		Scene->RemoveObject(PhysicsProxy);
	}

	PhysicsProxy = nullptr;
	AccelerationStructure.Reset();
}

void UClusterUnionComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	USceneComponent::OnUpdateTransform(UpdateTransformFlags, Teleport);
	bHasReceivedTransform = true;

	if (PhysicsProxy && !(UpdateTransformFlags & EUpdateTransformFlags::SkipPhysicsUpdate))
	{
		// If the component transform changes, we need to make sure this update is reflected on the physics thread as well.
		// This code path is generally used when setting the transform manually or when it's set via replication.
		const FTransform Transform = GetComponentTransform();
		PhysicsProxy->SetXR_External(Transform.GetLocation(), Transform.GetRotation());
	}
}

FBoxSphereBounds UClusterUnionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!bHasCachedLocalBounds)
	{
		if (!PhysicsProxy)
		{
			return Super::CalcBounds(LocalToWorld);
		}
		else
		{
			Chaos::FPhysicsObjectHandle Handle = PhysicsProxy->GetPhysicsObjectHandle();
			FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead({ &Handle, 1 });
			FBoxSphereBounds NewBounds = Interface->GetBounds({ &Handle, 1 });
			bHasCachedLocalBounds = true;
			CachedLocalBounds = NewBounds;
		}
	}

	return CachedLocalBounds.TransformBy(LocalToWorld);
}

bool UClusterUnionComponent::ShouldCreatePhysicsState() const
{
	return true;
}

bool UClusterUnionComponent::HasValidPhysicsState() const
{
	return PhysicsProxy != nullptr;
}

Chaos::FPhysicsObject* UClusterUnionComponent::GetPhysicsObjectById(Chaos::FPhysicsObjectId Id) const
{
	if (!PhysicsProxy)
	{
		return nullptr;
	}
	return PhysicsProxy->GetPhysicsObjectHandle();
}

Chaos::FPhysicsObject* UClusterUnionComponent::GetPhysicsObjectByName(const FName& Name) const
{
	return GetPhysicsObjectById(0);
}

TArray<Chaos::FPhysicsObject*> UClusterUnionComponent::GetAllPhysicsObjects() const
{
	return { GetPhysicsObjectById(0) };
}

Chaos::FPhysicsObjectId UClusterUnionComponent::GetIdFromGTParticle(Chaos::FGeometryParticle* Particle) const
{
	return 0;
}

void UClusterUnionComponent::HandleComponentPhysicsStateChange(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (!ChangedComponent || StateChange != EComponentPhysicsStateChange::Created)
	{
		return;
	}

	ChangedComponent->OnComponentPhysicsStateChanged.RemoveDynamic(this, &UClusterUnionComponent::HandleComponentPhysicsStateChange);

	if (FClusterUnionPendingAddData* PendingData = PendingComponentsToAdd.Find(ChangedComponent))
	{
		AddComponentToCluster(ChangedComponent, PendingData->BoneIds);
	}
}

void UClusterUnionComponent::HandleComponentPhysicsStateChangePostAddIntoClusterUnion(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (!ChangedComponent || StateChange != EComponentPhysicsStateChange::Destroyed)
	{
		return;
	}

	ChangedComponent->OnComponentPhysicsStateChanged.RemoveDynamic(this, &UClusterUnionComponent::HandleComponentPhysicsStateChangePostAddIntoClusterUnion);
	RemoveComponentFromCluster(ChangedComponent);
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::SyncClusterUnionFromProxy"), STAT_ClusterUnionComponent_SyncClusterUnionFromProxy, STATGROUP_Chaos);
void UClusterUnionComponent::SyncClusterUnionFromProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponent_SyncClusterUnionFromProxy);

	// NOTE THAT WE ARE ON THE GAME THREAD HERE.
	if (!PhysicsProxy || !GetWorld())
	{
		return;
	}

	FPhysScene* Scene = GetWorld()->GetPhysicsScene();
	if (!Scene)
	{
		return;
	}

	if (IsAuthority())
	{
		ReplicatedRigidState.bIsAnchored = PhysicsProxy->IsAnchored_External();
		ReplicatedRigidState.ObjectState = static_cast<uint8>(PhysicsProxy->GetObjectState_External());
		MARK_PROPERTY_DIRTY_FROM_NAME(UClusterUnionComponent, ReplicatedRigidState, this);
	}
	
	const Chaos::FClusterUnionSyncedData& FullData = PhysicsProxy->GetSyncedData_External();
	// Note that at the UClusterUnionComponent level we really only want to be dealing with components.
	// Hence why we need to modify each of the particles that we synced from the game thread into a
	// component + bone id combination for identification. 
	TMap<TObjectKey<UPrimitiveComponent>, TMap<int32, FTransform>> MappedData;
	for (const Chaos::FClusterUnionChildData& ChildData : FullData.ChildParticles)
	{
		// Using the scene's proxy to component mapping let's us detect a component physics state was destroyed.
		if (UPrimitiveComponent* Component = Scene->GetOwningComponent<UPrimitiveComponent>(ChildData.Proxy))
		{
			MappedData.FindOrAdd(Component).Add(ChildData.BoneId, ChildData.ChildToParent);
		}
	}

	// We need to handle any additions, deletions, and modifications to any child in the cluster union here.
	// If a component lives in MappedData but not in PerComponentData, new component!
	// If a component lives in both, then it's a modified component.
	for (const TPair<TObjectKey<UPrimitiveComponent>, TMap<int32, FTransform>>& Kvp : MappedData)
	{
		HandleAddOrModifiedClusteredComponent(Kvp.Key.ResolveObjectPtr(), Kvp.Value);
	}

	// If a component lives in PerComponentData but not in MappedData, deleted component!
	TArray<TObjectKey<UPrimitiveComponent>> ComponentsToRemove;
	for (const TPair<TObjectKey<UPrimitiveComponent>, FClusteredComponentData>& Kvp : PerComponentData)
	{
		if (!MappedData.Contains(Kvp.Key))
		{
			ComponentsToRemove.Add(Kvp.Key);
		}
	}

	for (TObjectKey<UPrimitiveComponent> Component : ComponentsToRemove)
	{
		HandleRemovedClusteredComponent(Component);
	}

	const FBoxSphereBounds OldBounds = CachedLocalBounds;
	bHasCachedLocalBounds = false;
	UpdateBounds();

	// Check if the old bounds is approximately equal to the new bounds. If so, we don't need to send an event
	// saying that the cluster union changed in shape/position.
	const bool bIsSameBounds = OldBounds.Origin.Equals(CachedLocalBounds.Origin) && OldBounds.BoxExtent.Equals(CachedLocalBounds.BoxExtent) && (FMath::Abs(OldBounds.SphereRadius - CachedLocalBounds.SphereRadius) < UE_KINDA_SMALL_NUMBER);
	if (!bIsSameBounds)
	{
		OnComponentBoundsChangedEvent.Broadcast(this, CachedLocalBounds);
	}
}

void UClusterUnionComponent::HandleAddOrModifiedClusteredComponent(UPrimitiveComponent* ChangedComponent, const TMap<int32, FTransform>& PerBoneChildToParent)
{
	if (!ChangedComponent || !ChangedComponent->HasValidPhysicsState() || !ChangedComponent->GetWorld())
	{
		return;
	}

	const bool bIsNew = !PerComponentData.Contains(ChangedComponent);
	FClusteredComponentData& ComponentData = PerComponentData.FindOrAdd(ChangedComponent);

	// A component shouldn't be in the PendingComponentSync map unless it's new.
	FClusterUnionPendingAddData PendingData;
	if (bIsNew && PendingComponentSync.RemoveAndCopyValue(ChangedComponent, PendingData))
	{
		ComponentData.CachedAccelerationPayloads.Append(PendingData.AccelerationPayloads);
	}

	// If this is a *new* component that we're keeping track of then there's additional book-keeping
	// we need to do to make sure we don't forget what exactly we're tracking. Additionally, we need to
	// modify the component and its parent actor to ensure their replication stops.
	if (bIsNew)
	{
		// Force the component and its parent actor to stop replicating movement.
		// Setting the component to not replicate should be sufficient since a simulating
		// component shouldn't be doing much more than replicating its position anyway.
		if (AActor* Owner = ChangedComponent->GetOwner())
		{
			if (FClusteredActorData* Data = ActorToComponents.Find(Owner))
			{
				Data->Components.Add(ChangedComponent);
			}
			else
			{
				FClusteredActorData NewData;
				NewData.Components.Add(ChangedComponent);
				NewData.bWasReplicatingMovement = Owner->IsReplicatingMovement();
				ActorToComponents.Add(Owner, NewData);

				if (IsAuthority())
				{
					Owner->SetReplicatingMovement(false);
				}
			}

			ComponentData.Owner = Owner;
		}

		ComponentData.bWasReplicating = ChangedComponent->GetIsReplicated();
		if (IsAuthority())
		{
			if (AActor* Owner = ChangedComponent->GetOwner())
			{
				// Create a replicated proxy component and add it to the actor being added to the cluster.
				// This component will take care of replicating this addition into the cluster.
				TObjectPtr<UClusterUnionReplicatedProxyComponent> ReplicatedProxy = NewObject<UClusterUnionReplicatedProxyComponent>(Owner);
				if (ensure(ReplicatedProxy))
				{
					ReplicatedProxy->RegisterComponent();
					ReplicatedProxy->SetParentClusterUnion(this);
					ReplicatedProxy->SetChildClusteredComponent(ChangedComponent);
					ReplicatedProxy->SetIsReplicated(true);
				}

				ComponentData.ReplicatedProxyComponent = ReplicatedProxy;
			}
		}

	}

	const TSet<int32> OldBoneIds{ ComponentData.BoneIds };
	PerBoneChildToParent.GetKeys(ComponentData.BoneIds);
	OnComponentAddedEvent.Broadcast(ChangedComponent, ComponentData.BoneIds, bIsNew);

	if (IsAuthority() && ComponentData.ReplicatedProxyComponent.IsValid())
	{
		// We really only need to do modifications on the server since that's where we're changing the replicated proxy to broadcast this data change.
		TObjectPtr<UClusterUnionReplicatedProxyComponent> ReplicatedProxy = ComponentData.ReplicatedProxyComponent.Get();
		ReplicatedProxy->SetParticleBoneIds(ComponentData.BoneIds.Array());
		for (const TPair<int32, FTransform>& Kvp : PerBoneChildToParent)
		{
			ReplicatedProxy->SetParticleChildToParent(Kvp.Key, Kvp.Value);
		}
	}

	TArray<Chaos::FPhysicsObjectHandle> AllPhysicsObjects = ChangedComponent->GetAllPhysicsObjects();
	FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(AllPhysicsObjects);

	// One more loop to ensure that our sets of physics objects are valid and up to date.
	// This needs to happen on both the client and the server.
	for (const TPair<int32, FTransform>& Kvp : PerBoneChildToParent)
	{
		Chaos::FPhysicsObjectHandle PhysicsObject = ChangedComponent->GetPhysicsObjectById(Kvp.Key);

		if (AccelerationStructure)
		{
			if (PhysicsObject)
			{
				const FBox HandleBounds = Interface->GetWorldBounds({ &PhysicsObject, 1 });
				FExternalSpatialAccelerationPayload Handle;
				Handle.Initialize(ChangedComponent, Kvp.Key);
				AccelerationStructure->UpdateElement(Handle, Chaos::TAABB<Chaos::FReal, 3>{HandleBounds.Min, HandleBounds.Max}, HandleBounds.IsValid != 0);
				ComponentData.CachedAccelerationPayloads.Add(Handle);
			}
		}
	}

	// In the case where we need to keep the acceleration structure up to date, we need to make sure old bone ids are properly
	// removed from the acceleration structure.
	if (AccelerationStructure)
	{
		for (int32 BoneId : OldBoneIds.Difference(ComponentData.BoneIds))
		{
			if (Chaos::FPhysicsObjectHandle PhysicsObject = ChangedComponent->GetPhysicsObjectById(BoneId))
			{
				FExternalSpatialAccelerationPayload Handle;
				Handle.Initialize(ChangedComponent, BoneId);
				AccelerationStructure->RemoveElement(Handle);
				ComponentData.CachedAccelerationPayloads.Remove(Handle);
			}
		}
	}
}

void UClusterUnionComponent::HandleRemovedClusteredComponent(TObjectKey<UPrimitiveComponent> RemovedComponent)
{
	if (FClusteredComponentData* Data = PerComponentData.Find(RemovedComponent))
	{
		if (AccelerationStructure)
		{
			for (const FExternalSpatialAccelerationPayload& Handle : Data->CachedAccelerationPayloads)
			{
				AccelerationStructure->RemoveElement(Handle);
			}
		}

		if (IsAuthority())
		{
			if (UClusterUnionReplicatedProxyComponent* ProxyComponent = Data->ReplicatedProxyComponent.Get())
			{
				ProxyComponent->DestroyComponent();
			}
		}

		if (AActor* Owner = Data->Owner.Get())
		{
			if (FClusteredActorData* ActorData = ActorToComponents.Find(Owner))
			{
				ActorData->Components.Remove(RemovedComponent);

				if (ActorData->Components.IsEmpty())
				{
					if (IsAuthority())
					{
						Owner->SetReplicatingMovement(ActorData->bWasReplicatingMovement);
					}
					ActorToComponents.Remove(Owner);
				}
			}
		}

		if (UPrimitiveComponent* ChangedComponent = RemovedComponent.ResolveObjectPtr())
		{
			OnComponentRemovedEvent.Broadcast(ChangedComponent);
		}
		PerComponentData.Remove(RemovedComponent);
	}
	PendingComponentsToAdd.Remove(RemovedComponent);
	PendingComponentSync.Remove(RemovedComponent);
}

void UClusterUnionComponent::OnRep_RigidState()
{
	if (!PhysicsProxy)
	{
		return;
	}

	PhysicsProxy->SetIsAnchored_External(ReplicatedRigidState.bIsAnchored);
	PhysicsProxy->SetObjectState_External(static_cast<Chaos::EObjectStateType>(ReplicatedRigidState.ObjectState));
}

void UClusterUnionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UPrimitiveComponent::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UClusterUnionComponent, ReplicatedRigidState, Params);

	// Allow the physical parameters of this component to be replicated via
	// physicsreplication even if component replication is enabled
	DISABLE_REPLICATED_PROPERTY_FAST(USceneComponent, RelativeLocation);
	DISABLE_REPLICATED_PROPERTY_FAST(USceneComponent, RelativeRotation);
}

void UClusterUnionComponent::ForceSetChildToParent(UPrimitiveComponent* InComponent, const TArray<int32>& BoneIds, const TArray<FTransform>& ChildToParent)
{
	if (IsAuthority() || !PhysicsProxy || !ensure(InComponent) || !ensure(BoneIds.Num() == ChildToParent.Num()))
	{
		return;
	}

	TArray< Chaos::FPhysicsObjectHandle> Objects;
	Objects.Reserve(BoneIds.Num());

	for (int32 Index = 0; Index < BoneIds.Num(); ++Index)
	{
		Chaos::FPhysicsObjectHandle Handle = InComponent->GetPhysicsObjectById(BoneIds[Index]);
		Objects.Add(Handle);
	}

	// If we're on the client we want to lock the child to parent transform for this particle as soon as we get a server authoritative value.
	PhysicsProxy->BulkSetChildToParent_External(Objects, ChildToParent, !IsAuthority());
}

void UClusterUnionComponent::SetSimulatePhysics(bool bSimulate)
{
	if (!PhysicsProxy)
	{
		return;
	}

	PhysicsProxy->SetObjectState_External(bSimulate ? Chaos::EObjectStateType::Dynamic : Chaos::EObjectStateType::Kinematic);
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::LineTraceComponentMulti"), STAT_ClusterUnionComponentLineTraceComponentMulti, STATGROUP_Chaos);
bool UClusterUnionComponent::LineTraceComponent(TArray<FHitResult>& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentLineTraceComponentMulti);
	if (AccelerationStructure)
	{
		return FGenericRaycastPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration>::RaycastMulti(*AccelerationStructure, GetWorld(), OutHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}
	OutHit.Reset();

	VisitAllCurrentChildComponentsForCollision(TraceChannel, Params, ResponseParams, ObjectParams,
		[&Start, &End, TraceChannel, &Params, &ResponseParams, &ObjectParams, &OutHit](UPrimitiveComponent* Component)
		{
			FHitResult ComponentHit;
			if (Component->LineTraceComponent(ComponentHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams))
			{
				OutHit.Add(ComponentHit);
			}

			return true;
		}
	);

	return !OutHit.IsEmpty();
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::SweepComponentMulti"), STAT_ClusterUnionComponentSweepComponentMulti, STATGROUP_Chaos);
bool UClusterUnionComponent::SweepComponent(TArray<FHitResult>& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FPhysicsGeometry& Geometry, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentSweepComponentMulti);
	if (AccelerationStructure)
	{
		return FGenericGeomPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration, FPhysicsGeometry>::GeomSweepMulti(*AccelerationStructure, GetWorld(), Geometry, ShapeWorldRotation, OutHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}

	OutHit.Reset();

	VisitAllCurrentChildComponentsForCollision(TraceChannel, Params, ResponseParams, ObjectParams,
		[&Geometry, &Start, &End, &ShapeWorldRotation, TraceChannel, &Params, &ResponseParams, &ObjectParams, &OutHit](UPrimitiveComponent* Component)
		{
			FHitResult ComponentHit;
			if (Component->SweepComponent(ComponentHit, Start, End, ShapeWorldRotation, Geometry, TraceChannel, Params, ResponseParams, ObjectParams))
			{
				OutHit.Add(ComponentHit);
			}

			return true;
		}
	);

	return !OutHit.IsEmpty();
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::LineTraceComponentSingle"), STAT_ClusterUnionComponentLineTraceComponentSingle, STATGROUP_Chaos);
bool UClusterUnionComponent::LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentLineTraceComponentSingle);
	if (AccelerationStructure)
	{
		return FGenericRaycastPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration>::RaycastSingle(*AccelerationStructure, GetWorld(), OutHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}

	bool bHasHit = false;
	OutHit.Distance = TNumericLimits<float>::Max();

	VisitAllCurrentChildComponentsForCollision(TraceChannel, Params, ResponseParams, ObjectParams,
		[&Start, &End, TraceChannel, &Params, &ResponseParams, &ObjectParams, &bHasHit, &OutHit](UPrimitiveComponent* Component)
		{
			FHitResult ComponentHit;
			if (Component->LineTraceComponent(ComponentHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams) && ComponentHit.Distance < OutHit.Distance)
			{
				bHasHit = true;
				OutHit = ComponentHit;
			}

			return true;
		}
	);

	return bHasHit;
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::SweepComponentSingle"), STAT_ClusterUnionComponentSweepComponentSingle, STATGROUP_Chaos);
bool UClusterUnionComponent::SweepComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FPhysicsGeometry& Geometry, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentSweepComponentSingle);
	if (AccelerationStructure)
	{
		return FGenericGeomPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration, FPhysicsGeometry>::GeomSweepSingle(*AccelerationStructure, GetWorld(), Geometry, ShapeWorldRotation, OutHit, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}

	bool bHasHit = false;
	OutHit.Distance = TNumericLimits<float>::Max();

	VisitAllCurrentChildComponentsForCollision(TraceChannel, Params, ResponseParams, ObjectParams,
		[&Geometry, &Start, &End, &ShapeWorldRotation, TraceChannel, &Params, &ResponseParams, &ObjectParams, &bHasHit, &OutHit](UPrimitiveComponent* Component)
		{
			FHitResult ComponentHit;
			if (Component->SweepComponent(ComponentHit, Start, End, ShapeWorldRotation, Geometry, TraceChannel, Params, ResponseParams, ObjectParams) && ComponentHit.Distance < OutHit.Distance)
			{
				bHasHit = true;
				OutHit = ComponentHit;
			}

			return true;
		}
	);

	return bHasHit;
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::OverlapComponentWithResult"), STAT_ClusterUnionComponentOverlapComponentWithResult, STATGROUP_Chaos);
bool UClusterUnionComponent::OverlapComponentWithResult(const FVector& Pos, const FQuat& Rot, const FPhysicsGeometry& Geometry, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, TArray<FOverlapResult>& OutOverlap) const
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentOverlapComponentWithResult);
	if (AccelerationStructure)
	{
		return FGenericGeomPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration, FPhysicsGeometry>::GeomOverlapMulti(*AccelerationStructure, GetWorld(), Geometry, Pos, Rot, OutOverlap, TraceChannel, Params, ResponseParams, ObjectParams);
	}

	bool bHasOverlap = false;

	FVector QueryHalfExtent = Geometry.BoundingBox().Extents();
	FBoxSphereBounds QueryBounds(FBox(-QueryHalfExtent, QueryHalfExtent));
	QueryBounds = QueryBounds.TransformBy(FTransform(Rot, Pos));

	VisitAllCurrentChildComponentsForCollision(TraceChannel, Params, ResponseParams, ObjectParams,
		[&QueryBounds, &Geometry, &Pos, &Rot, TraceChannel, &Params, &ResponseParams, &ObjectParams, &bHasOverlap, &OutOverlap](UPrimitiveComponent* Component)
		{
			if (FBoxSphereBounds::SpheresIntersect(QueryBounds, Component->Bounds))
			{
				TArray<FOverlapResult> SubOverlaps;
				if (Component->OverlapComponentWithResult(Pos, Rot, Geometry, TraceChannel, Params, ResponseParams, ObjectParams, SubOverlaps))
				{
					bHasOverlap = true;
					OutOverlap.Append(SubOverlaps);
				}
			}

			return true;
		}
	);

	return bHasOverlap;
}

DECLARE_CYCLE_STAT(TEXT("UClusterUnionComponent::ComponentOverlapComponentWithResultImpl"), STAT_ClusterUnionComponentComponentOverlapComponentWithResultImpl, STATGROUP_Chaos);
bool UClusterUnionComponent::ComponentOverlapComponentWithResultImpl(const class UPrimitiveComponent* const PrimComp, const FVector& Pos, const FQuat& Rot, const FCollisionQueryParams& Params, TArray<FOverlapResult>& OutOverlap) const
{
	SCOPE_CYCLE_COUNTER(STAT_ClusterUnionComponentComponentOverlapComponentWithResultImpl);
	if(!PrimComp)
	{
		return false;
	}

	bool bHasOverlap = false;
	if (AccelerationStructure)
	{
		TArray<Chaos::FPhysicsObjectHandle> InObjects = PrimComp->GetAllPhysicsObjects();
		FLockedReadPhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockRead(InObjects);
		InObjects = InObjects.FilterByPredicate(
			[&Interface](Chaos::FPhysicsObjectHandle Handle)
			{
				return !Interface->AreAllDisabled({ &Handle, 1 });
			}
		);

		Interface->VisitEveryShape(InObjects,
			[this, &bHasOverlap, &Pos, &Rot, &OutOverlap, &Params](const Chaos::FConstPhysicsObjectHandle Handle, Chaos::FShapeInstanceProxy* Shape)
			{
				if (Shape)
				{
					if (Chaos::TSerializablePtr<Chaos::FImplicitObject> Geometry = Shape->GetGeometry())
					{
						bHasOverlap |= FGenericGeomPhysicsInterfaceUsingSpatialAcceleration<IExternalSpatialAcceleration, FPhysicsGeometry>::GeomOverlapMulti(*AccelerationStructure, GetWorld(), *Geometry, Pos, Rot, OutOverlap, DefaultCollisionChannel, Params, FCollisionResponseParams::DefaultResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam);
					}
				}

				return true;
			});

		return bHasOverlap;
	}

	FBoxSphereBounds QueryBounds = PrimComp->Bounds.TransformBy(FTransform(Rot, Pos));

	VisitAllCurrentChildComponentsForCollision(DefaultCollisionChannel, Params, FCollisionResponseParams::DefaultResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam,
		[&QueryBounds, PrimComp, &Pos, &Rot, &Params, &bHasOverlap, &OutOverlap](UPrimitiveComponent* Component)
		{
			if (FBoxSphereBounds::SpheresIntersect(QueryBounds, Component->Bounds))
			{
				TArray<FOverlapResult> SubOverlaps;
				if (Component->ComponentOverlapComponentWithResult(PrimComp, Pos, Rot, Params, SubOverlaps))
				{
					bHasOverlap = true;
					OutOverlap.Append(SubOverlaps);
				}
			}

			return true;
		}
	);

	return bHasOverlap;
}

TArray<UPrimitiveComponent*> UClusterUnionComponent::GetAllCurrentChildComponents() const
{
	TArray<UPrimitiveComponent*> Components;
	Components.Reserve(PerComponentData.Num() + PendingComponentSync.Num());

	VisitAllCurrentChildComponents(
		[&Components](UPrimitiveComponent* Component)
		{
			Components.Add(Component);
			return true;
		}
	);

	return Components;
}

TArray<AActor*> UClusterUnionComponent::GetAllCurrentActors() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(ActorToComponents.Num());

	VisitAllCurrentActors(
		[&Actors](AActor* Actor)
		{
			Actors.Add(Actor);
			return true;
		}
	);

	return Actors;
}

void UClusterUnionComponent::VisitAllCurrentChildComponents(const TFunction<bool(UPrimitiveComponent*)>& Lambda) const
{
	for (const TPair<TObjectKey<UPrimitiveComponent>, FClusterUnionPendingAddData>& Kvp : PendingComponentSync)
	{
		if (UPrimitiveComponent* Component = Kvp.Key.ResolveObjectPtr(); Component && Component->HasValidPhysicsState())
		{
			if (!Lambda(Component))
			{
				return;
			}
		}
	}

	for (const TPair<TObjectKey<UPrimitiveComponent>, FClusteredComponentData>& Kvp : PerComponentData)
	{
		if (UPrimitiveComponent* Component = Kvp.Key.ResolveObjectPtr(); Component && Component->HasValidPhysicsState() && !Kvp.Value.bPendingDeletion)
		{
			if (!Lambda(Component))
			{
				return;
			}
		}
	}
}

void UClusterUnionComponent::VisitAllCurrentActors(const TFunction<bool(AActor*)>& Lambda) const
{
	for (const TPair<TObjectKey<AActor>, FClusteredActorData>& Kvp : ActorToComponents)
	{
		if (AActor* Actor = Kvp.Key.ResolveObjectPtr())
		{
			if (!Lambda(Actor))
			{
				return;
			}
		}
	}
}

void UClusterUnionComponent::VisitAllCurrentChildComponentsForCollision(ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams, const TFunction<bool(UPrimitiveComponent*)>& Lambda) const
{
	TSet<uint32> IgnoredActors;
	IgnoredActors.Reserve(Params.GetIgnoredActors().Num());
	for (uint32 Id : Params.GetIgnoredActors())
	{
		IgnoredActors.Add(Id);
	}

	TSet<uint32> IgnoredComponents;
	IgnoredComponents.Reserve(Params.GetIgnoredComponents().Num());
	for (uint32 Id : Params.GetIgnoredComponents())
	{
		IgnoredComponents.Add(Id);
	}

	VisitAllCurrentChildComponents(
		[TraceChannel, &IgnoredActors, &IgnoredComponents, &Lambda](UPrimitiveComponent* Component)
		{
			if (Component)
			{
				// TODO: Support all the other filters that could exist here.
				if (IgnoredComponents.Contains(Component->GetUniqueID()))
				{
					return true;
				}

				if (AActor* Owner = Component->GetOwner())
				{
					if (IgnoredActors.Contains(Owner->GetUniqueID()))
					{
						return true;
					}
				}

				if (Component->GetCollisionResponseToChannel(TraceChannel) == ECollisionResponse::ECR_Ignore)
				{
					return true;
				}

				Lambda(Component);
			}
			return true;
		}
	);
}

TArray<int32> UClusterUnionComponent::GetAddedBoneIdsForComponent(UPrimitiveComponent* Component) const
{
	if (const FClusteredComponentData* Data = PerComponentData.Find(Component))
	{
		return Data->BoneIds.Array();
	}

	if (const FClusterUnionPendingAddData* Data = PendingComponentSync.Find(Component))
	{
		return Data->BoneIds;
	}

	return {};
}

void UClusterUnionComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UPrimitiveComponent::AddReferencedObjects(InThis, Collector);

	UClusterUnionComponent* This = CastChecked<UClusterUnionComponent>(InThis);

	{
		const UScriptStruct* ScriptStruct = FClusteredComponentData::StaticStruct();
		for (TPair<TObjectKey<UPrimitiveComponent>, FClusteredComponentData>& Kvp : This->PerComponentData)
		{
			TWeakObjectPtr<const UScriptStruct> ScriptStructPtr{ScriptStruct};
			Collector.AddReferencedObjects(ScriptStructPtr, reinterpret_cast<void*>(&Kvp.Value), This, nullptr);
		}
	}

	{
		const UScriptStruct* ScriptStruct = FClusteredActorData::StaticStruct();
		for (TPair<TObjectKey<AActor>, FClusteredActorData>& Kvp : This->ActorToComponents)
		{
			TWeakObjectPtr<const UScriptStruct> ScriptStructPtr{ScriptStruct};
			Collector.AddReferencedObjects(ScriptStructPtr, reinterpret_cast<void*>(&Kvp.Value), This, nullptr);
		}
	}

	{
		const UScriptStruct* ScriptStruct = FClusterUnionPendingAddData::StaticStruct();
		for (TPair<TObjectKey<UPrimitiveComponent>, FClusterUnionPendingAddData>& Kvp : This->PendingComponentsToAdd)
		{
			TWeakObjectPtr<const UScriptStruct> ScriptStructPtr{ScriptStruct};			
			Collector.AddReferencedObjects(ScriptStructPtr, reinterpret_cast<void*>(&Kvp.Value), This, nullptr);
		}
	}

	{
		const UScriptStruct* ScriptStruct = FClusterUnionPendingAddData::StaticStruct();
		for (TPair<TObjectKey<UPrimitiveComponent>, FClusterUnionPendingAddData>& Kvp : This->PendingComponentSync)
		{
			Collector.AddReferencedObjects(ObjectPtrWrap(ScriptStruct), reinterpret_cast<void*>(&Kvp.Value), This, nullptr);
		}
	}
}

int32 UClusterUnionComponent::NumChildClusterComponents() const
{
	return PendingComponentSync.Num() + PerComponentData.Num();
}

bool UClusterUnionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	bool bHasData = false;
	VisitAllCurrentChildComponents(
		[&bHasData, &GeomExport](UPrimitiveComponent* Component)
		{
			if (Component)
			{
				bHasData |= Component->DoCustomNavigableGeometryExport(GeomExport);
			}
			return true;
		}
	);
	return bHasData;
}
