// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerRuntime/RewindDebuggerRuntime.h"

#include "Animation/AnimTrace.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "UObject/UObjectIterator.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "Features/IModularFeatures.h"

DEFINE_LOG_CATEGORY(LogRewindDebuggerRuntime)

namespace RewindDebugger
{

	FRewindDebuggerRuntime* FRewindDebuggerRuntime::InternalInstance = nullptr;

	void FRewindDebuggerRuntime::Initialize()
	{
		InternalInstance = new FRewindDebuggerRuntime();
	}
	
	void FRewindDebuggerRuntime::Shutdown()
	{
		delete InternalInstance;
		InternalInstance = nullptr;
	}

	static void IterateExtensions(TFunction<void(IRewindDebuggerRuntimeExtension* Extension)> IteratorFunction)
	{
		// update extensions
		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		const int32 NumExtensions = ModularFeatures.GetModularFeatureImplementationCount(IRewindDebuggerRuntimeExtension::ModularFeatureName);
		for (int32 ExtensionIndex = 0; ExtensionIndex < NumExtensions; ++ExtensionIndex)
		{
			IRewindDebuggerRuntimeExtension* Extension = static_cast<IRewindDebuggerRuntimeExtension*>(ModularFeatures.GetModularFeatureImplementation(IRewindDebuggerRuntimeExtension::ModularFeatureName, ExtensionIndex));
			IteratorFunction(Extension);
		}
	}
	
	static void DisableAllTraceChannels()
	{
		UE::Trace::EnumerateChannels([](const ANSICHAR* ChannelName, bool bEnabled, void*)
		{
			if (bEnabled)
			{
				FString ChannelNameFString(ChannelName);
				UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), false);
			}
		}
		, nullptr);
	}
	
	void FRewindDebuggerRuntime::StartRecording()
	{
#if OBJECT_TRACE_ENABLED
		// Clear caches  (maybe move these to plugins?)
		FObjectTrace::Reset();
#if ANIM_TRACE_ENABLED
		FAnimTrace::Reset();
#endif

		ClearRecording.Broadcast();
        	
        // Disable all trace channels, and then enable only the ones needed by RewindDebugger
        // for systems with RewindDebugger integration, they should enable their channel(s) in an Extension in "RecordingStarted"
        DisableAllTraceChannels();

		// Clear all buffered data and prevent data from previous recordings from leaking into the new recording
		FTraceAuxiliary::FOptions Options;
		Options.bExcludeTail = true;
	
		// FTraceAuxiliary::OnConnection.AddRaw(this, &FRewindDebugger::OnConnection); 
	
		FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("127.0.0.1"), TEXT(""), &Options, LogRewindDebuggerRuntime);
		
		UE::Trace::ToggleChannel(TEXT("Object"), true);
		UE::Trace::ToggleChannel(TEXT("ObjectProperties"), true);
		UE::Trace::ToggleChannel(TEXT("Frame"), true);
		
		bIsRecording = true;

		// update extensions
		IterateExtensions([](IRewindDebuggerRuntimeExtension* Extension)
			{
				Extension->RecordingStarted();
			}
		);
	
		// trace each play-in-editor world, and all the actors in it.
		for (TObjectIterator<UWorld> World; World; ++World)
		{
			FObjectTrace::ResetWorldElapsedTime(*World);
			
			TRACE_WORLD(*World);
				
			for (TActorIterator<AController> Iterator(*World); Iterator; ++Iterator)
			{
				if (APawn* Pawn = Iterator->GetPawn())
				{
					TRACE_PAWN_POSSESS(static_cast<UObject*>(*Iterator), static_cast<UObject*>(Pawn));
				}
			}
		}

		RecordingStarted.Broadcast();
	#endif // OBJECT_TRACE_ENABLED
	}
	
	void FRewindDebuggerRuntime::StopRecording()
	{
		if (bIsRecording)
		{
			// // update extensions
			IterateExtensions([](IRewindDebuggerRuntimeExtension* Extension)
				{
					Extension->RecordingStopped();
				}
			);
		
			bIsRecording = false;
		
			DisableAllTraceChannels();
			FTraceAuxiliary::Stop();
		
			RecordingStopped.Broadcast();
		}
	}


}
