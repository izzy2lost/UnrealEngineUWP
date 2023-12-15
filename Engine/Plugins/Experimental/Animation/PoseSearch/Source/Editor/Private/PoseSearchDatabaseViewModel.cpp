// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseViewModel.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MirrorDataTable.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InstancedStruct.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchDatabaseDataDetails.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchEditor.h"
#include "PropertyEditorModule.h"

namespace UE::PoseSearch
{
#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<float> CVarDatabasePreviewDebugDrawSamplerSize(TEXT("a.DatabasePreview.DebugDrawSamplerSize"), 0.f, TEXT("Debug Draw Sampler Positions Size"));
#endif

constexpr float StepDeltaTime = 1.0f / 30.0f;

// FDatabasePreviewActor
bool FDatabasePreviewActor::SpawnPreviewActor(UWorld* World, const UPoseSearchDatabase* PoseSearchDatabase, int32 IndexAssetIdx, int32 PoseIdxForTimeOffset)
{
	check(PoseSearchDatabase);
	const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
	const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIdx];

	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetAnimationAssetBase(IndexAsset.GetSourceAssetIdx());
	UAnimationAsset* PreviewAsset = Cast<UAnimationAsset>(DatabaseAnimationAsset->GetAnimationAsset());
	if (!PreviewAsset)
	{
		return false;
	}

	Sampler.Init(PreviewAsset, IndexAsset.GetBlendParameters());

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(PoseSearchDatabase->Schema->BoneIndicesWithParents, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *PoseSearchDatabase->Schema->Skeleton);
	Sampler.Process(BoneContainer);

	IndexAssetIndex = IndexAssetIdx;
	CurrentPoseIndex = INDEX_NONE;

	if (PoseIdxForTimeOffset < 0)
	{
		PlayTimeOffset = 0.f;
	}
	else
	{
		PlayTimeOffset = PoseSearchDatabase->GetRealAssetTime(PoseIdxForTimeOffset) - IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate);
	}

	// @todo: should we always use the PlayTimeOffset to extract the root transform?
	if (PlayTimeOffset != 0.f)
	{
		RootTransformOrigin = ExtractRootTransform(PlayTimeOffset);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ActorPtr = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	ActorPtr->SetFlags(RF_Transient);

	UDebugSkelMeshComponent* Mesh = NewObject<UDebugSkelMeshComponent>(ActorPtr.Get());
	Mesh->RegisterComponentWithWorld(World);

	UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(Mesh);

	Mesh->PreviewInstance = AnimInstance;
	AnimInstance->InitializeAnimation();

	USkeletalMesh* DatabasePreviewMesh = PoseSearchDatabase->PreviewMesh;
	Mesh->SetSkeletalMesh(DatabasePreviewMesh ? DatabasePreviewMesh : PoseSearchDatabase->Schema->Skeleton->GetPreviewMesh(true));
	Mesh->EnablePreview(true, PreviewAsset);
		
	AnimInstance->SetAnimationAsset(PreviewAsset, false, 0.0f);
	AnimInstance->SetBlendSpacePosition(IndexAsset.GetBlendParameters());
		
	if (IndexAsset.IsMirrored() && PoseSearchDatabase->Schema)
	{
		AnimInstance->SetMirrorDataTable(PoseSearchDatabase->Schema->MirrorDataTable);
		MirrorDataCache.Init(PoseSearchDatabase->Schema->MirrorDataTable, AnimInstance->GetRequiredBonesOnAnyThread());
	}

	AnimInstance->PlayAnim(false, 0.0f);

	if (!ActorPtr->GetRootComponent())
	{
		ActorPtr->SetRootComponent(Mesh);
	}

	AnimInstance->SetPlayRate(0.f);

	UE_LOG(LogPoseSearchEditor, Log, TEXT("Spawned preview Actor: %s"), *GetNameSafe(ActorPtr.Get()));
	return true;
}

void FDatabasePreviewActor::UpdatePreviewActor(const UPoseSearchDatabase* PoseSearchDatabase, float PlayTime, bool bQuantizeAnimationToPoseData)
{
	check(PoseSearchDatabase);

	const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();

	UAnimPreviewInstance* AnimInstance = GetAnimPreviewInstanceInternal();
	if (!AnimInstance || IndexAssetIndex >= SearchIndex.Assets.Num())
	{
		return;
	}

	const UAnimationAsset* PreviewAsset = AnimInstance->GetAnimationAsset();
	if (!PreviewAsset)
	{
		return;
	}

	CurrentTime = 0.f;
	const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
	float CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate) + PlayTimeOffset;
	FAnimationRuntime::AdvanceTime(false, CurrentPlayTime, CurrentTime, IndexAsset.GetLastSampleTime(PoseSearchDatabase->Schema->SampleRate));
			 
	// time to pose index
	CurrentPoseIndex = PoseSearchDatabase->GetPoseIndexFromTime(CurrentTime, IndexAsset);

	const float QuantizedTime = CurrentPoseIndex >= 0 ? PoseSearchDatabase->GetRealAssetTime(CurrentPoseIndex) : CurrentTime;
	if (bQuantizeAnimationToPoseData)
	{
		CurrentTime = QuantizedTime;
	}

	// SetPosition is in [0..1] range for blendspaces
	AnimInstance->SetPosition(Sampler.ToNormalizedTime(CurrentTime));
	AnimInstance->SetPlayRate(0.f);
	AnimInstance->SetBlendSpacePosition(IndexAsset.GetBlendParameters());

	// updating root transforms
	RootTransformCurrentQuantizedTime = ExtractRootTransform(QuantizedTime);
	RootTransformCurrentQuantizedTime.SetToRelativeTransform(RootTransformOrigin);

	if (CurrentTime == QuantizedTime)
	{
		RootTransformCurrent = RootTransformCurrentQuantizedTime;
	}
	else
	{
		RootTransformCurrent = ExtractRootTransform(CurrentTime);
		RootTransformCurrent.SetToRelativeTransform(RootTransformOrigin);
	}

	check(ActorPtr != nullptr);
	ActorPtr->SetActorTransform(RootTransformCurrent);

	// @todo: optimize this bone container, since we only need the root bone here...
	const FBoneContainer& BoneContainer = AnimInstance->GetRequiredBonesOnAnyThread();
	if (BoneContainer.GetNumBones() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&BoneContainer);
		ExtractPose(QuantizedTime, Pose);
		RootBoneTransformCurrentQuantizedTime = Pose[FCompactPoseBoneIndex(RootBoneIndexType)];
	}
}

void FDatabasePreviewActor::Destroy()
{
	if (ActorPtr != nullptr)
	{
		ActorPtr->Destroy();
	}
}

bool FDatabasePreviewActor::DrawPreviewActor(const UPoseSearchDatabase* PoseSearchDatabase, bool bDisplayRootMotionSpeed, bool bDisplayBlockTransition, TConstArrayView<float> QueryVector)
{
	if (!PoseSearchDatabase->GetSearchIndex().IsValidPoseIndex(GetCurrentPoseIndex()))
	{
		return false;
	}

	const UDebugSkelMeshComponent* Mesh = GetDebugSkelMeshComponent();
	if (!Mesh)
	{
		return false;
	}

	const FTransform RootBoneTransformCurrentQuantizedTimeWorld = RootBoneTransformCurrentQuantizedTime * RootTransformCurrentQuantizedTime;
	UE::PoseSearch::FDebugDrawParams DrawParams(Mesh->GetWorld(), Mesh, RootBoneTransformCurrentQuantizedTimeWorld, PoseSearchDatabase);
	DrawParams.DrawFeatureVector(GetCurrentPoseIndex());

	if (!QueryVector.IsEmpty())
	{
		DrawParams.DrawFeatureVector(QueryVector);
	}

	if (bDisplayRootMotionSpeed || bDisplayBlockTransition)
	{
		// initializing SampledRootMotion if required
		if (SampledRootMotion.IsEmpty())
		{
			const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];

			const int NumPoses = IndexAsset.GetNumPoses();
			if (NumPoses > 1)
			{
				SampledRootMotion.SetNumUninitialized(NumPoses);
				SampledRootMotionSpeed.SetNumUninitialized(NumPoses);

				for (int32 Index = 0; Index < NumPoses; ++Index)
				{
					const int32 IndexAssetPoseIdx = Index + IndexAsset.GetFirstPoseIdx();
					const float IndexAssetPoseTime = IndexAsset.GetTimeFromPoseIndex(IndexAssetPoseIdx, PoseSearchDatabase->Schema->SampleRate);
					FTransform IndexAssetPoseTransform = ExtractRootTransform(IndexAssetPoseTime);
					IndexAssetPoseTransform.SetToRelativeTransform(RootTransformOrigin);

					SampledRootMotion[Index] = IndexAssetPoseTransform.GetTranslation();
				}

				for (int32 Index = 1; Index < NumPoses; ++Index)
				{
					const FVector& Start = SampledRootMotion[Index - 1];
					const FVector& End = SampledRootMotion[Index];
					SampledRootMotionSpeed[Index] = (Start - End).Length() * PoseSearchDatabase->Schema->PermutationsSampleRate;
				}
				SampledRootMotionSpeed[0] = SampledRootMotionSpeed[1];
			}
		}
	}

	if (bDisplayRootMotionSpeed)
	{
		// drawing PreviewActor.SampledRootMotion
		const int32 SampledRootMotionNum = SampledRootMotion.Num();
		if (SampledRootMotionNum > 1)
		{
			for (int32 Index = 0; Index < SampledRootMotion.Num(); ++Index)
			{
				const FVector& EndDown = SampledRootMotion[Index];
				const FVector EndUp = EndDown + (SampledRootMotionSpeed[Index] * FVector::UpVector);

				DrawParams.DrawLine(EndDown, EndUp, FColor::Black);
				if (Index > 0)
				{
					const FColor RootMotionColor = Index % 2 == 0 ? FColor::Purple : FColor::Orange;
					const FVector& StartDown = SampledRootMotion[Index - 1];
					const FVector StartUp = StartDown + (SampledRootMotionSpeed[Index - 1] * FVector::UpVector);
					DrawParams.DrawLine(StartDown, EndDown, RootMotionColor);
					DrawParams.DrawLine(StartUp, EndUp, RootMotionColor);
				}
			}
		}
	}

	if (bDisplayBlockTransition)
	{
		const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];

		const int NumPoses = IndexAsset.GetNumPoses();
		if (NumPoses == SampledRootMotion.Num())
		{
			for (int32 Index = 0; Index < NumPoses; ++Index)
			{
				const int32 IndexAssetPoseIdx = Index + IndexAsset.GetFirstPoseIdx();
				if (SearchIndex.PoseMetadata[IndexAssetPoseIdx].IsBlockTransition())
				{
					DrawParams.DrawPoint(SampledRootMotion[Index], FColor::Red);
				}
				else
				{
					DrawParams.DrawPoint(SampledRootMotion[Index], FColor::Green);
				}
			}
		}
	}


#if ENABLE_ANIM_DEBUG
	const float DebugDrawSamplerSize = CVarDatabasePreviewDebugDrawSamplerSize.GetValueOnAnyThread();
	if (DebugDrawSamplerSize > UE_KINDA_SMALL_NUMBER)
	{
		// drawing the pose extracted from the Sampler to visually compare with the pose features and the mesh drawing
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&GetAnimPreviewInstance()->GetRequiredBonesOnAnyThread());
		ExtractPose(CurrentTime, Pose);

		const FTransform RootTransform = ExtractRootTransform(CurrentTime);

		FCSPose<FCompactPose> ComponentSpacePose;
		ComponentSpacePose.InitPose(MoveTemp(Pose));

		for (int32 BoneIndex = 0; BoneIndex < ComponentSpacePose.GetPose().GetNumBones(); ++BoneIndex)
		{
			const FTransform BoneWorldTransforms = ComponentSpacePose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)) * RootTransform;
			DrawParams.DrawPoint(BoneWorldTransforms.GetTranslation(), FColor::Red, DebugDrawSamplerSize);
		}
	}
#endif // ENABLE_ANIM_DEBUG

	return true;
}

FTransform FDatabasePreviewActor::ExtractRootTransform(float Time) const
{
	return MirrorDataCache.MirrorTransform(Sampler.ExtractRootTransform(Time));
}

void FDatabasePreviewActor::ExtractPose(float Time, FCompactPose& OutPose) const
{
	Sampler.ExtractPose(Time, OutPose);
	MirrorDataCache.MirrorPose(OutPose);
}

const UDebugSkelMeshComponent* FDatabasePreviewActor::GetDebugSkelMeshComponent() const
{
	if (ActorPtr != nullptr)
	{
		return Cast<UDebugSkelMeshComponent>(ActorPtr->GetRootComponent());
	}
	return nullptr;
}

const UAnimPreviewInstance* FDatabasePreviewActor::GetAnimPreviewInstance() const
{
	if (const UDebugSkelMeshComponent* Mesh = GetDebugSkelMeshComponent())
	{
		return Mesh->PreviewInstance.Get();
	}
	return nullptr;
}

UAnimPreviewInstance* FDatabasePreviewActor::GetAnimPreviewInstanceInternal()
{
	if (ActorPtr != nullptr)
	{
		if (UDebugSkelMeshComponent* Mesh = Cast<UDebugSkelMeshComponent>(ActorPtr->GetRootComponent()))
		{
			return Mesh->PreviewInstance.Get();
		}
	}
	return nullptr;
}

// FDatabaseViewModel
void FDatabaseViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PoseSearchDatabasePtr);
}

void FDatabaseViewModel::Initialize(UPoseSearchDatabase* InPoseSearchDatabase, const TSharedRef<FDatabasePreviewScene>& InPreviewScene, const TSharedRef<SDatabaseDataDetails>& InDatabaseDataDetails)
{
	PoseSearchDatabasePtr = InPoseSearchDatabase;
	PreviewScenePtr = InPreviewScene;
	DatabaseDataDetails = InDatabaseDataDetails;

	RemovePreviewActors();
}

void FDatabaseViewModel::BuildSearchIndex()
{
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(GetPoseSearchDatabase(), ERequestAsyncBuildFlag::NewRequest);
}

void FDatabaseViewModel::PreviewBackwardEnd()
{
	SetPlayTime(MinPreviewPlayLength, false);
}

void FDatabaseViewModel::PreviewBackwardStep()
{
	const float NewPlayTime = FMath::Clamp(PlayTime - StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	SetPlayTime(NewPlayTime, false);
}

void FDatabaseViewModel::PreviewBackward()
{
	DeltaTimeMultiplier = -1.f;
}

void FDatabaseViewModel::PreviewPause()
{
	DeltaTimeMultiplier = 0.f;
}

void FDatabaseViewModel::PreviewForward()
{
	DeltaTimeMultiplier = 1.f;
}

void FDatabaseViewModel::PreviewForwardStep()
{
	const float NewPlayTime = FMath::Clamp(PlayTime + StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	SetPlayTime(NewPlayTime, false);
}

void FDatabaseViewModel::PreviewForwardEnd()
{
	SetPlayTime(MaxPreviewPlayLength, false);
}

UWorld* FDatabaseViewModel::GetWorld()
{
	check(PreviewScenePtr.IsValid());
	return PreviewScenePtr.Pin()->GetWorld();
}

void FDatabaseViewModel::OnPreviewActorClassChanged()
{
	// todo: implement
}

void FDatabaseViewModel::Tick(float DeltaSeconds)
{
	const float DeltaPlayTime = DeltaSeconds * DeltaTimeMultiplier;
	if (!FMath::IsNearlyZero(DeltaPlayTime))
	{
		PlayTime += DeltaPlayTime;
		PlayTime = FMath::Clamp(PlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);

		if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
		{
			if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
				{
					PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
				}
			}
		}
	}
}

void FDatabaseViewModel::RemovePreviewActors()
{
	PlayTime = 0.f;
	DeltaTimeMultiplier = 1.f;
	MaxPreviewPlayLength = 0.f;
	MinPreviewPlayLength = 0.f;
	bIsEditorSelection = true;
	bDrawQueryVector = false;

	for (FDatabasePreviewActor& PreviewActor : PreviewActors)
	{
		PreviewActor.Destroy();
	}

	PreviewActors.Reset();
}

void FDatabaseViewModel::AddSequenceToDatabase(UAnimSequence* AnimSequence)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		FPoseSearchDatabaseSequence NewAsset;
		NewAsset.Sequence = AnimSequence;
		Database->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddBlendSpaceToDatabase(UBlendSpace* BlendSpace)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		FPoseSearchDatabaseBlendSpace NewAsset;
		NewAsset.BlendSpace = BlendSpace;
		Database->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddAnimCompositeToDatabase(UAnimComposite* AnimComposite)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		FPoseSearchDatabaseAnimComposite NewAsset;
		NewAsset.AnimComposite = AnimComposite;
		Database->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddAnimMontageToDatabase(UAnimMontage* AnimMontage)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		FPoseSearchDatabaseAnimMontage NewAsset;
		NewAsset.AnimMontage = AnimMontage;
		Database->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}
}

bool FDatabaseViewModel::DeleteFromDatabase(int32 AnimationAssetIndex)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			if (UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(DatabaseAnimationAssetBase->GetAnimationAsset()))
			{
				bool bModified = false;
				for (int32 NotifyIndex = AnimSequenceBase->Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
				{
					const FAnimNotifyEvent& NotifyEvent = AnimSequenceBase->Notifies[NotifyIndex];
					if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
					{
						if (PoseSearchBranchIn->Database == Database)
						{
							AnimSequenceBase->Notifies.RemoveAt(NotifyIndex);
							bModified = true;
						}
					}
				}

				if (bModified)
				{
					AnimSequenceBase->RefreshCacheData();	
					AnimSequenceBase->Modify();
				}
			}

			Database->AnimationAssets.RemoveAt(AnimationAssetIndex);
			Database->Modify();

			return true;
		}
	}

	return false;
}

void FDatabaseViewModel::SetDisableReselection(int32 AnimationAssetIndex, bool bEnabled)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetMutableAnimationAssetBase(AnimationAssetIndex))
		{
			DatabaseAnimationAsset->SetDisableReselection(bEnabled);
		}
	}
}

bool FDatabaseViewModel::IsDisableReselection(int32 AnimationAssetIndex) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			return DatabaseAnimationAsset->IsDisableReselection();
		}
	}

	return false;
}

void FDatabaseViewModel::SetIsEnabled(int32 AnimationAssetIndex, bool bEnabled)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetMutableAnimationAssetBase(AnimationAssetIndex))
		{
			Database->Modify();

			DatabaseAnimationAsset->SetIsEnabled(bEnabled);
		}
	}
}

bool FDatabaseViewModel::IsEnabled(int32 AnimationAssetIndex) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			return DatabaseAnimationAsset->IsEnabled();
		}
	}

	return false;
}

int32 FDatabaseViewModel::SetSelectedNode(int32 PoseIdx, bool bClearSelection, bool bDrawQuery, TConstArrayView<float> InQueryVector)
{
	int32 SelectedSourceAssetIdx = INDEX_NONE;

	if (bClearSelection)
	{
		RemovePreviewActors();
	}

	bIsEditorSelection = false;
	bDrawQueryVector = bDrawQuery;
	QueryVector = InQueryVector;

	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			if (SearchIndex.PoseMetadata.IsValidIndex(PoseIdx))
			{
				const uint32 IndexAssetIndex = SearchIndex.PoseMetadata[PoseIdx].GetAssetIndex();
				if (SearchIndex.Assets.IsValidIndex(IndexAssetIndex))
				{
					FDatabasePreviewActor PreviewActor;
					if (PreviewActor.SpawnPreviewActor(GetWorld(), Database, IndexAssetIndex, PoseIdx))
					{
						const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
						MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(Database->Schema->SampleRate) - PreviewActor.GetPlayTimeOffset());
						MinPreviewPlayLength = FMath::Min(MinPreviewPlayLength, IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate) - PreviewActor.GetPlayTimeOffset());
						PreviewActors.Add(PreviewActor);
						SelectedSourceAssetIdx = IndexAsset.GetSourceAssetIdx();
					}
				}
			}

			DatabaseDataDetails.Pin()->Reconstruct();

			for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
			{
				PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
			}

			SetPlayTime(0.f, false);
		}
	}

	ProcessSelectedActor(nullptr);

	return SelectedSourceAssetIdx;
}

void FDatabaseViewModel::SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes)
{
	RemovePreviewActors();

	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			TMap<int32, int32> AssociatedAssetIndices;
			for (int32 i = 0; i < InSelectedNodes.Num(); ++i)
			{
				AssociatedAssetIndices.FindOrAdd(InSelectedNodes[i]->SourceAssetIdx) = i;
			}

			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			for (int32 IndexAssetIndex = 0; IndexAssetIndex < SearchIndex.Assets.Num(); ++IndexAssetIndex)
			{
				const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
				if (const int32* SelectedNodesIndex = AssociatedAssetIndices.Find(IndexAsset.GetSourceAssetIdx()))
				{
					FDatabasePreviewActor PreviewActor;
					if (PreviewActor.SpawnPreviewActor(GetWorld(), Database, IndexAssetIndex))
					{
						MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(Database->Schema->SampleRate) - IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate));
						PreviewActors.Add(PreviewActor);
					}
				}
			}

			DatabaseDataDetails.Pin()->Reconstruct();
			for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
			{
				PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
			}
		}

		ProcessSelectedActor(nullptr);
	}
}

void FDatabaseViewModel::ProcessSelectedActor(AActor* Actor)
{
	SelectedActorIndexAssetIndex = INDEX_NONE;
	if (const FDatabasePreviewActor* SelectedPreviewActor = PreviewActors.FindByPredicate([Actor](const FDatabasePreviewActor& PreviewActor) { return PreviewActor.GetActor() == Actor; }))
	{
		SelectedActorIndexAssetIndex = SelectedPreviewActor->GetIndexAssetIndex();
	}
}

void FDatabaseViewModel::SetDrawQueryVector(bool bValue)
{
	if (bDrawQueryVector != bValue)
	{
		bDrawQueryVector = bValue;
		DatabaseDataDetails.Pin()->Reconstruct();
	}
}

const FSearchIndexAsset* FDatabaseViewModel::GetSelectedActorIndexAsset() const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			if (SearchIndex.Assets.IsValidIndex(SelectedActorIndexAssetIndex))
			{
				return &SearchIndex.Assets[SelectedActorIndexAssetIndex];
			}
		}
	}
	return nullptr;
}

TRange<double> FDatabaseViewModel::GetPreviewPlayRange() const
{
	constexpr double ViewRangeSlack = 0.2;
	return TRange<double>(MinPreviewPlayLength - ViewRangeSlack, MaxPreviewPlayLength + ViewRangeSlack);
}

float FDatabaseViewModel::GetPlayTime() const
{
	return PlayTime;
}

void FDatabaseViewModel::SetPlayTime(float NewPlayTime, bool bInTickPlayTime)
{
	NewPlayTime = FMath::Clamp(NewPlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	DeltaTimeMultiplier = bInTickPlayTime ? DeltaTimeMultiplier : 0.0f;

	if (!FMath::IsNearlyEqual(PlayTime, NewPlayTime))
	{
		PlayTime = NewPlayTime;
		
		if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
		{
			if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
				{
					PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
				}
			}
		}
	}
}

bool FDatabaseViewModel::GetAnimationTime(int32 SourceAssetIdx, float& CurrentPlayTime, FVector& BlendParameters) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			for (const FDatabasePreviewActor& PreviewActor : GetPreviewActors())
			{
				if (PreviewActor.GetIndexAssetIndex() >= 0 && PreviewActor.GetIndexAssetIndex() < SearchIndex.Assets.Num())
				{
					const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.GetIndexAssetIndex()];
					if (IndexAsset.GetSourceAssetIdx() == SourceAssetIdx)
					{
						CurrentPlayTime = PreviewActor.GetSampler().ToNormalizedTime(PlayTime + IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate) + PreviewActor.GetPlayTimeOffset());
						BlendParameters = IndexAsset.GetBlendParameters();
						return true;
					}
				}
			}

			for (const FSearchIndexAsset& IndexAsset : SearchIndex.Assets)
			{
				if (IndexAsset.GetSourceAssetIdx() == SourceAssetIdx)
				{
					CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate);
					BlendParameters = IndexAsset.GetBlendParameters();

					const bool bIsBlendSpace = Database->GetAnimationAssetStruct(IndexAsset).GetPtr<FPoseSearchDatabaseBlendSpace>() != nullptr;
					if (bIsBlendSpace && !FMath::IsNearlyEqual(MaxPreviewPlayLength, MinPreviewPlayLength))
					{
						CurrentPlayTime = (CurrentPlayTime - MaxPreviewPlayLength) / (MaxPreviewPlayLength - MinPreviewPlayLength);
					}
					return true;
				}
			}
		}
	}

	CurrentPlayTime = 0.f;
	BlendParameters = FVector::ZeroVector;
	return false;
}
}
