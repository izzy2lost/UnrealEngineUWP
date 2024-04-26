// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"
#include "Widgets/SCompoundWidget.h"

struct FChaosVDCharacterGroundConstraintSelectionHandle;
class FChaosVDScene;
class IChaosVDCharacterGroundConstraintDataProviderInterface;
class IStructureDetailsView;

class SChaosVDCharacterGroundConstraintDataInspector : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SChaosVDCharacterGroundConstraintDataInspector)
	{
	}

	SLATE_END_ARGS()

	virtual ~SChaosVDCharacterGroundConstraintDataInspector() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr);

	/** Sets a new constraint data to be inspected */
	void SetConstraintDataToInspect(const FChaosVDCharacterGroundConstraintSelectionHandle& InDataSelectionHandle);

	/** Set a constraint provider*/
	void SetConstraintDataProviderObjectToInspect(IChaosVDCharacterGroundConstraintDataProviderInterface* ConstraintDataProvider);

protected:
	TSharedPtr<IStructureDetailsView> CreateDataDetailsView();

	EVisibility GetOutOfDateWarningVisibility() const;
	EVisibility GetDetailsSectionVisibility() const;
	EVisibility GetNothingSelectedMessageVisibility() const;

	void RegisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo);
	void UnregisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo);

	void RegisterSceneEvents();
	void UnregisterSceneEvents();

	void HandleSceneUpdated();

	void ClearInspector();

	TSharedPtr<IStructureDetailsView> ConstraintDataDetailsView;
	TWeakPtr<FChaosVDScene> SceneWeakPtr;
	FChaosVDCharacterGroundConstraintSelectionHandle CurrentDataSelectionHandle;

	FName CurrentObjectBeingInspectedName;

	bool bIsUpToDate = true;
};
