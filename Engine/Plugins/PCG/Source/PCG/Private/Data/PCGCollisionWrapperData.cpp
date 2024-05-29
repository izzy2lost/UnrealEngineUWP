// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGCollisionWrapperData.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCollisionWrapperData)

FPCGCollisionWrapper::~FPCGCollisionWrapper()
{
	Uninitialize();
}

void FPCGCollisionWrapper::Uninitialize()
{
	// Implementation note: we do the full uninitialize even if we think we're not initialized due to the default move operators
	for (FBodyInstance* BodyInstance : BodyInstances)
	{
		if (BodyInstance->IsValidBodyInstance())
		{
			BodyInstance->TermBody();
		}

		delete BodyInstance;
	}

	BodyInstances.Reset();
	IndexToBodyInstance.Reset();

	bInitialized = false;
}

bool FPCGCollisionWrapper::Prepare(const IPCGAttributeAccessor* Accessor, const IPCGAttributeAccessorKeys* Keys, TArray<FSoftObjectPath>& MeshPathsToLoad)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollisionWrapper::Prepare);
	MeshPathsToLoad.Reset();

	if (!ensure(!bInitialized))
	{
		return true;
	}

	IndexToBodyInstance.Reset();

	if (!Accessor || !Keys)
	{
		return false;
	}

	auto GatherMeshPathsAndCreateMap = [this, &MeshPathsToLoad](const TArrayView<FSoftObjectPath>& Meshes, int Start, int Range)
	{
		for (const FSoftObjectPath& Mesh : Meshes)
		{
			IndexToBodyInstance.Add(Mesh.IsNull() ? INDEX_NONE : MeshPathsToLoad.AddUnique(Mesh));
		}
	};

	// Implementation note: if this fails, then the mesh paths to load will also be empty, which will behave like we're expecting it to.
	PCGMetadataElementCommon::ApplyOnAccessorRange<FSoftObjectPath>(*Keys, *Accessor, GatherMeshPathsAndCreateMap, EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible);
	return true;
}

void FPCGCollisionWrapper::CreateBodyInstances(const TArray<FSoftObjectPath>& MeshPaths)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollisionWrapper::CreateBodyInstances);
	if (!ensure(!bInitialized))
	{
		return;
	}

	check(BodyInstances.IsEmpty());
	BodyInstances.Reserve(MeshPaths.Num());

	for (const FSoftObjectPath& MeshPath : MeshPaths)
	{
		FBodyInstance* BodyInstance = nullptr;

		TSoftObjectPtr<UStaticMesh> Mesh(MeshPath);
		UStaticMesh* MeshPtr = nullptr;
	
		{
			//TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollisionWrapper::CreateBodyInstances::LoadMesh);
			MeshPtr = Mesh.LoadSynchronous();
		}

		if(MeshPtr)
		{
			//TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollisionWrapper::CreateBodyInstances::CreateBodyInstance);
			BodyInstance = new FBodyInstance();
			BodyInstance->bAutoWeld = false;
			BodyInstance->bSimulatePhysics = false;
			BodyInstance->InitBody(MeshPtr->GetBodySetup(), FTransform::Identity, nullptr, nullptr);
		}

		BodyInstances.Add(BodyInstance);
	}

	bInitialized = !MeshPaths.IsEmpty();
}

bool FPCGCollisionWrapper::Initialize(const IPCGAttributeAccessor* Accessor, const IPCGAttributeAccessorKeys* Keys)
{
	if (!ensure(!bInitialized))
	{
		return true;
	}

	TArray<FSoftObjectPath> MeshPathsToLoad;
	if (!Prepare(Accessor, Keys, MeshPathsToLoad))
	{
		return false;
	}

	CreateBodyInstances(MeshPathsToLoad);
	return true;
}

FBodyInstance* FPCGCollisionWrapper::GetBodyInstance(int32 EntryIndex) const
{
	return ((bInitialized && IndexToBodyInstance[EntryIndex] != INDEX_NONE) ? BodyInstances[IndexToBodyInstance[EntryIndex]] : nullptr);
}

bool FPCGCollisionWrapper::InitializeOctree(const UPCGPointData* InPointData, const TArray<FSoftObjectPath>& InMeshPaths, UPCGPointData::PointOctree& OutOctree, TArray<FPCGPointRef>* OutOctreePointRefs) const
{
	TArray<FBox> MeshBoundsList;
	MeshBoundsList.Reserve(InMeshPaths.Num());
	bool bHasValidBounds = false;

	for (int32 MeshIndex = 0; MeshIndex < InMeshPaths.Num(); ++MeshIndex)
	{
		TSoftObjectPtr<UStaticMesh> Mesh(InMeshPaths[MeshIndex]);
		if (Mesh.Get()) // should already be loaded by now.
		{
			MeshBoundsList.Add(Mesh->GetBoundingBox());
			bHasValidBounds = true;
		}
		else
		{
			MeshBoundsList.Emplace(EForceInit::ForceInit);
		}
	}

	if (bHasValidBounds)
	{
		const TArray<FPCGPoint>& Points = InPointData->GetPoints();
		TArray<FPCGPointRef> PointRefs;
		PointRefs.Reserve(Points.Num());

		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const FPCGPoint& Point = Points[PointIndex];
			const FBox& MeshBounds = MeshBoundsList[IndexToBodyInstance[PointIndex]];

			if (MeshBounds.IsValid)
			{
				PointRefs.Emplace(Point, MeshBounds);
			}
			else
			{
				PointRefs.Emplace(Point);
			}
		}

		FBox PointsBounds = InPointData->GetBounds();
		TOctree2<FPCGPointRef, FPCGPointRefSemantics> NewOctree(PointsBounds.GetCenter(), PointsBounds.GetExtent().Length());

		for (const FPCGPointRef& PointRef : PointRefs)
		{
			NewOctree.AddElement(PointRef);
		}

		OutOctree = MoveTemp(NewOctree);

		if (OutOctreePointRefs)
		{
			*OutOctreePointRefs = MoveTemp(PointRefs);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool UPCGCollisionWrapperData::Initialize(const UPCGPointData* InPointData, const FPCGAttributePropertyInputSelector& InCollisionSelector, bool bInUseComplexCollision, bool bInUseAccurateOctree)
{
	TArray<FSoftObjectPath> MeshesToLoad;
	if (PreInitializeAndGatherMeshesEx(InPointData, InCollisionSelector, bInUseComplexCollision, MeshesToLoad))
	{
		FinalizeInitializationEx(InPointData, MeshesToLoad, bInUseAccurateOctree);
		return true;
	}
	else
	{
		return false;
	}
}

bool UPCGCollisionWrapperData::PreInitializeAndGatherMeshesEx(const UPCGPointData* InPointData, const FPCGAttributePropertyInputSelector& InCollisionSelector, bool bInUseComplexCollision, TArray<FSoftObjectPath>& OutMeshesToLoad)
{
	CollisionWrapper.Uninitialize();

	check(InPointData);
	PointData = InPointData;
#if WITH_EDITOR
	RawPointData = InPointData;
#endif

	CollisionSelector = InCollisionSelector;
	bUseComplexCollision = bInUseComplexCollision;

	// Go through the point data on the attribute we'll use for the shapes
	// StaticMesh -> see what we're doing in UInstanceStaticMeshComponent::InitInstanceBody, or UStaticMeshComponent::UpdateCollisionFromStaticMesh
	FPCGAttributePropertyInputSelector InputSelector;
	InputSelector = InCollisionSelector.CopyAndFixLast(InPointData);

	TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, InputSelector);
	TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, InputSelector);

	if (!InputAccessor.IsValid() || !InputKeys.IsValid())
	{
		return false;
	}

	return CollisionWrapper.Prepare(InputAccessor.Get(), InputKeys.Get(), OutMeshesToLoad);
}

void UPCGCollisionWrapperData::FinalizeInitializationEx(const UPCGPointData* InPointData, const TArray<FSoftObjectPath>& InMeshPaths, bool bInUseAccurateOctree)
{
	CollisionWrapper.CreateBodyInstances(InMeshPaths);

	if (InPointData && bInUseAccurateOctree)
	{
		bUseCollisionAccurateOctree = CollisionWrapper.InitializeOctree(InPointData, InMeshPaths, CollisionAccurateOctree);
	}
}

void UPCGCollisionWrapperData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);
	GetPointData()->AddToCrc(Ar, bFullDataCrc);

	CollisionSelector.AddToCrc(Ar);
	uint32 UseCollisionAccurateOctree = bUseCollisionAccurateOctree ? 1 : 0;
	Ar << UseCollisionAccurateOctree;

	uint32 UseComplexCollision = bUseComplexCollision ? 1 : 0;
	Ar << UseComplexCollision;
}

void UPCGCollisionWrapperData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	const_cast<UPCGPointData*>(GetPointData())->GetResourceSizeEx(CumulativeResourceSize);
}

FBox UPCGCollisionWrapperData::GetBounds() const
{
	return GetPointData()->GetBounds();
}

FBox UPCGCollisionWrapperData::GetStrictBounds() const
{
	return GetPointData()->GetStrictBounds();
}

const UPCGPointData* UPCGCollisionWrapperData::ToPointData(FPCGContext* Context, const FBox& InBounds) const
{
	return GetPointData()->ToPointData(Context, InBounds);
}

bool UPCGCollisionWrapperData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	if(!CollisionWrapper.bInitialized)
	{
		return GetPointData()->SamplePoint(InTransform, InBounds, OutPoint, OutMetadata);
	}

	// Find all points matching this point in the point data octree
	// For all touching points, check against the actual physics shape we have for that point
	// E.g. get the point collision attribute value, match to our internal list, get the shape (might require loading), then test against the matching shapes
	// note that we need to place the stuff in the right referential too, since our shapes will all be at the origin
	const UPCGPointData::PointOctree& Octree = (bUseCollisionAccurateOctree ? CollisionAccurateOctree : GetPointData()->GetOctree());

	float Density = 0;
	FBox TransformedBounds = InBounds.TransformBy(InTransform);

	Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(TransformedBounds.GetCenter(), TransformedBounds.GetExtent()), [this, &InTransform, &InBounds, &Density](const FPCGPointRef& InPointRef)
	{
		if (InPointRef.Point->Density < Density)
		{
			return;
		}

		bool bHasOverlap = true;

		// Compute point index & find body instance
		// Test against body instance when present
		if (FBodyInstance* BodyInstance = CollisionWrapper.GetBodyInstance(InPointRef.Point - GetPointData()->GetPoints().GetData()))
		{
			// Compute collision shape rotation and transform, knowing that the body instance is at the origin.
			// e.g. we need to take the InTransform and apply the inverse point transform to it.
			FTransform RelativeTransform = InTransform.GetRelativeTransform(InPointRef.Point->Transform);
			FTransform GeomTransform(RelativeTransform.GetRotation(), RelativeTransform.GetLocation());

			FCollisionShape CollisionShape;
			CollisionShape.SetBox(FVector3f(InBounds.GetExtent() * RelativeTransform.GetScale3D()));

			bHasOverlap = FPhysicsInterface::Overlap_Geom(BodyInstance, CollisionShape, GeomTransform.GetRotation(), GeomTransform, nullptr, bUseComplexCollision);
		}

		if (bHasOverlap)
		{
			Density = InPointRef.Point->Density;
		}
	});

	if (Density > 0)
	{
		OutPoint = FPCGPoint();
		OutPoint.Transform = InTransform;
		OutPoint.SetLocalBounds(InBounds);
		OutPoint.Density = Density;

		return true;
	}
	else
	{
		return false;
	}
}

UPCGSpatialData* UPCGCollisionWrapperData::CopyInternal(FPCGContext* Context) const
{
	UPCGCollisionWrapperData* NewCollisionWrapperData = FPCGContext::NewObject_AnyThread<UPCGCollisionWrapperData>(Context);
	NewCollisionWrapperData->Initialize(PointData, CollisionSelector, bUseComplexCollision, bUseCollisionAccurateOctree);

	return NewCollisionWrapperData;
}
