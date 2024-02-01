// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaScreenshotTestActor.h"

#include "ActorFolder.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "AvaBlueprint.h"
#include "AvaTestBlueprintFunctionLibrary.h"
#include "AvaTestModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Tests/AutomationCommon.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AvaScreenshotTestActor"

namespace UE::Ava::Test::Private
{
	/** Finds and returns the first camera in the specified world. */
	ACameraActor* FindFirstCameraInWorld(const UWorld* InWorld)
	{
		for (ACameraActor* Camera : TActorRange<ACameraActor>(InWorld, ACameraActor::StaticClass(), EActorIteratorFlags::AllActors))
		{
			return Camera;
		}

		return nullptr;
	}
	
}

AAvaScreenshotTestActor::AAvaScreenshotTestActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScreenshotOptions.Resolution = { 1920, 1080 };
}

void AAvaScreenshotTestActor::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	Super::GetActorEyesViewPoint(OutLocation, OutRotation);

	if (ScreenshotCamera)
	{
		FMinimalViewInfo ViewInfo;
		ScreenshotCamera->GetCameraView(0.0f, ViewInfo);

		OutLocation = ViewInfo.Location;
		OutRotation = ViewInfo.Rotation;
	}
}

void AAvaScreenshotTestActor::SetupTest()
{
	const UAvalancheBlueprint* ResolvedBlueprintToTest = BlueprintToTest.LoadSynchronous();
	const bool bHasValidBP = ::IsValid(ResolvedBlueprintToTest);

	// Create a world (.umap) asset for the Ava BP, overwriting any existing
	if (bHasValidBP)
	{
		UAvaTestBlueprintFunctionLibrary::ExportMotionDesignBlueprintsToWorld({ ResolvedBlueprintToTest });
	}

	ReceiveSetupTest();
}

void AAvaScreenshotTestActor::PrepareTest()
{
	const UAvalancheBlueprint* ResolvedBlueprintToTest = BlueprintToTest.LoadSynchronous();
	const bool bHasValidBP = ::IsValid(ResolvedBlueprintToTest);
	if (bHasValidBP)
	{
		if (UWorld* CurrentWorld = GetWorld())
		{
			// Spawn a LevelInstance to contain the previously created World for the specified BP
			ALevelInstance* NewLevelInstanceActor =	CurrentWorld->SpawnActor<ALevelInstance>();
			if (!NewLevelInstanceActor)
			{
				UE_LOG(LogAvaTest, Error, TEXT("There was an error spawning ALevelInstance"));
			}
			else
			{
				const UPackage* const BlueprintPackage = ResolvedBlueprintToTest->GetPackage();
				const FString Path = FPackageName::GetLongPackagePath(BlueprintPackage->GetName()) + TEXT("/Exported/") + ResolvedBlueprintToTest->GetName();

				TSoftObjectPtr<UWorld> WorldCreatedByBP(Path);
				ensure(!WorldCreatedByBP.IsNull());

				// Set the World (umap) of the LevelInstance and wait for it to load
				NewLevelInstanceActor->SetWorldAsset(WorldCreatedByBP);
				NewLevelInstanceActor->GetLevelInstanceSubsystem()->BlockLoadLevelInstance(NewLevelInstanceActor);
				GEngine->BlockTillLevelStreamingCompleted(NewLevelInstanceActor->GetWorld());

				// Flag the LevelInstance for destruction when this test is complete
				AutoDestroyActors.Add(NewLevelInstanceActor);

				// Find and use the first camera in the newly loaded level, if it exists
				UWorld* LevelInstanceWorld = CurrentWorld;
				if (ACameraActor* FirstCameraInLevelInstance = UE::Ava::Test::Private::FindFirstCameraInWorld(LevelInstanceWorld))
				{
					DefaultScreenshotCamera = ScreenshotCamera;
					ScreenshotCamera = FirstCameraInLevelInstance->GetCameraComponent();
				}
				else
				{
					UE_LOG(LogAvaTest, Display, TEXT("No camera was found in the loaded Ava BP, using default (in Test Actor)."));
				}
			}
		}
	}
	
	// Only set if ScreenshotCamera is different to the Default (and thus the view target should be a different actor)
	if (DefaultScreenshotCamera && ScreenshotCamera != DefaultScreenshotCamera)
	{
		UGameViewportClient* GameViewportClient = AutomationCommon::GetAnyGameViewportClient();
		check(GameViewportClient);
		
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GameViewportClient->GetWorld(), 0);
		if (PlayerController)
		{
			if (ACameraActor* CameraActor = Cast<ACameraActor>(ScreenshotCamera->GetOwner()))
			{
				// @note: we're only copying the camera's FOV and transform, other settings (ie. lens) are not copied
				PlayerController->FOV(ScreenshotCamera->FieldOfView);
				SetActorRelativeTransform(CameraActor->GetActorTransform());	
			}
		}
	}

	Super::PrepareTest();
}

void AAvaScreenshotTestActor::FinishTest(EFunctionalTestResult TestResult, const FString& Message)
{
	// Restore camera
	ScreenshotCamera = DefaultScreenshotCamera;
	
	// Set actors to destroy before calling Super (which actually destroys them)
	{
		static const FString ProtectedFolderName = TEXT("DontDelete");

		// Forcibly deletes all actors outside of a reserved folder (see above) on test completion
		if (bClearLevelOnComplete)
		{
			if (const UWorld* World = GetWorld())
			{
				TArray<AActor*> ActorsToDestroy;
				ActorsToDestroy.Reserve(World->GetActorCount());

				for (AActor* const Actor : TActorRange<AActor>(World))
				{
					FFolder TopLevelActorFolder = Actor->GetFolder();
					FString TopLevelActorFolderName = TopLevelActorFolder.IsNone() || !TopLevelActorFolder.GetActorFolder()
						? TEXT("")
						: TopLevelActorFolder.GetActorFolder()->GetLabel();

					// Don't destroy an actor in this folder
					if (TopLevelActorFolderName.Equals(ProtectedFolderName, ESearchCase::IgnoreCase))
					{
						continue;
					}

					ActorsToDestroy.Add(Actor);
				}

				AutoDestroyActors.Append(ActorsToDestroy);
			}		
		}
	}

	Super::FinishTest(TestResult, Message);
}

#undef LOCTEXT_NAMESPACE
