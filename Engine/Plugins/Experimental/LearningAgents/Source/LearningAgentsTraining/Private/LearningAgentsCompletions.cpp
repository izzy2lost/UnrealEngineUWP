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

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnLocationDifferenceBelowThreshold(const FVector LocationA, const FVector LocationB, const float DistanceThreshold, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(FVector::Distance(LocationA, LocationB) < DistanceThreshold, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnLocationDifferenceAboveThreshold(const FVector LocationA, const FVector LocationB, const float DistanceThreshold, const ELearningAgentsCompletion CompletionType)
{
	return CompletionOnCondition(FVector::Distance(LocationA, LocationB) > DistanceThreshold, CompletionType);
}

ELearningAgentsCompletion ULearningAgentsCompletions::CompletionOnLocationOutsideBounds(
	const FVector Location,
	const FTransform BoundsTransform,
	const FVector BoundsMins,
	const FVector BoundsMaxs,
	const ELearningAgentsCompletion CompletionType)
{
	const FVector LocalLocation = BoundsTransform.InverseTransformPosition(Location);

	return CompletionOnCondition(
		LocalLocation.X < BoundsMins.X || LocalLocation.X > BoundsMaxs.X ||
		LocalLocation.Y < BoundsMins.Y || LocalLocation.Y > BoundsMaxs.Y ||
		LocalLocation.Z < BoundsMins.Z || LocalLocation.Z > BoundsMaxs.Z,
		CompletionType);
}

