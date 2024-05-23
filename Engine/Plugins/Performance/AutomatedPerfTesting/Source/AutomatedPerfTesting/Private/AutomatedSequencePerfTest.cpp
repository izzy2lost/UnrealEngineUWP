// Copyright Epic Games, Inc. All Rights Reserved.


#include "AutomatedSequencePerfTest.h"

#include "TimerManager.h"
#include "AutomatedPerfTesting.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequencePlayer.h"
#include "Misc/CommandLine.h"

namespace AutomatedPerfTesting
{
	static TAutoConsoleVariable<FString> CvarSequencePath(
	TEXT("AutomatedPerfTest.SequencePath"),
	"",
	TEXT("Full path to the sequence which should be run when the AutomatedSequencePerfTest is executed. EG. /Game/Profiling/SampleSequence.SampleSequence"),
	ECVF_Default);
}

FString UAutomatedSequencePerfTest::GetSequenceName() const
{
	return SequenceSoftPath.GetAssetName();
}

void UAutomatedSequencePerfTest::SetupTest()
{
	Super::SetupTest();

	UE_LOG(LogAutomatedPerfTest, Log, TEXT("Setting up test"));

	// make sure the world exists, then create a sequence player
	if(UWorld* const World = GetWorld())
	{
		// load the sequence specified by the user
		UE_LOG(LogAutomatedPerfTest, Log, TEXT("Loading sequence %s"), *SequenceSoftPath.ToString());
		ULevelSequence* TargetSequence = LoadObject<ULevelSequence>(NULL, *SequenceSoftPath.ToString(), NULL, LOAD_None, NULL);
		check(TargetSequence);
		
		UE_LOG(LogAutomatedPerfTest, Log, TEXT("World is valid, creating sequence player"));
		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, TargetSequence, FMovieSceneSequencePlaybackSettings(), SequenceActor);

		if (SequencePlayer == nullptr)
		{
			UE_LOG(LogAutomatedPerfTest, Error, TEXT("Unable to create sequence player when starting AutomatedSequencePerfTest, exiting..."));
			EndAutomatedPerfTest(1);
		}

		// set the sequence up at the beginning
		FMovieSceneSequencePlaybackParams PlaybackParams = FMovieSceneSequencePlaybackParams();
		FMovieSceneSequencePlayToParams PlayToParams = FMovieSceneSequencePlayToParams();

		PlaybackParams.Time = 0.0;
		PlaybackParams.UpdateMethod = EUpdatePositionMethod::Scrub;

		UE_LOG(LogAutomatedPerfTest, Log, TEXT("RunTest:: Scrubbing to start"));
		SequencePlayer->PlayTo(PlaybackParams, PlayToParams);
		
		FTimerHandle UnusedHandle;
		
		// TODO parameterize the presoak delay
		GetWorld()->GetTimerManager().SetTimer(UnusedHandle, this, &UAutomatedSequencePerfTest::RunTest, 1.0, false, 5.0);
	}
	// if we have an invalid world, we can't run the test, so we should bail out
	else
	{
		UE_LOG(LogAutomatedPerfTest, Error, TEXT("Invalid World when starting AutomatedSequencePerfTest, exiting..."));
		EndAutomatedPerfTest(1);
	}
}

void UAutomatedSequencePerfTest::RunTest()
{
	Super::RunTest();

	UE_LOG(LogAutomatedPerfTest, Log, TEXT("RunTest"));
	
	// make sure we have a valid sequence player
	if(SequencePlayer)
	{
		UE_LOG(LogAutomatedPerfTest, Log, TEXT("RunTest::Valid Sequence Player, proceeding"));
		
        SequencePlayer->Play();
		// TODO add a bit here that also triggers begin/end regions on camera cut
        SequencePlayer->OnFinished.AddDynamic(this, &UAutomatedSequencePerfTest::TeardownTest);
	}
	// otherwise bail out of the test
	else
	{
		UE_LOG(LogAutomatedPerfTest, Error, TEXT("Invalid SequencePlayer when starting AutomatedSequencePerfTest, exiting..."));
		EndAutomatedPerfTest(1);
	}
}

void UAutomatedSequencePerfTest::TeardownTest()
{
	UE_LOG(LogAutomatedPerfTest, Log, TEXT("AutomatedSequencePerfTest::TeardownTest"));
	
	Super::TeardownTest();
}

void UAutomatedSequencePerfTest::Exit()
{
	UE_LOG(LogAutomatedPerfTest, Log, TEXT("AutomatedSequencePerfTest::Exit"));
	Super::Exit();
}

void UAutomatedSequencePerfTest::OnInit()
{
	Super::OnInit();
	UE_LOG(LogAutomatedPerfTest, Log, TEXT("AutomatedSequencePerfTest::OnInit"));
	
	if (FParse::Value(FCommandLine::Get(), TEXT("AutomatedPerfTest.SequencePath="), SequencePathName))
	{
		SequencePathName.ReplaceInline(TEXT("\""), TEXT(""));
		SequencePathName.ReplaceInline(TEXT("="), TEXT(""));
		AutomatedPerfTesting::CvarSequencePath->Set(*SequencePathName);
		UE_LOG(LogAutomatedPerfTest, Log, TEXT("Input Sequence Path Name = %s"), *SequencePathName);
		SequenceSoftPath = FSoftObjectPath(SequencePathName);
	}
}

void UAutomatedSequencePerfTest::UnbindAllDelegates()
{
	Super::UnbindAllDelegates();

	// if we have a valid sequence player, make sure we unbind our events from it when we're wrapping up the test.
	if(SequencePlayer != nullptr)
	{
		SequencePlayer->OnFinished.RemoveAll(this);
	}

	// clear any stray timers that might be laying around
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	if(SequencePlayer)
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(SequencePlayer);
	}
}
