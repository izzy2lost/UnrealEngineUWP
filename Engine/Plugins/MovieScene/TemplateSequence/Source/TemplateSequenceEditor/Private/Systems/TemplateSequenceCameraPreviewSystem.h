// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinkerSharedExtension.h"

#include "TemplateSequenceCameraPreviewSystem.generated.h"

namespace UE::MovieScene
{

struct FEditorViewportLinkerExtension : public TSharedEntitySystemLinkerExtension<FEditorViewportLinkerExtension>
{
	static TEntitySystemLinkerExtensionID<FEditorViewportLinkerExtension> GetExtensionID();
	static TSharedPtr<FEditorViewportLinkerExtension> GetOrCreateExtension(UMovieSceneEntitySystemLinker* Linker);

	FEditorViewportLinkerExtension(UMovieSceneEntitySystemLinker* Linker);
};

}

UCLASS(MinimalAPI)
class UTemplateSequenceCameraPreviewSystem : public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UTemplateSequenceCameraPreviewSystem(const FObjectInitializer& ObjInit);

	static void EnableNextFrame();

protected:

	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

private:

	static bool bEnableNextFrame;
};

