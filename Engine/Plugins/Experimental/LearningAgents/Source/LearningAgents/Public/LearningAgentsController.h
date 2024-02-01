// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerListener.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"

#include "LearningArray.h"

#include "LearningAgentsController.generated.h"


DECLARE_DYNAMIC_DELEGATE_RetVal_FourParams(FLearningAgentsActionObjectElement, FEvaluateAgentControllerDelegate, ULearningAgentsActionObject*, InActionObject, const FLearningAgentsObservationObjectElement, InObservationObjectElement, const ULearningAgentsObservationObject*, InObservationObject, const int32, AgentId);

/**
 * A controller is an object that can be used to construct actions from observations - essentially a hand-made Policy. This can be useful for making 
 * a learning agents system that uses some other existing behavior, e.g. we may want to gather demonstrations from a human or AI behavior tree 
 * controlling our agent(s) for imitation learning purposes.
 */
UCLASS(Abstract, HideDropdown, BlueprintType, Blueprintable)
class LEARNINGAGENTS_API ULearningAgentsController : public ULearningAgentsManagerListener
{
	GENERATED_BODY()

public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsController();
	ULearningAgentsController(FVTableHelper& Helper);
	virtual ~ULearningAgentsController();

	/**
	 * Constructs a new controller for the given agent interactor.
	 * 
	 * @param InManager			The input Manager
	 * @param InInteractor		The input Interactor component
	 * @param Class				The controller class
	 * @param Name				The controller name
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DeterminesOutputType = "Class"))
	static ULearningAgentsController* MakeController(
		ULearningAgentsManager* InManager, 
		ULearningAgentsInteractor* InInteractor, 
		TSubclassOf<ULearningAgentsController> Class,
		const FName Name = TEXT("Controller"));

	/** Initializes this object to be used with the given agent interactor. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetupController(ULearningAgentsManager* InManager, ULearningAgentsInteractor* InInteractor);

	/**
	 * This callback should be implemented by the Controller and should produce an array of action object elements, from an array of observation 
	 * object elements.
	 * 
	 * @param OutActionObjectElements		Output Action Object Elements. This should be the same size as the input AgentIds and 
	 *                                      InObservationObjectElements arrays.
	 * @param InActionObject				Action object used to construct output elements.
	 * @param InObservationObjectElements	Input observation object elements
	 * @param InObservationObject			Input observation object
	 * @param AgentIds						Agent ids associated with each observation
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents", Meta = (ForceAsFunction))
	void EvaluateAgentController(
		TArray<FLearningAgentsActionObjectElement>& OutActionObjectElements, 
		ULearningAgentsActionObject* InActionObject, 
		const TArray<FLearningAgentsObservationObjectElement>& InObservationObjectElements,
		const ULearningAgentsObservationObject* InObservationObject,
		const TArray<int32>& AgentIds);

	/**
	 * This is a convenience function that can be used to implement `EvaluateAgentController` in terms of a callback which iterates over every agent
	 * individually and computes the actions from the observations.
	 *
	 * @param OutActionObjectElements		Output Action Object Elements. This should be the same size as the input AgentIds and
	 *                                      InObservationObjectElements arrays.
	 * @param InActionObject				Action object used to construct output elements.
	 * @param InObservationObjectElements	Input observation object elements
	 * @param InObservationObject			Input observation object
	 * @param AgentIds						Agent ids associated with each observation
	 * @param Delegate						The Delegate used to compute actions from observations.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	void EvaluateAgentControllerUsingDelegate(
		TArray<FLearningAgentsActionObjectElement>& OutActionObjectElements,
		ULearningAgentsActionObject* InActionObject,
		const TArray<FLearningAgentsObservationObjectElement>& InObservationObjectElements,
		const ULearningAgentsObservationObject* InObservationObject,
		const TArray<int32>& AgentIds,
		const FEvaluateAgentControllerDelegate& Delegate);

	/**
	 * Call this function when it is time to evaluate the controller and produce the actions for the agents. This should be called after 
	 * GatherObservations but before ScatterActions. This will call this controller's EvaluateAgentController event.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EvaluateController();

	/**
	 * Calls GatherObservations, followed by EvaluateController, followed by ScatterActions
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void RunController();

protected:

	/**
	 * Gets the agent interactor associated with this component.
	 * 
	 * @param AgentClass The class to cast the agent interactor to (in blueprint).
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DeterminesOutputType = "InteractorClass"))
	ULearningAgentsInteractor* GetInteractor(const TSubclassOf<ULearningAgentsInteractor> InteractorClass) const;

protected:

	/** The agent interactor this controller is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;
};
