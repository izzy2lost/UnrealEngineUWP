// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerSettings.h"
#include "UObject/WeakObjectPtr.h"


namespace TraceServices
{
	struct FFrame;
}

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
	void ClearSpawnedComponents();
	virtual void Clear(IRewindDebugger* RewindDebugger) override;
	
	void OnPIEResumed(bool bSimulating);
	void OnPIEStopped(bool bSimulating);
	void OnPIESingleStepped(bool bSimulating);

private:
	
	void ApplyPoseToMesh(const class IAnimationProvider* AnimationProvider, const IGameplayProvider* GameplayProvider, const TraceServices::FFrame& Frame,
		const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, USkeletalMeshComponent* MeshComponent, uint64 ObjectId, bool bQueueForReset, bool bApplyMesh);
	
	struct FMeshComponentResetData
	{
		TWeakObjectPtr<USkeletalMeshComponent> Component;
		FTransform RelativeTransform;
	};

	struct FSpawnedMeshComponentInfo
	{
		// mesh component object id
		uint64 id;
		// actor to hold the mesh component
		TObjectPtr<AActor> Actor;
		// mesh
		TObjectPtr<USkeletalMeshComponent> Component;
	};

	TMap<uint64, FSpawnedMeshComponentInfo> SpawnedMeshComponents;

	TMap<uint64, FMeshComponentResetData> MeshComponentsToReset;
	double LastScrubTime = 0.0;
};
