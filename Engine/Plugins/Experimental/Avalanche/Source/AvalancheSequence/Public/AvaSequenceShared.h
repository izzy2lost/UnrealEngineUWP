// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "AvaSequenceShared.generated.h"

class UAvaSequence;
class UMovieScene;

namespace UE::AvaSequence
{
	constexpr double SmallSubFrame = 0.00000005;
}

UENUM(BlueprintType)
enum class EAvaSequencePlayMode : uint8
{
	/** Sequence plays and loops from the beginning to the end. */
	Forward,

	/** Sequence plays and loops from the end to the beginning. */
	Reverse,
};

UENUM(BlueprintType)
enum class EAvaSequenceTimeType : uint8
{
	None UMETA(Hidden),
	Frame,
	Seconds,
	Mark,
};

USTRUCT(BlueprintType)
struct FAvaSequenceTime
{
	GENERATED_BODY()

	FAvaSequenceTime() = default;

	explicit FAvaSequenceTime(FFrameTime FrameTime)
		: TimeType(EAvaSequenceTimeType::Frame)
		, Frame(FrameTime.GetFrame().Value)
		, SubFrame(FrameTime.GetSubFrame())
	{
	}

	explicit FAvaSequenceTime(double InSeconds)
		: TimeType(EAvaSequenceTimeType::Seconds)
		, Seconds(InSeconds)
	{
	}

	explicit FAvaSequenceTime(const FString& InMarkLabel)
		: TimeType(EAvaSequenceTimeType::Mark)
		, MarkLabel(InMarkLabel)
	{
	}

	// Returns whether the provided Time is 'absolute' (i.e. should not be manipulated by whether it's playing forward/reversed) or not
	bool IsAbsoluteTime() const
	{
		return TimeType == EAvaSequenceTimeType::Mark;
	}

	AVALANCHESEQUENCE_API double ToSeconds(const UAvaSequence& InSequence, const UMovieScene& InMovieScene) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	EAvaSequenceTimeType TimeType = EAvaSequenceTimeType::Frame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence", meta=(EditCondition="TimeType==EAvaSequenceTimeType::Frame"))
	int32 Frame = 0;

	double SubFrame = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence", meta=(EditCondition="TimeType==EAvaSequenceTimeType::Seconds", Unit=s))
	double Seconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence", meta=(EditCondition="TimeType==EAvaSequenceTimeType::Mark"))
	FString MarkLabel;
};

USTRUCT(BlueprintType)
struct FAvaSequencePlayAdvancedSettings
{
	GENERATED_BODY()

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	int32 LoopCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	float PlaybackSpeed = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	bool bRestoreState = false;
};

USTRUCT(BlueprintType)
struct FAvaSequencePlayParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	FAvaSequenceTime Start = FAvaSequenceTime(0.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	FAvaSequenceTime End = FAvaSequenceTime(-1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	EAvaSequencePlayMode PlayMode = EAvaSequencePlayMode::Forward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design Sequence")
	FAvaSequencePlayAdvancedSettings AdvancedSettings;
};
