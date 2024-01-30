// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Visualizers/IAvaViewportBoundingBoxVisualizerInterface.h"

class AActor;

class FAvaLevelViewportBoundingBoxVisualizer : public IAvaViewportBoundingBoxVisualizerInterface
{
public:
	UE_AVA_INHERITS(FAvaLevelViewportBoundingBoxVisualizer, IAvaViewportBoundingBoxVisualizerInterface)

	FAvaLevelViewportBoundingBoxVisualizer();
	virtual ~FAvaLevelViewportBoundingBoxVisualizer() = default;

	//~ Begin IAvaViewportBoundingBoxVisualizerInterface
	virtual void Draw(UAvaSelectionProviderSubsystem& InSelectionProvider, UAvaBoundsProviderSubsystem& InBoundsProvider, FPrimitiveDrawInterface& InPDI) override;
	virtual EAvaViewportBoundingBoxOptimizationState GetOptimizationState() const override { return OptimizationState; }
	virtual void ResetOptimizationState() override;
	//~ End IAvaViewportBoundingBoxVisualizerInterface

protected:
	EAvaViewportBoundingBoxOptimizationState OptimizationState;

	void SetOptimizationRenderNothing(double InTaskTimeTaken);
	void SetOptimizationRenderSelectedActors(double InTaskTimeTaken);
	void SetOptimizationRenderSelectedActorsAndChildren(double InTaskTimeTaken);
	void SetOptimizationRenderSelectionBounds();
};
