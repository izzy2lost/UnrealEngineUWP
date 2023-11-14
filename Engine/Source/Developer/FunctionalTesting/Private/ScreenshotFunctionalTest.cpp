// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScreenshotFunctionalTest.h"

#include "Engine/GameViewportClient.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"
#include "UObject/AutomationObjectVersion.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Tests/AutomationCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ScreenshotFunctionalTest)

AScreenshotFunctionalTest::AScreenshotFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: AScreenshotFunctionalTestBase(ObjectInitializer)
	, bCameraCutOnScreenshotPrep(true)
	, bNeedsVariantRestore(false)
	, bShouldDoViewRectOffsetVariant(false)
{
}

void AScreenshotFunctionalTest::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FAutomationObjectVersion::GUID);

	if (Ar.CustomVer(FAutomationObjectVersion::GUID) < FAutomationObjectVersion::DefaultToScreenshotCameraCutAndFixedTonemapping)
	{
		bCameraCutOnScreenshotPrep = true;
	}
}

void AScreenshotFunctionalTest::PrepareTest()
{
	bShouldDoViewRectOffsetVariant = bSupportStereoTestVariants && FAutomationTestFramework::NeedPerformStereoTestVariants();

	// Pre-prep flush to allow rendering to temporary targets and other test resources
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	Super::PrepareTest();

	// Apply a camera cut if requested
	if (bCameraCutOnScreenshotPrep)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
			if (ScreenshotCamera)
			{
				ScreenshotCamera->NotifyCameraCut();
			}
		}
	}

	// Post-prep flush deal with any temporary resources allocated during prep before the main test
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();
}

void AScreenshotFunctionalTest::RequestScreenshot()
{
	Super::RequestScreenshot();

	if(IsMobilePlatform(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]))
	{
		// For mobile, use the high res screenshot API to ensure a fixed resolution screenshot is produced.
		// This means screenshot comparisons can compare with the output from any device.
		FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
		FIntPoint ScreenshotViewportSize = UAutomationBlueprintFunctionLibrary::GetAutomationScreenshotSize(ScreenshotOptions);
		if (Config.SetResolution(ScreenshotViewportSize.X, ScreenshotViewportSize.Y, 1.0f))
		{
			UGameViewportClient* GameViewportClient = AutomationCommon::GetAnyGameViewportClient();
			check(GameViewportClient);
			GameViewportClient->GetGameViewport()->TakeHighResScreenShot();
		}
	}
	else
	{
		// Screenshots in Unreal Engine work in this way:
		// 1. Call FScreenshotRequest::RequestScreenshot to ask the system to take a screenshot. The screenshot
		//    will have the same resolution as the current viewport;
		// 2. Register a callback to UGameViewportClient::OnScreenshotCaptured() delegate. The call back will be
		//    called with screenshot pixel data when the shot is taken;
		// 3. Wait till the next frame or call FSceneViewport::Invalidate to force a redraw. Screenshot is not
		//    taken until next draw where UGameViewportClient::ProcessScreenshots or
		//    FEditorViewportClient::ProcessScreenshots is called to read pixels back from the viewport. It also
		//    trigger the callback function registered in step 2.

		bool bShowUI = false;
		FScreenshotRequest::RequestScreenshot(bShowUI);
	}
}

void AScreenshotFunctionalTest::OnScreenShotCaptured(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
{
	UGameViewportClient* GameViewportClient = AutomationCommon::GetAnyGameViewportClient();
	check(GameViewportClient);

	GameViewportClient->OnScreenshotCaptured().RemoveAll(this);

#if WITH_AUTOMATION_TESTS
	const FString Context = AutomationCommon::GetWorldContext(GetWorld());

	TArray<uint8> CapturedFrameTrace = AutomationCommon::CaptureFrameTrace(Context, TestLabel);

	FAutomationScreenshotData Data = UAutomationBlueprintFunctionLibrary::BuildScreenshotData(Context, TestLabel, InSizeX, InSizeY);

	// Copy the relevant data into the metadata for the screenshot.
	Data.bHasComparisonRules = true;
	Data.ToleranceRed = ScreenshotOptions.ToleranceAmount.Red;
	Data.ToleranceGreen = ScreenshotOptions.ToleranceAmount.Green;
	Data.ToleranceBlue = ScreenshotOptions.ToleranceAmount.Blue;
	Data.ToleranceAlpha = ScreenshotOptions.ToleranceAmount.Alpha;
	Data.ToleranceMinBrightness = ScreenshotOptions.ToleranceAmount.MinBrightness;
	Data.ToleranceMaxBrightness = ScreenshotOptions.ToleranceAmount.MaxBrightness;
	Data.bIgnoreAntiAliasing = ScreenshotOptions.bIgnoreAntiAliasing;
	Data.bIgnoreColors = ScreenshotOptions.bIgnoreColors;
	Data.MaximumLocalError = ScreenshotOptions.MaximumLocalError;
	Data.MaximumGlobalError = ScreenshotOptions.MaximumGlobalError;

	// Add the notes
	Data.Notes = Notes;

	// If variant in use, pass on the name, then restore settings since capture is done
	Data.VariantName = CurrentVariantName;
	CurrentVariantName = "";

	if (bNeedsVariantRestore)
	{
		GEngine->Exec(nullptr, *VariantRestoreCommand);
		bNeedsVariantRestore = false;
	}

	if (GIsAutomationTesting)
	{
		FAutomationTestFramework::Get().OnScreenshotCompared.AddUObject(this, &AScreenshotFunctionalTest::OnComparisonComplete);
	}

	FAutomationTestFramework::Get().OnScreenshotAndTraceCaptured().ExecuteIfBound(InImageData, CapturedFrameTrace, Data);

	UE_LOG(LogScreenshotFunctionalTest, Log, TEXT("Screenshot captured as %s"), *Data.ScreenshotName);
#endif
}

void AScreenshotFunctionalTest::OnScreenshotTakenAndCompared()
{
	FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest();
	bool bSkipDueToError = FAutomationTestFramework::Get().NeedUseLightweightStereoTestVariants() && (!CurrentTest || CurrentTest->HasAnyErrors());

	if (bShouldDoViewRectOffsetVariant && !bSkipDueToError)
	{
		bShouldDoViewRectOffsetVariant = false;

		// 0: Off, 1: Center, 2: Top Left, 3: Top Right, 4: Bottom Left, 5: Bottom Right
		// We use bottom right to test both horizontal and vertical offsets (typically used for XR and split-screen, respectively).
		PerformVariant("ViewRectOffset", "r.Test.ViewRectOffset 5", "r.Test.ViewRectOffset 0");
	}
	else
	{
		// This ends the test and reports results
		Super::OnScreenshotTakenAndCompared();
	}
}

void AScreenshotFunctionalTest::PerformVariant(FString VariantName, FString SetupCommand, FString RestoreCommand)
{
	// Set up variant
	GEngine->Exec(nullptr, *SetupCommand);

	// Save variant name and command needed to restore in OnScreenShotCaptured
	CurrentVariantName = VariantName;
	if (!RestoreCommand.IsEmpty())
	{
		VariantRestoreCommand = RestoreCommand;
		bNeedsVariantRestore = true;
	}

	// Re-prepare test and request screenshot comparison
	if (bCameraCutOnScreenshotPrep)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
			if (ScreenshotCamera)
			{
				ScreenshotCamera->NotifyCameraCut();
			}
		}
	}

	RequestScreenshot(); 
}
