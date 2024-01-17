// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiClock.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiClock
{
	BEGIN_DEFINE_SPEC(
		FHarmonixMetasoundMidiClockSpec,
		"Harmonix.Metasound.DataTypes.MidiClock",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	TUniquePtr<HarmonixMetasound::FMidiClock> TestClock;

	void AddStateAtFrame(HarmonixMetasound::EMusicPlayerTransportState State, int32 Frame) const
	{
		const HarmonixMetasound::FMidiTimestampTransportState NewState
		{
			Frame,
			0,
			State
		};

		check(TestClock.IsValid());
		TestClock->AddTransportStateChangeToBlock(NewState);
	}

	END_DEFINE_SPEC(FHarmonixMetasoundMidiClockSpec)

	void FHarmonixMetasoundMidiClockSpec::Define()
	{
		Describe("When a new transport state is added to the block", [this]()
		{
			BeforeEach([this]()
			{
				TestClock = MakeUnique<HarmonixMetasound::FMidiClock>(Metasound::FOperatorSettings{ 48000, 100 });
			});
			
			It("and the block has no other state changes, it should succeed", [this]()
			{
				TestClock->PrepareBlock();

				TestFalse("No transport changes in block", TestClock->HasTransportStateChangeInBlock());

				constexpr HarmonixMetasound::EMusicPlayerTransportState NewState = HarmonixMetasound::EMusicPlayerTransportState::Playing;
				AddStateAtFrame(NewState, 0);

				TestTrue("There is a transport state change in block", TestClock->HasTransportStateChangeInBlock());
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("and the new state happens after the latest one, it should succeed", [this]()
			{
				AddStateAtFrame(HarmonixMetasound::EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr HarmonixMetasound::EMusicPlayerTransportState NewState = HarmonixMetasound::EMusicPlayerTransportState::Pausing;
				AddStateAtFrame(NewState, LastStateFrame + 1);

				TestEqual("There is another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates + 1);
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("and the new state happens at the same time as the latest one, it should succeed", [this]()
			{
				AddStateAtFrame(HarmonixMetasound::EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr HarmonixMetasound::EMusicPlayerTransportState NewState = HarmonixMetasound::EMusicPlayerTransportState::Paused;
				AddStateAtFrame(NewState, LastStateFrame );

				TestEqual("There is another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates + 1);
				TestEqual("State at end of block matches the one we added", TestClock->GetTransportStateAtEndOfBlock(), NewState);
			});

			It("and the new state happens before the latest one, it should fail", [this]()
			{
				AddStateAtFrame(HarmonixMetasound::EMusicPlayerTransportState::Playing, 0);
				TestTrue("There is already a transport state change in the block", TestClock->HasTransportStateChangeInBlock());

				const int32 NumInitialStates = TestClock->GetTransportTimestampsInBlock().Num();
				const HarmonixMetasound::EMusicPlayerTransportState LastState = TestClock->GetTransportStateAtEndOfBlock();
				const int32 LastStateFrame = TestClock->GetTransportTimestampsInBlock().Last().BlockSampleFrameIndex;

				constexpr HarmonixMetasound::EMusicPlayerTransportState NewState = HarmonixMetasound::EMusicPlayerTransportState::Continuing;
				AddStateAtFrame(NewState, LastStateFrame - 1);

				TestEqual("There is not another transport state change in block", TestClock->GetTransportTimestampsInBlock().Num(), NumInitialStates);
				TestEqual("State at end of block matches the one that was already there", TestClock->GetTransportStateAtEndOfBlock(), LastState);
			});
		});
	}

}

#endif