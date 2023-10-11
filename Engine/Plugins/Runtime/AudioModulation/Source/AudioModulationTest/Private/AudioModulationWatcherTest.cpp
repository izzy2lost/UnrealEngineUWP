// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AudioModulationStatics.h"
#include "Engine/Engine.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SoundControlBus.h"
#include "Sound/SoundAttenuation.h"
#include "SoundModulationWatcher.h"
#include "Templates/SharedPointer.h"
#include "Tests/AutomationCommon.h"


namespace AudioModulation::TestPrivate
{
	FString GetPluginContentDirectory()
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AudioModulation"));
		if (ensure(Plugin.IsValid()))
		{
			return Plugin->GetContentDir();
		}
		return FString();
	}

	FString GetPathToTestFilesDir()
	{
		FString OutPath =  FPaths::Combine(GetPluginContentDirectory(), TEXT("Test"));

		OutPath = FPaths::ConvertRelativePathToFull(OutPath);
		FPaths::NormalizeDirectoryName(OutPath);
		
		return OutPath;
	}

	FString GetPathToGeneratedFilesDir()
	{
		FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AudioModulation"));

		OutPath = FPaths::ConvertRelativePathToFull(OutPath);
		FPaths::NormalizeDirectoryName(OutPath);
		
		return OutPath;
	}

	FString GetPathToGeneratedAssetsDir()
	{
		FString OutPath = TEXT("/Game/AudioModulation/Generated/");
		FPaths::NormalizeDirectoryName(OutPath);
		return OutPath;
	}
} // AudioModulation::TestPrivate

// Tests return value of Watcher's GetValue command against the provided value
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FSoundModulationWatcherGetValueLatentCommand, FAutomationTestBase&, Test, USoundModulationWatcher*, Watcher, float, TestValue);

bool FSoundModulationWatcherGetValueLatentCommand::Update()
{
	if (Watcher)
	{
		const float WatcherValue = Watcher->GetValue();
		Test.AddErrorIfFalse(FMath::IsNearlyEqual(WatcherValue, TestValue), FString::Format(TEXT("Modulation Watcher is reporting value {0} but expected value is {1}"), { WatcherValue, TestValue }));
		return true;
	}

	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSoundModulationWatcherClearModulatorLatentCommand, FAutomationTestBase&, Test, USoundModulationWatcher*, Watcher);

bool FSoundModulationWatcherClearModulatorLatentCommand::Update()
{
	if (Watcher)
	{
		Watcher->ClearModulator();
		return true;
	}

	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FSoundModulationWatcherSetModulatorLatentCommand, FAutomationTestBase&, Test, USoundModulationWatcher*, Watcher, USoundModulatorBase*, Modulator);

bool FSoundModulationWatcherSetModulatorLatentCommand::Update()
{
	if (Watcher)
	{
		Watcher->SetModulator(Modulator);
		return true;
	}

	return false;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FSoundModulationRemoveFromRootLatentCommand, UObject*, ToRemove);

bool FSoundModulationRemoveFromRootLatentCommand::Update()
{
	if (ToRemove)
	{
		ToRemove->RemoveFromRoot();
		return true;
	}

	return false;
}


// This test creates a Modulation bus, watcher, and mix.  The bus's value is then tested both before and after the mix is activated.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundModulationWatcherGetValueTest, "Audio.Modulation.Watcher.GetValue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoundModulationWatcherGetValueTest::RunTest(const FString& Parameters)
{
	using namespace Audio;

	UWorld* TestWorld = nullptr;
	if (GEngine)
	{
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != nullptr)
			{
				if (Context.WorldType != EWorldType::Inactive && Context.WorldType != EWorldType::None)
				{
					TestWorld = Context.World();
				}
			}
		}
	}

	if (!TestWorld)
	{
		return false;
	}

	USoundModulationWatcher* TestWatcher = NewObject<USoundModulationWatcher>();
	TestWatcher->AddToRoot();

	USoundControlBus* TestBus = NewObject<USoundControlBus>();
	TestBus->AddToRoot();

	USoundControlBus* TestBus2 = NewObject<USoundControlBus>();
	TestBus2->AddToRoot();

	TestWatcher->SetModulator(TestBus);

	constexpr float TestMixValue = 0.25f;
	FSoundControlBusMixStage Stage;
	Stage.Bus = TestBus;
	Stage.Value = FSoundModulationMixValue { TestMixValue, 0.1f /* AttackTime */, 0.1f /* ReleaseTime */ };

	const float WatcherValue = TestWatcher->GetValue();
	AddErrorIfFalse(FMath::IsNearlyEqual(WatcherValue, 1.0f), FString::Format(TEXT("Modulation Watcher is reporting value {0} but initial expected value is 1.0"), { WatcherValue }));

	constexpr bool bActivate = true;
	USoundControlBusMix* TestMix = UAudioModulationStatics::CreateBusMix(TestWorld, "TestMix", { Stage }, bActivate);
	AddErrorIfFalse(TestMix != nullptr, TEXT("Failed to create control bus mix for Modulation Watcher 'GetValue' test"));

	constexpr float TestMix2Value = 0.33f;
	Stage.Bus = TestBus2;
	Stage.Value = FSoundModulationMixValue { TestMix2Value, 0.1f /* AttackTime */, 0.1f /* ReleaseTime */ };
	USoundControlBusMix* TestMix2 = UAudioModulationStatics::CreateBusMix(TestWorld, "TestMix2", { Stage }, bActivate);
	AddErrorIfFalse(TestMix2 != nullptr, TEXT("Failed to create control bus mix for Modulation Watcher 'GetValue' test"));

	auto Cleanup = [&]()
	{
		// Clean up, clean up, everybody everywhere
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationRemoveFromRootLatentCommand(TestWatcher));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationRemoveFromRootLatentCommand(TestBus));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationRemoveFromRootLatentCommand(TestBus2));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationRemoveFromRootLatentCommand(TestMix));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationRemoveFromRootLatentCommand(TestMix2));
	};

	if (TestMix && TestMix2)
	{
		TestMix->AddToRoot();
		TestMix2->AddToRoot();

		// Wait for mix to apply
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(0.125f));

		// Validate setting modulator works (value should be modified to test mix 2's value)
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherSetModulatorLatentCommand(*this, TestWatcher, TestBus2));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherGetValueLatentCommand(*this, TestWatcher, TestMix2Value));

		// Set back to initial bus for remainder of tests
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherSetModulatorLatentCommand(*this, TestWatcher, TestBus));

		// Validate mix application mutates modulator value over time
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherGetValueLatentCommand(*this, TestWatcher, TestMixValue));

		// Validate clearing value works
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherClearModulatorLatentCommand(*this, TestWatcher));
		ADD_LATENT_AUTOMATION_COMMAND(FSoundModulationWatcherGetValueLatentCommand(*this, TestWatcher, 1.0f));

		Cleanup();
		return true;
	}

	Cleanup();
	return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS
