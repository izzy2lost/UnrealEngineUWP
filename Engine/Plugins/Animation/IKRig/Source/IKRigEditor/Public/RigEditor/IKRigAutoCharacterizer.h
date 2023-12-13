// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Rig/IKRigDefinition.h"

enum class EPBIKLimitType : uint8;

enum class EPreferredAxis
{
	None,
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ,
};

// the central ground truth for standardized characterization labels used in Unreal
struct FCharacterizationStandard
{
	// standard biped chain names
	// these are Unreal's standardized retargeting chain labels
	
	// core
	static const FName Root;
	static const FName Spine;
	static const FName Neck;
	static const FName Head;
	// legs
	static const FName LeftLeg;
	static const FName RightLeg;
	// arms
	static const FName LeftClavicle;
	static const FName RightClavicle;
	static const FName LeftArm;
	static const FName RightArm;
	// left hand
	static const FName LeftThumbMetacarpal;
	static const FName LeftIndexMetacarpal;
	static const FName LeftMiddleMetacarpal;
	static const FName LeftRingMetacarpal;
	static const FName LeftPinkyMetacarpal;
	static const FName LeftThumb;
	static const FName LeftIndex;
	static const FName LeftMiddle;
	static const FName LeftRing;
	static const FName LeftPinky;
	// right hand
	static const FName RightThumbMetacarpal;
	static const FName RightIndexMetacarpal;
	static const FName RightMiddleMetacarpal;
	static const FName RightRingMetacarpal;
	static const FName RightPinkyMetacarpal;
	static const FName RightThumb;
	static const FName RightIndex;
	static const FName RightMiddle;
	static const FName RightRing;
	static const FName RightPinky;
	// left foot
	static const FName LeftBigToe;
	static const FName LeftIndexToe;
	static const FName LeftMiddleToe;
	static const FName LeftRingToe;
	static const FName LeftPinkyToe;
	// right foot
	static const FName RightBigToe;
	static const FName RightIndexToe;
	static const FName RightMiddleToe;
	static const FName RightRingToe;
	static const FName RightPinkyToe;

	// TODO add support for arbitrary bone chains (ie, 6-legged characters w/ LeftLegA, LeftLegB etc...)
	// FName GenerateStandardizedName(TArray<FName> BoneNames, LimbType, Side, LimbNum etc...)

	// standard bipedal IK goal names
	static const FName LeftHandIK;
	static const FName LeftFootIK;
	static const FName RightHandIK;
	static const FName RightFootIK;

	// TODO add support for arbitrary bone chains (ie, 6-legged characters w/ LeftLegA, LeftLegB etc...)
	// FName GenerateStandardizedIKGoalName(TArray<FName> BoneNames, LimbType, Side, LimbNum etc...)

	// standard bone settings for IK
	static constexpr float PelvisRotationStiffness = 0.95f;
	static constexpr float ClavicleRotationStiffness = 0.95f;
	static constexpr float FootRotationStiffness = 0.85f;
};

// clean names are used for comparison
// full names are used to resolve a retarget definition onto an actual skeleton (which may have prefixes)
enum class ECleanOrFullName
{
	Full,
	Clean
};

// an abstract representation of a skeleton including just their names and hierarchy
struct FAbstractHierarchy
{
	FAbstractHierarchy(USkeletalMesh* InMesh);
	
	FAbstractHierarchy(
		const TArray<FName>& InBones,
		const TArray<int32>& InParentIndices)
		: FullBoneNames(InBones),	ParentIndices(InParentIndices)
	{
		GenerateCleanBoneNames();
	};

	// get index of parent bone
	int32 GetParentIndex(const FName BoneName, const ECleanOrFullName NameType) const;

	// get the index from the name
	int32 GetBoneIndex(const FName BoneName, const ECleanOrFullName NameType) const;

	// get the name of a bone from it's index
	FName GetBoneName(const int32 BoneIndex, const ECleanOrFullName NameType) const;

	// returns true if PotentialChild is within the branch under Parent
	bool IsChildOf(const FName Parent, const FName PotentialChild, const ECleanOrFullName NameType) const;

	// outputs a list of bone names in order from root to leaf that reside between start and end (inclusive)
	void GetBonesInChain(
		const FName Start,
		const FName End,
		const ECleanOrFullName NameType,
		TArray<FName>& OutBonesInChain) const;

	// read-only access to the full names of the bones
	const TArray<FName>& GetBoneNames(const ECleanOrFullName NameType) const;

	// get a list of the immediate children of BoneName
	void GetImmediateChildren(
		const FName& BoneName,
		const ECleanOrFullName NameType,
		TArray<FName>& OutChildren) const;

	// returns an integer score representing the number of bones in Other matching this
	// OutMissingBones will contain the names of all bones in the template that were not found in the input
	// OutBonesWithDifferentParent will contain the names of all bones in the input that have a different parent in the template
	void Compare(
		const FAbstractHierarchy& OtherHierarchy,
		TArray<FName>& OutMissingBones,
		TArray<FName>& OutBonesWithDifferentParent,
		int32& OutNumMatchingBones,
		float& OutPercentOfTemplateMatched) const;

	// return a list of all bones in this hierarchy that are NOT in OtherHierarchy
	void FindBonesNotInOther(
		const FAbstractHierarchy& OtherHierarchy,
		TArray<FName>& OutMissingBones,
		TArray<FName>& OutBonesWithDifferentParent) const;
	
	// return the total number of bones that are also in OtherHierarchy with the same parent
	int32 GetNumMatchingBones(const FAbstractHierarchy& OtherHierarchy) const;

private:
	// strips extra junk off the bone names to make direct comparisons
	void GenerateCleanBoneNames();

	// checks if bone exists and has the same parent in both hierarchies (expects the clean bone name for apples-apples comparison)
	void CheckBoneExistsAndHasSameParent(
		const FName& CleanBoneName,
		const FAbstractHierarchy& OtherHierarchy,
		bool& OutExists,
		bool& OutSameParent) const;

	// returns the largest prefix that is common to ALL names in the input array
	static FString FindLargestCommonPrefix(const TArray<FName>& ArrayOfNames);

	TArray<FName> FullBoneNames;
	TArray<FName> CleanBoneNames;
	TArray<int32> ParentIndices;
};

// the results of auto characterizing an input skeletal mesh
struct FAutoCharacterizeResults
{
	// the retarget root and a list of bone chains
	FRetargetDefinition RetargetDefinition;
	// did the auto characterizer use a template or procedurally generate the retarget definition?
	bool bUsedTemplate = false;
	// the template that most closely matched with the input skeleton
	FName BestTemplateName = NAME_None;
	// the number of bones that matched
	int32 BestNumMatchingBones = 0;
	// the score of how closely the template matched the input skeleton (0-1)
	float BestPercentageOfTemplateScore = 0.0f;
	// bones that were in the template, but not found in the input
	TArray<FName> MissingBones;
	// bones that do not have the same parent as the equivalent in the template
	TArray<FName> BonesWithMissingParent;
	// number of bones we extended the spine/neck chains beyond what the template provides
	int32 NumBonesAddedToSpineChain = 0;
	int32 NumBonesAddedToNeckChain = 0;
};

struct FBoneSettingsForIK
{
	FName BoneToApplyTo;
	float RotationStiffness = 0.f;
	EPreferredAxis PreferredAxis = EPreferredAxis::None;
	bool bIsHinge = false;
	bool bExcluded = false;

	FVector GetPreferredAxisAsAngles() const;
	void LockNonPreferredAxes(EPBIKLimitType& OutX, EPBIKLimitType& OutY, EPBIKLimitType& OutZ) const;
	
private:
	static constexpr float PreferredAngleMagnitude = 90.f;
};

struct FAllBoneSettingsForIK
{
	void SetPreferredAxis(const FName BoneName, const EPreferredAxis PreferredAxis, const bool bTreatAsHinge=true);
	void SetRotationStiffness(const FName BoneName, const float RotationStiffness);
	void SetExcluded(const FName BoneName, const bool bExclude);
	const TArray<FBoneSettingsForIK>& GetBoneSettings() const { return AllBoneSettings; };

private:
	FBoneSettingsForIK& GetOrAddBoneSettings(const FName BoneName);
	TArray<FBoneSettingsForIK> AllBoneSettings;
};

// a hard coded template representing a "known" hierarchy that is used in the world (ie UE5 Mannequin, Fortnite skeleton etc..)
// contains the recommended retarget definition to use for this template, including the retarget root, retarget chains and bone settings
struct FTemplateHierarchy
{
	FTemplateHierarchy(const FName& InName, const FAbstractHierarchy& InHierarchy) : Name(InName), Hierarchy(InHierarchy){}

	FName Name;
	FAbstractHierarchy Hierarchy;
	FRetargetDefinition RetargetDefinition;
	FAllBoneSettingsForIK BoneSettingsForIK;
};

// a collection of FTemplateHierarchy to compare against
struct FKnownTemplateHierarchies
{
	FKnownTemplateHierarchies();

	// iterate over all the known hierarchies, find the one that is closest to the given hierarchy
	// outputs the FAutoCharacterizeResults which includes a retarget definition to use
	void GetClosestMatchingKnownHierarchy(const FAbstractHierarchy& InHierarchy, FAutoCharacterizeResults& Results) const;

	// get a pointer to a template hierarchy by name
	const FTemplateHierarchy* GetKnownHierarchyByName(const FName Name) const;

private:

	// add a template hierarchy
	FTemplateHierarchy& AddTemplateHierarchy(const FName& Label, const TArray<FName>& BoneNames, const TArray<int32>& ParentIndices);
	
	TArray<FTemplateHierarchy> KnownHierarchies;
};

// contains all the known hierarchies in popular usage, and provides a function to compare any given Skeletal Mesh against them
// if a template is matched to the input skeleton, then the retarget definition is adapted to the given skeleton
struct FAutoCharacterizer
{
	FAutoCharacterizer() = default;

	// call this function with any skeletal mesh to auto-generate a retarget definition for it.
	// the results includes the retarget definition itself, as well as the scores indicating how closely the skeleton was matched to a known hierarchy
	void GenerateRetargetDefinitionFromMesh(USkeletalMesh* Mesh, FAutoCharacterizeResults& Results) const;

	// get read-only access to a template by name
	const FTemplateHierarchy* GetKnownTemplateHierarchy(const FName& TemplateName) const;

private:

	// adapts the retarget definition of a template to apply to a different hierarchy
	// may require removing chains that are not valid on the target hierarchy and changing start/end bones to better match it
	void AdaptTemplateToHierarchy(
		const FTemplateHierarchy& Template,
		const FAbstractHierarchy& TargetHierarchy,
		FAutoCharacterizeResults& Results) const;

	// expand the given bone chain to include bones beyond the end bone specified by the template (useful for Spines and Necks)
	// returns the number of bones that the chain was expanded by
	int32 ExpandChain(FBoneChain* ChainToExpand, const FAbstractHierarchy& Hierarchy) const;

	// outputs the closest name to NameToMatch in NamesToCheck. OutBestScore ranges from 0-1 as percentage of string that matched NameToMatch.
	void FindClosestNameInArray(
		const FName& NameToMatch,
		const TArray<FName>& NamesToCheck,
		FName& OutClosestName,
		float& OutBestScore) const;

	// all the known templates to compare against
	FKnownTemplateHierarchies KnownHierarchies;
};
