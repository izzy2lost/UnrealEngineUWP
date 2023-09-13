// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerComponent.h"
#include "MLDeformerMorphModelInputInfo.h"
#include "Components/ExternalMorphSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/MorphTargetVertexInfoBuffers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerMorphModel)

#define LOCTEXT_NAMESPACE "MLDeformerMorphModel"

UMLDeformerMorphModel::UMLDeformerMorphModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MorphTargetSet = MakeShared<FExternalMorphSet>();
	MorphTargetSet->Name = GetClass()->GetFName();
}

void UMLDeformerMorphModel::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerMorphModel::Serialize)

	Super::Serialize(Archive);

	// Check if we have initialized our compressed morph buffers.
	bool bHasMorphData = false;
	if (Archive.IsSaving())
	{
		// Strip editor only data on cook.
		if (Archive.IsCooking())
		{			
			MorphTargetDeltas.Empty();
		}

		bHasMorphData = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.IsMorphCPUDataValid() : false;
	}
	Archive << bHasMorphData;

	// Load or save the compressed morph buffers, if they exist.
	if (bHasMorphData)
	{
		check(MorphTargetSet.IsValid());
		Archive << MorphTargetSet->MorphBuffers;
	}
}

void UMLDeformerMorphModel::PostLoad()
{
	Super::PostLoad();

	// If we have an input info, but it isn't one inherited from the MorphInputInfo, try to create a new one.
	// This is because we introduced a UMLDeformerMorphModelInputInfo later on, and we want to convert old assets to use this new class.
	UMLDeformerInputInfo* CurrentInputInfo = GetInputInfo();
	if (CurrentInputInfo && !CurrentInputInfo->IsA<UMLDeformerMorphModelInputInfo>())
	{
		UMLDeformerMorphModelInputInfo* MorphInputInfo = Cast<UMLDeformerMorphModelInputInfo>(CreateInputInfo());
		MorphInputInfo->CopyMembersFrom(CurrentInputInfo);
		CurrentInputInfo->ConditionalBeginDestroy();
		check(MorphInputInfo); // The input info class should be inherited from the UMLDeformerMorphModelInputInfo class.
		SetInputInfo(MorphInputInfo);
	}

	UpdateStatistics();

#if WITH_EDITOR
	InvalidateMemUsage();
#endif
}

bool UMLDeformerMorphModel::CanDynamicallyUpdateMorphTargets() const
{
	return GetMorphTargetDeltas().Num() == (GetNumBaseMeshVerts() * GetNumMorphTargets());
}

UMLDeformerModelInstance* UMLDeformerMorphModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UMLDeformerMorphModelInstance>(Component);
}

void UMLDeformerMorphModel::SetMorphTargetDeltaFloats(const TArray<float>& Deltas)
{
	FloatArrayToVector3Array(Deltas, MorphTargetDeltas);
}

void UMLDeformerMorphModel::SetMorphTargetDeltas(const TArray<FVector3f>& Deltas)
{
	MorphTargetDeltas = Deltas;
}

int32 UMLDeformerMorphModel::GetMorphTargetDeltaStartIndex(int32 BlendShapeIndex) const
{
	if (MorphTargetDeltas.Num() == 0)
	{
		return INDEX_NONE;
	}

	return GetNumBaseMeshVerts() * BlendShapeIndex;
}

void UMLDeformerMorphModel::BeginDestroy()
{
	if (MorphTargetSet)
	{
		BeginReleaseResource(&MorphTargetSet->MorphBuffers);
		RenderCommandFence.BeginFence();
	}
	Super::BeginDestroy();
}

bool UMLDeformerMorphModel::IsReadyForFinishDestroy()
{
	// Wait for associated render resources to be released.
	return Super::IsReadyForFinishDestroy() && RenderCommandFence.IsFenceComplete();
}

void UMLDeformerMorphModel::SetMorphTargetsErrorOrder(const TArray<int32>& MorphTargetOrder, const TArray<float>& ErrorValues)
{
	MorphTargetErrorOrder = MorphTargetOrder;
	MorphTargetErrors = ErrorValues;
}

void UMLDeformerMorphModel::UpdateStatistics()
{
	NumMorphTargets = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.GetNumMorphs() : 0;
	CompressedMorphDataSizeInBytes = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.GetMorphDataSizeInBytes() : 0;
	UncompressedMorphDataSizeInBytes = GetMorphTargetDeltas().Num() * GetMorphTargetDeltas().GetTypeSize();
}

void UMLDeformerMorphModel::SetMorphTargetsMinMaxWeights(const TArray<FFloatInterval>& MinMaxValues)
{
	MorphTargetsMinMaxWeights = MinMaxValues;
}

void UMLDeformerMorphModel::SetMorphTargetsMinMaxWeights(const TArray<float>& MinValues, const TArray<float>& MaxValues)
{
	check(MinValues.Num() == MaxValues.Num());
	const int32 NumWeights = MinValues.Num();

	MorphTargetsMinMaxWeights.Reset();
	MorphTargetsMinMaxWeights.AddUninitialized(NumWeights);
	for (int32 Index = 0; Index < MinValues.Num(); ++Index)
	{
		MorphTargetsMinMaxWeights[Index].Min = MinValues[Index];
		MorphTargetsMinMaxWeights[Index].Max = MaxValues[Index];
	}
}

void UMLDeformerMorphModel::ClampMorphTargetWeights(TArrayView<float> WeightsArray)
{
	if (MorphTargetsMinMaxWeights.Num() != WeightsArray.Num())
	{
		return;
	}

	for (int32 MorphIndex = 0; MorphIndex < WeightsArray.Num(); ++MorphIndex)
	{
		const FFloatInterval& MorphMinMax = MorphTargetsMinMaxWeights[MorphIndex];
		WeightsArray[MorphIndex] = FMath::Clamp(WeightsArray[MorphIndex], MorphMinMax.Min, MorphMinMax.Max);;
	}
}

TArrayView<const float> UMLDeformerMorphModel::GetMorphTargetErrorValues() const
{
	return MorphTargetErrors;
}

TArrayView<const int32> UMLDeformerMorphModel::GetMorphTargetErrorOrder() const
{ 
	return MorphTargetErrorOrder;
}

TArrayView<const FMLDeformerMorphModelQualityLevel> UMLDeformerMorphModel::GetQualityLevels() const
{
	return QualityLevels;
}

TArray<FMLDeformerMorphModelQualityLevel>& UMLDeformerMorphModel::GetQualityLevelsArray()
{
	return QualityLevels;
}

float UMLDeformerMorphModel::GetMorphTargetError(int32 MorphIndex) const
{ 
	return MorphTargetErrors[MorphIndex];
}

void UMLDeformerMorphModel::SetMorphTargetError(int32 MorphIndex, float Error)
{ 
	MorphTargetErrors[MorphIndex] = Error;
}

int32 UMLDeformerMorphModel::GetNumActiveMorphs(int32 QualityLevel) const
{
	if (QualityLevels.IsEmpty() || MorphTargetErrorOrder.IsEmpty() || MorphTargetErrors.IsEmpty())
	{
		return FMath::Max<int32>(0, NumMorphTargets - 1);	// -1 Because this number includes the means morph target.
	}

	const int32 ClampedQualityLevel = FMath::Clamp<int32>(QualityLevel, 0, QualityLevels.Num() - 1);
	const int32 NumActiveMorphs = FMath::Clamp<int32>(QualityLevels[ClampedQualityLevel].GetMaxActiveMorphs(), 0, NumMorphTargets - 1);	// -1 Because we want to exclude the means morph target.
	return NumActiveMorphs;
}

#if WITH_EDITOR
void UMLDeformerMorphModel::UpdateMemoryUsage()
{
	Super::UpdateMemoryUsage();

	// Remove the raw uncompressed deltas from the cooked size and memory usage, as they are stripped during cook.
	// This means the game itself won't have this data in the asset or memory.
	CookedAssetSizeInBytes -= UncompressedMorphDataSizeInBytes;
	MemUsageInBytes -= UncompressedMorphDataSizeInBytes;

	// Add the compressed morph target data size.
	// We add this to both the GPU memory, and the cooked asset size.
	// The morph targets are stored in a compressed way inside the asset.
	const uint64 GPUMorphSize = GetCompressedMorphDataSizeInBytes();
	GPUMemUsageInBytes += GPUMorphSize;
	CookedAssetSizeInBytes += GPUMorphSize;
}
#endif

#undef LOCTEXT_NAMESPACE

