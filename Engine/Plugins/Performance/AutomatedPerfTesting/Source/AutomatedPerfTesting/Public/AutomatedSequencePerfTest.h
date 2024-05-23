// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutomatedPerfTestControllerBase.h"
#include "AutomatedSequencePerfTest.generated.h"

class ULevelSequence;
class ALevelSequenceActor;
class ULevelSequencePlayer;

/**
 * 
 */
UCLASS()
class AUTOMATEDPERFTESTING_API UAutomatedSequencePerfTest : public UAutomatedPerfTestControllerBase
{
	GENERATED_BODY()

public:
	FString GetSequenceName() const;
	
	virtual void SetupTest() override;

	UFUNCTION()
	virtual void RunTest() override;

	UFUNCTION()
	virtual void TeardownTest() override;
	virtual void Exit() override;
	
protected:
	virtual void OnInit() override;
	virtual void UnbindAllDelegates() override;

private:
	FString SequencePathName;
	FSoftObjectPath SequenceSoftPath;

	ALevelSequenceActor* SequenceActor;
	ULevelSequencePlayer* SequencePlayer;
};
