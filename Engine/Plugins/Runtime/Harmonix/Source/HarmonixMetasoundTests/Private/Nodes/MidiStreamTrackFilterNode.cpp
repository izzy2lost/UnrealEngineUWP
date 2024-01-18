// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTestGraphBuilder.h"
#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiAsset.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/Nodes/MidiStreamTrackFilterNode.h"
#include "HarmonixMidi/MidiMsg.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiStreamTrackFilterNode
{
	using GraphBuilder = Metasound::Test::FNodeTestGraphBuilder;
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace HarmonixMetasound;

	UMidiFile* BuildMidiFile()
	{
		// Make a midi file...
		UMidiFile* TheMidi = NewObject<UMidiFile>();
		// Set the initial tempo...
		TheMidi->GetSongMaps()->GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(120), 0);
		// Make two tracks to test filtering
		FMidiTrack* FirstTrack = TheMidi->AddTrack("FirstTrack");
		FMidiTrack* SecondTrack = TheMidi->AddTrack("SecondTrack");
		// Add midi events to the two tracks
		FMidiMsg FirstMessage = FMidiMsg::CreateNoteOn(1, 1, 1);		
		FMidiMsg SecondMessage = FMidiMsg::CreateNoteOn(1, 2, 2);
		FMidiEvent FirstEvent(0, FirstMessage);
		FMidiEvent SecondEvent(0, SecondMessage);		
		FirstTrack->AddEvent(FirstEvent);
		SecondTrack->AddEvent(SecondEvent);
		// Tell the midi file its tracks have been changed so it can recalculate song length data...
		TheMidi->TracksChanged();
		// Now round appropriately...
		TheMidi->ConformMidiFileLength(EMidiFileLengthConformOption::RoundUp);
		return TheMidi;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiStreamTrackIndexFunctionalityTest,
		"Harmonix.Metasound.Nodes.MidiStreamTrackFilterNode.TestFunctionality",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FMidiStreamTrackIndexFunctionalityTest::RunTest(const FString&)
	{
		//************************************************************************************************************************
		// Build the graph.
		//************************************************************************************************************************
		GraphBuilder Builder;
		Builder.AddOutput("AudioOut", GetMetasoundDataTypeName<FAudioBuffer>());

		// transport...
		Frontend::FNodeHandle TransportNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "TriggerToTransport", "" }, 0);
		UTEST_TRUE("Transport Node Created", TransportNode.Get().IsValid());
		// hoist up play trigger input...
		Builder.AddAndConnectDataReferenceInput(TransportNode, CommonPinNames::Inputs::TransportPlayName, GetMetasoundDataTypeName<FTrigger>());

		// midi player...
		Frontend::FNodeHandle MidiPlayerNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "MidiPlayer", "" }, 0);
		UTEST_TRUE("MidiPlayer Node Created", MidiPlayerNode.Get().IsValid());
		// hoist up midi asset input...
		Builder.AddAndConnectDataReferenceInput(MidiPlayerNode, CommonPinNames::Inputs::MidiFileAssetName, GetMetasoundDataTypeName<FMidiAsset>());

		// wire transport to midi player...
		bool ConnectionSuccess = Builder.ConnectNodes(TransportNode, CommonPinNames::Outputs::TransportName, MidiPlayerNode, CommonPinNames::Inputs::TransportName);
		UTEST_TRUE("Transport Out connected to MidiPlayer node", ConnectionSuccess);

		// Midi stream track index filter...
		FNodeHandle MidiStreamTrackFilterNode = Builder.AddNode(HarmonixMetasound::Nodes::MidiStreamTrackFilter::GetClassName(), 0);
		UTEST_TRUE("MidiStreamTrackFilter Node Created", MidiStreamTrackFilterNode.Get().IsValid());
		// hoist up filter input...
		Builder.AddAndConnectDataReferenceInput(MidiStreamTrackFilterNode, CommonPinNames::Inputs::MidiTrackIndexFilterSpecifierName, GetMetasoundDataTypeName<FString>());
		// hoist up output trigger...
		Builder.AddAndConnectDataReferenceOutput(MidiStreamTrackFilterNode, CommonPinNames::Outputs::MidiStreamName, GetMetasoundDataTypeName<FMidiStream>(), CommonPinNames::Outputs::MidiStreamName);

		// wire midi player to midi stream track index filter...
		ConnectionSuccess = Builder.ConnectNodes(MidiPlayerNode, CommonPinNames::Outputs::MidiStreamName, MidiStreamTrackFilterNode, CommonPinNames::Inputs::MidiStreamName);
		UTEST_TRUE("MidiPlayer Out connected to MidiStreamTrackFilter node", ConnectionSuccess);

		//************************************************************************************************************************
		// Make the generator.
		//************************************************************************************************************************
		constexpr int32 NumSamplesPerBlock = 480; // 10ms per block
		const TUniquePtr<FMetasoundGenerator> Generator = Builder.BuildGenerator(48000, NumSamplesPerBlock);
		UTEST_TRUE("Graph successfully built", Generator.IsValid());

		//************************************************************************************************************************
		// Get reference to outputs we want to check.
		//************************************************************************************************************************
		TOptional<FMidiStreamReadRef> OutputMidiStream = Generator->GetOutputReadReference<FMidiStream>(CommonPinNames::Outputs::MidiStreamName);
		UTEST_TRUE("Output exists", OutputMidiStream.IsSet());		

		//************************************************************************************************************************
		// Make initial inputs.
		//************************************************************************************************************************
		// Midi file to play...
		UMidiFile* MidiFile = BuildMidiFile();
		TSharedPtr<Audio::IProxyData> MidiFileProxy = MidiFile->CreateProxyData({ "MidiStreamTrackFilterTest" });
		Generator->SetInputValue<FMidiAsset>(CommonPinNames::Inputs::MidiFileAssetName, MidiFileProxy);

		// Single Track Index to Filter
		Generator->SetInputValue<FString>(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifierName, "2");

		// Trigger transport...
		Generator->ApplyToInputValue<FTrigger>(CommonPinNames::Inputs::TransportPlayName, [](FTrigger& TriggerIn) { TriggerIn.TriggerFrame(0); });

		//************************************************************************************************************************
		// Generate 10ms...
		//************************************************************************************************************************
		TAudioBuffer<float> Buffer{ Generator->GetNumChannels(), NumSamplesPerBlock, EAudioBufferCleanupMode::Delete };
		Generator->OnGenerateAudio(Buffer.GetRawChannelData(0), Buffer.GetNumTotalValidSamples());

		// There are 2 note-on messages in the midi stream, in two different tracks.
		// Ensure that the MidiStreamTrackFilter node correctly filtered out one track.
		int32 MidiMessageCount = 0;
		for (const FMidiStreamEvent& MidiStreamEvent : (*OutputMidiStream)->GetEventsInBlock())
		{
			if (MidiStreamEvent.MidiMessage.IsNoteOn())
			{
				++MidiMessageCount;
			}
		}

		UTEST_EQUAL("Number of midi events matches filtered", MidiMessageCount, 1);

		return true;
	}
}

#endif
