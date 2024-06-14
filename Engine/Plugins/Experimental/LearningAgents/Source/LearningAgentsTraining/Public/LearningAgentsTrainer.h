// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningTrainer.h"

#include "LearningAgentsTrainer.generated.h"

/** Enumeration of the training devices. */
UENUM(BlueprintType, Category = "LearningAgents")
enum class ELearningAgentsTrainingDevice : uint8
{
	CPU,
	GPU,
};

namespace UE::Learning::Agents
{
	/** Get the learning agents trainer device from the UE::Learning trainer device. */
	LEARNINGAGENTSTRAINING_API ELearningAgentsTrainingDevice GetLearningAgentsTrainingDevice(const ETrainerDevice Device);

	/** Get the UE::Learning trainer device from the learning agents trainer device. */
	LEARNINGAGENTSTRAINING_API ETrainerDevice GetTrainingDevice(const ELearningAgentsTrainingDevice Device);
}

/** The path settings for the trainer. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsTrainerProcessSettings
{
	GENERATED_BODY()

public:

	FLearningAgentsTrainerProcessSettings();

	/** The relative path to the engine for editor builds. Defaults to FPaths::EngineDir. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FDirectoryPath EditorEngineRelativePath;

	/**
	 * The relative path to the editor engine folder for non-editor builds.
	 *
	 * If we want to run training in cooked, non-editor builds, then by default we wont have access to python and the
	 * LearningAgents training scripts - these are editor-only things and are stripped during the cooking process.
	 *
	 * However, running training in non-editor builds can be very important - we probably want to disable rendering
	 * and sound while we are training to make experience gathering as fast as possible - and for any non-trivial game
	 * is simply may not be realistic to run it for a long time in play-in-editor.
	 *
	 * For this reason even in non-editor builds we let you provide the path where all of these editor-only things can
	 * be found. This allows you to run training when these things actually exist somewhere accessible to the executable,
	 * which will usually be the case on a normal development machine or cloud machine if it is set up that way.
	 *
	 * Since non-editor builds can be produced in a number of different ways, this is not set by default and cannot
	 * use a directory picker since it is relative to the final location of where your cooked, non-editor executable
	 * will exist rather than the current with-editor executable.
	 */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	FString NonEditorEngineRelativePath;

	/** The relative path to the Intermediate directory. Defaults to FPaths::ProjectIntermediateDir. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FDirectoryPath EditorIntermediateRelativePath;

	/** The relative path to the intermediate folder for non-editor builds. */
	UPROPERTY(EditAnywhere, Category = "LearningAgents")
	FString NonEditorIntermediateRelativePath;

public:

	/** Gets the Relative Editor Engine Path accounting for if this is an editor build or not  */
	FString GetEditorEnginePath() const;

	/** Gets the Relative Intermediate Path  */
	FString GetIntermediatePath() const;
};
