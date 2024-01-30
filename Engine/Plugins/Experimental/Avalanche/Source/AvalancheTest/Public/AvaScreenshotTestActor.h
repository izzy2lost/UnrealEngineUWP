// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScreenshotFunctionalTest.h"

#include "AvaScreenshotTestActor.generated.h"

class UAvalancheBlueprint;

/**  */
UCLASS(Blueprintable, BlueprintType, HideCategories = ("Replication", "Collision", "Physics", "Networking", "HLOD", "LevelInstance", "Cooking"))
class AVALANCHETEST_API AAvaScreenshotTestActor
	: public AScreenshotFunctionalTest
{
	GENERATED_BODY()

public:
	AAvaScreenshotTestActor(const FObjectInitializer& ObjectInitializer);

	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** Optionally specify the Ava BP to test (and spawn in this level). */
	UPROPERTY(EditAnywhere, Category = "Ava", meta = (EditFixedOrder))
	TSoftObjectPtr<UAvalancheBlueprint> BlueprintToTest;

	/** If true, tests the alpha channel output (only!). */
	UPROPERTY(EditAnywhere, Category = "Ava")
	bool bTestAlphaOnly = false;

	/** If true, all actors outside of the "DontDelete" folder are destroyed on test completion. */
	UPROPERTY(EditAnywhere, Category = "Ava")
	bool bClearLevelOnComplete = true;

	/** Optionally implement to perform a pre-play, editor only setup step. Good for asset generation or modification that breaks in PIE. */
	virtual void SetupTest();

	/** Optionally implement to perform a pre-play, editor only setup step. Good for asset generation or modification that breaks in PIE. */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="SetupTest"))
	void ReceiveSetupTest();

protected:
	//~ Begin AFunctionalTest
	virtual void PrepareTest() override;
	virtual void FinishTest(EFunctionalTestResult TestResult, const FString& Message) override;
	//~ End AFunctionalTest

private:
	/** Stores the default camera for restoration after another is temporarily used. */
	UPROPERTY(Transient)
	TObjectPtr<class UCameraComponent> DefaultScreenshotCamera;
};
