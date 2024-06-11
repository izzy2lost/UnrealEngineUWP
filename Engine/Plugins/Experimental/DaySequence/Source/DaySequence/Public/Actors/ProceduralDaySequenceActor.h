// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SimpleDaySequenceActor.h"

#include "ProceduralDaySequenceActor.generated.h"

class UProceduralDaySequenceBuilder;

UCLASS(Abstract)
class DAYSEQUENCE_API AProceduralDaySequenceActor : public ASimpleDaySequenceActor
{
	GENERATED_BODY()

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	/**
	 * Populates ProceduralSequence with keys.
	 * By default this does nothing, derived classes should override this to modify ProceduralSequence through the provided builder.
	 * 
	 * @param SequenceBuilder A ready-to-use sequence builder.
	 */
	virtual void BuildSequence(UProceduralDaySequenceBuilder* SequenceBuilder);

	virtual void InitializeDaySequences() override final;

protected:
	bool bProceduralSequenceInvalid = false;
	
private:
	// Wrapper that instantiates the sequence builder, invokes BuildSequence, and assigns the created sequence to ProceduralSequence.
	void CreateProceduralSequence();
	
	/** The procedural sequence. This is constructed in BuildSequence and added to the RootSequence in InitializeDaySequences. */
	UPROPERTY(Transient)
	TObjectPtr<UDaySequence> ProceduralSequence;
};
