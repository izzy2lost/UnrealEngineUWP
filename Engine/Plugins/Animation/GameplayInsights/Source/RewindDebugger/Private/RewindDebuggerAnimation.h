// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerSettings.h"
#include "UObject/WeakObjectPtr.h"

// Rewind debugger extension for animation support
//  replay of animated pose data
//  updating animation blueprint debugger

class FRewindDebuggerAnimation : public IRewindDebuggerExtension
{
public:


	FRewindDebuggerAnimation();
	virtual ~FRewindDebuggerAnimation() {};
	void Initialize();
	void Shutdown();

	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) override;
	
	void OnPIEResumed(bool bSimulating);
	void OnPIEStopped(bool bSimulating);
	void OnPIESingleStepped(bool bSimulating);

private:
	struct FMeshComponentResetData
	{
		TWeakObjectPtr<USkeletalMeshComponent> Component;
		FTransform RelativeTransform;
	};

	TMap<uint64, FMeshComponentResetData> MeshComponentsToReset;
	double LastScrubTime = 0.0;
};
