// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::ConcertTrace::Private
{
	struct FConcertReplicationSequenceContext
	{
		FSoftObjectPath Object;
		int32 FrameNumber;

		FString ToString() const { return Object.ToString() + FString::FromInt(FrameNumber); }
	};
}