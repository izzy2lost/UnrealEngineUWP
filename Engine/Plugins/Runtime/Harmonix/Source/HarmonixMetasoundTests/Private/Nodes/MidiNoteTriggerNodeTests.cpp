// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTestGraphBuilder.h"
#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "Misc/AutomationTest.h"
#include "HarmonixMetasound/DataTypes/MidiAsset.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMidi/MidiMsg.h"
#include "HarmonixMidi/MidiFile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiNoteTriggerNode
{
	//testing Note Numbers
	constexpr int32 TestNoteNumberShouldPass = 60; //C4, frequency = 261.63 Hz
	constexpr int32 TestNoteNumberShouldPass2 = 62; //D4, frequency = 293.66 Hz

	constexpr int32 TestNoteNumberShouldNotPass = 72; //C5, frequency = 523.25 Hz
	//Min and Max Note Numbers for input 
	constexpr int32 TestMaxNoteNumberDefault = 100;
	constexpr int32 TestMinNoteNumberDefault = 20;
	//Note Numbers that are outside of the Min to Max range
	constexpr int32 TestNoteNumberLessThanMin = 10;
	constexpr int32 TestNoteNumberGreaterThanMax = 127;

	//testing velocity values
	constexpr int32 TestNoteVelocityShouldPass = 90;
	constexpr float TestNoteNormalizedVelocityShouldPass = 90 / 127.f;
	//Min and Max Velocity values
	constexpr int32 TestMaxVelocityDefault = 100;
	constexpr int32 TestMinVelocityDefault = 20;
	//velocity values outside of the Min to Max range
	constexpr int32 TestVelocityLessThanMin = 10;
	constexpr int32 TestVelocityGreaterThanMax = 127;
	constexpr int32 TestNoteVelocityShouldNotPass = 127;//maybe we don't need this?
	
	//test midi tracks
	constexpr int32 TestMidiTrackShouldPass = 1;
	constexpr int32 TestMidiTrackShouldNotPass = 2;

	//test midi channel
	constexpr int32 TestMidiChannelShouldPass = 0;
	constexpr int32 TestMidiChannelShouldNotPass = 1;

	//frequency outputs
	constexpr float ExpectedFrequencyOnPass = 261.625549f; //needs verification 
	constexpr float ExpectedFrequencyNotPass = 0.0f;

	//other expected values if event does not pass through
	constexpr int32 ExpectedNoteNumberNotPass = 0;
	constexpr int32 ExpectedVelocityNotPass = 0;
	constexpr float ExpectedNormalizedVelocityNotPass = 0.0f;

	//expected number of triggers that pass through during each block
	constexpr int32 ExpectedTriggerCountOnPass = 1;
	constexpr int32 ExpectedTriggerCountNotPass = 0;

	//expected trigger frames
	constexpr int32 ExpectedTriggerFrameIndexNotPass = -1;

	//Time indices (Ms) for Midi events in Testing blocks
	constexpr float TestingMsNoteOnBlock1 = 0.0f;
	constexpr float TestingMsNoteOffBlock2 = 13.0f;

	constexpr float TestingMsNoteOnBlock2 = 18.0f;
	constexpr float TestingMsNoteOffBlock3 = 23.0f;
	constexpr float TestingMsNoteOnBlock3 = 28.0f;
	constexpr float TestingMsNoteOffBlock4 = 33.0f;	
	constexpr float TestingMsNoteOnBlock4 = 38.0f;
	
	//last block tests for Non-Note events
	constexpr float TestingMsNoteOffBlock5 = 43.0f;
	constexpr float TestingMsEventBlock5 = 48.0f;

	//number of test blocks to generate during tests
	constexpr int32 NumBlocksToTest = 5;

	UMidiFile* BuildMidiFile()
	{
		UMidiFile* TheMidiFile = NewObject<UMidiFile>();

		//add tempo info bpm = 120
		TheMidiFile->GetSongMaps()->GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(120), 0);

		//Add two midi tracks for testing
		FMidiTrack* MidiNoteTrackDefault = TheMidiFile->AddTrack("MidiNoteTestTrackDefault");//track 1
		FMidiTrack* MidiNoteTrackDifferent = TheMidiFile->AddTrack("MidiNoteTestTrackDifferent");//track 2

		/**
		* Midi Events for Testing Block 1:
		* Add 2 Note On events in the first block with different channel information and different note number for testing
		*/
		//Note On Events
		FMidiEvent MidiChannelMatchNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock1),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberShouldPass, TestNoteVelocityShouldPass));
		//this should not pass
		FMidiEvent MidiChannelMismatchNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock1),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldNotPass, TestNoteNumberShouldNotPass, TestNoteVelocityShouldNotPass));
		
		//Add 2 Note On events to default track 1
		MidiNoteTrackDefault->AddEvent(MidiChannelMismatchNoteOn);
		MidiNoteTrackDefault->AddEvent(MidiChannelMatchNoteOn);


		/**
		 * Midi Events for Testing Block 2:
		 * Add 2 Note Off events for the 2 Note On events in the previous block
		 * (NOTE: Putting Note On and Note Off Events in separate blocks for testing the number of output triggers
		 * since Note Offs will reset output)
		 * Add 2 Note On events to 2 separate midi tracks at the same tick, default is track 1
		 */
		 //Note Off Events pairing with Note On events in block 1
		FMidiEvent MidiChannelMatchNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock2),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberShouldPass));
		//this should not pass
		FMidiEvent MidiChannelMismatchNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock2),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldNotPass, TestNoteNumberShouldNotPass));
		//Add Note Off events to track 1
		MidiNoteTrackDefault->AddEvent(MidiChannelMismatchNoteOff);
		MidiNoteTrackDefault->AddEvent(MidiChannelMatchNoteOff);

		FMidiEvent MidiTrackMatchNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock2),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberShouldPass, TestNoteVelocityShouldPass));
		//this should not pass
		FMidiEvent MidiTrackMismatchNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock2),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberShouldNotPass, TestNoteVelocityShouldNotPass));

		//Add 1 Note On event to default track 1 and the other to track 2
		MidiNoteTrackDifferent->AddEvent(MidiTrackMismatchNoteOn);
		MidiNoteTrackDefault->AddEvent(MidiTrackMatchNoteOn);

		/**
		* Midi Events for Testing Block 3:
		* Add 2 Note Off events for the 2 Note On events in the previous block
		* Add 2 pairs of Note On events that have note numbers less than min note number or greater than max note number
		* Neither should pass through to output 
		*/
		//Not Off Events pairing with Note On events from the previous block
		FMidiEvent MidiTrackMatchNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock3),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberShouldPass));
		//this should not pass
		FMidiEvent MidiTrackMismatchNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock3),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberShouldNotPass));
		//Add Note Off events to their corresponding tracks
		MidiNoteTrackDifferent->AddEvent(MidiTrackMismatchNoteOff);
		MidiNoteTrackDefault->AddEvent(MidiTrackMatchNoteOff);

		//Note On Events that have Note Numbers greater than the max or less than min Note Number
		FMidiEvent NoteNumberLessThanMinNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock3),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberLessThanMin, TestNoteVelocityShouldPass));
		FMidiEvent NoteNumberGreaterThanMaxNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock3),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberGreaterThanMax, TestNoteVelocityShouldPass));
		//Add 2 Note On events to default track 1, neither should pass
		MidiNoteTrackDefault->AddEvent(NoteNumberLessThanMinNoteOn);
		MidiNoteTrackDefault->AddEvent(NoteNumberGreaterThanMaxNoteOn);

		/**
		* Midi Events for Testing Block 4:
		* Add 2 Note Off events for the 2 Note On events in the previous block
		* Add 1 event that has a different type (should NOT pass)
		*/
		//Not Off events pairing with Note On events from the previous block
		FMidiEvent NoteNumberLessThanMinNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock4),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberLessThanMin));
		FMidiEvent NoteNumberGreaterThanMaxNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock4),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberGreaterThanMax));
		//Add 2 Note Off events to default track 1, neither should pass
		MidiNoteTrackDefault->AddEvent(NoteNumberLessThanMinNoteOff);
		MidiNoteTrackDefault->AddEvent(NoteNumberGreaterThanMaxNoteOff);
		
		//Note On Events that have velocity values greater than the max or less than min velocity value
		FMidiEvent NoteVelocityLessThanMinNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock4),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberShouldPass, TestVelocityLessThanMin));
		FMidiEvent NoteVelocityGreaterThanMaxNoteOn(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOnBlock4),
			FMidiMsg::CreateNoteOn(TestMidiChannelShouldPass, TestNoteNumberShouldPass, TestVelocityGreaterThanMax));
		//Add 2 Note On events to default track 1, neither should pass
		MidiNoteTrackDefault->AddEvent(NoteVelocityLessThanMinNoteOn);
		MidiNoteTrackDefault->AddEvent(NoteVelocityGreaterThanMaxNoteOn);
		


		/**
		* Midi Events for Testing Block 5:
		* Add 2 Note Off events for the 2 Note On events in the previous block
		* Add 1 event that has a different type (should NOT pass), this one is a Pitch Bend event
		*/
		//Not Off events pairing with Note On events from the previous block
		FMidiEvent NoteVelocityLessThanMinNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock5),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberShouldPass));
		FMidiEvent NoteVelocityGreaterThanMaxNoteOff(TheMidiFile->GetSongMaps()->MsToTick(TestingMsNoteOffBlock5),
			FMidiMsg::CreateNoteOff(TestMidiChannelShouldPass, TestNoteNumberShouldPass));

		//Add Note Off events to default track 1, neither should pass
		MidiNoteTrackDefault->AddEvent(NoteVelocityLessThanMinNoteOff);
		MidiNoteTrackDefault->AddEvent(NoteVelocityGreaterThanMaxNoteOff);

		//Add a Pitch Bend event to default track 1, should not pass
		FMidiEvent EventTypeMismatch(TheMidiFile->GetSongMaps()->MsToTick(TestingMsEventBlock5),
			FMidiMsg(MidiConstants::kPitch, TestNoteNumberShouldPass2, TestNoteVelocityShouldPass));
		MidiNoteTrackDefault->AddEvent(EventTypeMismatch);

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

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiNoteTriggerCreateNodeTest,
		"Harmonix.Metasound.Nodes.MidiNoteTriggerNode.CreateNode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FMidiNoteTriggerCreateNodeTest::RunTest(const FString&)
	{
		// Build the graph.
		constexpr int32 NumSamplesPerBlock = 256;
		const TUniquePtr<FMetasoundGenerator> Generator = GraphBuilder::MakeSingleNodeGraph(
			{ HarmonixMetasound::HarmonixNodeNamespace, "MidiNoteTrigger", "" },
			0,
			48000,
			NumSamplesPerBlock);
		UTEST_TRUE("Graph successfully built", Generator.IsValid());

		// execute a block
		{
			TAudioBuffer<float> Buffer{ Generator->GetNumChannels(), NumSamplesPerBlock, EAudioBufferCleanupMode::Delete};
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
		}

		// Validate output.
		TOptional<FTriggerReadRef> OutputNoteOn = Generator->GetOutputReadReference<FTrigger>(CommonPinNames::Outputs::NoteOnName);
		UTEST_TRUE("Output exists", OutputNoteOn.IsSet());
		UTEST_EQUAL("Number of Note On triggers", (*OutputNoteOn)->NumTriggeredInBlock(), 0);
		UTEST_EQUAL("Note On Trigger sample", (*OutputNoteOn)->First(), -1);

		TOptional<FTriggerReadRef> OutputNoteOff = Generator->GetOutputReadReference<FTrigger>(CommonPinNames::Outputs::NoteOffName);
		UTEST_TRUE("Output exists", OutputNoteOff.IsSet());
		UTEST_EQUAL("Number of Note Off triggers", (*OutputNoteOff)->NumTriggeredInBlock(), 0);
		UTEST_EQUAL("Note Off Trigger sample", (*OutputNoteOff)->First(), -1);

		TOptional<FInt32ReadRef> OutputMidiNoteNum = Generator->GetOutputReadReference<int32>("Midi Note #");
		UTEST_TRUE("Output exists", OutputMidiNoteNum.IsSet());
		UTEST_EQUAL("Midi Note Num check", **OutputMidiNoteNum, 0);

		TOptional<FFloatReadRef> OutputFrequency = Generator->GetOutputReadReference<float>("Frequency");
		UTEST_TRUE("Output exists", OutputFrequency.IsSet());
		UTEST_EQUAL("Frequency check", **OutputFrequency, 0.0f);

		TOptional<FInt32ReadRef> OutputVelocity = Generator->GetOutputReadReference<int32>("Velocity");
		UTEST_TRUE("Output exists", OutputVelocity.IsSet());
		UTEST_EQUAL("Velocity check", **OutputVelocity, 0);

		TOptional<FFloatReadRef> OutputNormalizedVelocity = Generator->GetOutputReadReference<float>("Normalized Velocity");
		UTEST_TRUE("Output exists", OutputNormalizedVelocity.IsSet());
		UTEST_EQUAL("Normalized Velocity check", **OutputNormalizedVelocity, 0.0f);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiNoteTriggerNodeTestBasic,
		"Harmonix.Metasound.Nodes.MidiNoteTriggerNode.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FMidiNoteTriggerNodeTestBasic::RunTest(const FString&)
	{
		//************************************************************************************************************************
		// Build the graph.
		//************************************************************************************************************************
		GraphBuilder Builder;
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
		// Midi Note Trigger node to test
		FNodeHandle NoteTriggerNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "MidiNoteTrigger", "" }, 0);
		UTEST_TRUE("MidiNoteTrigger Node Created", NoteTriggerNode.Get().IsValid());

		//Midi Input Channel Number 
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MidiChannelNumberName, GetMetasoundDataTypeName<int32>());
		//Midi Input Track Number
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MidiTrackNumberName, GetMetasoundDataTypeName<int32>());
		//min Input Midi Note Number
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MinMidiNoteName, GetMetasoundDataTypeName<int32>());
		//Max Input Midi Note Number
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MaxMidiNoteName, GetMetasoundDataTypeName<int32>());
		//Input Min Velocity
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MinMidiVelocityName, GetMetasoundDataTypeName<int32>());
		//Input Max Velocity
		Builder.AddAndConnectDataReferenceInput(NoteTriggerNode, CommonPinNames::Inputs::MaxMidiVelocityName, GetMetasoundDataTypeName<int32>());
		

		//output Note On trigger
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::NoteOnName, GetMetasoundDataTypeName<FTrigger>(), CommonPinNames::Outputs::NoteOnName);
		//output Note Off trigger
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::NoteOffName, GetMetasoundDataTypeName<FTrigger>(), CommonPinNames::Outputs::NoteOffName);
		
		//output Midi Note number
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::MidiNoteNumberName, GetMetasoundDataTypeName<int32>(), CommonPinNames::Outputs::MidiNoteNumberName);

		//output Midi Note Velocity (Int32)
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::MidiVelocityName, GetMetasoundDataTypeName<int32>(), CommonPinNames::Outputs::MidiVelocityName);
		//output normalized Midi Note Velocity (float)
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::NormalizedVelocityName, GetMetasoundDataTypeName<float>(), CommonPinNames::Outputs::NormalizedVelocityName);

		//output frequency 
		Builder.AddAndConnectDataReferenceOutput(NoteTriggerNode, CommonPinNames::Outputs::FrequencyName, GetMetasoundDataTypeName<float>(), CommonPinNames::Outputs::FrequencyName);

		//connect midi player node to Note Trigger node
		ConnectionSuccess = Builder.ConnectNodes(MidiPlayerNode, CommonPinNames::Outputs::MidiStreamName, NoteTriggerNode, CommonPinNames::Inputs::MidiStreamName);
		UTEST_TRUE("MidiPlayer Out successfully connected to Midi Note Trigger node", ConnectionSuccess);

		//************************************************************************************************************************
		// Make the generator.
		//************************************************************************************************************************
		constexpr int32 NumSamplesPerBlock = 480; // 10ms per block for sample rate = 48000
		const TUniquePtr<FMetasoundGenerator> Generator = Builder.BuildGenerator(48000, NumSamplesPerBlock);
		UTEST_TRUE("Graph successfully built", Generator.IsValid());

		//************************************************************************************************************************
		// Get reference to outputs we want to check.
		//************************************************************************************************************************
		
		TOptional<FTriggerReadRef> OutputNoteOnTrigger = Generator->GetOutputReadReference<FTrigger>(CommonPinNames::Outputs::NoteOnName);
		UTEST_TRUE("Output exists", OutputNoteOnTrigger.IsSet());
		UTEST_EQUAL("Number of Note Triggers at start", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), 0);
		UTEST_EQUAL("Note On Output Trigger sample", (*OutputNoteOnTrigger)->First(), -1);
		
		TOptional<FTriggerReadRef> OutputNoteOffTrigger = Generator->GetOutputReadReference<FTrigger>(CommonPinNames::Outputs::NoteOffName);
		UTEST_TRUE("Output exists", OutputNoteOffTrigger.IsSet());
		UTEST_EQUAL("Number of Note Triggers at start", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), 0);
		UTEST_EQUAL("Note off Output Trigger sample", (*OutputNoteOffTrigger)->First(), -1);

		TOptional<FInt32ReadRef> OutputMidiNoteNumber = Generator->GetOutputReadReference<int32>(CommonPinNames::Outputs::MidiNoteNumberName);
		UTEST_TRUE("Output exists", OutputMidiNoteNumber.IsSet());
		UTEST_EQUAL("Midi Note Number at start", **OutputMidiNoteNumber, 0);

		TOptional<FInt32ReadRef> OutputMidiVelocity = Generator->GetOutputReadReference<int32>(CommonPinNames::Outputs::MidiVelocityName);
		UTEST_TRUE("Output exists", OutputMidiVelocity.IsSet());
		UTEST_EQUAL("Midi Velocity (int32) at start", **OutputMidiVelocity, 0);

		TOptional<FFloatReadRef> OutputNormalizedVelocity = Generator->GetOutputReadReference<float>(CommonPinNames::Outputs::NormalizedVelocityName);
		UTEST_TRUE("Output exists", OutputNormalizedVelocity.IsSet());
		UTEST_EQUAL("Normalized Midi Velocity (float) at start", **OutputNormalizedVelocity, 0.0f);

		TOptional<FFloatReadRef> OutputFrequency= Generator->GetOutputReadReference<float>(CommonPinNames::Outputs::FrequencyName);
		UTEST_TRUE("Output exists", OutputFrequency.IsSet());
		UTEST_EQUAL("Normalized Midi Velocity (float) at start", **OutputFrequency, 0.0f);

		//************************************************************************************************************************
		// Make initial inputs
		//************************************************************************************************************************
		// Make a Midi File to play by Midi Player
		UMidiFile* MidiFile = BuildMidiFile();
		TSharedPtr<Audio::IProxyData> MidiFileProxy = MidiFile->CreateProxyData({ "MidiNoteTriggerTest" });
		Generator->SetInputValue<FMidiAsset>(CommonPinNames::Inputs::MidiFileAssetName, MidiFileProxy);

		// Set the testing Midi Channel (default = 1)
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MidiChannelNumberName, TestMidiChannelShouldPass + 1);
		// Set the testing Midi Track (default = 1)
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MidiTrackNumberName, TestMidiTrackShouldPass);
		// Set the testing Min Midi Note
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MinMidiNoteName, TestMinNoteNumberDefault);
		// Set the testing Max Midi Note
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MaxMidiNoteName, TestMaxNoteNumberDefault);
		// Set the testing Min Velocity
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MinMidiVelocityName, TestMinVelocityDefault);
		// Set the testing Max Velocity
		Generator->SetInputValue<int32>(CommonPinNames::Inputs::MaxMidiVelocityName, TestMaxVelocityDefault);

		// Trigger transport
		Generator->ApplyToInputValue<FTrigger>(CommonPinNames::Inputs::TransportPlayName, [](FTrigger& TriggerIn) { TriggerIn.TriggerFrame(0); });

		//************************************************************************************************************************
		// Generate 5 10ms blocks for testing various Midi Events in file/stream
		//************************************************************************************************************************

		TAudioBuffer<float> Buffer{ Generator->GetNumChannels(), NumSamplesPerBlock, EAudioBufferCleanupMode::Delete};
			
			//Block 1:
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			//Expected: 1 Note On Trigger, 0 Note Off Trigger, 
			// Note Number/Velocity/Frequency outputs contain values equal to inputs
				
			//check output triggers
			UTEST_EQUAL("Number of Note On Output Triggers", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountOnPass);
			UTEST_EQUAL("Number of Note Off Output Triggers", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);

			//check output values
			UTEST_EQUAL("Midi Note Number check", **OutputMidiNoteNumber, TestNoteNumberShouldPass);
			UTEST_EQUAL("Midi Velocity (int32) check", **OutputMidiVelocity, TestNoteVelocityShouldPass);
			UTEST_EQUAL("Midi Normalized Velocity (float) check", **OutputNormalizedVelocity, TestNoteNormalizedVelocityShouldPass);
			UTEST_EQUAL("Output frequency check", **OutputFrequency, ExpectedFrequencyOnPass);

			//Block 2:
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			//Expected: 1 Note Off Trigger (for previous Note On that also should pass), 1 Note On Trigger,
			//Note Number/Velocity/Frequency outputs contain values equal to inputs

			//check output triggers
			UTEST_EQUAL("Number of Note Off Output Triggers", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountOnPass);
			UTEST_EQUAL("Number of Note On Output Triggers", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountOnPass);
				
			//check output values
			UTEST_EQUAL("Midi Note Number check", **OutputMidiNoteNumber, TestNoteNumberShouldPass);
			UTEST_EQUAL("Midi Velocity (int32) check", **OutputMidiVelocity, TestNoteVelocityShouldPass);
			UTEST_EQUAL("Midi Normalized Velocity (float) check", **OutputNormalizedVelocity, TestNoteNormalizedVelocityShouldPass);
			UTEST_EQUAL("Output frequency check", **OutputFrequency, ExpectedFrequencyOnPass);

			//Block 3:
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			//Expected: 1 Note Off Trigger (for previous Note On that also should pass), 0 Note On Trigger
			//Note Number/Frequency should be the same as the previous block
			//Velocity should be 0
				
			//check output triggers
			UTEST_EQUAL("Number of Note Off Output Triggers", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountOnPass);
			UTEST_EQUAL("Number of Note On Output Triggers", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);

			//check output values
			UTEST_EQUAL("Midi Note Number check", **OutputMidiNoteNumber, TestNoteNumberShouldPass);
			UTEST_EQUAL("Midi Velocity (int32) check", **OutputMidiVelocity, ExpectedVelocityNotPass);
			UTEST_EQUAL("Midi Normalized Velocity (float) check", **OutputNormalizedVelocity, ExpectedNormalizedVelocityNotPass);
			UTEST_EQUAL("Output frequency check", **OutputFrequency, ExpectedFrequencyOnPass);


			//Block 4:
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			//Expected: 0 Note On/Note Off trigger,
			//Note Number/Frequency should be the same as the previous block
			//Velocity should be 0

			//check output triggers (should all be 0)
			UTEST_EQUAL("Number of Note Off Output Triggers", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);
			UTEST_EQUAL("Number of Note On Output Triggers", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);

			//check output values
			UTEST_EQUAL("Midi Note Number check", **OutputMidiNoteNumber, TestNoteNumberShouldPass);
			UTEST_EQUAL("Midi Velocity (int32) check", **OutputMidiVelocity, ExpectedVelocityNotPass);
			UTEST_EQUAL("Midi Normalized Velocity (float) check", **OutputNormalizedVelocity, ExpectedNormalizedVelocityNotPass);
			UTEST_EQUAL("Output frequency check", **OutputFrequency, ExpectedFrequencyOnPass);

			//Block 5:
			Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());
			//Expected: 0 Note On/Note Off trigger,
			//Note Number/Frequency should be the same as the previous block
			//Velocity should be 0

			//check output triggers (should all be 0)
			UTEST_EQUAL("Number of Note Off Output Triggers", (*OutputNoteOffTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);
			UTEST_EQUAL("Number of Note On Output Triggers", (*OutputNoteOnTrigger)->NumTriggeredInBlock(), ExpectedTriggerCountNotPass);

			//check output values
			UTEST_EQUAL("Midi Note Number check", **OutputMidiNoteNumber, TestNoteNumberShouldPass);
			UTEST_EQUAL("Midi Velocity (int32) check", **OutputMidiVelocity, ExpectedVelocityNotPass);
			UTEST_EQUAL("Midi Normalized Velocity (float) check", **OutputNormalizedVelocity, ExpectedNormalizedVelocityNotPass);
			UTEST_EQUAL("Output frequency check", **OutputFrequency, ExpectedFrequencyOnPass);

		return true;
	}
}

#endif
