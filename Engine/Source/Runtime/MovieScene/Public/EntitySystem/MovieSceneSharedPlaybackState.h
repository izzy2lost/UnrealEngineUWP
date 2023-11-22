// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/MovieSceneCompiledDataID.h"
#include "CoreTypes.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"
#include "Evaluation/MovieScenePlaybackCapabilities.h"
#include "MovieSceneSequenceID.h"

class FMovieSceneEntitySystemRunner;
class UMovieSceneCompiledDataManager;
struct FMovieSceneObjectCache;
struct FMovieSceneSequenceHierarchy;

namespace UE::MovieScene
{

/**
 * Parameter structure for initializing a new shared playback state.
 */
struct FSharedPlaybackStateCreateParams
{
	/**
	 * The playback context in which the root sequence will be evaluated.
	 *
	 * Requires that RootInstanceHandle and Runner are also set.
	 */
	UObject* PlaybackContext = nullptr;

	/**
	 * The handle of the root sequence instance, if the created playback state
	 * is meant to relate to an instance that has also been created inside
	 * a runner/linker's instance registry.
	 *
	 * Requires that PlaybackContext and Runner are also set.
	 */
	FRootInstanceHandle RootInstanceHandle;

	/**
	 * The runner that will be evaluating the sequence that the created playback
	 * state relates to.
	 *
	 * Requires that PlaybackContext and RootInstanceHandle are also set.
	 */
	TSharedPtr<FMovieSceneEntitySystemRunner> Runner;

	/**
	 * The compiled data manager with which the root sequence was compiled, or
	 * will be compiled. If unset, the default global manager will be used.
	 */
	UMovieSceneCompiledDataManager* CompiledDataManager = nullptr;
};

/**
 * A structure that stores playback state for an entire sequence hierarchy.
 */
struct FSharedPlaybackState : TSharedFromThis<FSharedPlaybackState>
{
public:

	FSharedPlaybackState();
	FSharedPlaybackState(
			UMovieSceneSequence& InRootSequence,
			const FSharedPlaybackStateCreateParams& CreateParams);

public:
	
	/** Gets the playback context */
	UObject* GetPlaybackContext() const { return WeakPlaybackContext.Get(); }

	/** Gets the root sequence */
	UMovieSceneSequence* GetRootSequence() const { return WeakRootSequence.Get(); }

	/** Gets the runner evaluating this root sequence */
	TSharedPtr<FMovieSceneEntitySystemRunner> GetRunner() const { return WeakRunner.Pin(); }

	/** Gets the compiled data manager that contains the data for the root sequence */
	TObjectPtr<UMovieSceneCompiledDataManager> GetCompiledDataManager() const { return CompiledDataManager; }

	/** Gets the handle of the root sequence */
	const FRootInstanceHandle& GetRootInstanceHandle() const { return RootInstanceHandle; }

	/** Gets the compiled data ID for the root sequence */
	const FMovieSceneCompiledDataID&  GetRootCompiledDataID() const { return RootCompiledDataID; }

public:

	// General utility methods

	MOVIESCENE_API UMovieSceneEntitySystemLinker* GetLinker() const;

	MOVIESCENE_API const FMovieSceneSequenceHierarchy* GetHierarchy() const;
	MOVIESCENE_API UMovieSceneSequence* GetSequence(FMovieSceneSequenceIDRef SequenceID) const;

public:

	/**
	 * Gets the capabilities container.
	 */
	FPlaybackCapabilities& GetCapabilities() { return Capabilities; }

	/**
	 * Gets the capabilities container.
	 */
	const FPlaybackCapabilities& GetCapabilities() const { return Capabilities; }

	/**
	 * Returns whether the root sequence has the specified capability.
	 */
	template<typename T>
	bool HasCapability() const
	{
		return Capabilities.HasCapability(T::ID);
	}

	/**
	 * Finds the specified capability on the root sequence.
	 */
	template<typename T>
	T* FindCapability() const
	{
		return Capabilities.FindCapability<T>(T::ID);
	}

	/**
	 * Builds the specified capability for the root sequence.
	 */
	template<typename T, typename ...ArgTypes>
	T& AddCapability(ArgTypes&&... InArgs)
	{
		T& Cap = Capabilities.AddCapability<T>(T::ID, Forward<ArgTypes>(InArgs)...);
		MaybeInitialize(Cap);
		return Cap;
	}

	/**
	 * Adds the specified capability on the root sequence as a raw pointer.
	 */
	template<typename T, typename ...ArgTypes>
	T& AddCapabilityRaw(T* InPointer)
	{
		T& Cap = Capabilities.AddCapabilityRaw<T>(T::ID, InPointer);
		MaybeInitialize(Cap);
		return Cap;
	}

	/**
	 * Adds the specified capability on the root sequence as a shared pointer.
	 */
	template<typename T, typename ...ArgTypes>
	T& AddCapabilityShared(TSharedRef<T> InSharedRef)
	{
		T& Cap = Capabilities.AddCapabilityShared<T>(T::ID, InSharedRef);
		MaybeInitialize(Cap);
		return Cap;
	}

	/**
	 * Adds the specified capability on the root sequence as a raw pointer.
	 * If the capability already exists, it must be stored as a raw pointer and its
	 * value will be replaced by the new pointer.
	 */
	template<typename T, typename ...ArgTypes>
	T& SetOrAddCapabilityRaw(T* InPointer)
	{
		if (HasCapability<T>())
		{
			T& Cap = Capabilities.OverwriteCapabilityRaw<T>(T::ID, InPointer);
			MaybeInitialize(Cap);
			return Cap;
		}
		else
		{
			T& Cap = Capabilities.AddCapabilityRaw<T>(T::ID, InPointer);
			MaybeInitialize(Cap);
			return Cap;
		}
	}

	/**
	 * Adds the specified capability on the root sequence as a shared pointer.
	 * If the capability already exists, it must be stored as a shared pointer and its
	 * value will be replaced by the new pointer.
	 */
	template<typename T, typename ...ArgTypes>
	T& SetOrAddCapabilityShared(TSharedRef<T> InSharedRef)
	{
		if (HasCapability<T>())
		{
			T& Cap = Capabilities.OverwriteCapabilityShared<T>(T::ID, InSharedRef);
			MaybeInitialize(Cap);
			return Cap;
		}
		else
		{
			T& Cap = Capabilities.AddCapabilityShared<T>(T::ID, InSharedRef);
			MaybeInitialize(Cap);
			return Cap;
		}
	}

public:

	void InvalidateCachedData();

private:

	template<typename T>
	void MaybeInitialize(T& Cap)
	{
		if constexpr (TPointerIsConvertibleFromTo<T, IPlaybackCapability>::Value)
		{
			IPlaybackCapability* InterfacePtr = static_cast<IPlaybackCapability*>(&Cap);
			InterfacePtr->Initialize(SharedThis(this));
		}
	}

private:

	/** The root sequence */
	TWeakObjectPtr<UMovieSceneSequence> WeakRootSequence;

	/** The playback context */
	TWeakObjectPtr<UObject> WeakPlaybackContext;

	/** The runner evaluating this root sequence */
	TWeakPtr<FMovieSceneEntitySystemRunner> WeakRunner;

	/** The compiled data manager that contains the data for the root sequence */
	TObjectPtr<UMovieSceneCompiledDataManager> CompiledDataManager;

	/** The handle of the root sequence */
	FRootInstanceHandle RootInstanceHandle;

	/** The compiled data ID for the root sequence */
	FMovieSceneCompiledDataID  RootCompiledDataID;

	/** Playback capabilities for the root sequence */
	FPlaybackCapabilities Capabilities;
};

} // namespace UE::MovieScene

