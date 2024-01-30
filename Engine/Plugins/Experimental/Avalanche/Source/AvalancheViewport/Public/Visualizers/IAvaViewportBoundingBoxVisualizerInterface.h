// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Templates/SharedPointer.h"

class AActor;
class FPrimitiveDrawInterface;
class UAvaBoundsProviderSubsystem;
class UAvaSelectionProviderSubsystem;

enum class EAvaViewportBoundingBoxOptimizationState : uint8
{
	RenderNothing,
	RenderSelectedActors,
	RenderSelectedActorsAndChildren,
	RenderSelectionBounds
};

class IAvaViewportBoundingBoxVisualizerInterface : public IAvaTypeCastable
{
public:
	UE_AVA_INHERITS(IAvaViewportBoundingBoxVisualizerInterface, IAvaTypeCastable)

	virtual void Draw(UAvaSelectionProviderSubsystem& InSelectionProvider, UAvaBoundsProviderSubsystem& InBoundsProvider, FPrimitiveDrawInterface& InPDI) = 0;
	virtual EAvaViewportBoundingBoxOptimizationState GetOptimizationState() const = 0;
	virtual void ResetOptimizationState() = 0;
};

struct AVALANCHEVIEWPORT_API FAvaViewportBoundingBoxVisualizerProvider
{
	static TSharedRef<IAvaViewportBoundingBoxVisualizerInterface> CreateVisualizer();
};
