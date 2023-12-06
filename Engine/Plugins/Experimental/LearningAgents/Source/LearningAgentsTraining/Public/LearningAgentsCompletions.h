// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsCompletions.generated.h"

namespace UE::Learning
{
	enum class ECompletionMode : uint8;
}

/** Completion modes for episodes. */
UENUM(BlueprintType, Category = "LearningAgents", meta = (ScriptName = "LearningAgentsCompletionEnum"))
enum class ELearningAgentsCompletion : uint8
{
	/** Episode is still running. */
	Running 	UMETA(DisplayName = "Running"),

	/** Episode ended while in progress. Critic will be used to estimate final return. */
	Truncation	UMETA(DisplayName = "Truncation"),

	/** Episode ended and zero reward was expected for all future steps. */
	Termination	UMETA(DisplayName = "Termination"),
};

namespace UE::Learning::Agents
{
	/** Get the learning agents completion from the UE::Learning completion. */
	LEARNINGAGENTSTRAINING_API ELearningAgentsCompletion GetLearningAgentsCompletion(const ECompletionMode CompletionMode);

	/** Get the UE::Learning completion from the learning agents completion. */
	LEARNINGAGENTSTRAINING_API ECompletionMode GetCompletionMode(const ELearningAgentsCompletion Completion);
}

UCLASS(BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsCompletions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static bool CompletionIsRunning(const ELearningAgentsCompletion Completion);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static bool CompletionIsCompleted(const ELearningAgentsCompletion Completion);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static bool CompletionIsTruncation(const ELearningAgentsCompletion Completion);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static bool CompletionIsTermination(const ELearningAgentsCompletion Completion);

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (CommutativeAssociativeBinaryOperator))
	static ELearningAgentsCompletion CompletionOr(ELearningAgentsCompletion A, ELearningAgentsCompletion B);

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (CommutativeAssociativeBinaryOperator))
	static ELearningAgentsCompletion CompletionAnd(ELearningAgentsCompletion A, ELearningAgentsCompletion B);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionNot(const ELearningAgentsCompletion A, const ELearningAgentsCompletion NotRunningType = ELearningAgentsCompletion::Termination);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnCondition(const bool bCondition, const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Termination);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnTimeElapsed(const float Time, const float TimeThreshold = 10.0f, const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Truncation);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnEpisodeStepsRecorded(const int32 EpisodeSteps, const int32 MaxEpisodeSteps = 64, const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Truncation);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnTranslationDifferenceBelowThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold = 100.0f, const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Termination);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnTranslationDifferenceAboveThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold = 100.0f, const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Termination);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	static ELearningAgentsCompletion CompletionOnTranslationOutsideBounds(
		const FVector Translation, 
		const FTransform BoundsTransform = FTransform(),
		const FVector BoundsMins = FVector(-100.0f, -100.0f, -100.0f),
		const FVector BoundsMaxs = FVector(+100.0f, +100.0f, +100.0f),
		const ELearningAgentsCompletion CompletionType = ELearningAgentsCompletion::Termination);
};
