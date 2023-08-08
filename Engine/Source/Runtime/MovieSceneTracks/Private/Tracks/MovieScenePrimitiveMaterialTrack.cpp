// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieScenePrimitiveMaterialTrack)


UMovieScenePrimitiveMaterialTrack::UMovieScenePrimitiveMaterialTrack(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);
	SupportedBlendTypes.Add(EMovieSceneBlendType::Additive);

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(64,192,64,75);
#endif
}

UMovieSceneSection* UMovieScenePrimitiveMaterialTrack::CreateNewSection()
{
	return NewObject<UMovieScenePrimitiveMaterialSection>(this, NAME_None, RF_Transactional);
}

bool UMovieScenePrimitiveMaterialTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieScenePrimitiveMaterialSection::StaticClass();
}

#if WITH_EDITORONLY_DATA
void UMovieScenePrimitiveMaterialTrack::PostLoad()
{
	Super::PostLoad();
	// Backwards compatibility with MaterialIndex alone as a way to reference materials.
	if (MaterialInfo.MaterialType == EComponentMaterialType::Empty)
	{
		MaterialInfo.MaterialType = EComponentMaterialType::IndexedMaterial;
		MaterialInfo.MaterialSlotIndex = MaterialIndex_DEPRECATED;
	}
}
#endif

int32 UMovieScenePrimitiveMaterialTrack::GetMaterialIndex() const
{
	return MaterialInfo.MaterialSlotIndex;
}

void UMovieScenePrimitiveMaterialTrack::SetMaterialInfo(const FComponentMaterialInfo& InMaterialInfo)
{
	MaterialInfo = InMaterialInfo;
}

const FComponentMaterialInfo& UMovieScenePrimitiveMaterialTrack::GetMaterialInfo() const
{
	return MaterialInfo;
}

void UMovieScenePrimitiveMaterialTrack::SetMaterialIndex(int32 InMaterialIndex)
{
	MaterialInfo.MaterialSlotIndex = InMaterialIndex;
	// Assumption is if this is being called by old code, we should be indexed
	MaterialInfo.MaterialType = EComponentMaterialType::IndexedMaterial;
}