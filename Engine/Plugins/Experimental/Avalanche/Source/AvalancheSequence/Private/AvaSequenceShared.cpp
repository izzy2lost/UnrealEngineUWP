// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceShared.h"
#include "AvaSequence.h"

double FAvaSequenceTime::ToSeconds(const UAvaSequence& InSequence, const UMovieScene& InMovieScene) const
{
	switch (TimeType)
	{
	case EAvaSequenceTimeType::Frame:
		return InMovieScene.GetDisplayRate().AsSeconds(FFrameTime(Frame, SubFrame));

	case EAvaSequenceTimeType::Seconds:
		return Seconds;

	case EAvaSequenceTimeType::Mark:
		{
			FAvaMark Mark;
			if (InSequence.GetMark(MarkLabel, Mark) && !Mark.Frames.IsEmpty())
			{
				return InMovieScene.GetTickResolution().AsSeconds(Mark.Frames[0]);
			}
		}
	}
	return -1.0;
}
