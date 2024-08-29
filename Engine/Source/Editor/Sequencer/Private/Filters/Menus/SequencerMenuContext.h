// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "SequencerMenuContext.generated.h"

class FSequencer;

UCLASS()
class USequencerMenuContext : public UObject
{
	GENERATED_BODY()

public:
	void Init(const TWeakPtr<FSequencer>& InWeakSequencer)
	{
		WeakSequencer = InWeakSequencer;
	}

	TSharedPtr<FSequencer> GetSequencer() const
	{
		return WeakSequencer.Pin();
	}

protected:
	TWeakPtr<FSequencer> WeakSequencer;
};
