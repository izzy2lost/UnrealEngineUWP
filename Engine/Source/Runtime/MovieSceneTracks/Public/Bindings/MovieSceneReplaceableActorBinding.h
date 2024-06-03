// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MovieSceneReplaceableBinding.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "MovieSceneReplaceableActorBinding.generated.h"

/*
* An implementation of UMovieSceneReplaceableBindingBase that uses UMovieSceneSpawnableActorBinding as the preview spawnable,
* and has no implementation of ResolveRuntimeBindingInternal, relying instead of Sequencer's built in BindingOverride mechanism for binding at runtime.
*/
UCLASS(BlueprintType, MinimalAPI, EditInlineNew, DefaultToInstanced, Meta=(DisplayName="Replaceable Actor"))
class UMovieSceneReplaceableActorBinding
	: public UMovieSceneReplaceableBindingBase
{
public:

	GENERATED_BODY()

public:

	/* MovieSceneCustomBinding overrides*/
	/* Note that we specifically don't implement CreateCustomBinding here- it's implemented in the base class and separately calls
	 *	CreateInnerSpawnable and InitReplaceableBinding which we implement here (though InitReplaceableBinding has an empty implementation in this class). 
	 */
#if WITH_EDITOR
	FText GetBindingTypePrettyName() const override;
#endif

protected:
	/* MovieSceneReplaceableBindingBase overrides*/

	// By default we return nullptr here, as we rely on Sequencer's BindingOverride mechanism to bind these actors during runtime.
	// This can be overridden if desired in subclasses to provide a different way to resolve to an actor at runtime while still using spawnable actor as the preview.
	MOVIESCENETRACKS_API virtual FMovieSceneBindingResolveResult ResolveRuntimeBindingInternal(const FMovieSceneBindingResolveParams& ResolveParams, int32 BindingIndex, TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState) const override { return FMovieSceneBindingResolveResult(); }
	
	// Empty implementation by default as we don't need to initialize any data members other than the spawnable,which is initialized by CreateInnerSpawnable above
	MOVIESCENETRACKS_API virtual void InitReplaceableBinding(UObject* SourceObject, UMovieScene& OwnerMovieScene) override {}

	MOVIESCENETRACKS_API virtual TSubclassOf<UMovieSceneSpawnableBindingBase> GetInnerSpawnableClass() const override;

	virtual int32 GetCustomBindingPriority() const override { return 9; }

};

