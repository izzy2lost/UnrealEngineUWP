// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMesh/SkinWeightsPaintTool.h"

#include "AssetViewerSettings.h"
#include "Engine/SkeletalMesh.h"
#include "InteractiveToolManager.h"
#include "SkeletalMeshAttributes.h"
#include "Math/UnrealMathUtility.h"
#include "Components/SkeletalMeshComponent.h"

#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ModelingToolTargetUtil.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "MeshModelingToolsEditorOnlyExp.h"
#include "PointSetAdapter.h"
#include "Animation/MirrorDataTable.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Parameterization/MeshLocalParam.h"
#include "Spatial/FastWinding.h"
#include "Async/Async.h"
#include "BaseGizmos/BrushStampIndicator.h"
#include "DynamicMesh/MeshAdapterUtil.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "Spatial/PointSetHashTable.h"
#include "Operations/SmoothBoneWeights.h"
#include "ContextObjectStore.h"
#include "DynamicMeshToMeshDescription.h"
#include "DynamicSubmesh3.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "SkeletalDebugRendering.h"
#include "ToolSetupUtil.h"
#include "ToolTargetManager.h"
#include "Editor/Persona/Public/IPersonaEditorModeManager.h"
#include "Editor/Persona/Public/PersonaModule.h"
#include "Preferences/PersonaOptions.h"
#include "PreviewProfileController.h"
#include "Animation/SkinWeightProfile.h"
#include "AnimationRuntime.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "Operations/TransferBoneWeights.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinWeightsPaintTool)

#define LOCTEXT_NAMESPACE "USkinWeightsPaintTool"

using namespace SkinPaintTool;

namespace SkinPaintTool
{
	
EMeshLODIdentifier GetLODId(const FName InLODName)
{
	static const TMap<FName, EMeshLODIdentifier> LODs({
		{"LOD0", EMeshLODIdentifier::LOD0},
		{"LOD1", EMeshLODIdentifier::LOD1},
		{"LOD2", EMeshLODIdentifier::LOD2},
		{"LOD3", EMeshLODIdentifier::LOD3},
		{"LOD4", EMeshLODIdentifier::LOD4},
		{"LOD5", EMeshLODIdentifier::LOD5},
		{"LOD6", EMeshLODIdentifier::LOD6},
		{"LOD7", EMeshLODIdentifier::LOD7},
		{"HiResSource", EMeshLODIdentifier::HiResSource},
		{"Default", EMeshLODIdentifier::Default},
		{"MaxQuality", EMeshLODIdentifier::MaxQuality}
	});

	const EMeshLODIdentifier* LODIdFound = LODs.Find(InLODName);
	return LODIdFound ? *LODIdFound : EMeshLODIdentifier::Default;
}

FName GetLODName(const EMeshLODIdentifier InLOD)
{
	static const TMap<EMeshLODIdentifier, FName> LODs({
		{EMeshLODIdentifier::LOD0, "LOD0"},
		{EMeshLODIdentifier::LOD1, "LOD1"},
		{EMeshLODIdentifier::LOD2, "LOD2"},
		{EMeshLODIdentifier::LOD3, "LOD3"},
		{EMeshLODIdentifier::LOD4, "LOD4"},
		{EMeshLODIdentifier::LOD5, "LOD5"},
		{EMeshLODIdentifier::LOD6, "LOD6"},
		{EMeshLODIdentifier::LOD7, "LOD7"},
		{EMeshLODIdentifier::HiResSource, "HiResSource"},
		{EMeshLODIdentifier::Default, "Default"},
		{EMeshLODIdentifier::MaxQuality, "MaxQuality"}
	});

	const FName* Name = LODs.Find(InLOD);
	return Name ? *Name : NAME_None;
}

USkeletalMeshComponent* GetSkeletalMeshComponent(const UToolTarget* InTarget)
{
	if (!ensure(InTarget))
	{
		return nullptr;
	}
	
	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(InTarget);
	if (!ensure(TargetComponent))
	{
		return nullptr;
	}
	
	USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent());
	if (!ensure(Component && Component->GetSkeletalMeshAsset()))
	{
		return nullptr;
	}

	return Component;
}

const FName& CreateNewName()
{
	static const FName CreateNew("Create New...");
	return CreateNew;
}

FSkinWeightsVertexAttributesRef GetOrCreateSkinWeightsAttribute(FMeshDescription& InMesh, const FName InProfileName)
{
	FSkeletalMeshAttributes MeshAttribs(InMesh);
	const TArray<FName> Profiles = MeshAttribs.GetSkinWeightProfileNames();
	if (!Profiles.Contains(InProfileName))
	{
		ensure( MeshAttribs.RegisterSkinWeightAttribute(InProfileName) );
	}
	return MeshAttribs.GetVertexSkinWeights(InProfileName);
}

bool RenameSkinWeightsAttribute(FMeshDescription& InMesh, const FName InOldName, const FName InNewName)
{
	FSkeletalMeshAttributes MeshAttribs(InMesh);
	const TArray<FName> Profiles = MeshAttribs.GetSkinWeightProfileNames();
	if (Profiles.Contains(InOldName))
	{
		FSkinWeightsVertexAttributesRef NewWeightsAttr = GetOrCreateSkinWeightsAttribute(InMesh, InNewName);
		NewWeightsAttr = MeshAttribs.GetVertexSkinWeights(InOldName);
		return MeshAttribs.UnregisterSkinWeightAttribute(InOldName);
	}
	return false;
}
	
}

// thread pool to use for async operations
static EAsyncExecution SkinPaintToolAsyncExecTarget = EAsyncExecution::ThreadPool;

// any weight below this value is ignored, since it won't be representable in unsigned 16-bit precision
constexpr float MinimumWeightThreshold = 1.0f / 65535.0f;

class FPaintToolWeightsDataSource : public UE::Geometry::TBoneWeightsDataSource<int32, float>
{
public:

	FPaintToolWeightsDataSource(const SkinPaintTool::FSkinToolWeights* InWeights)
	:
	Weights(InWeights) 
	{
		checkSlow(Weights);
	}

	virtual ~FPaintToolWeightsDataSource() = default;

	virtual int32 GetBoneNum(const int32 VertexID) override
	{
		return Weights->PreChangeWeights[VertexID].Num();
	}

	virtual int32 GetBoneIndex(const int32 VertexID, const int32 Index) override
	{
		return Weights->PreChangeWeights[VertexID][Index].BoneIndex;
	}

	virtual float GetBoneWeight(const int32 VertexID, const int32 Index) override
	{
		return Weights->PreChangeWeights[VertexID][Index].Weight;
	}

	virtual float GetWeightOfBoneOnVertex(const int32 VertexID, const int32 BoneIndex) override
	{
		return Weights->GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights->PreChangeWeights);
	}

protected:

	const SkinPaintTool::FSkinToolWeights* Weights = nullptr;
};

void FDirectEditWeightState::Reset()
{
	bInTransaction = false;
	StartValue = CurrentValue = GetModeDefaultValue();
}

float FDirectEditWeightState::GetModeDefaultValue()
{
	static TMap<EWeightEditOperation, float> DefaultModeValues = {
		{EWeightEditOperation::Add, 0.0f},
		{EWeightEditOperation::Replace, .0f},
		{EWeightEditOperation::Multiply, 1.f},
		{EWeightEditOperation::Relax, 0.f}
	};

	return DefaultModeValues[EditMode];
}

float FDirectEditWeightState::GetModeMinValue()
{
	static TMap<EWeightEditOperation, float> MinModeValues = {
		{EWeightEditOperation::Add, -1.f},
		{EWeightEditOperation::Replace, 0.f},
		{EWeightEditOperation::Multiply, 0.f},
		{EWeightEditOperation::Relax, 0.f}
	};

	return MinModeValues[EditMode];
}

float FDirectEditWeightState::GetModeMaxValue()
{
	static TMap<EWeightEditOperation, float> MaxModeValues = {
		{EWeightEditOperation::Add, 1.f},
		{EWeightEditOperation::Replace, 1.f},
		{EWeightEditOperation::Multiply, 2.f},
		{EWeightEditOperation::Relax, 10.f}
	};

	return MaxModeValues[EditMode];
}

USkinWeightsPaintToolProperties::USkinWeightsPaintToolProperties()
{
	BrushConfigs.Add(EWeightEditOperation::Add, &BrushConfigAdd);
	BrushConfigs.Add(EWeightEditOperation::Replace, &BrushConfigReplace);
	BrushConfigs.Add(EWeightEditOperation::Multiply, &BrushConfigMultiply);
	BrushConfigs.Add(EWeightEditOperation::Relax, &BrushConfigRelax);

	LoadConfig();

	if (ColorRamp.IsEmpty())
	{
		// default color ramp simulates a heat map
		ColorRamp.Add(FLinearColor(0.8f, 0.4f, 0.8f)); // Purple
		ColorRamp.Add(FLinearColor(0.0f, 0.0f, 0.5f)); // Dark Blue
		ColorRamp.Add(FLinearColor(0.2f, 0.2f, 1.0f)); // Light Blue
		ColorRamp.Add(FLinearColor(0.0f, 1.0f, 0.0f)); // Green
		ColorRamp.Add(FLinearColor(1.0f, 1.0f, 0.0f)); // Yellow
		ColorRamp.Add(FLinearColor(1.0f, 0.65f, 0.0f)); // Orange
		ColorRamp.Add(FLinearColor(1.0f, 0.0f, 0.0f, 0.0f)); // Red
	}
}

namespace SkinWeightLayer
{

TArray<FName> GetLODs(UToolTarget* InTarget)
{
    static TArray<FName> Dummy;

    if (!ensure(InTarget))
    {
    	return Dummy;
    }

    bool bSupportsLODs = false;
    const TArray<EMeshLODIdentifier> LODIDSs = UE::ToolTarget::GetMeshDescriptionLODs(InTarget, bSupportsLODs);
    if (!ensure(bSupportsLODs))
    {
    	return Dummy;
    }

    TArray<FName> LODs;
    LODs.Reserve(LODIDSs.Num());
    for (const EMeshLODIdentifier LODId: LODIDSs)
    {
    	const FName LODName = GetLODName(LODId);
    	if (LODName != NAME_None)
    	{
    		LODs.Add(LODName);
    	}
    }
    ensure(!LODs.IsEmpty());
    
    return LODs;
}

TArray<FName> GetSkinWeightProfilesFunc(const FMeshDescription& InMeshDescription)
{
	const FSkeletalMeshConstAttributes MeshAttribs(InMeshDescription);
	return MeshAttribs.GetSkinWeightProfileNames();
}

}

TArray<FName> USkinWeightsPaintToolProperties::GetLODsFunc() const
{
	static TArray<FName> Dummy;
	if (!ensure(WeightTool && WeightTool->GetTarget()))
	{
		return Dummy;
	}
	
	return SkinWeightLayer::GetLODs(WeightTool->GetTarget());
}

TArray<FName> USkinWeightsPaintToolProperties::GetSkinWeightProfilesFunc() const
{
	if (const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(WeightTool->GetTarget()))
	{
		const EMeshLODIdentifier LODId = GetLODId(ActiveLOD);
		const FGetMeshParameters Params(true, LODId);
		const FMeshDescription* MeshDescription = UE::ToolTarget::GetMeshDescription(WeightTool->GetTarget(), Params);
		TArray Profiles = SkinWeightLayer::GetSkinWeightProfilesFunc(*MeshDescription);
		Profiles.Add(CreateNewName());
		return Profiles;
	}

	static const TArray Profiles = {FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName, CreateNewName()};
	return Profiles;
}

TArray<FName> USkinWeightsPaintToolProperties::GetSourceLODsFunc() const
{
	if (WeightTool->GetSourceTarget())
	{
		return SkinWeightLayer::GetLODs(WeightTool->GetSourceTarget());	
	}
	return GetLODsFunc();	
}

TArray<FName> USkinWeightsPaintToolProperties::GetSourceSkinWeightProfilesFunc() const
{
	if (USkeletalMesh* SrcSkeletalMesh = SourceSkeletalMesh.Get())
	{
		const EMeshLODIdentifier LODId = GetLODId(SourceLOD);
		const FGetMeshParameters Params(true, LODId);
		const FMeshDescription* MeshDescription = UE::ToolTarget::GetMeshDescription(WeightTool->GetSourceTarget(), Params);
		return SkinWeightLayer::GetSkinWeightProfilesFunc(*MeshDescription);
	}
	return GetSkinWeightProfilesFunc();
}

FName USkinWeightsPaintToolProperties::GetActiveSkinWeightProfile() const
{
	return bShowNewProfileName ? NewSkinWeightProfile : ActiveSkinWeightProfile;
}

FSkinWeightBrushConfig& USkinWeightsPaintToolProperties::GetBrushConfig()
{
	return *BrushConfigs[BrushMode];
}

void FMultiBoneWeightEdits::MergeSingleEdit(
	const int32 BoneIndex,
	const int32 VertexID,
	const float OldWeight,
	const float NewWeight)
{
	FSingleBoneWeightEdits& BoneWeightEdit = PerBoneWeightEdits.FindOrAdd(BoneIndex);
	BoneWeightEdit.BoneIndex = BoneIndex;
	BoneWeightEdit.NewWeights.Add(VertexID, NewWeight);
	BoneWeightEdit.OldWeights.FindOrAdd(VertexID, OldWeight);
}

void FMultiBoneWeightEdits::MergeEdits(const FSingleBoneWeightEdits& BoneWeightEdits)
{
	// make sure bone has an entry in the map of weight edits
	const int32 BoneIndex = BoneWeightEdits.BoneIndex;
	PerBoneWeightEdits.FindOrAdd(BoneIndex);
	PerBoneWeightEdits[BoneIndex].BoneIndex = BoneIndex;
	
	for (const TTuple<int32, float>& NewWeight : BoneWeightEdits.NewWeights)
	{
		int32 VertexIndex = NewWeight.Key;
		PerBoneWeightEdits[BoneIndex].NewWeights.Add(VertexIndex, NewWeight.Value);
		PerBoneWeightEdits[BoneIndex].OldWeights.FindOrAdd(VertexIndex, BoneWeightEdits.OldWeights[VertexIndex]);
	}
}

float FMultiBoneWeightEdits::GetVertexDeltaFromEdits(const int32 BoneIndex, const int32 VertexIndex)
{
	PerBoneWeightEdits.FindOrAdd(BoneIndex);
	if (const float* NewVertexWeight = PerBoneWeightEdits[BoneIndex].NewWeights.Find(VertexIndex))
	{
		return *NewVertexWeight - PerBoneWeightEdits[BoneIndex].OldWeights[VertexIndex];
	}

	return 0.0f;
}

void FMultiBoneWeightEdits::GetEditedVertexIndices(TSet<int32>& OutVerticesToEdit) const
{
	TSet<int32> VerticesInEdit;
	for (const TTuple<int32, FSingleBoneWeightEdits>& Pair : PerBoneWeightEdits)
	{
		Pair.Value.NewWeights.GetKeys(VerticesInEdit);
		OutVerticesToEdit.Append(VerticesInEdit);
	}
}

void FSkinToolDeformer::Initialize(const USkeletalMeshComponent* InSkelMeshComponent, const FMeshDescription* InMeshDescription)
{
	// get all bone transforms in the reference pose store a copy in component space
	Component = InSkelMeshComponent;
	const FReferenceSkeleton& RefSkeleton = Component->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FTransform> &LocalSpaceBoneTransforms = RefSkeleton.GetRefBonePose();
	const int32 NumBones = LocalSpaceBoneTransforms.Num();
	InvCSRefPoseTransforms.SetNumUninitialized(NumBones);
	for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
	{
		const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		const FTransform& LocalTransform = LocalSpaceBoneTransforms[BoneIndex];
		if (ParentBoneIndex != INDEX_NONE)
		{
			InvCSRefPoseTransforms[BoneIndex] = LocalTransform * InvCSRefPoseTransforms[ParentBoneIndex];
		}
		else
		{
			InvCSRefPoseTransforms[BoneIndex] = LocalTransform;
		}
	}
	
	for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
	{
		// pre-invert the transforms so we don't have to at runtime
		InvCSRefPoseTransforms[BoneIndex] = InvCSRefPoseTransforms[BoneIndex].Inverse();

		// store map of bone indices to bone names
		FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		BoneNames.Add(BoneName);
		BoneNameToIndexMap.Add(BoneName, BoneIndex);
	}

	// store reference pose vertex positions
	const TArrayView<const FVector3f> VertexPositions = InMeshDescription->GetVertexPositions().GetRawArray();
	RefPoseVertexPositions = VertexPositions;

	// set all vertices to be updated on first tick
	SetAllVerticesToBeUpdated();

	// record "prev" bone transforms to detect change in pose
	PreviousPoseComponentSpace = Component->GetComponentSpaceTransforms();
}

void FSkinToolDeformer::SetAllVerticesToBeUpdated()
{
	VerticesWithModifiedWeights.Empty(RefPoseVertexPositions.Num());
	for (int32 VertexID=0; VertexID<RefPoseVertexPositions.Num(); ++VertexID)
	{
		VerticesWithModifiedWeights.Add(VertexID);
	}
}

void FSkinToolDeformer::SetToRefPose(USkinWeightsPaintTool* Tool)
{
	// get ref pose
	const FReferenceSkeleton& RefSkeleton = Component->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FTransform>& RefPoseLocalSpace = RefSkeleton.GetRefBonePose();
	// convert to global space and store in current pose
	FAnimationRuntime::FillUpComponentSpaceTransforms(RefSkeleton, RefPoseLocalSpace, RefPoseComponentSpace);
	// update mesh to new pose
	UpdateVertexDeformation(Tool, RefPoseComponentSpace);
}

void FSkinToolDeformer::UpdateVertexDeformation(
	USkinWeightsPaintTool* Tool,
	const TArray<FTransform>& PoseComponentSpace)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateDeformationTotal);

	// if no weights have been modified, we must check for a modified pose which requires re-calculation of skinning
	if (VerticesWithModifiedWeights.IsEmpty())
	{
		for (int32 BoneIndex=0; BoneIndex<PoseComponentSpace.Num(); ++BoneIndex)
		{
			if (!Tool->Weights.IsBoneWeighted[BoneIndex])
			{
				continue;
			}
			
			const FTransform& CurrentBoneTransform = PoseComponentSpace[BoneIndex];
			const FTransform& PrevBoneTransform = PreviousPoseComponentSpace[BoneIndex];
			if (!CurrentBoneTransform.Equals(PrevBoneTransform))
			{
				SetAllVerticesToBeUpdated();
				break;
			}
		}
	}

	if (VerticesWithModifiedWeights.IsEmpty())
	{
		return;
	}
	
	// update vertex positions
	UPreviewMesh* PreviewMesh = Tool->PreviewMesh;
	const TArray<VertexWeights>& CurrentWeights = Tool->Weights.CurrentWeights;
	PreviewMesh->DeferredEditMesh([this, &CurrentWeights, &PoseComponentSpace](FDynamicMesh3& Mesh)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateDeformation);
		const TArray<int32> VertexIndices = VerticesWithModifiedWeights.Array();
		
		ParallelFor( VerticesWithModifiedWeights.Num(), [this, &VertexIndices, &Mesh, &PoseComponentSpace, &CurrentWeights](int32 Index)
		{
			const int32 VertexID = VertexIndices[Index];
			FVector VertexNewPosition = FVector::ZeroVector;
			const VertexWeights& VertexPerBoneData = CurrentWeights[VertexID];
			for (const FVertexBoneWeight& VertexData : VertexPerBoneData)
			{
				if (VertexData.BoneIndex == INDEX_NONE)
				{
					continue;
				}
				const FTransform& CurrentTransform = PoseComponentSpace[VertexData.BoneIndex];
				VertexNewPosition += CurrentTransform.TransformPosition(VertexData.VertexInBoneSpace) * VertexData.Weight;
			}
			
			Mesh.SetVertex(VertexID, VertexNewPosition, false);
		});
	}, false);
	PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::Positions, false);

	// what mode are we in?
	const EWeightEditMode EditingMode = Tool->WeightToolProperties->EditingMode;
	
	// update data structures used by the brush mode	
	if (EditingMode == EWeightEditMode::Brush)
	{
		// update vertex acceleration structure
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateVertexOctree);
			Tool->VerticesOctree->RemoveVertices(VerticesWithModifiedWeights);
			Tool->VerticesOctree->InsertVertices(VerticesWithModifiedWeights);
		}
		
		// update triangle acceleration structure
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateTriangleOctree);

			// create list of triangles that were affected by the vertices that were deformed
			TArray<int32>& AffectedTriangles = Tool->TrianglesToReinsert; // reusable buffer of triangles to update
			{
				AffectedTriangles.Reset();

				// reinsert all triangles containing an updated vertex
				const FDynamicMesh3* DynamicMesh = PreviewMesh->GetMesh();
				for (const int32 TriangleID : DynamicMesh->TriangleIndicesItr())
				{
					UE::Geometry::FIndex3i TriVerts = DynamicMesh->GetTriangle(TriangleID);
					bool bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[0]);
					bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[1]) ? true : bIsTriangleAffected;
					bIsTriangleAffected = VerticesWithModifiedWeights.Contains(TriVerts[2]) ? true : bIsTriangleAffected;
					if (bIsTriangleAffected)
					{
						AffectedTriangles.Add(TriangleID);
					}
				}
			}

			// ensure previous async update is finished before queuing the next one...
			Tool->TriangleOctreeFuture.Wait();
		
			// asynchronously update the octree, this normally finishes well before the next update
			// but in the unlikely event that it does not, it would result in a frame where the paint brush
			// is not perfectly aligned with the mesh; not a deal breaker.
			UE::Geometry::FDynamicMeshOctree3& OctreeToUpdate = *Tool->TrianglesOctree;
			Tool->TriangleOctreeFuture = Async(SkinPaintToolAsyncExecTarget, [&OctreeToUpdate, &AffectedTriangles]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::TriangleOctreeReinsert);	
				OctreeToUpdate.ReinsertTriangles(AffectedTriangles);
			});
		}
	}

	// update data structures used by the selection mode
	if (EditingMode == EWeightEditMode::Mesh)
	{
		// update AABB Tree for vertex selection
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateAABBTree);
		Tool->MeshSpatial->Build();
		Tool->PolygonSelectionMechanic->UMeshTopologySelectionMechanic::GetTopologySelector()->Invalidate(true, false);
	}

	// empty queue of vertices to update
	VerticesWithModifiedWeights.Reset();

	// record the skeleton state we used to update the deformations
	PreviousPoseComponentSpace = PoseComponentSpace;
}

void FSkinToolDeformer::SetVertexNeedsUpdated(int32 VertexIndex)
{
	VerticesWithModifiedWeights.Add(VertexIndex);
}

void FSkinToolWeights::InitializeSkinWeights(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FMeshDescription* Mesh)
{
	static constexpr int32 RootBoneIndex = 0;
	static constexpr float FullWeight = 1.f;

	// initialize deformer data
	Deformer.Initialize(SkeletalMeshComponent, Mesh);

	// initialize current weights (using compact format: num_verts * max_influences)
	const FSkeletalMeshConstAttributes MeshAttribs(*Mesh);
	const FSkinWeightsVertexAttributesConstRef VertexSkinWeights = MeshAttribs.GetVertexSkinWeights(Profile);
	const int32 NumVertices = Mesh->Vertices().Num();
	CurrentWeights.SetNum(NumVertices);
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		const FVertexID VertexID(VertexIndex);
		int32 InfluenceIndex = 0;
		for (UE::AnimationCore::FBoneWeight BoneWeight: VertexSkinWeights.Get(VertexID))
		{
			check(InfluenceIndex < MAX_TOTAL_INFLUENCES);
			int32 BoneIndex = BoneWeight.GetBoneIndex();
			if (!ensure(Deformer.InvCSRefPoseTransforms.IsValidIndex(BoneIndex)))
			{
				UE_LOG(LogMeshModelingToolsEditor, Warning, TEXT("InitializeSkinWeights: Invalid bone index provided (%d); falling back to 0 as bone index."), BoneIndex);
				BoneIndex = 0;
			}
			const float Weight = BoneWeight.GetWeight();
			const FVector& RefPoseVertexPosition = Deformer.RefPoseVertexPositions[VertexIndex];
			const FTransform& InvRefPoseTransform = Deformer.InvCSRefPoseTransforms[BoneIndex];
			const FVector& BoneLocalPositionInRefPose = InvRefPoseTransform.TransformPosition(RefPoseVertexPosition);
			CurrentWeights[VertexIndex].Emplace(BoneIndex, BoneLocalPositionInRefPose, Weight);
			++InfluenceIndex;
		}

		// if there are no bone weights, default to root bone 
		if (InfluenceIndex == 0)
		{
			const FVector& RefPoseVertexPosition = Deformer.RefPoseVertexPositions[VertexIndex];
			const FTransform& InvRefPoseTransform = Deformer.InvCSRefPoseTransforms[RootBoneIndex];
			const FVector& BoneLocalPositionInRefPose = InvRefPoseTransform.TransformPosition(RefPoseVertexPosition);
			CurrentWeights[VertexIndex].Emplace(RootBoneIndex, BoneLocalPositionInRefPose, FullWeight);
		}
	}
	
	// maintain duplicate weight map
	PreChangeWeights = CurrentWeights;

	// maintain relax-per stroke map
	MaxFalloffPerVertexThisStroke.SetNumZeroed(NumVertices);

	// maintain bool-per-bone if weighted or not
	IsBoneWeighted.Init(false, Deformer.BoneNames.Num());
	for (const VertexWeights& VertexData : CurrentWeights)
	{
		for (const FVertexBoneWeight& VertexBoneData : VertexData)
		{
			if (VertexBoneData.Weight > UE::AnimationCore::BoneWeightThreshold)
			{
				IsBoneWeighted[VertexBoneData.BoneIndex] = true;
			}
		}
	}
}

void FSkinToolWeights::EditVertexWeightAndNormalize(
	const int32 BoneToHoldIndex,
	const int32 VertexID,
	float NewWeightValue,
	FMultiBoneWeightEdits& WeightEdits)
{
	// clamp new weight
	NewWeightValue = FMath::Clamp(NewWeightValue, 0.0f, 1.0f);
	
	// calculate the sum of all the weights on this vertex (not including the one we currently applied)
	TArray<int32> BonesAffectingVertex;
	TArray<float> ValuesToNormalize;
	float Total = 0.0f;
	const VertexWeights& VertexData = PreChangeWeights[VertexID];
	for (const FVertexBoneWeight& VertexBoneData : VertexData)
	{
		if (VertexBoneData.BoneIndex == BoneToHoldIndex)
		{
			continue;
		}
		
		if (VertexBoneData.Weight < MinimumWeightThreshold)
		{
			continue;
		}
		
		BonesAffectingVertex.Add(VertexBoneData.BoneIndex);
		ValuesToNormalize.Add(VertexBoneData.Weight);
		Total += VertexBoneData.Weight;
	}

	// are there no OTHER influences on this vertex?
	const bool bVertexHasNoOtherInfluences = Total <= MinimumWeightThreshold;
	
	// if user applied any weight to this vertex AND there's no other weights of any significance,
	// then simply apply full weight to this vertex, set all other influences to zero and return
	const bool bApplyWeightToThisVertex = NewWeightValue >= MinimumWeightThreshold;
	if (bApplyWeightToThisVertex && bVertexHasNoOtherInfluences)
	{
		// set all other influences to 0.0f
		for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
		{
			const int32 BoneIndex = BonesAffectingVertex[i];
			const float OldWeight = ValuesToNormalize[i];
			constexpr float NewWeight = 0.0f;
			WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
		}

		// set current bone value to 1.0f
		const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
		WeightEdits.MergeSingleEdit(
			BoneToHoldIndex,
			VertexID,
			PrevWeight,
			1.0f);
		
		return;
	}

	// is the user trying to prune ALL weight from this vertex, AND all other weights are equal to zero?
	// in this case, we have two options:
	// 1. if there are other influences recorded for this vertex, then split remaining influence among them
	// 2. if there are NO other influences recorded for this vertex, then move the weight to the parent as last ditch effort
	const bool bApplyZeroWeightToThisVertex = NewWeightValue <= MinimumWeightThreshold;
	if (bApplyZeroWeightToThisVertex && bVertexHasNoOtherInfluences)
	{
		if (ValuesToNormalize.IsEmpty())
		{
			// assign all weight to the parent
			int32 BoneIndex = 0;
			if (const USkeletalMesh* SkeletalMesh = Deformer.Component->GetSkeletalMeshAsset())
			{
				BoneIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneToHoldIndex);
				BoneIndex = BoneIndex == INDEX_NONE ? 0 : BoneIndex;
			}
			
			constexpr float OldWeight = 0.f;
			constexpr float NewWeight = 1.f;
			WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
		}
		else
		{
			// evenly distribute weight among other influences
			const float NewWeight = 1.f / ValuesToNormalize.Num();
			for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
			{
				const int32 BoneIndex = BonesAffectingVertex[i];
				const float OldWeight = ValuesToNormalize[i];
				WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
			}
		}

		// set current bone value to 0.0f
		const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
		WeightEdits.MergeSingleEdit(
			BoneToHoldIndex,
			VertexID,
			PrevWeight,
			0.f);
		
		return;
	}

	// calculate amount we have to spread across the other bones affecting this vertex
	const float AvailableTotal = 1.0f - NewWeightValue;

	// normalize weights into available space not set by current bone
	for (int32 i=0; i<ValuesToNormalize.Num(); ++i)
	{
		float NormalizedValue = 0.f;
		if (AvailableTotal > MinimumWeightThreshold && Total > KINDA_SMALL_NUMBER)
		{
			NormalizedValue = (ValuesToNormalize[i] / Total) * AvailableTotal;	
		}
		const int32 BoneIndex = BonesAffectingVertex[i];
		const float OldWeight = ValuesToNormalize[i];
		const float NewWeight = NormalizedValue;
		WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
	}

	// record current bone edit
	const float PrevWeight = GetWeightOfBoneOnVertex(BoneToHoldIndex, VertexID, PreChangeWeights);
	WeightEdits.MergeSingleEdit(
		BoneToHoldIndex,
		VertexID,
		PrevWeight,
		NewWeightValue);
}

void FSkinToolWeights::ApplyCurrentWeightsToMeshDescription(FMeshDescription* MeshDescription)
{
	FSkeletalMeshAttributes MeshAttribs(*MeshDescription);
	FSkinWeightsVertexAttributesRef VertexSkinWeights = MeshAttribs.GetVertexSkinWeights(Profile);
	
	UE::AnimationCore::FBoneWeightsSettings Settings;
	Settings.SetNormalizeType(UE::AnimationCore::EBoneWeightNormalizeType::None);

	TArray<UE::AnimationCore::FBoneWeight> SourceBoneWeights;
	SourceBoneWeights.Reserve(UE::AnimationCore::MaxInlineBoneWeightCount);

	const int32 NumVertices = MeshDescription->Vertices().Num();
	if (!ensure(CurrentWeights.Num() == NumVertices))
	{
		// weights are out of sync with mesh description you're trying to apply them to
		return;
	}
	
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
	{
		SourceBoneWeights.Reset();

		const VertexWeights& VertexWeights = CurrentWeights[VertexIndex];
		for (const FVertexBoneWeight& SingleBoneWeight : VertexWeights)
		{
			SourceBoneWeights.Add(UE::AnimationCore::FBoneWeight(SingleBoneWeight.BoneIndex, SingleBoneWeight.Weight));
		}

		VertexSkinWeights.Set(FVertexID(VertexIndex), UE::AnimationCore::FBoneWeights::Create(SourceBoneWeights, Settings));
	}
}

float FSkinToolWeights::GetWeightOfBoneOnVertex(
	const int32 BoneIndex,
	const int32 VertexID,
	const TArray<VertexWeights>& InVertexWeights)
{
	const VertexWeights& VertexWeights = InVertexWeights[VertexID];
	for (const FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		if (BoneWeight.BoneIndex == BoneIndex)
		{
			return BoneWeight.Weight;
		}
	}

	return 0.f;
}

void FSkinToolWeights::SetWeightOfBoneOnVertex(
	const int32 BoneIndex,
	const int32 VertexID,
	const float Weight,
	TArray<VertexWeights>& InOutVertexWeights)
{
	Deformer.SetVertexNeedsUpdated(VertexID);
	
	// incoming weights are assumed to be normalized already, so set it directly
	VertexWeights& VertexWeights = InOutVertexWeights[VertexID];
	for (FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		if (BoneWeight.BoneIndex == BoneIndex)
		{
			BoneWeight.Weight = Weight;
			return;
		}
	}

	// bone not already an influence on this vertex, so we need to add it..

	// if vertex has room for more influences, then simply add it
	if (VertexWeights.Num() < UE::AnimationCore::MaxInlineBoneWeightCount)
	{
		// add a new influence to this vertex
		const FVector PosLocalToBone = Deformer.InvCSRefPoseTransforms[BoneIndex].TransformPosition(Deformer.RefPoseVertexPositions[VertexID]);
		VertexWeights.Emplace(BoneIndex, PosLocalToBone, Weight);
		return;
	}

	//
	// uh oh, we're out of room for more influences on this vertex, so lets kick the smallest influence to make room
	//

	// find the smallest influence
	float SmallestInfluence = TNumericLimits<float>::Max();
	int32 SmallestInfluenceIndex = INDEX_NONE;
	for (int32 InfluenceIndex=0; InfluenceIndex<VertexWeights.Num(); ++InfluenceIndex)
	{
		const FVertexBoneWeight& BoneWeight = VertexWeights[InfluenceIndex];
		if (BoneWeight.Weight <= SmallestInfluence)
		{
			SmallestInfluence = BoneWeight.Weight;
			SmallestInfluenceIndex = InfluenceIndex;
		}
	}

	// replace smallest influence
	FVertexBoneWeight& BoneWeightToReplace = VertexWeights[SmallestInfluenceIndex];
	BoneWeightToReplace.Weight = Weight;
	BoneWeightToReplace.BoneIndex = BoneIndex;
	BoneWeightToReplace.VertexInBoneSpace = Deformer.InvCSRefPoseTransforms[BoneIndex].TransformPosition(Deformer.RefPoseVertexPositions[VertexID]);

	// now we need to re-normalize because the stamp does not handle maximum influences
	float TotalWeight = 0.f;
	for (const FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		TotalWeight += BoneWeight.Weight;
	}
	for (FVertexBoneWeight& BoneWeight : VertexWeights)
	{
		BoneWeight.Weight /= TotalWeight;
	}
}

void FSkinToolWeights::SwapAfterChange()
{
	PreChangeWeights = CurrentWeights;

	for (int32 i=0; i<MaxFalloffPerVertexThisStroke.Num(); ++i)
	{
		MaxFalloffPerVertexThisStroke[i] = 0.f;
	}
}

float FSkinToolWeights::SetCurrentFalloffAndGetMaxFalloffThisStroke(int32 VertexID, float CurrentStrength)
{
	float& MaxFalloffThisStroke = MaxFalloffPerVertexThisStroke[VertexID];
	if (MaxFalloffThisStroke < CurrentStrength)
	{
		MaxFalloffThisStroke = CurrentStrength;
	}
	return MaxFalloffThisStroke;
}

void FSkinToolWeights::ApplyEditsToCurrentWeights(const FMultiBoneWeightEdits& Edits)
{
	// apply weight edits to the CurrentWeights data
	for (const TTuple<BoneIndex, FSingleBoneWeightEdits>& BoneWeightEdits : Edits.PerBoneWeightEdits)
	{
		const FSingleBoneWeightEdits& WeightEdits = BoneWeightEdits.Value;
		const int32 BoneIndex = WeightEdits.BoneIndex;
		for (const TTuple<int32, float>& NewWeight : WeightEdits.NewWeights)
		{
			const int32 VertexID = NewWeight.Key;
			const float Weight = NewWeight.Value;
			SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, CurrentWeights);
		}
	}

	// weights on Bones were modified, so update IsBoneWeighted array
	for (const TTuple<BoneIndex, FSingleBoneWeightEdits>& BoneWeightEdits : Edits.PerBoneWeightEdits)
	{
		const BoneIndex CurrentBoneIndex = BoneWeightEdits.Key;
		UpdateIsBoneWeighted(CurrentBoneIndex);
	}
}

void FSkinToolWeights::UpdateIsBoneWeighted(BoneIndex BoneToUpdate)
{
	IsBoneWeighted[BoneToUpdate] = false;
	for (const VertexWeights& VertexData : CurrentWeights)
	{
		for (const FVertexBoneWeight& VertexBoneData : VertexData)
		{
			if (VertexBoneData.BoneIndex == BoneToUpdate && VertexBoneData.Weight > UE::AnimationCore::BoneWeightThreshold)
			{
				IsBoneWeighted[BoneToUpdate] = true;
				break;
			}
		}
		if (IsBoneWeighted[BoneToUpdate])
		{
			break;
		}
	}
}

void FMeshSkinWeightsChange::Apply(UObject* Object)
{
	USkinWeightsPaintTool* Tool = CastChecked<USkinWeightsPaintTool>(Object);

	Tool->ExternalUpdateSkinWeightLayer(LOD, SkinWeightProfile);
	for (TTuple<int32, FSingleBoneWeightEdits>& Pair : AllWeightEdits.PerBoneWeightEdits)
	{
		Tool->ExternalUpdateWeights(Pair.Key, Pair.Value.NewWeights);
	}
}

void FMeshSkinWeightsChange::Revert(UObject* Object)
{
	USkinWeightsPaintTool* Tool = CastChecked<USkinWeightsPaintTool>(Object);

	Tool->ExternalUpdateSkinWeightLayer(LOD, SkinWeightProfile);
	for (TTuple<int32, FSingleBoneWeightEdits>& Pair : AllWeightEdits.PerBoneWeightEdits)
	{
		Tool->ExternalUpdateWeights(Pair.Key, Pair.Value.OldWeights);
	}

	// notify dependent systems
	Tool->OnWeightsChanged.Broadcast();
}

void FMeshSkinWeightsChange::AddBoneWeightEdit(const FSingleBoneWeightEdits& BoneWeightEdit)
{
	AllWeightEdits.MergeEdits(BoneWeightEdit);
}

/*
 * ToolBuilder
 */

UMeshSurfacePointTool* USkinWeightsPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	USkinWeightsPaintTool* Tool = NewObject<USkinWeightsPaintTool>(SceneState.ToolManager);
	Tool->Init(SceneState);
	return Tool;
}

void USkinWeightsPaintTool::Init(const FToolBuilderState& InSceneState)
{
	const UContextObjectStore* ContextObjectStore = InSceneState.ToolManager->GetContextObjectStore();
	EditorContext = ContextObjectStore->FindContext<USkeletalMeshEditorContextObjectBase>();
	PersonaModeManagerContext = ContextObjectStore->FindContext<UPersonaEditorModeManagerContext>();
	TargetManager = InSceneState.TargetManager;
}

void USkinWeightsPaintTool::Setup()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::Setup);
	
	UDynamicMeshBrushTool::Setup();

	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	check(TargetComponent);
	const USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent());
	check(Component && Component->GetSkeletalMeshAsset())

	// prepare mesh for skin editing
	CleanMesh();

	// create a mesh description for editing (this must be done before calling UpdateBonePositionInfos) 
	bool bSupportsLODs = false;
	const EMeshLODIdentifier DefaultLOD = UE::ToolTarget::GetTargetMeshDescriptionLOD(Target, bSupportsLODs);
	ensure(bSupportsLODs);

	EditedMesh = &EditedMeshes.Emplace(DefaultLOD);
	*EditedMesh = *UE::ToolTarget::GetMeshDescription(Target);

	// create a custom set of properties inheriting from the base tool properties
	WeightToolProperties = NewObject<USkinWeightsPaintToolProperties>(this);
	WeightToolProperties->RestoreProperties(this);
	WeightToolProperties->WeightTool = this;
	WeightToolProperties->bSpecifyRadius = true;
	// watch for skin weight layer changes
	WeightToolProperties->ActiveLOD = "LOD0";
	int32 WatcherIndex = WeightToolProperties->WatchProperty(WeightToolProperties->ActiveLOD, [this](FName) { OnActiveLODChanged(); });
	WeightToolProperties->SilentUpdateWatcherAtIndex(WatcherIndex);
	WeightToolProperties->ActiveSkinWeightProfile = FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName;
	WatcherIndex = WeightToolProperties->WatchProperty(WeightToolProperties->ActiveSkinWeightProfile, [this](FName) { OnActiveSkinWeightProfileChanged(); });
	WeightToolProperties->SilentUpdateWatcherAtIndex(WatcherIndex);
	WatcherIndex = WeightToolProperties->WatchProperty(WeightToolProperties->NewSkinWeightProfile, [this](FName) { OnNewSkinWeightProfileChanged(); });
	WeightToolProperties->SilentUpdateWatcherAtIndex(WatcherIndex);
	WeightToolProperties->SourceSkeletalMesh = nullptr;
    WeightToolProperties->SourcePreviewOffset = FTransform::Identity;
		
	// replace the base brush properties
	ReplaceToolPropertySource(BrushProperties, WeightToolProperties);
	BrushProperties = WeightToolProperties;
	// brush render customization
	BrushStampIndicator->bScaleNormalByStrength = true;
	BrushStampIndicator->SecondaryLineThickness = 1.0f;
	BrushStampIndicator->SecondaryLineColor = FLinearColor::Yellow;
	RecalculateBrushRadius();

	// default to the root bone as current bone
	PendingCurrentBone = CurrentBone = Component->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(0);

	// configure preview mesh
	PreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
	PreviewMesh->SetShadowsEnabled(false);

	// selection colors
	constexpr FLinearColor FaceSelectedOrange = FLinearColor(0.886f, 0.672f, 0.473f);
	constexpr FLinearColor VertexSelectedPurple = FLinearColor(0.78f, 0.f, 0.78f);
	constexpr FLinearColor VertexSelectedYellow = FLinearColor(1.f,1.f,0.f);
	// configure secondary render material for selected triangles
	// NOTE: the material returned by ToolSetupUtil::GetSelectionMaterial has a checkerboard pattern on back faces which makes it hard to use
	if (UMaterialInterface* Material = LoadObject<UMaterial>(nullptr, TEXT("/MeshModelingToolsetExp/Materials/SculptMaterial")))
	{
		if (UMaterialInstanceDynamic* MatInstance = UMaterialInstanceDynamic::Create(Material, GetToolManager()))
		{
			MatInstance->SetVectorParameterValue(TEXT("Color"), FaceSelectedOrange);
			PreviewMesh->SetSecondaryRenderMaterial(MatInstance);
		}
	}
	// set up vertex selection mechanic
	PolygonSelectionMechanic = NewObject<UPolygonSelectionMechanic>(this);
	PolygonSelectionMechanic->bAddSelectionFilterPropertiesToParentTool = false;
	PolygonSelectionMechanic->Setup(this);
	PolygonSelectionMechanic->SetIsEnabled(false);
	PolygonSelectionMechanic->OnSelectionChanged.AddLambda([this](){OnSelectionChanged.Broadcast();} );
	// adjust selection rendering for this context
	PolygonSelectionMechanic->HilightRenderer.PointColor = FLinearColor::Blue;
	PolygonSelectionMechanic->HilightRenderer.PointSize = 10.0f;
	// vertex highlighting once selected
	PolygonSelectionMechanic->SelectionRenderer.LineThickness = 1.0f;
	PolygonSelectionMechanic->SelectionRenderer.PointColor = VertexSelectedYellow;
	PolygonSelectionMechanic->SelectionRenderer.PointSize = 5.0f;
	PolygonSelectionMechanic->SelectionRenderer.DepthBias = 2.0f;
	// despite the name, this renders the vertices
	PolygonSelectionMechanic->PolyEdgesRenderer.PointColor = VertexSelectedPurple;
	PolygonSelectionMechanic->PolyEdgesRenderer.PointSize = 5.0f;
	PolygonSelectionMechanic->PolyEdgesRenderer.DepthBias = 2.0f;
	PolygonSelectionMechanic->PolyEdgesRenderer.LineThickness = 1.0f;
	// restore saved mode
	SetComponentSelectionMode(WeightToolProperties->ComponentSelectionMode);
	// secondary triangle buffer used to render face selection
	PreviewMesh->EnableSecondaryTriangleBuffers([this](const FDynamicMesh3* Mesh, int32 TriangleID)
	{
		return PolygonSelectionMechanic->GetActiveSelection().IsSelectedTriangle(Mesh, SelectionTopology.Get(), TriangleID);
	});
	// notify preview mesh when triangle selection has been updated
	PolygonSelectionMechanic->OnSelectionChanged.AddWeakLambda(this, [this]()
	{
		UpdateSelectedVertices();
		PreviewMesh->FastNotifySecondaryTrianglesChanged();
	});
	PolygonSelectionMechanic->OnFaceSelectionPreviewChanged.AddWeakLambda(this, [this]()
	{
		PreviewMesh->FastNotifySecondaryTrianglesChanged();
	});

	// run all initialization for mesh/weights
	PostEditMeshInitialization(Component, *PreviewMesh->GetMesh(), *EditedMesh);

	// bind the skeletal mesh editor context
	if (EditorContext.IsValid())
	{
		EditorContext->BindTo(this);
	}

	// trigger last used mode
	ToggleEditingMode();

	// modify viewport render settings to optimize for painting weights
	FPreviewProfileController PreviewProfileController;
	PreviewProfileToRestore = PreviewProfileController.GetActiveProfile();
	PreviewProfileController.SetActiveProfile(UDefaultEditorProfiles::EditingProfileName.ToString());
	// turn on bone colors
	bBoneColorsToRestore = GetDefault<UPersonaOptions>()->bShowBoneColors;
	GetMutableDefault<UPersonaOptions>()->bShowBoneColors = true;
	// set focus to viewport so brush hotkey works
	SetFocusInViewport();
	
	// inform user of tool keys
	// TODO talk with UX team about viewport overlay to show hotkeys
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartSkinWeightsPaint", "Paint per-bone skin weights. [ and ] change brush size, Ctrl to Erase/Subtract, Shift to Smooth"),
		EToolMessageLevel::UserNotification);
}

void USkinWeightsPaintTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	Super::DrawHUD(Canvas, RenderAPI);

	if (PolygonSelectionMechanic)
	{
		PolygonSelectionMechanic->DrawHUD(Canvas, RenderAPI);
	}
}

void USkinWeightsPaintTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (WeightToolProperties->EditingMode == EWeightEditMode::Brush)
	{
		Super::Render(RenderAPI);	
	}
	else if (PolygonSelectionMechanic && WeightToolProperties->EditingMode == EWeightEditMode::Mesh)
	{
		PolygonSelectionMechanic->Render(RenderAPI);
	}
}

FBox USkinWeightsPaintTool::GetWorldSpaceFocusBox()
{
	if (!WeightToolProperties)
	{
		return PreviewMesh->GetActor()->GetComponentsBoundingBox();
	}
	
	// 1. Prioritize Brush & Vertex modes
	switch (WeightToolProperties->EditingMode)
	{
	case EWeightEditMode::Brush:
			{
				const FVector Radius(CurrentBrushRadius);
				return FBox(LastBrushStamp.WorldPosition - Radius, LastBrushStamp.WorldPosition + Radius);
			}
			break;
	case EWeightEditMode::Mesh:
		{
			FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
			UpdateSelectedVertices();
			if (!SelectedVertices.IsEmpty())
			{
				const FDynamicMesh3* Mesh = PreviewMesh->GetMesh();
				const FTransform3d Transform(PreviewMesh->GetTransform());
				for (const int32 VertexID : SelectedVertices)
				{
					Bounds.Contain(Transform.TransformPosition(Mesh->GetVertex(VertexID)));
				}
			}
			if (Bounds.MaxDim() > FMathf::ZeroTolerance)
			{
				return static_cast<FBox>(Bounds);
			}
		}
		break;
	case EWeightEditMode::Bones:
	default:
		break;
	}

	// 2. Fallback on framing selected bones (if there are any)
	// TODO, there are several places in the engine that frame bone selections. Let's consolidate this logic.
	if (!SelectedBoneIndices.IsEmpty())
	{
		const USkeletalMeshComponent* MeshComponent = Weights.Deformer.Component;
		const FReferenceSkeleton& RefSkeleton = MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
		const TArray<FTransform>& CurrentBoneTransforms = MeshComponent->GetComponentSpaceTransforms();
		if (!CurrentBoneTransforms.IsEmpty())
		{
			FAxisAlignedBox3d Bounds = FAxisAlignedBox3d::Empty();
			for (const int32 BoneIndex : SelectedBoneIndices)
			{
				// add bone position and position of all direct children to the frame bounds
				const FVector BonePosition = CurrentBoneTransforms[BoneIndex].GetLocation();
				Bounds.Contain(BonePosition);
				TArray<int32> ChildrenIndices;
				RefSkeleton.GetDirectChildBones(BoneIndex, ChildrenIndices);
				if (ChildrenIndices.IsEmpty())
				{
					constexpr float SingleBoneSize = 10.f;
					FVector BoneOffset = FVector(SingleBoneSize, SingleBoneSize, SingleBoneSize);
					Bounds.Contain(BonePosition + BoneOffset);
					Bounds.Contain(BonePosition - BoneOffset);
				}
				else
				{
					for (const int32 ChildIndex : ChildrenIndices)
					{
						Bounds.Contain(CurrentBoneTransforms[ChildIndex].GetLocation());
					}
				}	
			}
			if (Bounds.MaxDim() > FMathf::ZeroTolerance)
			{
				return static_cast<FBox>(Bounds);
			}
		}
	}

	// 3. Finally, fallback on component bounds if nothing else is selected
	static constexpr bool bNonColliding = true;
	FBox PreviewBox = PreviewMesh->GetActor()->GetComponentsBoundingBox(bNonColliding);
	
	if (WeightToolProperties->bShowSourcePreview && SourcePreviewMesh)
	{
		if (AActor* SourceActor = SourcePreviewMesh->GetActor())
		{
			PreviewBox += SourceActor->GetComponentsBoundingBox(bNonColliding);
		}
	}

	return PreviewBox;
}

void USkinWeightsPaintTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	// toggle Relax mode on while shift key is held, then swap back to prior mode on release
	if (ModifierID == ShiftModifier)
	{
		if (bIsOn)
		{
			// when shift key is pressed
			if (!bShiftToggle)
			{
				WeightToolProperties->PriorBrushMode = WeightToolProperties->BrushMode;
				WeightToolProperties->SetBrushMode(EWeightEditOperation::Relax);
			}
		}
		else
		{
			// when shift key is released
			if (bShiftToggle)
			{
				WeightToolProperties->SetBrushMode(WeightToolProperties->PriorBrushMode);
			}
		}
	}

	Super::OnUpdateModifierState(ModifierID, bIsOn);
}

FInputRayHit USkinWeightsPaintTool::CanBeginClickDragSequence(const FInputDeviceRay& InPressPos)
{
	// NOTE: this function is only overridden to prevent left-click fly camera behavior while brushing
	// this should eventually be removed once we have a clear way of disabling the fly-cam mode
	
	if (WeightToolProperties->EditingMode != EWeightEditMode::Brush)
	{
		return FInputRayHit(); // allow other behaviors to capture mouse while not brushing
	}
	
	const FInputRayHit Hit = Super::CanBeginClickDragSequence(InPressPos);
	if (Hit.bHit)
	{
		return Hit;
	}

	// always return a hit so we always capture and prevent accidental camera movement
	return FInputRayHit(TNumericLimits<float>::Max());
}

void USkinWeightsPaintTool::OnTick(float DeltaTime)
{
	if (bPendingUpdateFromPartialMesh)
	{
		FinishIsolatedSelection();
		bPendingUpdateFromPartialMesh = false;
	}
	
	if (bStampPending)
	{
		ApplyStamp(LastStamp);
		bStampPending = false;
	}

	if (PendingCurrentBone.IsSet())
	{
		UpdateCurrentBone(*PendingCurrentBone);
		PendingCurrentBone.Reset();
	}

	if (bVertexColorsNeedUpdated)
	{
		UpdateVertexColorForAllVertices();
		bVertexColorsNeedUpdated = false;
	}

	if (!VerticesToUpdateColor.IsEmpty())
	{
		UpdateVertexColorForSubsetOfVertices();
		VerticesToUpdateColor.Empty();
	}

	// sparsely updates vertex positions (only on vertices with modified weights)
	Weights.Deformer.UpdateVertexDeformation(this, Weights.Deformer.Component->GetComponentSpaceTransforms());
}

void USkinWeightsPaintTool::PostEditMeshInitialization(
	const USkeletalMeshComponent* InComponent,
	const FDynamicMesh3& InDynamicMesh,
	const FMeshDescription& InMeshDescription)
{
	// update the preview mesh
	PreviewMesh->ReplaceMesh(InDynamicMesh);
	PreviewMesh->EditMesh([](FDynamicMesh3& Mesh)
	{
		Mesh.EnableAttributes();
		Mesh.Attributes()->DisablePrimaryColors();
		Mesh.Attributes()->EnablePrimaryColors();
		Mesh.Attributes()->PrimaryColors()->CreateFromPredicate([](int ParentVID, int TriIDA, int TriIDB){return true;}, 0.f);
	});
	SetDisplayVertexColors(WeightToolProperties->ColorMode != EWeightColorMode::FullMaterial);
	
	// update vertices & triangle octrees (this must be done after PreviewMesh has been updated)
	InitializeOctrees();

	// update the polygon selection mechanic (this must be done after PreviewMesh has been updated)
	InitializeSelectionMechanic();

	// update weights
	Weights = FSkinToolWeights();
	if (!IsProfileValid(WeightToolProperties->GetActiveSkinWeightProfile()))
	{
		WeightToolProperties->ActiveSkinWeightProfile = FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName;
	}
	Weights.Profile = WeightToolProperties->GetActiveSkinWeightProfile();
	Weights.InitializeSkinWeights(InComponent, &InMeshDescription);
	bVertexColorsNeedUpdated = true;
	
	// update smooth operator (this must be done after PreviewMesh & Weights have been updated)
	InitializeSmoothWeightsOperator();
}

void USkinWeightsPaintTool::CleanMesh() const
{
	if (PreviewMesh->GetMesh()->HasUnusedVertices())
	{
		// orphaned vertices wreak havoc on our selection tools
		PreviewMesh->EditMesh([](FDynamicMesh3& Mesh)
		{
			Mesh.RemoveUnusedVertices();
			Mesh.CompactInPlace();
		});
	
		IDynamicMeshCommitter* DynamicMeshCommitter = Cast<IDynamicMeshCommitter>(Target);
		DynamicMeshCommitter->CommitDynamicMesh(*PreviewMesh->GetMesh());
	}
}

void USkinWeightsPaintToolProperties::SetComponentMode(EComponentSelectionMode InComponentMode)
{
	ComponentSelectionMode = InComponentMode;
	
	WeightTool->SetComponentSelectionMode(ComponentSelectionMode);
	WeightTool->SetFocusInViewport();
}

void USkinWeightsPaintToolProperties::SetFalloffMode(EWeightBrushFalloffMode InFalloffMode)
{
	GetBrushConfig().FalloffMode = InFalloffMode;
	SaveConfig();

	WeightTool->SetFocusInViewport();
}

void USkinWeightsPaintToolProperties::SetColorMode(EWeightColorMode InColorMode)
{
	ColorMode = InColorMode;
	WeightTool->SetDisplayVertexColors(ColorMode!=EWeightColorMode::FullMaterial);
	WeightTool->SetFocusInViewport();
}

void USkinWeightsPaintToolProperties::SetBrushMode(EWeightEditOperation InBrushMode)
{
	BrushMode = InBrushMode;

	// sync base tool settings with the mode specific saved values
	// these are the source of truth for the base class viewport rendering of brush
	BrushRadius = GetBrushConfig().Radius;
	BrushStrength = GetBrushConfig().Strength;
	BrushFalloffAmount = GetBrushConfig().Falloff;

	WeightTool->SetFocusInViewport();
}

bool USkinWeightsPaintTool::HitTest(const FRay& Ray, FHitResult& OutHit)
{
	// do not query the triangle octree until all async ops are finished
	TriangleOctreeFuture.Wait();
	
	// put ray in local space of skeletal mesh component
	// currently no way to transform skeletal meshes in the editor,
	// but at some point in the future we may add the ability to move parts around
	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	const FTransform3d CurTargetTransform(TargetComponent->GetWorldTransform());
	FRay3d LocalRay(
		CurTargetTransform.InverseTransformPosition((FVector3d)Ray.Origin),
		CurTargetTransform.InverseTransformVector((FVector3d)Ray.Direction));
	UE::Geometry::Normalize(LocalRay.Direction);
	
	const FDynamicMesh3* Mesh = PreviewMesh->GetMesh();

	FViewCameraState StateOut;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(StateOut);
	FVector3d LocalEyePosition(CurTargetTransform.InverseTransformPosition((FVector3d)StateOut.Position));
	const int32 TriID = TrianglesOctree->FindNearestHitObject(
		LocalRay,
		[this, Mesh, &LocalEyePosition](int TriangleID)
	{
		FVector3d Normal, Centroid;
		double Area;
		Mesh->GetTriInfo(TriangleID, Normal, Area, Centroid);
		return Normal.Dot((Centroid - LocalEyePosition)) < 0;
	});
	
	if (TriID != IndexConstants::InvalidID)
	{	
		FastTriWinding::FTriangle3d Triangle;
		Mesh->GetTriVertices(TriID, Triangle.V[0], Triangle.V[1], Triangle.V[2]);
		UE::Geometry::FIntrRay3Triangle3d Query(LocalRay, Triangle);
		Query.Find();

		StampLocalPos = LocalRay.PointAt(Query.RayParameter);
		TriangleUnderStamp = TriID;

		OutHit.FaceIndex = TriID;
		OutHit.Distance = Query.RayParameter;
		OutHit.Normal = CurTargetTransform.TransformVector(Mesh->GetTriNormal(TriID));
		OutHit.ImpactPoint = CurTargetTransform.TransformPosition(StampLocalPos);
		return true;
	}
	
	return false;
}

void USkinWeightsPaintTool::OnBeginDrag(const FRay& WorldRay)
{
	UDynamicMeshBrushTool::OnBeginDrag(WorldRay);

	bInvertStroke = GetCtrlToggle();
	BeginChange();
	StartStamp = UBaseBrushTool::LastBrushStamp;
	LastStamp = StartStamp;
	bStampPending = true;
	LongTransactions.Open(LOCTEXT("PaintWeightChange", "Paint skin weights."), GetToolManager());
}

void USkinWeightsPaintTool::OnUpdateDrag(const FRay& WorldRay)
{
	UDynamicMeshBrushTool::OnUpdateDrag(WorldRay);
	
	LastStamp = UBaseBrushTool::LastBrushStamp;
	bStampPending = true;
}

void USkinWeightsPaintTool::OnEndDrag(const FRay& Ray)
{
	UDynamicMeshBrushTool::OnEndDrag(Ray);

	bInvertStroke = false;
	bStampPending = false;

	if (ActiveChange)
	{
		// close change, record transaction
		const FText TransactionLabel = LOCTEXT("PaintWeightChange", "Paint skin weights.");
		EndChange(TransactionLabel);
		LongTransactions.Close(GetToolManager());
	}
}

bool USkinWeightsPaintTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	UDynamicMeshBrushTool::OnUpdateHover(DevicePos);
	return true;
}

double USkinWeightsPaintTool::EstimateMaximumTargetDimension()
{
	if (const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target))
	{
		const USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent());
		if (USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset())
		{
			return SkeletalMesh->GetBounds().SphereRadius * 2.0f;
		}
	}
	
	return Super::EstimateMaximumTargetDimension();
}

void USkinWeightsPaintTool::CalculateVertexROI(
	const FBrushStampData& Stamp,
	TArray<VertexIndex>& VertexIDs,
	TArray<float>& VertexFalloffs)
{
	using namespace UE::Geometry;

	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::CalculateVertexROI);

	auto DistanceToFalloff = [this](int32 InVertexID, float InDistanceSq)-> float
	{
		const float CurrentFalloff = CalculateBrushFalloff(FMath::Sqrt(InDistanceSq));
		const float UseFalloff = Weights.SetCurrentFalloffAndGetMaxFalloffThisStroke(InVertexID, CurrentFalloff);
		return UseFalloff;
	};
	
	if (WeightToolProperties->GetBrushConfig().FalloffMode == EWeightBrushFalloffMode::Volume)
	{
		const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
		const FTransform3d Transform(TargetComponent->GetWorldTransform());
		const FVector3d StampPosLocal = Transform.InverseTransformPosition((FVector3d)Stamp.WorldPosition);
		const float RadiusSqr = CurrentBrushRadius * CurrentBrushRadius;
		const FDynamicMesh3* Mesh = PreviewMesh->GetPreviewDynamicMesh();
		const FAxisAlignedBox3d QueryBox(StampPosLocal, CurrentBrushRadius);
		VerticesOctree->RangeQuery(QueryBox,
			[&](int32 VertexID) { return FVector3d::DistSquared(Mesh->GetVertex(VertexID), StampPosLocal) < RadiusSqr; },
			VertexIDs);

		for (const int32 VertexID : VertexIDs)
		{
			const float DistSq = FVector3d::DistSquared(Mesh->GetVertex(VertexID), StampPosLocal);
			VertexFalloffs.Add(DistanceToFalloff(VertexID, DistSq));
		}
		
		return;
	}

	if (WeightToolProperties->GetBrushConfig().FalloffMode == EWeightBrushFalloffMode::Surface)
	{
		// get coordinate frame from stamp
		auto GetFrameFromStamp = [](const FBrushStampData& InStamp) -> FFrame3d
		{
			const FVector3d Origin = InStamp.WorldPosition;
			const FVector3d Normal = InStamp.WorldNormal;
			FVector3d NonCollinear = Normal;
			// get a guaranteed non collinear vector to the normal
			// doesn't matter where in the plane, stamp is radially symmetric
			do 
			{
				NonCollinear.X = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Y = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Z = FMath::RandRange(-1.0f, 1.0f);
				NonCollinear.Normalize();
					
			} while (FMath::Abs(NonCollinear.Dot(Normal)) > 0.8f);

			const FVector3d Plane = Normal.Cross(NonCollinear);
			const FVector3d Cross = Plane.Cross(Normal);
			return FFrame3d(Origin, Cross, Plane, Normal);
				
		};
		const FFrame3d SeedFrame = GetFrameFromStamp(Stamp);
			
		// create the ExpMap generator, computes vertex polar coordinates in a plane tangent to the surface
		const FDynamicMesh3* Mesh = PreviewMesh->GetPreviewDynamicMesh();
		TMeshLocalParam<FDynamicMesh3> Param(Mesh);
		Param.ParamMode = ELocalParamTypes::ExponentialMapUpwindAvg;
		const FIndex3i TriVerts = Mesh->GetTriangle(TriangleUnderStamp);
		Param.ComputeToMaxDistance(SeedFrame, TriVerts, Stamp.Radius);
		{
			// store vertices under the brush and their distances from the stamp
			const float StampRadSq = FMath::Pow(Stamp.Radius, 2);
			for (int32 VertexID : Mesh->VertexIndicesItr())
			{
				if (!Param.HasUV(VertexID))
				{
					continue;
				}
				
				FVector2d UV = Param.GetUV(VertexID);
				const float DistSq = UV.SizeSquared();
				if (DistSq >= StampRadSq)
				{
					continue;
				}

				
				VertexFalloffs.Add(DistanceToFalloff(VertexID, DistSq));
				VertexIDs.Add(VertexID);
			}
		}
		
		return;
	}
	
	checkNoEntry();
}

FVector4f USkinWeightsPaintTool::GetColorOfVertex(VertexIndex InVertexIndex, BoneIndex InCurrentBoneIndex) const
{
	switch (WeightToolProperties->ColorMode)
	{
	case EWeightColorMode::Greyscale:
		{
			if (InCurrentBoneIndex == INDEX_NONE)
			{
				return FLinearColor::Black; // with no bone selected, all vertices are drawn black
			}
			const float Value = Weights.GetWeightOfBoneOnVertex(InCurrentBoneIndex, InVertexIndex, Weights.CurrentWeights);
			return FMath::Lerp(FLinearColor::Black, FLinearColor::White, Value);
		}
	case EWeightColorMode::Ramp:
		{
			if (InCurrentBoneIndex == INDEX_NONE)
			{
				return FLinearColor::Black; // with no bone selected, all vertices are drawn black
			}

			// get user-specified colors
			const TArray<FLinearColor>& Colors = WeightToolProperties->ColorRamp;
			// get weight value
			float Value = Weights.GetWeightOfBoneOnVertex(InCurrentBoneIndex, InVertexIndex, Weights.CurrentWeights);
			Value = FMath::Clamp(Value, 0.0f, 1.0f);

			// ZERO user supplied colors, then revert to greyscale
			if (Colors.IsEmpty())
			{
				return FMath::Lerp(FLinearColor::Black, FLinearColor::White, Value);
			}

			// ONE user defined color, blend it with black
			if (Colors.Num() == 1)
			{
				return FMath::Lerp(FLinearColor::Black, Colors[0], Value);
			}

			// TWO user defined color, simple LERP
			if (Colors.Num() == 2)
			{
				return FMath::Lerp(Colors[0], Colors[1], Value);
			}
			
			// blend colors between min and max value
			constexpr float MinValue = 0.1f;
			constexpr float MaxValue = 0.9f;
			
			// early out zero weights to min color
			if (Value <= MinValue)
			{
				return Colors[0];
			}

			// early out full weights to max color
			if (Value >= MaxValue)
			{
				return Colors.Last();
			}
			
			// remap from 0-1 to range of MinValue to MaxValue
			const float ScaledValue = (Value - MinValue) * 1.0f / (MaxValue - MinValue);
			// interpolate within two nearest ramp colors
			const float PerColorRange = 1.0f / (Colors.Num() - 1);
			const int ColorIndex = static_cast<int>(ScaledValue / PerColorRange);
			const float RangeStart = ColorIndex * PerColorRange;
			const float RangeEnd = (ColorIndex + 1) * PerColorRange;
			const float Param = (ScaledValue - RangeStart) / (RangeEnd - RangeStart);
			const FLinearColor& StartColor = Colors[ColorIndex];
			const FLinearColor& EndColor = Colors[ColorIndex+1];
			return UE::Geometry::ToVector4<float>(FMath::Lerp(StartColor, EndColor, Param));
		}
	case EWeightColorMode::BoneColors:
		{
			FVector4f Color = FVector4f::Zero();
			const VertexWeights& VertexWeights = Weights.CurrentWeights[InVertexIndex];
			for (const FVertexBoneWeight& BoneWeight : VertexWeights)
			{
				if (BoneWeight.Weight < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				
				const float Value = InCurrentBoneIndex == BoneWeight.BoneIndex ? 1.0f: 0.6f;
				constexpr float Saturation = 0.75f;
				const FLinearColor BoneColor = SkeletalDebugRendering::GetSemiRandomColorForBone(BoneWeight.BoneIndex, Value, Saturation);
				Color = FLinearColor::LerpUsingHSV(Color, BoneColor, BoneWeight.Weight);
			}
			return Color;
		}
	case EWeightColorMode::FullMaterial:
		return FLinearColor::White;
	default:
		checkNoEntry();
		return FLinearColor::Black;
	}	
}


void USkinWeightsPaintTool::UpdateVertexColorForAllVertices()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateVertexColors);
	
	const int32 CurrentBoneIndex = GetBoneIndexFromName(CurrentBone);
	
	// update mesh with new value colors
	PreviewMesh->DeferredEditMesh([this, &CurrentBoneIndex](FDynamicMesh3& Mesh)
	{
		const UE::Geometry::FNonManifoldMappingSupport NonManifoldMappingSupport(Mesh);
		UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
		for (const int32 ElementId : ColorOverlay->ElementIndicesItr())
		{
			const int32 VertexID = ColorOverlay->GetParentVertex(ElementId);	
			const int32 SrcVertexID = NonManifoldMappingSupport.GetOriginalNonManifoldVertexID(VertexID);
			ColorOverlay->SetElement(ElementId, GetColorOfVertex(SrcVertexID, CurrentBoneIndex));
		}
		
	}, false);
	PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::VertexColors, false);
}

void USkinWeightsPaintTool::UpdateVertexColorForSubsetOfVertices()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::UpdateVertexColors);
	
	PreviewMesh->DeferredEditMesh([this](FDynamicMesh3& Mesh)
		{
			if (CurrentBone == NAME_None)
			{
				
			}
			TArray<int> ElementIds;
			UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
			const int32 CurrentBoneIndex = GetBoneIndexFromName(CurrentBone);
			for (const int32 VertexID : VerticesToUpdateColor)
			{
				FVector4f NewColor(GetColorOfVertex(VertexID, CurrentBoneIndex));
				ColorOverlay->GetVertexElements(VertexID, ElementIds);
				for (const int32 ElementId : ElementIds)
				{
					ColorOverlay->SetElement(ElementId, NewColor);
				}
				ElementIds.Reset();
			}
			
		}, false);
	PreviewMesh->NotifyDeferredEditCompleted(UPreviewMesh::ERenderUpdateMode::FastUpdate, EMeshRenderAttributeFlags::VertexColors, false);
}

float USkinWeightsPaintTool::CalculateBrushFalloff(float Distance) const
{
	const float f = FMathd::Clamp(1.f - BrushProperties->BrushFalloffAmount, 0.f, 1.f);
	float d = Distance / CurrentBrushRadius;
	double w = 1;
	if (d > f)
	{
		d = FMathd::Clamp((d - f) / (1.0 - f), 0.0, 1.0);
		w = (1.0 - d * d);
		w = w * w * w;
	}
	return w;
}

void USkinWeightsPaintTool::ApplyStamp(const FBrushStampData& Stamp)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyStamp);

	// must select a bone to paint
	if (CurrentBone == NAME_None)
	{
		return;
	}
	
	// get the vertices under the brush, and their squared distances to the brush center
	// when using "Volume" brush, distances are straight line
	// when using "Surface" brush, distances are geodesics
	TArray<int32> VerticesInStamp;
	TArray<float> VertexFalloffs;
	CalculateVertexROI(Stamp, VerticesInStamp, VertexFalloffs);

	// gather sparse set of modifications made from this stamp, these edits are merged throughout
	// the lifetime of a single brush stroke in the "ActiveChange" allowing for undo/redo
	FMultiBoneWeightEdits WeightEditsFromStamp;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::EditWeightOfVerticesInStamp);
		// generate a weight edit from this stamp (includes modifications caused by normalization)
		if (WeightToolProperties->BrushMode == EWeightEditOperation::Relax)
		{
			// use mesh topology to iteratively smooth weights across neighboring vertices
			const float UseStrength = CalculateBrushStrengthToUse(EWeightEditOperation::Relax);
			constexpr int32 RelaxIterationsPerStamp = 3;
			RelaxWeightOnVertices(VerticesInStamp, VertexFalloffs, RelaxIterationsPerStamp, UseStrength, WeightEditsFromStamp);
		}
		else
		{
			// edit weight; either by "Add", "Remove", "Replace", "Multiply"
			const float UseStrength = CalculateBrushStrengthToUse(WeightToolProperties->BrushMode);
			const int32 CurrentBoneIndex = GetCurrentBoneIndex();
			EditWeightOfBoneOnVertices(
				WeightToolProperties->BrushMode,
				CurrentBoneIndex,
				VerticesInStamp,
				VertexFalloffs,
				UseStrength,
				WeightEditsFromStamp);
		}
	}

	// apply weight edits to the mesh without closing the transaction
	ApplyWeightEditsToMeshMidChange(WeightEditsFromStamp);
}

float USkinWeightsPaintTool::CalculateBrushStrengthToUse(EWeightEditOperation EditMode) const
{
	float UseStrength = BrushProperties->BrushStrength;

	// invert brush strength differently depending on brush mode
	switch (EditMode)
	{
	case EWeightEditOperation::Add:
		{
			UseStrength *= bInvertStroke ? -1.0f : 1.0f;
			break;
		}
	case EWeightEditOperation::Replace:
		{
			UseStrength = bInvertStroke ? 1.0f - UseStrength : UseStrength;
			break;
		}
	case EWeightEditOperation::Multiply:
		{
			UseStrength = bInvertStroke ? 1.0f + UseStrength : UseStrength;
			break;
		}
	case EWeightEditOperation::Relax:
		{
			UseStrength = bInvertStroke ? 1.0f - UseStrength : UseStrength;
			break;
		}
	default:
		checkNoEntry();
	}

	return UseStrength;
}

void USkinWeightsPaintTool::EditWeightOfBoneOnVertices(
	EWeightEditOperation EditOperation,
	const BoneIndex Bone,
	const TArray<int32>& VertexIndices,
	const TArray<float>& VertexFalloffs,
	const float UseStrength,
	FMultiBoneWeightEdits& InOutWeightEdits)
{
	// spin through the vertices in the stamp and store new weight values in NewValuesFromStamp
	// afterwards, these values are normalized while taking into consideration the user's desired changes
	const int32 NumVerticesInStamp = VertexIndices.Num();
	for (int32 Index = 0; Index < NumVerticesInStamp; ++Index)
	{
		const int32 VertexID = VertexIndices[Index];
		const float UseFalloff = VertexFalloffs.IsValidIndex(Index) ? VertexFalloffs[Index] : 1.f;
		const float ValueBeforeStroke = Weights.GetWeightOfBoneOnVertex(Bone, VertexID, Weights.PreChangeWeights);

		// calculate new weight value
		float NewValueAfterStamp = ValueBeforeStroke;
		switch (EditOperation)
		{
		case EWeightEditOperation::Add:
			{
				NewValueAfterStamp = ValueBeforeStroke + (UseStrength * UseFalloff);
				break;
			}
		case EWeightEditOperation::Replace:
			{
				NewValueAfterStamp = FMath::Lerp(ValueBeforeStroke, UseStrength, UseFalloff);
				break;
			}
		case EWeightEditOperation::Multiply:
			{
				const float DeltaFromThisStamp = ((ValueBeforeStroke * UseStrength) - ValueBeforeStroke) * UseFalloff;
				NewValueAfterStamp = ValueBeforeStroke + DeltaFromThisStamp;
				break;
			}
		default:
			// relax operation not supported by this function, use RelaxWeightOnVertices()
			checkNoEntry();
		}

		// normalize the values across all bones affecting this vertex, and record the bone edits
		// normalization is done while holding all weights on the current bone constant so that user edits are not overwritten
		Weights.EditVertexWeightAndNormalize(
			Bone,
			VertexID,
			NewValueAfterStamp,
			InOutWeightEdits);
	}
}

void USkinWeightsPaintTool::RelaxWeightOnVertices(
	TArray<int32> VertexIndices,
	TArray<float> VertexFalloffs,
	int32 Iterations,
	const float UseStrength,
	FMultiBoneWeightEdits& InOutWeightEdits)
{
	if (!ensure(SmoothWeightsOp))
	{
		return;
	}
	
	constexpr float PercentPerIteration = 0.95f;
	for (int32 Iteration=0; Iteration<Iterations; ++Iteration)
	{
		for (int32 VertexIndex = 0; VertexIndex < VertexIndices.Num(); ++VertexIndex)
		{
			const int32 VertexID = VertexIndices[VertexIndex];
			const float UseFalloff = (VertexFalloffs.IsValidIndex(VertexIndex) ? VertexFalloffs[VertexIndex] * UseStrength : UseStrength) * PercentPerIteration;

			TMap<int32, float> FinalWeights;
			const bool bSmoothSuccess = SmoothWeightsOp->SmoothWeightsAtVertex(VertexID, UseFalloff, FinalWeights);

			if (ensure(bSmoothSuccess))
			{
				// apply weight edits
				for (const TTuple<BoneIndex, float>& FinalWeight : FinalWeights)
				{
					// record an edit for this vertex, for this bone
					const int32 BoneIndex = FinalWeight.Key;
					const float NewWeight = FinalWeight.Value;
					const float OldWeight = Weights.GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights.PreChangeWeights);
					InOutWeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
				}
			}
		}
	}
}

void USkinWeightsPaintTool::InitializeOctrees()
{
	if (!ensure(PreviewMesh && PreviewMesh->GetMesh()))
	{
		return;
	}

	// build octree for vertices
	VerticesOctree = MakeUnique<DynamicVerticesOctree>();
	VerticesOctree->Initialize(PreviewMesh->GetMesh(), true);

	// build octree for triangles
	TrianglesOctree = MakeUnique<DynamicTrianglesOctree>();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::InitTriangleOctree);
		
		TriangleOctreeFuture = Async(SkinPaintToolAsyncExecTarget, [&]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::InitTriangleOctreeRun);
			TrianglesOctree->Initialize(PreviewMesh->GetMesh());
		});
	}
}

void USkinWeightsPaintTool::InitializeSelectionMechanic()
{
	const USkeletalMeshComponent* Component = GetSkeletalMeshComponent(Target);
	if (!ensure(Component))
	{
		return;
	}
	
	if (!ensure(PreviewMesh && PreviewMesh->GetMesh()))
	{
		return;
	}

	static constexpr bool bAutoBuild = true;
	const FDynamicMesh3* DynamicMesh = PreviewMesh->GetMesh();
	SelectionTopology = MakeUnique<UE::Geometry::FTriangleGroupTopology>(DynamicMesh, bAutoBuild);
	MeshSpatial = MakeUnique<FDynamicMeshAABBTree3>(DynamicMesh, bAutoBuild);
	PolygonSelectionMechanic->Initialize(
		DynamicMesh,
		FTransform::Identity,
		Component->GetWorld(),
		SelectionTopology.Get(),
		[this]() { return MeshSpatial.Get(); }
	);
	PolygonSelectionMechanic->ClearSelection();
}

void USkinWeightsPaintTool::InitializeSmoothWeightsOperator()
{
	if (!ensure(PreviewMesh && PreviewMesh->GetMesh()))
	{
		return;
	}

	// NOTE: this could probably be initialized lazily as it's only used with the relax brush 
	SmoothWeightsDataSource = MakeUnique<FPaintToolWeightsDataSource>(&Weights);
	SmoothWeightsOp = MakeUnique<UE::Geometry::TSmoothBoneWeights<int32, float>>(PreviewMesh->GetMesh(), SmoothWeightsDataSource.Get());
	SmoothWeightsOp->MinimumWeightThreshold = MinimumWeightThreshold;
}

void USkinWeightsPaintTool::ApplyWeightEditsToMeshMidChange(const SkinPaintTool::FMultiBoneWeightEdits& WeightEdits)
{
	// store weight edits from all stamps made during a single stroke (1 transaction per stroke)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyWeightEditsToActiveChange);
		for (const TTuple<BoneIndex, SkinPaintTool::FSingleBoneWeightEdits>& BoneWeightEdits : WeightEdits.PerBoneWeightEdits)
		{
			ActiveChange->AddBoneWeightEdit(BoneWeightEdits.Value);
		}
	}

	// apply weights to current weights (triggers sparse deformation update)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinTool::ApplyWeightEditsToCurrentWeights);
		Weights.ApplyEditsToCurrentWeights(WeightEdits);
	}
	
	// queue update of vertex colors
	WeightEdits.GetEditedVertexIndices(VerticesToUpdateColor);
}

void USkinWeightsPaintTool::ApplyWeightEditsAsTransaction(const SkinPaintTool::FMultiBoneWeightEdits& WeightEdits, const FText& TransactionLabel)
{
	// clear the active change to start a new one
	BeginChange();

	// store weight edits in the active change
	for (const TTuple<BoneIndex, FSingleBoneWeightEdits>& BoneWeightEdits : WeightEdits.PerBoneWeightEdits)
	{
		ActiveChange->AddBoneWeightEdit(BoneWeightEdits.Value);
	}
	
	// apply the weight edits of the active change to the actual mesh
	// - copies weight modifications to the tool's weight data structure and updates the vertex colors
	// - updates PreChangeWeights
	ActiveChange->Apply(this);
	
	// store active change in the transaction buffer
	EndChange(TransactionLabel);
}

void USkinWeightsPaintTool::UpdateCurrentBone(const FName& BoneName)
{
	CurrentBone = BoneName;
	bVertexColorsNeedUpdated = true;
	OnSelectionChanged.Broadcast();
}

BoneIndex USkinWeightsPaintTool::GetBoneIndexFromName(const FName BoneName) const
{
	if (BoneName == NAME_None)
	{
		return  INDEX_NONE;		
	}
	const BoneIndex* Found = Weights.Deformer.BoneNameToIndexMap.Find(BoneName);
	return Found ? *Found : INDEX_NONE;
}

void USkinWeightsPaintTool::SetFocusInViewport() const
{
	if (PersonaModeManagerContext.IsValid())
	{
		PersonaModeManagerContext->SetFocusInViewport();	
	}
}

void USkinWeightsPaintTool::OnShutdown(EToolShutdownType ShutdownType)
{
	// save tool properties
	WeightToolProperties->SaveProperties(this);

	// shutdown polygon selection mechanic
	if (PolygonSelectionMechanic)
	{
		PolygonSelectionMechanic->Shutdown();
		PolygonSelectionMechanic = nullptr;
	}

	// apply changes to asset
	if (ShutdownType == EToolShutdownType::Accept)
	{
		// apply the weights to the mesh description
		Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh);

		// this block bakes the modified DynamicMeshComponent back into the StaticMeshComponent inside an undo transaction
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SkinWeightsPaintTool", "Paint Skin Weights"));
		static constexpr bool bHaveTargetLOD = true;
		for (auto& [LOD, MeshDescription] : EditedMeshes)
		{
			const FCommitMeshParameters CommitParams(bHaveTargetLOD, LOD);
			UE::ToolTarget::CommitMeshDescriptionUpdate(Target, &MeshDescription, nullptr, CommitParams);	
		}
		GetToolManager()->EndUndoTransaction();
	}

	// restore viewport show flags and preview settings
	FPreviewProfileController PreviewProfileController;
	PreviewProfileController.SetActiveProfile(PreviewProfileToRestore);
	GetMutableDefault<UPersonaOptions>()->bShowBoneColors = bBoneColorsToRestore;
	// mesh selection mode takes control of "Show Edges" render flag
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ShowFlag.MeshEdges")))
	{
		CVar->Unset(ECVF_SetByCode);
	}

	if (EditorContext.IsValid())
	{
		EditorContext->UnbindFrom(this);
	}

	if (PersonaModeManagerContext.IsValid())
	{
		PersonaModeManagerContext->GetPersonaEditorModeManager()->DeactivateMode(FPersonaEditModes::SkeletonSelection);
	}

	ResetSourceForTransfer();
}

void USkinWeightsPaintTool::BeginChange()
{
	const EMeshLODIdentifier LOD = GetLODId(WeightToolProperties->ActiveLOD);
	const FName SkinProfile = WeightToolProperties->GetActiveSkinWeightProfile();
	ActiveChange = MakeUnique<FMeshSkinWeightsChange>(LOD, SkinProfile);
}

void USkinWeightsPaintTool::EndChange(const FText& TransactionLabel)
{
	// swap weight buffers
	Weights.SwapAfterChange();
	
	// record transaction
	GetToolManager()->BeginUndoTransaction(TransactionLabel);
	GetToolManager()->EmitObjectChange(this, MoveTemp(ActiveChange), TransactionLabel);
	GetToolManager()->EndUndoTransaction();

	// notify dependent systems
	OnWeightsChanged.Broadcast();
}

void USkinWeightsPaintTool::ExternalUpdateWeights(const int32 BoneIndex, const TMap<int32, float>& NewValues)
{
	for (const TTuple<int32, float>& Pair : NewValues)
	{
		const int32 VertexID = Pair.Key;
		const float Weight = Pair.Value;
		Weights.SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, Weights.CurrentWeights);
		Weights.SetWeightOfBoneOnVertex(BoneIndex, VertexID, Weight, Weights.PreChangeWeights);

		// queue update of vertex colors
		VerticesToUpdateColor.Add(VertexID);
	}

	Weights.UpdateIsBoneWeighted(BoneIndex);
}

void USkinWeightsPaintTool::ExternalUpdateSkinWeightLayer(const EMeshLODIdentifier InLOD, const FName InSkinWeightProfile)
{
	enum class ESkinWeightChangeState
	{
		SkinProfile,
		LOD,
		None
	} State = ESkinWeightChangeState::None;
	
	if (InSkinWeightProfile != WeightToolProperties->GetActiveSkinWeightProfile())
	{
		WeightToolProperties->ActiveSkinWeightProfile = InSkinWeightProfile;
		State = ESkinWeightChangeState::SkinProfile;
	}

	const FName LODName = GetLODName(InLOD);
	if (LODName != NAME_None && LODName != WeightToolProperties->ActiveLOD)
	{
		WeightToolProperties->ActiveLOD = LODName;
		State = ESkinWeightChangeState::LOD;
	}

	switch (State)
	{
	case ESkinWeightChangeState::SkinProfile:
		return OnActiveSkinWeightProfileChanged();
	case ESkinWeightChangeState::LOD:
		return OnActiveLODChanged();
	case ESkinWeightChangeState::None:
		default:
		break;
	}
}

void FSkinMirrorData::RegenerateMirrorData(
    const TArray<FName>& BoneNames,
    const TMap<FName, BoneIndex>& BoneNameToIndexMap,
	const FReferenceSkeleton& RefSkeleton,
	const TArray<FVector>& RefPoseVertices,
	EAxis::Type InMirrorAxis,
	EMirrorDirection InMirrorDirection)
{
	if (bIsInitialized && InMirrorAxis == Axis && InMirrorDirection==Direction)
	{
		// already initialized, just re-use cached data
		return;
	}

	// need to re-initialize
	bIsInitialized = false;
	Axis = InMirrorAxis;
	Direction = InMirrorDirection;
	
	// build bone map for mirroring
	// TODO, provide some way to edit the mirror bone mapping, either by providing a UMirrorDataTable input or editing directly in the hierarchy view.
	for (FName BoneName : BoneNames)
	{
		FName MirroredBoneName = UMirrorDataTable::FindBestMirroredBone(BoneName, RefSkeleton, Axis);

		int32 BoneIndex = BoneNameToIndexMap[BoneName];
		int32 MirroredBoneIndex = BoneNameToIndexMap[MirroredBoneName];
		BoneMap.Add(BoneIndex, MirroredBoneIndex);
		
		// debug view bone mapping
		//UE_LOG(LogTemp, Log, TEXT("Bone    : %s"), *BoneName.ToString());
		//UE_LOG(LogTemp, Log, TEXT("Mirrored: %s"), *MirroredBoneName.ToString());
		//UE_LOG(LogTemp, Log, TEXT("-------"));
	}

	// hash grid constants
	constexpr float HashGridCellSize = 2.0f;
	constexpr float ThresholdRadius = 0.1f;

	// build a point set of the rest pose vertices
	UE::Geometry::FDynamicPointSet3d PointSet;
	for (int32 PointID = 0; PointID < RefPoseVertices.Num(); ++PointID)
	{
		PointSet.InsertVertex(PointID, RefPoseVertices[PointID]);
	}

	// build a spatial hash map from the point set
	UE::Geometry::FPointSetAdapterd PointSetAdapter = UE::Geometry::MakePointsAdapter(&PointSet);
	UE::Geometry::FPointSetHashtable PointHash(&PointSetAdapter);
	PointHash.Build(HashGridCellSize, FVector3d::Zero());
	
	// generate a map of point IDs on the target side, to their equivalent vertex ID on the source side 
	TArray<int> PointsInThreshold;
	TArray<int> PointsInSphere;
	bAllVerticesMirrored = true;
	for (int32 TargetVertexID = 0; TargetVertexID < RefPoseVertices.Num(); ++TargetVertexID)
	{
		const FVector& TargetPosition = RefPoseVertices[TargetVertexID];

		if (Direction == EMirrorDirection::PositiveToNegative && TargetPosition[Axis-1] >= 0.f)
		{
			continue; // copying to negative side, but vertex is on positive side
		}
		if (Direction == EMirrorDirection::NegativeToPositive && TargetPosition[Axis-1] <= 0.f)
		{
			continue; // copying to positive side, but vertex is on negative side
		}

		// flip position across the mirror axis
		FVector MirroredPosition = TargetPosition;
		MirroredPosition[Axis-1] *= -1.f;

		// query spatial hash near mirrored position, gradually increasing search radius until at least 1 point is found
		PointsInSphere.Reset();
		float SearchRadius = ThresholdRadius;
		while(PointsInSphere.IsEmpty())
		{
			PointHash.FindPointsInBall(MirroredPosition, SearchRadius, PointsInSphere);
			SearchRadius += ThresholdRadius;

			// forcibly break out if search radius gets bigger than the mesh bounds.
			// this could potentially happen if mesh is highly non-symmetrical along mirror axis
			if (SearchRadius >= HashGridCellSize)
			{
				break;
			}
		}

		// no mirrored points?
		if (PointsInSphere.IsEmpty())
		{
			bAllVerticesMirrored = false;
			continue;
		}

		// find the closest single point
		float ClosestDistSq = TNumericLimits<float>::Max();
		int32 ClosestVertexID = INDEX_NONE;
		for (const int32 PointInSphereID : PointsInSphere)
		{
			const float DistSq = FVector::DistSquared(RefPoseVertices[PointInSphereID], MirroredPosition);
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestVertexID = PointInSphereID;
			}
		}
		
		// record the mirrored vertex ID for this vertex 
		VertexMap.FindOrAdd(TargetVertexID, ClosestVertexID); // (TO, FROM)
	}
	
	bIsInitialized = true;
}


void USkinWeightsPaintTool::MirrorWeights(EAxis::Type Axis, EMirrorDirection Direction)
{
	check(Axis != EAxis::None);
	
	// get all ref pose vertices
	const TArray<FVector>& RefPoseVertices = Weights.Deformer.RefPoseVertexPositions;
	const FReferenceSkeleton& RefSkeleton = Weights.Deformer.Component->GetSkeletalMeshAsset()->GetRefSkeleton();

	// refresh mirror tables (cached / lazy generated)
	MirrorData.RegenerateMirrorData(
		Weights.Deformer.BoneNames,
		Weights.Deformer.BoneNameToIndexMap,
		RefSkeleton,
		RefPoseVertices,
		Axis,
		Direction);

	// get a reference to the mirror tables
	const TMap<int32, int32>& BoneMap = MirrorData.GetBoneMap();
	const TMap<int32, int32>& VertexMirrorMap = MirrorData.GetVertexMap(); // <Target, Source>

	// get set of vertices to mirror
	TArray<VertexIndex> AllVerticesToEdit = GetVerticesToEdit();

	// convert all vertex indices to the target side of the mirror plane
	TSet<VertexIndex> VerticesToMirror;
	
	for (const VertexIndex SelectedVertex : AllVerticesToEdit)
	{
		if (VertexMirrorMap.Contains(SelectedVertex))
		{
			// vertex is located across the mirror plane (target side, to copy TO)
			VerticesToMirror.Add(SelectedVertex);
		}
		else
		{
			// vertex is located on the source side (to copy FROM), so we need to search for it's mirror target vertex
			for (const TPair<int32, int32>& ToFromPair : VertexMirrorMap)
			{
				if (ToFromPair.Value != SelectedVertex)
				{
					continue;
				}
				VerticesToMirror.Add(ToFromPair.Key);
				break;
			}
		}
	}
	
	// spin through all target vertices to mirror and copy weights from source
	FMultiBoneWeightEdits WeightEditsFromMirroring;
	for (const VertexIndex VertexToMirror : VerticesToMirror)
	{
		const int32 SourceVertexID = VertexMirrorMap[VertexToMirror];
		const int32 TargetVertexID = VertexToMirror;

		// remove all weight on vertex
		for (const FVertexBoneWeight& TargetBoneWeight : Weights.PreChangeWeights[TargetVertexID])
		{
			const float OldWeight = TargetBoneWeight.Weight;
			constexpr float NewWeight = 0.f;
			WeightEditsFromMirroring.MergeSingleEdit(TargetBoneWeight.BoneIndex, TargetVertexID, OldWeight, NewWeight);
		}

		// copy source weights, but with mirrored bones
		for (const FVertexBoneWeight& SourceBoneWeight : Weights.PreChangeWeights[SourceVertexID])
		{
			const int32 MirroredBoneIndex = BoneMap[SourceBoneWeight.BoneIndex];
			const float OldWeight = Weights.GetWeightOfBoneOnVertex(MirroredBoneIndex, TargetVertexID, Weights.PreChangeWeights);
			const float NewWeight = SourceBoneWeight.Weight;
			WeightEditsFromMirroring.MergeSingleEdit(MirroredBoneIndex, TargetVertexID, OldWeight, NewWeight);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("MirrorWeightChange", "Mirror skin weights.");
	ApplyWeightEditsAsTransaction(WeightEditsFromMirroring, TransactionLabel);

	// warn if some vertices were not mirrored
	if (!MirrorData.GetAllVerticesMirrored())
	{
		UE_LOG(LogMeshModelingToolsEditor, Log, TEXT("Mirror Skin Weights: some vertex weights were not mirrored because a vertex was not found close enough to the mirrored location."));
	}
}

void USkinWeightsPaintTool::EditWeightsOnVertices(
	BoneIndex Bone,
	const float Value,
	EWeightEditOperation EditOperation,
	const TArray<VertexIndex>& VertexIndices,
	const bool bShouldTransact)
{
	// create weight edits from setting the weight directly
	FMultiBoneWeightEdits DirectWeightEdits;
	const TArray<float> VertexFalloffs = {}; // no falloff

	if (EditOperation == EWeightEditOperation::Relax)
	{
		const int32 Iterations = static_cast<int32>(Value);
		constexpr float Strength = 1.0f; // always use max strength, and interpret value as iterations for relaxation
		RelaxWeightOnVertices(GetVerticesToEdit(), VertexFalloffs, Iterations, Strength, DirectWeightEdits);
	}
	else
	{
		EditWeightOfBoneOnVertices(
				EditOperation,
				Bone,
				VertexIndices,
				VertexFalloffs,
				Value,
				DirectWeightEdits);
	}
	
	// apply the changes
	if (bShouldTransact)
	{
		const FText TransactionLabel = LOCTEXT("EditWeightChange", "Edit skin weights directly.");
		ApplyWeightEditsAsTransaction(DirectWeightEdits, TransactionLabel);
	}
	else
	{
		ApplyWeightEditsToMeshMidChange(DirectWeightEdits);
	}
}

void USkinWeightsPaintTool::PruneWeights(float Threshold)
{
	// set weights below the given threshold to zero
	FMultiBoneWeightEdits WeightEditsFromPrune;
	const TArray<VertexIndex>& VerticesToPrune = GetVerticesToEdit();
	const bool bPruningAllBones = SelectedBoneIndices.IsEmpty();
	for (const VertexIndex VertexID : VerticesToPrune)
	{
		const VertexWeights& VertexWeights = Weights.CurrentWeights[VertexID];
		for (const FVertexBoneWeight& BoneWeight : VertexWeights)
		{
			// are we pruning weights only on selected bones?
			if (!bPruningAllBones && !SelectedBoneIndices.Contains(BoneWeight.BoneIndex))
			{
				// not the bone we're pruning...
				continue;
			}
			
			if (BoneWeight.Weight > Threshold)
			{
				continue;
			}

			// note: removing ALL weight from a vertex disqualifies it as a candidate to receive weight during normalization
			// so there is no need to remove the influence from the per-vertex influence array to "prune" it.
			Weights.EditVertexWeightAndNormalize(BoneWeight.BoneIndex, VertexID,0.f,WeightEditsFromPrune);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("PruneWeightValuesChange", "Prune skin weights.");
	ApplyWeightEditsAsTransaction(WeightEditsFromPrune, TransactionLabel);
}

void USkinWeightsPaintTool::AverageWeights()
{
	// get vertices to edit weights on
	const TArray<VertexIndex>& VerticesToAverage = GetVerticesToEdit();

	// accumulate ALL the weights on the vertices
	TMap<BoneIndex, float> AccumulatedWeightMap;
	for (const VertexIndex VertexID : VerticesToAverage)
	{
		for (const FVertexBoneWeight& BoneWeight : Weights.PreChangeWeights[VertexID])
		{
			float& AccumulatedWeight = AccumulatedWeightMap.FindOrAdd(BoneWeight.BoneIndex);
			AccumulatedWeight += BoneWeight.Weight;
		}
	}

	// sort influences by total weight
	AccumulatedWeightMap.ValueSort([](const float& A, const float& B)
	{
		return A > B;
	});

	// truncate to MaxInfluences
	int32 Index = 0;
	for (TMap<BoneIndex, float>::TIterator It(AccumulatedWeightMap); It; ++It)
	{
		if (Index >= MAX_TOTAL_INFLUENCES)
		{
			It.RemoveCurrent();
		}
		else
		{
			++Index;
		}
	}

	// normalize remaining influences
	float TotalWeight = 0.f;
	for (const TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
	{
		TotalWeight += AccumulatedWeight.Value;
	}
	for (TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
	{
		AccumulatedWeight.Value /= TotalWeight;
	}

	// apply averaged weights to vertices
	FMultiBoneWeightEdits WeightEditsFromAveraging;
	for (const VertexIndex VertexID : VerticesToAverage)
	{
		// remove influences not a part of the average results
		for (const FVertexBoneWeight& BoneWeight : Weights.PreChangeWeights[VertexID])
		{
			if (!AccumulatedWeightMap.Contains(BoneWeight.BoneIndex))
			{
				const float OldWeight = BoneWeight.Weight;
				constexpr float NewWeight = 0.f;
				WeightEditsFromAveraging.MergeSingleEdit(BoneWeight.BoneIndex, VertexID, OldWeight, NewWeight);
			}
		}

		// add influences from the averaging results
		for (const TTuple<BoneIndex, float>& AccumulatedWeight : AccumulatedWeightMap)
		{
			const BoneIndex IndexOfBone = AccumulatedWeight.Key;
			const float OldWeight = Weights.GetWeightOfBoneOnVertex(IndexOfBone, VertexID, Weights.PreChangeWeights);
			const float NewWeight = AccumulatedWeight.Value;
			WeightEditsFromAveraging.MergeSingleEdit(IndexOfBone, VertexID, OldWeight, NewWeight);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("AverageWeightValuesChange", "Average skin weights.");
	ApplyWeightEditsAsTransaction(WeightEditsFromAveraging, TransactionLabel);
}

void USkinWeightsPaintTool::NormalizeWeights()
{
	// re-set a weight on each vertex to force normalization
	FMultiBoneWeightEdits WeightEditsFromNormalization;
	const TArray<VertexIndex> VerticesToNormalize = GetVerticesToEdit();
	for (const VertexIndex VertexID : VerticesToNormalize)
	{
		const VertexWeights& VertexWeights = Weights.CurrentWeights[VertexID];
		for (const FVertexBoneWeight& BoneWeight : VertexWeights)
		{
			// set weight to current value, just to force re-normalization
			Weights.EditVertexWeightAndNormalize(BoneWeight.BoneIndex, VertexID, BoneWeight.Weight,WeightEditsFromNormalization);
		}
	}

	// apply the changes
	const FText TransactionLabel = LOCTEXT("NormalizeWeightValuesChange", "Normalize skin weights.");
	ApplyWeightEditsAsTransaction(WeightEditsFromNormalization, TransactionLabel);
}

void USkinWeightsPaintTool::TransferWeights()
{
	using namespace UE::Geometry;
	using namespace UE::AnimationCore;

	if (!SourceTarget)
	{
		return;
	}

	const EMeshLODIdentifier TargetLODId = GetLODId(WeightToolProperties->ActiveLOD);
    const FGetMeshParameters TargetParams(true, TargetLODId);
	FDynamicMesh3 TargetMesh = UE::ToolTarget::GetDynamicMeshCopy(Target, TargetParams);

	const EMeshLODIdentifier SourceLODId = GetLODId(WeightToolProperties->SourceLOD);
	const FGetMeshParameters SourceParams(true, SourceLODId);
	const FDynamicMesh3 SourceMesh = UE::ToolTarget::GetDynamicMeshCopy(SourceTarget, SourceParams);
	
	if (!SourceMesh.HasAttributes() || !SourceMesh.Attributes()->HasBones())
	{
		return;
	}
	
	if (SourceMesh.Attributes()->GetNumBones() == 0)
	{
		return;
	}
	
	FTransferBoneWeights TransferBoneWeights(&SourceMesh, WeightToolProperties->SourceSkinWeightProfile);
	TransferBoneWeights.TransferMethod = FTransferBoneWeights::ETransferBoneWeightsMethod::InpaintWeights;
	
	if (!TargetMesh.HasAttributes())
	{
		TargetMesh.EnableAttributes();
	}

	FDynamicMeshAttributeSet* TargetAttributes = TargetMesh.Attributes();
	if (!TargetAttributes->HasBones())
	{
		TargetAttributes->CopyBoneAttributes(*SourceMesh.Attributes());
	}

	// NOTE should we expose all the options?
	// 	TransferBoneWeights.NormalThreshold;
	// 	TransferBoneWeights.SearchRadius
	// 	TransferBoneWeights.NumSmoothingIterations;
	// 	TransferBoneWeights.SmoothingStrength;
	// 	TransferBoneWeights.LayeredMeshSupport;
	// 	TransferBoneWeights.ForceInpaintWeightMapName;
	
	if (TransferBoneWeights.Validate() != EOperationValidationResult::Ok)
	{
		return;
	}

	if (WeightToolProperties->EditingMode == EWeightEditMode::Mesh)
	{
		TransferBoneWeights.TargetVerticesSubset = GetSelectedVertices();
	}

	const FName TargetProfile = WeightToolProperties->GetActiveSkinWeightProfile();
	if (TransferBoneWeights.TransferWeightsToMesh(TargetMesh, TargetProfile))
	{
		// store weight edits
		BeginChange();
		FMultiBoneWeightEdits WeightEdits;
		
		{	
			FDynamicMeshVertexSkinWeightsAttribute* TransferedSkinWeights = TargetAttributes->GetSkinWeightsAttribute(TargetProfile);
			check(TransferedSkinWeights);
			
			const bool bUseSubset = !TransferBoneWeights.TargetVerticesSubset.IsEmpty();

			static constexpr float ZeroWeight = 0.f;
			
			const int32 NumVertices = bUseSubset ? TransferBoneWeights.TargetVerticesSubset.Num() : TargetMesh.VertexCount();
			for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				const int32 VertexID = bUseSubset ? TransferBoneWeights.TargetVerticesSubset[VertexIndex] : VertexIndex;
				
				// remove all weight on vertex
				const VertexWeights& VertexBoneWeights = Weights.PreChangeWeights[VertexID];
				if (!VertexBoneWeights.IsEmpty())
				{
					for (const FVertexBoneWeight& BoneWeight : VertexBoneWeights)
					{
						const float OldWeight = BoneWeight.Weight;
						WeightEdits.MergeSingleEdit(BoneWeight.BoneIndex, VertexID, OldWeight, ZeroWeight);
					}
				}
				else
				{
					WeightEdits.MergeSingleEdit(0, VertexID, 1.f, ZeroWeight);
				}

				// update with new weight
				FBoneWeights TransferedBoneWeights;
				TransferedSkinWeights->GetValue(VertexID, TransferedBoneWeights);
				for (FBoneWeight BoneWeight: TransferedBoneWeights)
				{
					const int32 BoneIndex = BoneWeight.GetBoneIndex();					
					const float OldWeight = Weights.GetWeightOfBoneOnVertex(BoneIndex, VertexID, Weights.PreChangeWeights);
					const float NewWeight = BoneWeight.GetWeight();
					WeightEdits.MergeSingleEdit(BoneIndex, VertexID, OldWeight, NewWeight);
				}
			}
		}

		// set new weights (we could probably use TransferedSkinWeights instead of a full convert) 
		FDynamicMeshToMeshDescription Converter;
		Converter.Convert(&TargetMesh, *EditedMesh);

		// update weights
		Weights = FSkinToolWeights();
		Weights.Profile = TargetProfile;
		Weights.InitializeSkinWeights(GetSkeletalMeshComponent(Target), EditedMesh);
		bVertexColorsNeedUpdated = true;

		// store weight edits in the active change & commit
		{
			for (const TTuple<int32, FSingleBoneWeightEdits>& BoneWeightEdits : WeightEdits.PerBoneWeightEdits)
			{
				ActiveChange->AddBoneWeightEdit(BoneWeightEdits.Value);
			}

			static const FText TransactionLabel = LOCTEXT("TransferWeightsChange", "Transfer skin weights.");
			EndChange(TransactionLabel);
		}
	}
}

void USkinWeightsPaintTool::HandleSkeletalMeshModified(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType)
{
	switch (InNotifyType)
	{
	case ESkeletalMeshNotifyType::BonesAdded:
		break;
	case ESkeletalMeshNotifyType::BonesRemoved:
		break;
	case ESkeletalMeshNotifyType::BonesMoved:
		{
			// TODO update only vertices weighted to modified bones (AND CHILDREN!?)
			Weights.Deformer.SetAllVerticesToBeUpdated();
			break;	
		}
	case ESkeletalMeshNotifyType::BonesSelected:
		{
			// store selected bones
			SelectedBoneNames = InBoneNames;
			PendingCurrentBone = InBoneNames.IsEmpty() ? NAME_None : InBoneNames[0];

			// update selected bone indices from names
			SelectedBoneIndices.Reset();
			for (const FName SelectedBoneName : SelectedBoneNames)
			{
				SelectedBoneIndices.Add(GetBoneIndexFromName(SelectedBoneName));
			}
		}
		break;
	case ESkeletalMeshNotifyType::BonesRenamed:
		break;
	case ESkeletalMeshNotifyType::HierarchyChanged:
		break;
	default:
		checkNoEntry();
	}
}

void USkinWeightsPaintTool::OnActiveLODChanged()
{
	const USkeletalMeshComponent* Component = GetSkeletalMeshComponent(Target);
	if (!ensure(Component))
	{
		return;
	}

	if (IsSelectionIsolated())
	{
		FinishIsolatedSelection();
	}

	// apply previous changes
	Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh);

	// update EditedMesh using the new LOD
	const EMeshLODIdentifier LODId = GetLODId(WeightToolProperties->ActiveLOD);
	const FGetMeshParameters Params(true, LODId);
	EditedMesh = EditedMeshes.Find(LODId);
	if (!EditedMesh)
	{
		EditedMesh = &EditedMeshes.Emplace(LODId);
		*EditedMesh = *UE::ToolTarget::GetMeshDescription(Target, Params);
	}

	// reinitialize all mesh data structures
	const FDynamicMesh3 DynamicMesh = UE::ToolTarget::GetDynamicMeshCopy(Target, Params);
	PostEditMeshInitialization(Component, DynamicMesh, *EditedMesh);
}

void USkinWeightsPaintTool::OnActiveSkinWeightProfileChanged()
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(Target);
	if (!SkeletalMeshComponent)
	{
		return;
	}

	WeightToolProperties->bShowNewProfileName = WeightToolProperties->ActiveSkinWeightProfile == CreateNewName();

	if (IsSelectionIsolated())
	{
		FinishIsolatedSelection();
	}

	if (WeightToolProperties->bShowNewProfileName)
	{
		if (!IsProfileValid(WeightToolProperties->NewSkinWeightProfile))
		{
			GetOrCreateSkinWeightsAttribute(*EditedMesh, WeightToolProperties->NewSkinWeightProfile);
		} 
	}
	
	if (!IsProfileValid(WeightToolProperties->GetActiveSkinWeightProfile()))
	{
		WeightToolProperties->ActiveSkinWeightProfile = FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName;
		WeightToolProperties->bShowNewProfileName = false;
	}
	
	if (WeightToolProperties->GetActiveSkinWeightProfile() == Weights.Profile)
	{
		return;
	}

	// apply previous changes
	Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh);

	// re-init Weights with new skin profile
	Weights = FSkinToolWeights();
	Weights.Profile = WeightToolProperties->GetActiveSkinWeightProfile();
	Weights.InitializeSkinWeights(SkeletalMeshComponent, EditedMesh);
	bVertexColorsNeedUpdated = true;
}

void USkinWeightsPaintTool::OnNewSkinWeightProfileChanged()
{
	if (WeightToolProperties->bShowNewProfileName && WeightToolProperties->NewSkinWeightProfile != Weights.Profile)
	{
		const bool bRenamed = RenameSkinWeightsAttribute(*EditedMesh, Weights.Profile, WeightToolProperties->NewSkinWeightProfile);
		if (ensure(bRenamed))
		{
			Weights.Profile = WeightToolProperties->NewSkinWeightProfile;
		}
	}
}

bool USkinWeightsPaintTool::IsProfileValid(const FName InProfileName) const
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(Target);
	if (!SkeletalMeshComponent)
	{
		return false;
	}

	// check current MeshDescription
	const FSkeletalMeshConstAttributes MeshAttribs(*EditedMesh);
	const TArray<FName> MeshDescProfiles = MeshAttribs.GetSkinWeightProfileNames();
	const bool bHasProfile = MeshDescProfiles.ContainsByPredicate([InProfileName](const FName Name)
	{
		return Name == InProfileName;
	});
	
	return bHasProfile;
}

void USkinWeightsPaintTool::ToggleEditingMode()
{
	Weights.Deformer.SetAllVerticesToBeUpdated();

	// toggle brush mode
	SetBrushEnabled(WeightToolProperties->EditingMode == EWeightEditMode::Brush);

	// toggle mesh mode
	PolygonSelectionMechanic->SetIsEnabled(WeightToolProperties->EditingMode == EWeightEditMode::Mesh);

	// toggle bone select mode
	// this mode is set to be compatible with the 
	if (PersonaModeManagerContext.IsValid())
	{
		if (WeightToolProperties->EditingMode == EWeightEditMode::Bones)
		{
			PersonaModeManagerContext->GetPersonaEditorModeManager()->ActivateMode(FPersonaEditModes::SkeletonSelection);	
		}
		else
		{
			PersonaModeManagerContext->GetPersonaEditorModeManager()->DeactivateMode(FPersonaEditModes::SkeletonSelection);	
		}
	}

	SetFocusInViewport();
}

void USkinWeightsPaintTool::SetComponentSelectionMode(EComponentSelectionMode InMode)
{
	PolygonSelectionMechanic->Properties->bSelectVertices = InMode == EComponentSelectionMode::Vertices;
	PolygonSelectionMechanic->Properties->bSelectEdges = InMode == EComponentSelectionMode::Edges;
	PolygonSelectionMechanic->Properties->bSelectFaces = InMode == EComponentSelectionMode::Faces;
	
	PolygonSelectionMechanic->SetShowSelectableCorners(InMode == EComponentSelectionMode::Vertices);
	PolygonSelectionMechanic->SetShowEdges(InMode == EComponentSelectionMode::Edges);

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ShowFlag.MeshEdges"));
	if (CVar)
	{
		const float Value = (InMode == EComponentSelectionMode::Edges) ? 0.0f : 1.0f;
		CVar->Set(Value, ECVF_SetByCode);
	}
}

void USkinWeightsPaintTool::GrowSelection() const
{
	PolygonSelectionMechanic->GrowSelection();
}

void USkinWeightsPaintTool::ShrinkSelection() const
{
	PolygonSelectionMechanic->ShrinkSelection();
}

void USkinWeightsPaintTool::FloodSelection() const
{
	PolygonSelectionMechanic->FloodSelection();
}

bool USkinWeightsPaintTool::IsAnyComponentSelected() const
{
	if (!PolygonSelectionMechanic)
	{
		return false;
	}

	return PolygonSelectionMechanic->HasSelection();
}

bool USkinWeightsPaintTool::IsSelectionIsolated() const
{
	return PartialMeshDescription.IsValid();
}

void USkinWeightsPaintTool::SetIsolateSelected(const bool bIsolateSelection)
{
	// if we are turning off an isolated selection, we must queue the Tick() to update the full mesh
	if (!bIsolateSelection && PartialMeshDescription.IsValid())
	{
		bPendingUpdateFromPartialMesh = true;
		return;
	}
	
	if (PartialMeshDescription.IsValid())
	{
		ensure(false); // should be reset to null
		return;
	}
	
	if (!ensure(PolygonSelectionMechanic))
	{
		return;
	}

	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(Target);
	if (!ensure(SkeletalMeshComponent))
	{
		return;
	}

	if (!ensure(EditedMesh))
	{
		return;
	}

	// apply previous changes
	Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh);
	
	// put into ref pose, BEFORE copying the mesh, so that submesh deformer initializes with vertices in ref pose
	Weights.Deformer.SetToRefPose(this);

	// store selection to be restored
	IsolatedSelectionToRestoreVertices.Reset();
	IsolatedSelectionToRestoreEdges.Reset();
	IsolatedSelectionToRestoreFaces.Reset();
	IsolatedSelectionToRestoreVertices.ElementType = UE::Geometry::EGeometryElementType::Vertex;
	IsolatedSelectionToRestoreEdges.ElementType = UE::Geometry::EGeometryElementType::Edge;
	IsolatedSelectionToRestoreFaces.ElementType = UE::Geometry::EGeometryElementType::Face;
	PolygonSelectionMechanic->GetSelection_AsTriangleTopology(IsolatedSelectionToRestoreVertices);
	PolygonSelectionMechanic->GetSelection_AsTriangleTopology(IsolatedSelectionToRestoreEdges);
	PolygonSelectionMechanic->GetSelection_AsTriangleTopology(IsolatedSelectionToRestoreFaces);

	// store copy of original FDynamicMesh to restore
	FDynamicMesh3 DynamicMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(EditedMesh, DynamicMesh);
	FullDynamicMesh = MoveTemp(DynamicMesh);

	// create a submesh from the selected triangles (or triangles connected to selected vertices/edges)
	TArray<int32> TrianglesToIsolate;
	GetSelectedTriangles(TrianglesToIsolate);
	if (TrianglesToIsolate.IsEmpty())
	{
		return;
	}
	PartialSubMesh = UE::Geometry::FDynamicSubmesh3(&FullDynamicMesh, TrianglesToIsolate);

	// create mesh description for sub-mesh
	PartialMeshDescription = MakeShared<FMeshDescription>();
	// registering skeletal mesh attributes is required to create room to copy attributes during conversion from dynamic mesh
	FSkeletalMeshAttributes Attributes(*PartialMeshDescription);
	Attributes.Register();
	// convert the partial dynamic mesh to a mesh description
	// NOTE: this copies vertex weights to partial mesh description (later used to load weights into the tool)
	FDynamicMeshToMeshDescription DnyToDescConverter;
	constexpr bool bCopyTangents = true;
	DnyToDescConverter.Convert(&PartialSubMesh.GetSubmesh(), *PartialMeshDescription, bCopyTangents);
	
	// reinitialize all mesh data structures
	PostEditMeshInitialization(SkeletalMeshComponent, PartialSubMesh.GetSubmesh(), *PartialMeshDescription.Get());
}

void USkinWeightsPaintTool::FinishIsolatedSelection()
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(Target);
	if (!ensure(SkeletalMeshComponent))
	{
		return;
	}

	if (!PartialMeshDescription)
	{
		// nothing hidden
		return;
	}

	// apply partial mesh weights to partial mesh description
	Weights.ApplyCurrentWeightsToMeshDescription(PartialMeshDescription.Get());

	// reinitialize with full mesh
	PostEditMeshInitialization(SkeletalMeshComponent, FullDynamicMesh, *EditedMesh);

	// copy the remapped weights back to the full mesh
	const FSkeletalMeshConstAttributes MeshAttribs(*PartialMeshDescription.Get());
	const FSkinWeightsVertexAttributesConstRef AllVertexWeights = MeshAttribs.GetVertexSkinWeights(WeightToolProperties->GetActiveSkinWeightProfile());
	const int32 NumVerticesInPartialMesh = PartialMeshDescription.Get()->Vertices().Num();
	for (int32 VertexIndexPartial = 0; VertexIndexPartial < NumVerticesInPartialMesh; VertexIndexPartial++)
	{
		// get the equivalent vertex on the full mesh
		const int32 VertexIndexFull = PartialSubMesh.MapVertexToBaseMesh(VertexIndexPartial);
		// clear all the weights on this vertex
		Weights.CurrentWeights[VertexIndexFull].Init(FVertexBoneWeight(), UE::AnimationCore::MaxInlineBoneWeightCount);
		// replace with weights from partial mesh
		const FVertexBoneWeightsConst& VertexWeightsPartial = AllVertexWeights.Get(VertexIndexPartial);
		for (int32 InfluenceIndex=0; InfluenceIndex<VertexWeightsPartial.Num(); ++InfluenceIndex)
		{
			const UE::AnimationCore::FBoneWeight& SingleBoneWeight = VertexWeightsPartial[InfluenceIndex];
			FVertexBoneWeight& VertexBoneWeight = Weights.CurrentWeights[VertexIndexFull][InfluenceIndex];
			VertexBoneWeight.BoneIndex = SingleBoneWeight.GetBoneIndex();
			VertexBoneWeight.Weight = SingleBoneWeight.GetWeight();
			VertexBoneWeight.VertexInBoneSpace = Weights.Deformer.InvCSRefPoseTransforms[VertexBoneWeight.BoneIndex].TransformPosition(Weights.Deformer.RefPoseVertexPositions[VertexIndexFull]);
		}
	}
	// sync both weight buffers
	Weights.PreChangeWeights = Weights.CurrentWeights;
	// apply full mesh weights to full mesh description
	Weights.ApplyCurrentWeightsToMeshDescription(EditedMesh);

	// restore selection (allows for easily adjusting crop)
	PolygonSelectionMechanic->SetSelection_AsTriangleTopology(IsolatedSelectionToRestoreVertices);
	PolygonSelectionMechanic->SetSelection_AsTriangleTopology(IsolatedSelectionToRestoreEdges);
	PolygonSelectionMechanic->SetSelection_AsTriangleTopology(IsolatedSelectionToRestoreFaces);

	PartialMeshDescription = nullptr;
}

void USkinWeightsPaintTool::UpdateSelectedVertices()
{
	SelectedVertices.Empty();
	if (!PolygonSelectionMechanic)
	{
		return;
	}
	
	const FGroupTopologySelection& Selection = PolygonSelectionMechanic->GetActiveSelection();
	const FDynamicMesh3* DynamicMesh = PreviewMesh->GetMesh();
	const FVertexArray& AllVertices = EditedMesh->Vertices();

	// validate and add vertices to the output array
	auto AddVertices = [this, &AllVertices](const TSet<int32>& VerticesToAdd)
	{
		// we have to make sure that the vertex ids are safe to use as PolygonSelectionMechanic does not act on the
		// mesh description but on the dynamic mesh that can duplicate vertices when dealing with degenerate triangles.
		// cf. FMeshDescriptionToDynamicMesh::Convert for more details.
		Algo::CopyIf(VerticesToAdd, SelectedVertices, [&](int32 VertexID)
		{
			return AllVertices.IsValid(VertexID);	
		});
	};

	// add selected vertices
	AddVertices(Selection.SelectedCornerIDs);

	// add vertices on selected edges
	{
		TSet<int32> VerticesInSelectedEdges;
		for (const int32 SelectedEdgeIndex : Selection.SelectedEdgeIDs)
		{
			FDynamicMesh3::FEdge CurrentEdge = DynamicMesh->GetEdge(SelectedEdgeIndex);
			VerticesInSelectedEdges.Add(CurrentEdge.Vert.A);
			VerticesInSelectedEdges.Add(CurrentEdge.Vert.B);
		}
		
		AddVertices(VerticesInSelectedEdges);
	}

	// add vertices in selected faces
	{
		TSet<int32> VerticesInSelectedFaces;
		for (const int32 SelectedFaceIndex : Selection.SelectedGroupIDs)
		{
			UE::Geometry::FIndex3i TriangleVertices = DynamicMesh->GetTriangleRef(SelectedFaceIndex);
			VerticesInSelectedFaces.Add(TriangleVertices[0]);
			VerticesInSelectedFaces.Add(TriangleVertices[1]);
			VerticesInSelectedFaces.Add(TriangleVertices[2]);
		}
		
		AddVertices(VerticesInSelectedFaces);
	}
}

const TArray<int32>& USkinWeightsPaintTool::GetSelectedVertices() const
{
	return SelectedVertices;
}

const TArray<int32>& USkinWeightsPaintTool::GetVerticesToEdit()
{
	// prioritize re-using selected vertices array (if there are any)
	if (!SelectedVertices.IsEmpty())
	{
		return SelectedVertices;
	}
	
	// fallback on vertices weighted to selected bones
	VerticesToEdit.Reset();
	if (!SelectedBoneIndices.IsEmpty())
	{
		VertexIndex VertexID = 0;
		for (const VertexWeights& VertWeights : Weights.PreChangeWeights)
		{
			for (const FVertexBoneWeight& BoneWeight : VertWeights)
			{
				if (SelectedBoneIndices.Contains(BoneWeight.BoneIndex) && BoneWeight.Weight > MinimumWeightThreshold)
				{
					VerticesToEdit.Add(VertexID);	
				}
			}
			++VertexID;
		}
	}

	return VerticesToEdit;
}

void USkinWeightsPaintTool::GetSelectedTriangles(TArray<int32>& OutTriangleIndices) const
{
	OutTriangleIndices.Empty();
	if (!PolygonSelectionMechanic)
	{
		return;
	}
	
	const FGroupTopologySelection& Selection = PolygonSelectionMechanic->GetActiveSelection();
	const FDynamicMesh3* DynamicMesh = PreviewMesh->GetMesh();
	TSet<int32> TriangleSet;

	// add triangles connected to selected vertices
	for (const int32 VertexIndex : Selection.SelectedCornerIDs)
	{
		DynamicMesh->EnumerateVertexTriangles(VertexIndex, [&TriangleSet](int32 TriangleIndex)
		{
			TriangleSet.Add(TriangleIndex);
		});
	}

	// add triangles connected to selected edges
	for (const int32 EdgeIndex : Selection.SelectedEdgeIDs)
	{
		DynamicMesh->EnumerateEdgeTriangles(EdgeIndex, [&TriangleSet](int32 TriangleIndex)
		{
			TriangleSet.Add(TriangleIndex);
		});
	}

	// add selected triangles
	TriangleSet.Append(Selection.SelectedGroupIDs);	

	OutTriangleIndices = TriangleSet.Array();
}

void USkinWeightsPaintTool::GetInfluences(const TArray<int32>& VertexIndices, TArray<BoneIndex>& OutBoneIndices)
{
	for (const int32 SelectedVertex : VertexIndices)
	{
		for (const FVertexBoneWeight& VertexBoneData : Weights.CurrentWeights[SelectedVertex])
		{
			if (VertexBoneData.Weight >= MinimumWeightThreshold)
			{
				OutBoneIndices.AddUnique(VertexBoneData.BoneIndex);
			}
		}
	}
	
	// sort hierarchically (bone indices are sorted root to leaf)
	OutBoneIndices.Sort([](BoneIndex A, BoneIndex B) {return A < B;});
}

float USkinWeightsPaintTool::GetAverageWeightOnBone(
	const BoneIndex InBoneIndex,
	const TArray<int32>& VertexIndices)
{
	float TotalWeight = 0.f;
	float NumVerticesInfluencedByBone = 0.f;
	
	for (const int32 SelectedVertex : VertexIndices)
	{
		if (!Weights.CurrentWeights.IsValidIndex(SelectedVertex))
		{
			continue;
		}
		
		for (const FVertexBoneWeight& VertexBoneData : Weights.CurrentWeights[SelectedVertex])
		{
			if (VertexBoneData.BoneIndex == InBoneIndex)
			{
				++NumVerticesInfluencedByBone;
				TotalWeight += VertexBoneData.Weight;
			}
		}
	}

	return NumVerticesInfluencedByBone > 0 ? TotalWeight / NumVerticesInfluencedByBone : TotalWeight;
}

FName USkinWeightsPaintTool::GetBoneNameFromIndex(BoneIndex InIndex) const
{
	const TArray<FName>& Names = Weights.Deformer.BoneNames;
	if (Names.IsValidIndex(InIndex))
	{
		return Names[InIndex];
	}

	return NAME_None;
}


BoneIndex USkinWeightsPaintTool::GetCurrentBoneIndex() const
{
	return GetBoneIndexFromName(CurrentBone);
}

void USkinWeightsPaintTool::SetDisplayVertexColors(bool bShowVertexColors)
{
	if (bShowVertexColors)
	{
		UMaterialInterface* VtxColorMaterial = GetToolManager()->GetContextQueriesAPI()->GetStandardMaterial(EStandardToolContextMaterials::VertexColorMaterial);
		PreviewMesh->SetOverrideRenderMaterial(VtxColorMaterial);
		bVertexColorsNeedUpdated = true;
	}
	else
	{
		PreviewMesh->ClearOverrideRenderMaterial();
	}
}

void USkinWeightsPaintTool::OnPropertyModified(UObject* ModifiedObject, FProperty* ModifiedProperty)
{
	Super::OnPropertyModified(ModifiedObject, ModifiedProperty);

	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, BrushStrength))
	{
		WeightToolProperties->GetBrushConfig().Strength = WeightToolProperties->BrushStrength;
	}
	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, BrushRadius))
	{
		WeightToolProperties->GetBrushConfig().Radius = WeightToolProperties->BrushRadius;
	}
	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, BrushFalloffAmount))
	{
		WeightToolProperties->GetBrushConfig().Falloff = WeightToolProperties->BrushFalloffAmount;
	}
	
	const FString NameOfModifiedProperty = ModifiedProperty->GetNameCPP();

	// invalidate vertex color cache when any weight color properties are modified
	const TArray<FString> ColorPropertyNames = {
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, ColorMode),
		GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, ColorRamp),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, R),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, G),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, B),
		GET_MEMBER_NAME_STRING_CHECKED(FLinearColor, A)};
	if (ColorPropertyNames.Contains(NameOfModifiedProperty))
	{
		bVertexColorsNeedUpdated = true;

		// force all colors to have Alpha = 1
		for (FLinearColor& Color : WeightToolProperties->ColorRamp)
		{
			Color.A = 1.f;
		}
	}

	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, SourceSkeletalMesh))
	{
		ResetSourceForTransfer(WeightToolProperties->SourceSkeletalMesh.Get());
	}

	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, SourceLOD))
	{
		if (SourcePreviewMesh)
		{
			const EMeshLODIdentifier SourceLODId = GetLODId(WeightToolProperties->SourceLOD);
			const FGetMeshParameters SourceParams(true, SourceLODId);
			SourcePreviewMesh->ReplaceMesh(UE::ToolTarget::GetDynamicMeshCopy(SourceTarget, SourceParams));
		}
	}

	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, bShowSourcePreview))
	{
		if (SourcePreviewMesh)
		{
			SourcePreviewMesh->SetVisible(WeightToolProperties->bShowSourcePreview);
		}
	}

	if (ModifiedProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(USkinWeightsPaintToolProperties, SourcePreviewOffset))
	{
		if (SourcePreviewMesh)
		{
			SourcePreviewMesh->SetTransform(WeightToolProperties->SourcePreviewOffset);
		}
	}
	
	SetFocusInViewport();
}

void USkinWeightsPaintTool::ResetSourceForTransfer(USkeletalMesh* InSkeletalMesh)
{
	if (SourcePreviewMesh)
	{
		SourcePreviewMesh->SetVisible(false);
		SourcePreviewMesh->Disconnect();
		SourcePreviewMesh = nullptr;
	}

	if (SourceTarget)
	{
		SourceTarget = nullptr;
	}

	if (InSkeletalMesh)
	{
		SourceTarget = TargetManager->BuildTarget(InSkeletalMesh, FToolTargetTypeRequirements());

		SourcePreviewMesh = NewObject<UPreviewMesh>(this);
		SourcePreviewMesh->CreateInWorld(TargetWorld.Get(), FTransform::Identity);

		if (USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponent(Target))
		{
			const FBoxSphereBounds TargetBounds = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetBounds();
			const FBoxSphereBounds SourceBounds = InSkeletalMesh->GetBounds();

			FTransform Transform = UE::ToolTarget::GetLocalToWorldTransform(Target);
			FVector Location = Transform.GetLocation();
			Location.X += TargetBounds.GetBoxExtrema(1).X;
			Location.X += 1.1 * SourceBounds.GetBoxExtrema(1).X;
			Transform.SetLocation(Location);
			WeightToolProperties->SourcePreviewOffset = Transform;
		}
		
		SourcePreviewMesh->SetTransform(WeightToolProperties->SourcePreviewOffset);

		ToolSetupUtil::ApplyRenderingConfigurationToPreview(SourcePreviewMesh, SourceTarget);
		SourcePreviewMesh->SetTangentsMode(EDynamicMeshComponentTangentsMode::AutoCalculated);
		SourcePreviewMesh->ReplaceMesh(UE::ToolTarget::GetDynamicMeshCopy(SourceTarget));

		const FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(SourceTarget);
		SourcePreviewMesh->SetMaterials(MaterialSet.Materials);

		SourcePreviewMesh->SetVisible(WeightToolProperties->bShowSourcePreview);
	}
}

#undef LOCTEXT_NAMESPACE