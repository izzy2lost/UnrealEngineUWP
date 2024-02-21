// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "InputAction.h"
#include "InputActionValue.h"

class APawn;

// Class for testing input of a Pawn by injecting InputActions
class CQTEST_API FTestAction
{
public:
	virtual ~FTestAction()
	{
	}

	/**
	 * Custom input functionality to be applied on the provided Pawn.
	 *
	 * @param Pawn - Actor which the input logic will be applied to.
	 */
	virtual void operator()(const APawn* Pawn);

	FString InputActionName;
	FInputActionValue InputActionValue;

private:
	/**
	 * Finds the appropriate InputAction mapping from the Pawn using the name provided from InputActionName.
	 *
	 * @param Pawn - Pawn to search for the specified InputAction mapping.
	 */
	void FindInputAction(const APawn* Pawn);

	const UInputAction* InputAction{ nullptr };
};

// Class for processing FTestAction objects
class CQTEST_API FInputTestActions
{
public:
	FInputTestActions(APawn* InPawn) : Pawn(InPawn)
	{
	}

	virtual ~FInputTestActions();

protected:
	/**
	 * Processes the action within the current tick.
	 *
	 * @param Action - Function with the logic to be processed on the given Pawn.
	 * @param Predicate - Function used to determine if the Action should be executed.
	 */
	void PerformAction(TFunction<void(const APawn* Pawn)> Action, TFunction<bool()> Predicate = nullptr);

	/** Clears all active timers. */
	virtual void Reset();

	APawn* Pawn{ nullptr };

private:
	/**
	 * Processes repeat actions every tick.
	 *
	 * @param DeltaTime - Time elapsed since the last tick.
	 * @return true if tick has completed.
	 */
	bool Tick(float DeltaTime);

	FTSTicker::FDelegateHandle TickHandle;

	using FTestActionPair = TPair<TFunction<void(const APawn* Pawn)>, TFunction<bool()>>;
	TArray<FTestActionPair> TestActions;
};