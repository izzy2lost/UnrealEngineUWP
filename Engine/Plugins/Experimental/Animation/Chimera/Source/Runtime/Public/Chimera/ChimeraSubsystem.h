// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Chimera/ChimeraLibrary.h"
#include "ChimeraSubsystem.generated.h"

struct FPoseSearchContinuingProperties;
class UAnimInstance;
class UPoseSearchDatabase;

namespace UE::Chimera
{
	struct FIsland;
	struct FSearchContext;

	struct FChimeraAvailabilityEx : public FChimeraAvailability
	{
		FChimeraAvailabilityEx(const FChimeraAvailability& InAvailability, FName InPoseHistoryName, const FAnimNode_PoseSearchHistoryCollector_Base* InHistoryCollector)
			: FChimeraAvailability(InAvailability)
			, PoseHistoryName(InPoseHistoryName)
			, HistoryCollector(InHistoryCollector)
		{
		}

		FString GetPoseHistoryName() const;
		const FAnimNode_PoseSearchHistoryCollector_Base* GetHistoryCollector(const UAnimInstance* AnimInstance) const;

	private:
		FName PoseHistoryName;
		const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector = nullptr;
	};
	typedef TMap<TWeakObjectPtr<UObject>, TArray<FChimeraAvailabilityEx>> FAvailabilityRequestsMap;
} // namespace UE::Chimera

// World subsystem accepting the publication of characters (via their AnimInstance(s)) FChimeraAvailability, representing the characters willingness to partecipate in an 
// interaction with other characters from the next frame forward via Query_AnyThread method.
//
// The same method will return the FChimeraBlueprintResult from the PREVIOUS Tick processing (categorization of FChimeraAvailability(s) in multiple FIsland(s)),
// to the requesting character, containing the animation to play at what time, and the assigned role to partecipate in the selected interaction within the assigned 
// FIsland

// Execution model and threading details:
/////////////////////////////////////////
// 
// - by calling UChimeraLibrary::ChimeraQuery_Pure(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance), characters publish their availabilities
//   to partecipate in interactions to the UChimeraSubsystem
// - UChimeraSubsystem::Tick processes those FChimeraAvailability(s) and creates/updates UE::Chimera::FIsland. For each FIsland it injects 
//   a tick prerequisite via FIsland::InjectToActor (that calls AddPrerequisite) to all the Actors in the same island.
//   NoTe: the next frame the execution will be:
//			for each island[k]
//			{
//				for each Actor[k][i]
//				{
//					Tick CharacterMovementComponent[k][i]
//				}
// 
//				Tick Island[k].PreTickFunction (that eventually generates the trajectories with all the updated CMCs)
//
//				Tick Actor[k][0].SkeletalMeshComponent (that performs the chimera queries for all the involved actors via DoSearch_AnyThread)
// 
//				Tick Island[k].PostTickFunction (currently just a threading fence for the execution of all the other SkeletalMeshComponent(s))
// 
//				for each Actor[k][i]
//				{
//					if (i != 0)
//						Tick SkeletalMeshComponent[k][i] (that DoSearch_AnyThread get the cached result calculated by Tick Actor[k][0].SkeletalMeshComponent)
//				}
//			}
// - next frame UChimeraLibrary::ChimeraQuery_Pure(TArray<FChimeraAvailability> Availabilities, UObject* AnimInstance), with the context of all the published 
//   availabilities and created islands, will find the associated FIsland to the AnimInstance and call FIsland::DoSearch_AnyThread 
//   (via UChimeraSubsystem::Query_AnyThread) that will perform ALL (YES, ALL, so the bigger the island the slower the execution) the motion matching searches
//   for all the possible Actors / databases / Roles permutations, and populate FIsland::SearchResults with ALL the results for the island.
//   Ultimately the ChimeraQuery_Pure will return the SearchResults associated to the requesting AnimInstance with information about what animation to play
//   at what time with wich Role.

UCLASS(Category="Animation|Chimera")
class CHIMERA_API UChimeraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UChimeraSubsystem* GetSubsystem_AnyThread(UObject* AnimInstance);

	virtual void Deinitialize() override;

	// it processes FChimeraAvailability(s) and creates/updates FIsland
	virtual void Tick(float DeltaSeconds) override;
	
	virtual TStatId GetStatId() const override;

	// publishing FChimeraAvailability(s) for the requesting character (AnimInstance) and getting the FChimeraBlueprintResult from the PREVIOUS Tick update
	// containing the animation to play at what time, and the assigned role to partecipate in the selected interaction
	// Either a PoseHistoryName or a HistoryCollector are required to perform the associated motion matching searches
	void Query_AnyThread(const TArrayView<const FChimeraAvailability> Availabilities, UObject* AnimInstance, 
		const FPoseSearchContinuingProperties& ContinuingProperties, FChimeraBlueprintResult& Result,
		FName PoseHistoryName, const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector, bool bValidateResultAgainstAvailabilities);
private:
	UE::Chimera::FIsland& CreateIsland();
	UE::Chimera::FIsland& GetAvailableIsland();

	void DestroyIsland(int32 Index);
	void DestroyAllIslands();
	void UninjectAllIslands();
	bool ValidateAllIslands() const;

	void PopulateContinuingProperties(UE::Chimera::FSearchContext& SearchContext, float DeltaSeconds) const;
	UE::Chimera::FIsland* FindIsland(UObject* InAnimInstance);
	
	void DebugDraw() const;

	UE::Chimera::FAvailabilityRequestsMap AvailabilityRequestsMap;
	FCriticalSection AvailabilityRequestsMapMutex;

	// array of groups of characters that needs to be anaylzed together for possible interactions
	TArray<UE::Chimera::FIsland*> Islands;

	// critical section to retrieve the subsystem in a thread safe manner
	static FCriticalSection RetrieveSubsystemMutex;
};