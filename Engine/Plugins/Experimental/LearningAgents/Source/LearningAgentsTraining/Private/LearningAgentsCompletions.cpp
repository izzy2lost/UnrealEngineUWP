// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCompletions.h"

#include "LearningCompletion.h"
#include "LearningLog.h"

namespace UE::Learning::Agents
{
	ELearningAgentsCompletion GetLearningAgentsCompletion(const ECompletionMode CompletionMode)
	{
		switch (CompletionMode)
		{
		case ECompletionMode::Running: return ELearningAgentsCompletion::Running;
		case ECompletionMode::Terminated: return ELearningAgentsCompletion::Termination;
		case ECompletionMode::Truncated: return ELearningAgentsCompletion::Truncation;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Completion Mode.")); return ELearningAgentsCompletion::Running;
		}
	}

	ECompletionMode GetCompletionMode(const ELearningAgentsCompletion Completion)
	{
		switch (Completion)
		{
		case ELearningAgentsCompletion::Running: return ECompletionMode::Running;
		case ELearningAgentsCompletion::Termination: return ECompletionMode::Terminated;
		case ELearningAgentsCompletion::Truncation: return ECompletionMode::Truncated;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Completion.")); return ECompletionMode::Running;
		}
	}
}

bool ULearningAgentsCompletions::CompletionIsRunning(const ELearningAgentsCompletion Completion)
{
	return Completion == ELearningAgentsCompletion::Running;
}

bool ULearningAgentsCompletions::CompletionIsCompleted(const ELearningAgentsCompletion Completion)
{
	return Completion == ELearningAgentsCompletion::Truncation || Completion == ELearningAgentsCompletion::Termination;
}

bool ULearningAgentsCompletions::CompletionIsTruncation(const ELearningAgentsCompletion Completion)
{
	return Completion == ELearningAgentsCompletion::Truncation;
}

bool ULearningAgentsCompletions::CompletionIsTermination(const ELearningAgentsCompletion Completion)
{
	return Completion == ELearningAgentsCompletion::Termination;
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOr(ELearningAgentsCompletion A, ELearningAgentsCompletion B)
{
	if ((A == ELearningAgentsCompletion::Running && B != ELearningAgentsCompletion::Running) ||
		(A == ELearningAgentsCompletion::Truncation && B == ELearningAgentsCompletion::Termination))
	{
		return B;
	}
	else
	{
		return A;
	}
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionAnd(ELearningAgentsCompletion A, ELearningAgentsCompletion B)
{
	if (A == ELearningAgentsCompletion::Running ||
		(A == ELearningAgentsCompletion::Truncation && B != ELearningAgentsCompletion::Running))
	{
		return A;
	}
	else
	{
		return B;
	}
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionNot(const ELearningAgentsCompletion A, const ELearningAgentsCompletion NotRunningType)
{
	return A == ELearningAgentsCompletion::Running ? NotRunningType : ELearningAgentsCompletion::Running;
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnCondition(const bool bCondition, const ELearningAgentsCompletion CompletionType)
{
	return bCondition ? CompletionType : ELearningAgentsCompletion::Running;
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnTimeElapsed(const float Time, const float TimeThreshold, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(Time > TimeThreshold, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnEpisodeStepsRecorded(const int32 EpisodeSteps, const int32 MaxEpisodeSteps, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(EpisodeSteps >= MaxEpisodeSteps, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnTranslationDifferenceBelowThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(FVector::Distance(TranslationA, TranslationB) < DistanceThreshold, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnTranslationDifferenceAboveThreshold(const FVector TranslationA, const FVector TranslationB, const float DistanceThreshold, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(FVector::Distance(TranslationA, TranslationB) > DistanceThreshold, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnTranslationOutsideBounds(
	const FVector Translation,
	const FTransform BoundsTransform,
	const FVector BoundsMins,
	const FVector BoundsMaxs,
	const ELearningAgentsCompletion CompletionType)
{
	const FVector LocalTranslation = BoundsTransform.InverseTransformPosition(Translation);

	return CompletionOnCondition(
		LocalTranslation.X < BoundsMins.X || LocalTranslation.X > BoundsMaxs.X ||
		LocalTranslation.Y < BoundsMins.Y || LocalTranslation.Y > BoundsMaxs.Y ||
		LocalTranslation.Z < BoundsMins.Z || LocalTranslation.Z > BoundsMaxs.Z,
		CompletionType);
}

