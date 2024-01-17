// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTestGraphBuilder.h"
#include "HarmonixDsp/AudioBuffer.h"
#include "HarmonixMetasound/Common.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HarmonixMetasoundTests::MidiVoiceManagerNode
{
	using GraphBuilder = Metasound::Test::FNodeTestGraphBuilder;
	using namespace Metasound;
	using namespace Metasound::Frontend;
	using namespace HarmonixMetasound;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FMidiVoiceManagerCreateNodeTest,
		"Harmonix.Metasound.Nodes.MidiVoiceManager.CreateNode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
		bool FMidiVoiceManagerCreateNodeTest::RunTest(const FString&)
	{
		// Build graph with user-provided default value
		const GraphBuilder Builder;
		const FName NumVoicesInputName = "Num Voices";
		const FNodeHandle VoiceManagerNode = Builder.AddNode({ HarmonixMetasound::HarmonixNodeNamespace, "MidiVoiceManager", "" }, 0);
		const FNodeHandle NumVoicesInput = Builder.AddConstructorInput<int32>(NumVoicesInputName, 8);
		const FOutputHandle OutputToConnect = NumVoicesInput->GetOutputWithVertexName(NumVoicesInputName);
		const FInputHandle InputToConnect = VoiceManagerNode->GetInputWithVertexName(NumVoicesInputName);
		UTEST_TRUE("Connected inputs", InputToConnect->Connect(*OutputToConnect));

		// Build graph with vertex default
		const TUniquePtr<FMetasoundGenerator> Generator = GraphBuilder::MakeSingleNodeGraph(
			{ HarmonixMetasound::HarmonixNodeNamespace, "MidiVoiceManager", "" },
			0,
			48000,
			128);
		UTEST_TRUE("Generator created", Generator.IsValid());

		return true;

	}
}

#endif
