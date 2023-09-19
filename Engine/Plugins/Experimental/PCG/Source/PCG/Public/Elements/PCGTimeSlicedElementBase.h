// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"

/**
 * Rundown for utilizing the Time Slice Element and Context:
 * 
 * Create a struct to contain the state data for the element that will contain "per-execution" state data to be calculated only once
 * Create a struct to contain the state data for the element that will contain "per-iteration" state data for each item (usually validated inputs) to iterate through
 * Note: Either struct is optional and can be substituted with an empty struct if that aspect of the element is stateless
 * Override the element type with TTimeSlicedPCGElement with two template arguments, the first being the static struct,and the second being the "per-iteration"
 * Pass a function or lambda matching the correct signature to InitializePerExecutionState and initialize the static struct within
 * Do the same for InitializePerIterationStates, but also pass the number of iterations. This will iterate through a state initialization for each iteration
 * [Optional] If needed, mark UObjects that need to bypass GC with RootAndTrackObject or RootAndTrackObjectByName if you need to retrieve it later. The objects will be garbage collected at the end of the context lifetime
 * DataIsPrepared SHOULD BE used to verify initialization was successful, such as in the ExecuteInternal if data was previously initialized in PrepareDataInternal
 * Call ExecuteSlice with an execution function or lambda that returns a boolean that is true once the full execution is completed, or false otherwise
 *
 * Note: See PCGSurfaceSampler.h/.cpp as an example.
 */

// Forward declaration for friending
template <typename PerExecutionStateT, typename PerIterationStateT> class TPCGTimeSlicedElementBase;

namespace PCGTimeSlice
{
	struct FEmptyStruct {};
}

/**
 * A PCG context with helper utility to enable element authors to more easily implement timeslicing.
 * @tparam PerExecutionStateT Struct type of the "per-execution" static data state
 * @tparam PerIterationStateT Struct type of the "per-iteration" data state
 */
template <typename PerExecutionStateT = PCGTimeSlice::FEmptyStruct, typename PerIterationStateT = PCGTimeSlice::FEmptyStruct>
struct TPCGTimeSlicedContext : public FPCGContext
{
	virtual ~TPCGTimeSlicedContext() override;

	virtual bool TimeSliceIsEnabled() const override final { return bTimeSliceIsEnabled; }
	void SetTimeSliceIsEnabled(const bool bEnableTimeSlice = true) { bTimeSliceIsEnabled = bEnableTimeSlice; }

	/** Retrieves the number of times this context was executed */
	uint32 GetExecutionCount() const { return ExecutionCount; }

	using InitSignature = bool(TPCGTimeSlicedContext* Context, PerExecutionStateT& OutState);

	/** Initializes per execution state data if required. Returns true if data is completely initialized properly and false if a problem should result in early termination. */
	bool InitializePerExecutionState(TFunctionRef<InitSignature> InitFunc = []{ return true; });

	using IterSignature = bool(PerIterationStateT& OutState, const uint32 IterationIndex);

	/** Initializes per execution state data if required. An array will be created with a state element for every execution iteration in the context. */
	bool InitializePerIterationStates(int32 NumIterations = 1, TFunctionRef<IterSignature> IterFunc = []{ return true; });

	PerExecutionStateT& GetPerExecutionState() { return PerExecutionStateData; }
	const PerExecutionStateT& GetPerExecutionState() const { return PerExecutionStateData; }

	TArray<PerIterationStateT>& GetPerIterationStateArray() { return PerIterationStateArray; }
	const TArray<PerIterationStateT>& GetPerIterationStateArray() const { return PerIterationStateArray; }

	/** Returns true if both the execution state and iteration state were fully initialized. Can be used to bypass any further initialization. */
	bool DataIsPrepared() const { return bPerExecutionStateIsInitialized && bPerIterationStateIsInitialized; }

	/** Fire and forget function to root a UObject for the duration of the Context, and then mark for garbage collection at context lifecycle end. */
	void RootAndTrackObject(UObject* Object);

private:
	// Allow exposure to iteration index, etc
	friend class TPCGTimeSlicedElementBase<PerExecutionStateT, PerIterationStateT>;

	/** The number of times this context has been time sliced */
	uint32 ExecutionCount = 0u;
	/** The index of which iteration being processed. Ie. If a volume sampler has two volume inputs, it will be processed twice. */
	int32 IterationIndex = 0;

	bool bTimeSliceIsEnabled = true;
	bool bPerExecutionStateIsInitialized = false;
	bool bPerIterationStateIsInitialized = false;

	/** The state of the timesliced context that won't change each iteration. Ie. the node settings. */
	PerExecutionStateT PerExecutionStateData;
	/** An array of the various states of timesliced context that will change per iteration. Ie. generating shape */
	TArray<PerIterationStateT> PerIterationStateArray;

	/** Tracks rooted UObjects that need to avoid garbage collection during the lifetime of the context */
	TArray<UObject*> RootedAndTrackedObjectArray;
};

/**
 * A PCG Element that will utilize a Time Slice Context.
 * @tparam PerExecutionStateT Struct type of the "per-execution" static data state
 * @tparam PerIterationStateT Struct type of the "per-iteration" data state
 */
template <typename PerExecutionStateT, typename PerIterationStateT>
class TPCGTimeSlicedElementBase : public FSimplePCGElement
{
public:
	// Aliases, for ease of use with the template
	using ExecStateType = PerExecutionStateT;
	using IterStateType = PerIterationStateT;
	using ContextType = TPCGTimeSlicedContext<ExecStateType, IterStateType>;

	virtual FPCGContext* CreateContext() override { return new ContextType(); }

	using ExecSignature = bool(ContextType* Context, const PerExecutionStateT& PerExecutionState, const PerIterationStateT& PerIterationState, const uint32 IterationIndex);

	/** Executes the delegate for every iteration. Will return false while still processing and true only when all tasks for all iterations are complete. */
	bool ExecuteSlice(ContextType* Context, TFunctionRef<ExecSignature> ExecFunc) const;

	// TODO: [FUTURE WORK] Consider callback 'on completion' and other Execution styles, like parallel iterations
};

template <typename PerExecutionStateT, typename PerIterationStateT>
TPCGTimeSlicedContext<PerExecutionStateT, PerIterationStateT>::~TPCGTimeSlicedContext()
{
	for (UObject* Object : RootedAndTrackedObjectArray)
	{
		// Sanity check
		check(Object);
		if (!ensure(IsValid(Object) && Object->IsRooted()))
		{
			continue;
		}

		Object->RemoveFromRoot();
		Object->MarkAsGarbage();
	}

	RootedAndTrackedObjectArray.Empty();
}

template <typename PerExecutionStateT, typename PerIterationStateT>
bool TPCGTimeSlicedContext<PerExecutionStateT, PerIterationStateT>::InitializePerExecutionState(TFunctionRef<InitSignature> InitFunc)
{
	// Should only ever be initialized once, so if its called again, ignore it. This allows flexibility for the call to be somewhere that might be invoked numerous times
	if (!bPerExecutionStateIsInitialized)
	{
		bPerExecutionStateIsInitialized = InitFunc(this, PerExecutionStateData);
	}

	return bPerExecutionStateIsInitialized;
}

template <typename PerExecutionStateT, typename PerIterationStateT>
bool TPCGTimeSlicedContext<PerExecutionStateT, PerIterationStateT>::InitializePerIterationStates(int32 NumIterations, TFunctionRef<IterSignature> IterFunc)
{
	// Same as InitializePerExecutionState. Should only ever be initialized once, so if its called again, ignore it.
	if (bPerIterationStateIsInitialized)
	{
		return true;
	}

	bPerIterationStateIsInitialized = true;

	// An empty iteration state is still valid
	if (NumIterations < 1)
	{
		return true;
	}

	bool bAnySucceeded{false};
	PerIterationStateArray.Reserve(NumIterations);

	for (int32 I = 0; I < NumIterations; ++I)
	{
		const bool bCurrentSucceeded = IterFunc(PerIterationStateArray.Emplace_GetRef(), I);
		bAnySucceeded |= bCurrentSucceeded;

		if (!bCurrentSucceeded)
		{
			// If it fails, remove the new state from the array and continue
			PerIterationStateArray.RemoveAt(PerIterationStateArray.Num() - 1);
		}
	}

	// Returns true if any succeeded
	return bAnySucceeded;
}

template <typename PerExecutionStateT, typename PerIterationStateT>
void TPCGTimeSlicedContext<PerExecutionStateT, PerIterationStateT>::RootAndTrackObject(UObject* Object)
{
	check(Object && IsValid(Object) && !Object->IsRooted());
	Object->AddToRoot();
	RootedAndTrackedObjectArray.AddUnique(Object);
}

template <typename PerExecutionStateT, typename PerIterationStateT>
bool TPCGTimeSlicedElementBase<PerExecutionStateT, PerIterationStateT>::ExecuteSlice(ContextType* Context, TFunctionRef<ExecSignature> ExecFunc) const
{
	++Context->ExecutionCount;

	// The user is responsible to check for this before execution, but just in case
	if (!ensureMsgf(Context->DataIsPrepared() && !Context->PerIterationStateArray.IsEmpty(), TEXT("State data was not properly initialized.")))
	{
		return true;
	}

	do // The elements must execute at least once
	{
		if (ExecFunc(Context, Context->PerExecutionStateData, Context->PerIterationStateArray[Context->IterationIndex], Context->IterationIndex))
		{
			++Context->IterationIndex;
		}
		else
		{
			return false;
		}
	} while (Context->IterationIndex < Context->PerIterationStateArray.Num() && !Context->ShouldStop());

	return Context->IterationIndex == Context->PerIterationStateArray.Num();
}
