// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneBindingLifetimeSection.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"

#include "EntitySystem/MovieSceneEntityBuilder.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Evaluation/MovieSceneEvaluationField.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingLifetimeSection)

UMovieSceneBindingLifetimeSection::UMovieSceneBindingLifetimeSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	bSupportsInfiniteRange = true;
}

void UMovieSceneBindingLifetimeSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.AddConditional(BuiltInComponentTypes->BindingLifetime, FMovieSceneBindingLifetimeComponentData{ Params.GetObjectBindingID(), EMovieSceneBindingLifetimeState::Active }, Params.GetObjectBindingID().IsValid())
	);
}

bool UMovieSceneBindingLifetimeSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	// By default, Binding Lifetime sections do not populate any evaluation field entries
	// that is the job of its outer UMovieSceneTrack through a call to ExternalPopulateEvaluationField
	return true;
}

void UMovieSceneBindingLifetimeSection::ExternalPopulateEvaluationField(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	using namespace UE::MovieScene;

	const int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);

	const int32 EntityIndex = OutFieldBuilder->FindOrAddEntity(this, 1);
	OutFieldBuilder->AddPersistentEntity(EffectiveRange, EntityIndex, MetaDataIndex);
}

