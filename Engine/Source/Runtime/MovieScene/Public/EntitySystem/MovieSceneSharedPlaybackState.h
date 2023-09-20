// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"

namespace UE::MovieScene
{

struct FSharedPlaybackState
{
	/** Playback capabilities for the root sequence */
	FPlaybackCapabilities Capabilities;

public:

	template<typename T>
	T* FindCapability() const
	{
		return Capabilities.FindCapability<T>(T::ID);
	}

	template<typename T, typename ...ArgTypes>
	T& AddCapability(ArgTypes&&... InArgs)
	{
		return Capabilities.AddCapability<T>(T::ID, Forward<ArgTypes>(InArgs)...);
	}

	template<typename T, typename ...ArgTypes>
	T& AddCapabilityRaw(T* InPointer)
	{
		return Capabilities.AddCapabilityRaw<T>(T::ID, InPointer);
	}

	template<typename T, typename ...ArgTypes>
	T& AddCapabilityShared(TSharedRef<T> InSharedRef)
	{
		return Capabilities.AddCapabilityShared<T>(T::ID, InSharedRef);
	}

public:

	void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker)
	{
		Capabilities.InvalidateCachedData(Linker);
	}
};

} // namespace UE::MovieScene

