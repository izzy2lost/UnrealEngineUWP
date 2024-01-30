// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTestModule.h"

#include "AvaScreenshotTestActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FunctionalTestBase.h"
#include "LevelEditorSubsystem.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAvaTest);

#define LOCTEXT_NAMESPACE "AvalancheTest"

namespace UE::AvaTest::Private
{
	static constexpr const TCHAR* AutomationTestFrameworkModuleName = TEXT("AutomationWorker");

	// Duplicate of ClientFuncTestPerforming.cpp ParseTestMapInfo
	static void ParseTestMapInfo(const FString& InParameters, FString& OutMapObjectPath, FString& OutMapPackageName, FString& OutMapTestName)
	{
		TArray<FString> ParamArray;
		InParameters.ParseIntoArray(ParamArray, TEXT(";"), true);

		OutMapObjectPath = ParamArray[0];
		OutMapPackageName = ParamArray[1];
		OutMapTestName = (ParamArray.Num() > 2) ? ParamArray[2] : TEXT("");
	}

	static AAvaScreenshotTestActor* FindActorForTestName(UWorld* InWorld, const FString& InTestName)
	{
		check(InWorld);

		for (const TWeakObjectPtr<AAvaScreenshotTestActor> Actor : TActorRange<AAvaScreenshotTestActor>(InWorld))
		{
			if (Actor->GetName() == InTestName)
			{
				return Actor.Get();
			}
		}

		return nullptr;
	}
}

void FAvaTestModule::StartupModule()
{
	FAutomationTestFramework::Get().OnTestStartEvent.AddRaw(this, &FAvaTestModule::OnTestStart);
}

void FAvaTestModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(UE::AvaTest::Private::AutomationTestFrameworkModuleName))
	{
		FAutomationTestFramework::Get().OnTestStartEvent.RemoveAll(this);
	}
}

void FAvaTestModule::OnTestStart(FAutomationTestBase* InTest)
{
	check(InTest); // This shouldn't ever happen

#if WITH_EDITOR
	ensure(!GIsPlayInEditorWorld); // Shouldn't be playing!
#endif

	// If it's one of our tests, run pre-test setup
	if (InTest && InTest->GetTestName().StartsWith(TEXT("FClientFunctionalTesting")))
	{
#if WITH_EDITOR
		ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
		const ULevel* CurrentLevel = LevelEditorSubsystem->GetCurrentLevel();
#endif

		const FFunctionalTestBase* FunctionalTest = static_cast<FFunctionalTestBase*>(InTest);

		TArray<FAutomationTestInfo> TestsToRun;
		FunctionalTest->GenerateTestNames(TestsToRun);

		for (const FAutomationTestInfo& TestInfo : TestsToRun)
		{
			FString MapObjectPath;
			FString MapPackageName;
			FString MapTestName; // Usually this is the actual actor name (not outliner label!)
			UE::AvaTest::Private::ParseTestMapInfo(TestInfo.GetTestParameter(), MapObjectPath, MapPackageName, MapTestName);

			// @note: this MUST occur in-editor
#if WITH_EDITOR
			// 1. load map (if not already current)

			// @todo: need to prevent new, unsaved levels from being saved
			if (!CurrentLevel->GetPathName().IsEmpty())
			{
				LevelEditorSubsystem->SaveCurrentLevel();
			}

			// Only load if not current
			if (CurrentLevel->GetPathName() != MapObjectPath)
			{
				if (!LevelEditorSubsystem->LoadLevel(MapObjectPath))
				{
					UE_LOG(LogAvaTest, Error, TEXT("There was an error loading the specified level: %s"), *MapObjectPath);
					continue;
				}

				CurrentLevel = LevelEditorSubsystem->GetCurrentLevel();
			}
#endif

			// 2. locate actor for testname
			UWorld* World = CurrentLevel->GetWorld();
			AAvaScreenshotTestActor* TestActor = UE::AvaTest::Private::FindActorForTestName(World, MapTestName);
			if (!ensure(TestActor))
			{
				UE_LOG(LogAvaTest, Error, TEXT("The TestActor for test \"%s\" was not found"), *MapTestName);
				continue;
			}
			
			// 3. if actor found, run pre-test func - @todo: support latent/async actions within?
			TestActor->SetupTest();

#if WITH_EDITOR
			// 4. save map if dirtied
			LevelEditorSubsystem->SaveCurrentLevel();
#endif
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvaTestModule, AvalancheTest)

