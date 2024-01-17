// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTestGraphBuilder.h"
#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MidiAsset.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "Misc/AutomationTest.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMetasound/DataTypes/MidiControllerID.h"
#include "HarmonixMetasound/Nodes/MidiCCTriggerNode.h"

#if WITH_DEV_AUTOMATION_TESTS
/**
 * Helper enum class for evaluating different 
 * blocks containing different Midi Events in a Midi file
 */
UENUM()
enum class EMidiEventTestingBlock : uint8
{
	ZeroBlock,
	MidiChannelMismatchBlock,
	MidiTrackMismatchBlock,
	MidiEventTypeMismatchBlock,
	MidiControllerIDMismatchBlock,
	Count
};

namespace HarmonixMetasoundTests::MidiCCTriggerNode
{
	//expected values for tests
	constexpr int32 ExpectedCCInt32ValueOnPass = 127;
	constexpr int32 ExpectedCCInt32ValueNotPass = 0;
	constexpr float ExpectedCCFloatValueOnPass = 1.0f;
	constexpr float ExpectedCCFloatValueNotPass = 0.0f;
	constexpr int32 ExpectedTriggerCountOnPass = 1;
	constexpr int32 ExpectedTriggerCountNotPass = 0;
	constexpr int32 ExpectedTriggerFrameIndexOnPass = 0;
	constexpr int32 ExpectedTriggerFrameIndexNotPass = -1;

	//testing values
	constexpr int32 TestInputChannel = 1;
	constexpr int32 TestDefaultTrackIndex = 1;
	constexpr uint8 DefaultTestCCValue = 127;
	constexpr uint8 DifferentTestCCValue = 100;
	constexpr uint8 TestControllerIDShouldPass = static_cast<uint8>(EStdMidiControllerID::Hold);
	constexpr uint8 TestControllerIDShouldNotPass = static_cast<uint8>(EStdMidiControllerID::Hold2);

	constexpr int32 DefaultTrackNumber = 0;
	constexpr int32 DifferentTrackNumber = 1;
	
	//time frames of midi events to be added to midi file for testing
	//for block 2 - 4 (block 1 starts on tick 0, 0.0ms)
	//each block is 480 samples / 10ms long
	constexpr float TestingMsBlock2 = 15.0f;
	constexpr float TestingMsBlock3 = 25.0f;
	constexpr float TestingMsBlock4 = 35.0f;

	UMidiFile* BuildMidiFile()
	{
		UMidiFile* TheMidiFile = NewObject<UMidiFile>();

		//add tempo info bpm = 120
		TheMidiFile->GetSongMaps()->GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(120), 0);

		//Add two midi tracks for testing
		FMidiTrack* CCTrackDefault = TheMidiFile->AddTrack("CCEventTestTrackDefault");//track 1
		FMidiTrack* CCTrackDifferent = TheMidiFile->AddTrack("CCEventTestTrackDifferent");//track 2

		/**
		 * Midi Events for Testing Block 1:
		 * Add 2 CC events at tick 0 with different channel information and different cc value for testing
		 */
		FMidiEvent CCEventChannelMatch(0, FMidiMsg(MidiConstants::kControl, TestControllerIDShouldPass, DefaultTestCCValue));
		//change channel information to 2 by modifying the lowest 4 bits (should not pass)
		FMidiEvent CCEventChannelMismatch(0, FMidiMsg(MidiConstants::kControl + 0x01, TestControllerIDShouldPass, DifferentTestCCValue));
		//add both events to track 1 (default)
		CCTrackDefault->AddEvent(CCEventChannelMatch);
		CCTrackDefault->AddEvent(CCEventChannelMismatch);

		/**
		 * Midi Events for Testing Block 2:
		 * Add 2 CC events to 2 midi tracks at the same tick, default is track 1
		 */
		FMidiEvent CCEventTrackMatch(TheMidiFile->GetSongMaps()->MsToTick(TestingMsBlock2), 
			FMidiMsg(MidiConstants::kControl, TestControllerIDShouldPass, DefaultTestCCValue));
		CCTrackDefault->AddEvent(CCEventTrackMatch);//added to track 1
		//add the other event to track to, which should NOT pass through to output in this test, 
		//changing the control change value 
		FMidiEvent CCEventTrackMismatch(TheMidiFile->GetSongMaps()->MsToTick(TestingMsBlock2),
			FMidiMsg(MidiConstants::kControl, TestControllerIDShouldPass, DifferentTestCCValue));
		CCTrackDifferent->AddEvent(CCEventTrackMismatch);//added to track 2

		/**
		* Midi Events for Testing Block 3:
		* Add 1 event that has a different controller ID (should NOT pass)
		*/
		FMidiEvent CCEventControllerIDMismatch(TheMidiFile->GetSongMaps()->MsToTick(TestingMsBlock3),
			FMidiMsg(MidiConstants::kControl, TestControllerIDShouldNotPass, DefaultTestCCValue));
		CCTrackDefault->AddEvent(CCEventControllerIDMismatch);

		/**
		* Midi Events for Testing Block 4:
		* Add 1 event that has a different type (should NOT pass)
		*/
		FMidiEvent CCEventTypeMismatch(TheMidiFile->GetSongMaps()->MsToTick(TestingMsBlock4),
			FMidiMsg::CreateNoteOn(0, 64, DefaultTestCCValue));
		CCTrackDefault->AddEvent(CCEventTypeMismatch);

		//call TracksChanged() to update SongLengthData
		TheMidiFile->TracksChanged();

		//conform midi file length to an integer bar length
		TheMidiFile->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);

		return TheMidiFile;
	}

	using GraphBuilder = Metasound::Test::FNodeTestGraphBuilder;
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace HarmonixMetasound;
	using namespace Metasound::Test;
	using namespace HarmonixMetasound::Nodes::MidiCCTriggerNode;
	
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiCCTriggerNodeTestBasic,
		"Harmonix.Metasound.Nodes.MidiCCTriggerNode.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FMidiCCTriggerNodeTestBasic::RunTest(const FString&)
	{
		//************************************************************************************************************************
		// Build the graph.
		//************************************************************************************************************************
		FNodeTestGraphBuilder Builder;
		Builder.AddOutput("AudioOut", GetMetasoundDataTypeName<FAudioBuffer>());
		//transport
		Frontend::FNodeHandle TransportNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "TriggerToTransport", "" }, 0);
		UTEST_TRUE("Transport Node Created", TransportNode.Get().IsValid());
		// hoist up play trigger input
		Builder.AddAndConnectDataReferenceInput(TransportNode, CommonPinNames::Inputs::TransportPlayName, GetMetasoundDataTypeName<FTrigger>());
		// midi player
		Frontend::FNodeHandle MidiPlayerNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "MidiPlayer", "" }, 0);
		UTEST_TRUE("MidiPlayer Node Created", MidiPlayerNode.Get().IsValid());
		// hoist up midi asset input
		Builder.AddAndConnectDataReferenceInput(MidiPlayerNode, CommonPinNames::Inputs::MidiFileAssetName, GetMetasoundDataTypeName<FMidiAsset>());
		//connect transport to midi player
		bool ConnectionSuccess = Builder.ConnectNodes(TransportNode, CommonPinNames::Outputs::TransportName, MidiPlayerNode, CommonPinNames::Inputs::TransportName);
		UTEST_TRUE("Transport Out connected to MidiPlayer node", ConnectionSuccess);
		// Midi CC trigger node to test
		FNodeHandle CCTriggerNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "MidiCCTrigger", "" }, 0);
		UTEST_TRUE("MidiCCTrigger Node Created", CCTriggerNode.Get().IsValid());

		//Midi Channel Number 
		Builder.AddAndConnectDataReferenceInput(CCTriggerNode, Inputs::MidiChannelNumberName, GetMetasoundDataTypeName<int32>());
		//Midi Track Number
		Builder.AddAndConnectDataReferenceInput(CCTriggerNode, Inputs::MidiTrackNumberName, GetMetasoundDataTypeName<int32>());
		//Midi Controller ID input 
		Builder.AddAndConnectDataReferenceInput(CCTriggerNode, Inputs::InputMidiControllerIDName, GetMetasoundDataTypeName<FEnumStdMidiControllerID>());

		//output Midi Control Change Value (int32)
		Builder.AddAndConnectDataReferenceOutput(CCTriggerNode, Outputs::OutputControlChangeValueInt32Name, GetMetasoundDataTypeName<int32>());

		//output normalized Midi Control Change Value (float)
		Builder.AddAndConnectDataReferenceOutput(CCTriggerNode, Outputs::OutputControlChangeValueFloatName, GetMetasoundDataTypeName<float>(), Outputs::OutputControlChangeValueFloatName);

		//output trigger
		Builder.AddAndConnectDataReferenceOutput(CCTriggerNode, Outputs::OutputTriggerName, GetMetasoundDataTypeName<FTrigger>(), Outputs::OutputTriggerName);
		
		//connect midi player node to CC Trigger node
		ConnectionSuccess = Builder.ConnectNodes(MidiPlayerNode, CommonPinNames::Outputs::MidiStreamName, CCTriggerNode, Inputs::MidiStreamName);
		UTEST_TRUE("MidiPlayer Out successfully connected to Midi CC Trigger node", ConnectionSuccess);
		
		//************************************************************************************************************************
		// Make the generator.
		//************************************************************************************************************************
		constexpr int32 NumSamplesPerBlock = 480; // 10ms per block for sample rate = 48000
		const TUniquePtr<FMetasoundGenerator> Generator = Builder.BuildGenerator(48000, NumSamplesPerBlock);
		UTEST_TRUE("Graph successfully built", Generator.IsValid());
		
		//************************************************************************************************************************
		// Get reference to outputs we want to check.
		//************************************************************************************************************************
		TOptional<FInt32ReadRef> OutputControlChangeValueInt32 = Generator->GetOutputReadReference<int32>(Outputs::OutputControlChangeValueInt32Name);
		UTEST_TRUE("Output exists", OutputControlChangeValueInt32.IsSet());
		UTEST_EQUAL("Control Change value (int32) at start", **OutputControlChangeValueInt32, 0);

		TOptional<FFloatReadRef> OutputControlChangeValueFloat = Generator->GetOutputReadReference<float>(Outputs::OutputControlChangeValueFloatName);
		UTEST_TRUE("Output exists", OutputControlChangeValueFloat.IsSet());
		UTEST_EQUAL("Control Change value (float) at start", **OutputControlChangeValueFloat, 0.0f);

		TOptional<FTriggerReadRef> CCOutputTrigger = Generator->GetOutputReadReference<FTrigger>(Outputs::OutputTriggerName);
		UTEST_TRUE("Output exists", CCOutputTrigger.IsSet());
		UTEST_EQUAL("Number of CC triggers at start", (*CCOutputTrigger)->NumTriggeredInBlock(), 0);
		UTEST_EQUAL("CC Output Trigger sample", (*CCOutputTrigger)->First(), -1);
		
		//************************************************************************************************************************
		// Make initial inputs
		//************************************************************************************************************************
		// Make a Midi File to play by Midi Player
		UMidiFile* MidiFile = BuildMidiFile();
		TSharedPtr<Audio::IProxyData> MidiFileProxy = MidiFile->CreateProxyData({ "MidiCCTriggerTest" });
		Generator->SetInputValue<FMidiAsset>(CommonPinNames::Inputs::MidiFileAssetName, MidiFileProxy);
		
		// Set the testing Midi Channel (default = 1)
		Generator->SetInputValue<int32>(Inputs::MidiChannelNumberName, TestInputChannel);
		// Set the testing Midi Track (default = 1)
		Generator->SetInputValue<int32>(Inputs::MidiTrackNumberName, TestDefaultTrackIndex);
		// Set the testing Midi Controller ID
		Generator->SetInputValue<FEnumStdMidiControllerID>(Inputs::InputMidiControllerIDName, static_cast<FEnumStdMidiControllerID>(TestControllerIDShouldPass));
		
		// Trigger transport
		Generator->ApplyToInputValue<FTrigger>(CommonPinNames::Inputs::TransportPlayName, [](FTrigger& TriggerIn) { TriggerIn.TriggerFrame(0); });

		//************************************************************************************************************************
		// Generate 4 10ms blocks for testing various Midi Events in file/stream
		//************************************************************************************************************************
		
		constexpr uint8 FirstTestBlock = static_cast<uint8>(EMidiEventTestingBlock::MidiChannelMismatchBlock);
		constexpr uint8 NumBlocksToTest = static_cast<uint8>(EMidiEventTestingBlock::Count) - 1;

		TAudioBuffer<float> Buffer{ Generator->GetNumChannels(), NumSamplesPerBlock, EAudioBufferCleanupMode::Delete};
		for (uint8 BlockIndex = FirstTestBlock; BlockIndex <= NumBlocksToTest; ++BlockIndex)
		{
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			
			//Block 1 & 2 each should  have 1 out of 2 events pass through to output with their correlating CC values
			if (static_cast<EMidiEventTestingBlock>(BlockIndex) == EMidiEventTestingBlock::MidiChannelMismatchBlock ||
				static_cast<EMidiEventTestingBlock>(BlockIndex) == EMidiEventTestingBlock::MidiTrackMismatchBlock)
			{
				//check output CC values
				UTEST_EQUAL("Control Change value (int32) check", **OutputControlChangeValueInt32, ExpectedCCInt32ValueOnPass);
				UTEST_EQUAL("Control Change value (float) check", **OutputControlChangeValueFloat, ExpectedCCFloatValueOnPass);

				//check if we have a trigger
				UTEST_EQUAL("Number of CC Output Triggers", (*CCOutputTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountOnPass);
				
				//check trigger sample frame at the first block
				if (BlockIndex == 1)
				{
					UTEST_EQUAL("CC Output Trigger sample", (*CCOutputTrigger)->First(), ExpectedTriggerFrameIndexOnPass);
				}
			}
			else {
				//Block 3 & 4 each should have 1 event ONLY,and NONE should pass through to output
				
				//check output CC value (should both be 0)
				UTEST_EQUAL("Control Change value (int32) check", **OutputControlChangeValueInt32, ExpectedCCInt32ValueNotPass);
				UTEST_EQUAL("Control Change value (float) check", **OutputControlChangeValueFloat, ExpectedCCFloatValueNotPass);

				//no trigger should pass through
				UTEST_EQUAL("Number of CC Output Triggers", (*CCOutputTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);
				UTEST_EQUAL("CC Output Trigger sample", (*CCOutputTrigger)->First(), ExpectedTriggerFrameIndexNotPass);
			}
		}

		return true;
	}
}
#endif