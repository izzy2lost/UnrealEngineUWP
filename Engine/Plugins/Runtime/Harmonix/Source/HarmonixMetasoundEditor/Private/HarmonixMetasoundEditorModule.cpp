// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasoundEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "MetasoundEditorModule.h"
#include "HarmonixMetasoundSlateStyle.h"
#include "MidiStepSequenceDetailCustomization.h"
#include "HarmonixMetasound/DataTypes/MidiStepSequence.h"
#include "AssetDefinition_MidiStepSequence.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

DEFINE_LOG_CATEGORY(LogHarmonixMetasoundEditor)

void FHarmonixMetasoundEditorModule::StartupModule()
{
	const HarmonixMetasoundEditor::FSlateStyle& Style = HarmonixMetasoundEditor::FSlateStyle::Get();
	Metasound::Editor::IMetasoundEditorModule& MetasoundEditorModule = FModuleManager::GetModuleChecked<Metasound::Editor::IMetasoundEditorModule>("MetasoundEditor");
	MetasoundEditorModule.RegisterPinType("MidiAsset");
	MetasoundEditorModule.RegisterPinType("MidiStepSequenceAsset");
	MetasoundEditorModule.RegisterPinType("FusionPatchAsset");
	MetasoundEditorModule.RegisterPinType("Enum:SubdivisionQuantizationType","Int32");
	MetasoundEditorModule.RegisterPinType("Enum:DelayFilterType", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:DelayStereoType", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:TimeSyncOption", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:DistortionType", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:Harmonix:BiquadFilterType", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:Distortion:FilterPasses", "Int32");
	MetasoundEditorModule.RegisterPinType("Enum:StdMidiControllerID", "Int32");

	MetasoundEditorModule.RegisterPinType("MidiStream", {}, {}, Style.GetMidiStreamConnectedIcon(), Style.GetMidiStreamDisconnectedIcon());
	MetasoundEditorModule.RegisterPinType("MidiClock", {}, {}, Style.GetMidiClockConnectedIcon(), Style.GetMidiClockDisconnectedIcon());
	MetasoundEditorModule.RegisterPinType("MusicTransport", {}, {}, Style.GetTransportConnectedIcon(), Style.GetTransportDisconnectedIcon());

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		UMidiStepSequence::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FMidiStepSequenceDetailCustomization::MakeInstance)
	);

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FHarmonixMetasoundEditorModule::ShutdownModule()
{

}

IMPLEMENT_MODULE(FHarmonixMetasoundEditorModule, HarmonixMetasoundEditor);

#undef LOCTEXT_NAMESPACE
