// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

REWINDDEBUGGERRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogRewindDebuggerRuntime, Log, All);


namespace RewindDebugger
{

	class REWINDDEBUGGERRUNTIME_API FRewindDebuggerRuntime
	{
	public:
		static void Initialize();
		static void Shutdown();
		static FRewindDebuggerRuntime* Instance() { return InternalInstance; }
			
		void StartRecording();
		void StopRecording();

		bool IsRecording() const { return bIsRecording; }

		FSimpleMulticastDelegate RecordingStarted;
		FSimpleMulticastDelegate ClearRecording;
		FSimpleMulticastDelegate RecordingStopped;
	private:
		bool bIsRecording = false;
		static FRewindDebuggerRuntime* InternalInstance;
	};
}
