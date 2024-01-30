// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "StateTree.h"
#include "AvaTransitionTree.generated.h"

UCLASS(DisplayName = "Motion Design Transition Tree")
class AVALANCHETRANSITION_API UAvaTransitionTree : public UStateTree
{
	GENERATED_BODY()

public:
	FAvaTagHandle GetTransitionLayer() const;

	void SetTransitionLayer(FAvaTagHandle InTransitionLayer);

	bool IsEnabled() const;

	void SetEnabled(bool bInEnabled);

private:
	/** The Layer this Transition Logic Tree deals with */
	UPROPERTY()
	FAvaTagHandle TransitionLayer;

	/**
	 * Determines whether this Transition Logic is enabled, by default.
	 * Can be overriden by a Transition Instance to force the logic to run regardless
	 */
	UPROPERTY()
	bool bEnabled = true;
};
