// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigAutoCharacterizer.h"
#include "Engine/SkeletalMesh.h"
#include "Algo/LevenshteinDistance.h"

#define LOCTEXT_NAMESPACE "AutoCharacterizer"

// core
const FName FCharacterizationStandard::Root = FName("Root");
const FName FCharacterizationStandard::Spine = FName("Spine");
const FName FCharacterizationStandard::Neck = FName("Neck");
const FName FCharacterizationStandard::Head = FName("Head");
// legs
const FName FCharacterizationStandard::LeftLeg = FName("LeftLeg");
const FName FCharacterizationStandard::RightLeg = FName("RightLeg");
// arms
const FName FCharacterizationStandard::LeftClavicle = FName("LeftClavicle");
const FName FCharacterizationStandard::RightClavicle = FName("RightClavicle");
const FName FCharacterizationStandard::LeftArm = FName("LeftArm");
const FName FCharacterizationStandard::RightArm = FName("RightArm");
// left hand
const FName FCharacterizationStandard::LeftThumbMetacarpal = FName("LeftThumbMetacarpal");
const FName FCharacterizationStandard::LeftIndexMetacarpal = FName("LeftIndexMetacarpal");
const FName FCharacterizationStandard::LeftMiddleMetacarpal = FName("LeftMiddleMetacarpal");
const FName FCharacterizationStandard::LeftRingMetacarpal = FName("LeftRingMetacarpal");
const FName FCharacterizationStandard::LeftPinkyMetacarpal = FName("LeftPinkyMetacarpal");
const FName FCharacterizationStandard::LeftThumb = FName("LeftThumb");
const FName FCharacterizationStandard::LeftIndex = FName("LeftIndex");
const FName FCharacterizationStandard::LeftMiddle = FName("LeftMiddle");
const FName FCharacterizationStandard::LeftRing = FName("LeftRing");
const FName FCharacterizationStandard::LeftPinky = FName("LeftPinky");
// right hand
const FName FCharacterizationStandard::RightThumbMetacarpal = FName("RightThumbMetacarpal");
const FName FCharacterizationStandard::RightIndexMetacarpal = FName("RightIndexMetacarpal");
const FName FCharacterizationStandard::RightMiddleMetacarpal = FName("RightMiddleMetacarpal");
const FName FCharacterizationStandard::RightRingMetacarpal = FName("RightRingMetacarpal");
const FName FCharacterizationStandard::RightPinkyMetacarpal = FName("RightPinkyMetacarpal");
const FName FCharacterizationStandard::RightThumb = FName("RightThumb");
const FName FCharacterizationStandard::RightIndex = FName("RightIndex");
const FName FCharacterizationStandard::RightMiddle = FName("RightMiddle");
const FName FCharacterizationStandard::RightRing = FName("RightRing");
const FName FCharacterizationStandard::RightPinky = FName("RightPinky");
// left foot
const FName FCharacterizationStandard::LeftBigToe = FName("LeftBigToe");
const FName FCharacterizationStandard::LeftIndexToe = FName("LeftIndexToe");
const FName FCharacterizationStandard::LeftMiddleToe = FName("LeftMiddleToe");
const FName FCharacterizationStandard::LeftRingToe = FName("LeftRingToe");
const FName FCharacterizationStandard::LeftPinkyToe = FName("LeftPinkyToe");
// right foot
const FName FCharacterizationStandard::RightBigToe = FName("RightBigToe");
const FName FCharacterizationStandard::RightIndexToe = FName("RightIndexToe");
const FName FCharacterizationStandard::RightMiddleToe = FName("RightMiddleToe");
const FName FCharacterizationStandard::RightRingToe = FName("RightRingToe");
const FName FCharacterizationStandard::RightPinkyToe = FName("RightPinkyToe");

FAbstractHierarchy::FAbstractHierarchy(USkeletalMesh* InMesh)
{
	if (!InMesh)
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = InMesh->GetRefSkeleton();
	
	FullBoneNames.Reserve(RefSkeleton.GetNum());
	ParentIndices.Reserve(RefSkeleton.GetNum());
	for (int32 BoneIndex=0; BoneIndex<RefSkeleton.GetNum(); ++BoneIndex)
	{
		FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		FullBoneNames.Add(BoneName);
		ParentIndices.Add(ParentIndex);
	}

	GenerateCleanBoneNames();
}

int32 FAbstractHierarchy::GetParentIndex(const FName BoneName, const ECleanOrFullName NameType) const
{
	const int32 BoneIndex = GetBoneIndex(BoneName, NameType);
	if (ParentIndices.IsValidIndex(BoneIndex))
	{
		return ParentIndices[BoneIndex];
	}
	return INDEX_NONE;
}

int32 FAbstractHierarchy::GetBoneIndex(const FName BoneName, const ECleanOrFullName NameType) const
{
	return GetBoneNames(NameType).Find(BoneName);
}

FName FAbstractHierarchy::GetBoneName(const int32 BoneIndex, const ECleanOrFullName NameType) const
{
	if (!ensure(ParentIndices.IsValidIndex(BoneIndex)))
	{
		return NAME_None;
	}
	
	return GetBoneNames(NameType)[BoneIndex];
}

bool FAbstractHierarchy::IsChildOf(const FName InParent, const FName PotentialChild, const ECleanOrFullName NameType) const
{
	const TArray<FName>& BoneNames = GetBoneNames(NameType);
	const bool bParentExists = BoneNames.Find(InParent) != INDEX_NONE;
	const bool bPotentialChildExists = BoneNames.Find(PotentialChild) != INDEX_NONE;
	if (!(bParentExists && bPotentialChildExists))
	{
		// can't be a child of parent unless both exist
		return false;
	}
	
	int32 ParentIndex = GetParentIndex(PotentialChild, NameType);
	while(ParentIndex != INDEX_NONE)
	{
		FName ParentName = BoneNames[ParentIndex];
		if (ParentName == InParent)
		{
			return true;
		}
		ParentIndex = GetParentIndex(ParentName, NameType);
	}
	return false;
}

void FAbstractHierarchy::GetBonesInChain(
	const FName Start,
	const FName End,
	const ECleanOrFullName NameType,
	TArray<FName>& OutBonesInChain) const
{
	OutBonesInChain.Reset();

	// optionally return either the full names or clean names
	TArray<FName> BoneNames = GetBoneNames(NameType);

	// can form a chain if either end doesn't exist
	const int32 StartBoneIndex = BoneNames.Find(Start);
	const int32 EndBoneIndex = BoneNames.Find(End);
	if (StartBoneIndex == INDEX_NONE || EndBoneIndex == INDEX_NONE)
	{
		return;
	}

	// single bone chain?
	if (Start == End)
	{
		OutBonesInChain.Add(Start);
		return;
	}

	// bones must be a valid chain in the hierarchy
	if (!IsChildOf(Start, End, NameType))
	{
		return;
	}

	// gather list of bones from end to start
	OutBonesInChain.Add(End);
	int32 ParentIndex = GetParentIndex(End, NameType);
	while(ParentIndex != StartBoneIndex)
	{
		FName ParentName = BoneNames[ParentIndex];
		OutBonesInChain.Add(ParentName);
		ParentIndex = GetParentIndex(ParentName, NameType);
	}
	OutBonesInChain.Add(Start);
	
	// we want root to leaf order
	Algo::Reverse(OutBonesInChain);
}

const TArray<FName>& FAbstractHierarchy::GetBoneNames(const ECleanOrFullName NameType) const
{
	return NameType == ECleanOrFullName::Clean ? CleanBoneNames : FullBoneNames;
}

void FAbstractHierarchy::GetImmediateChildren(
	const FName& BoneName,
	const ECleanOrFullName NameType,
	TArray<FName>& OutChildren) const
{
	const int32 ParentIndexToFind = GetBoneIndex(BoneName, NameType);
	const TArray<FName>& BoneNames = GetBoneNames(NameType);
	for (int32 BoneIndex=0; BoneIndex<ParentIndices.Num(); ++BoneIndex)
	{
		if (ParentIndices[BoneIndex] == ParentIndexToFind)
		{
			OutChildren.Add(BoneNames[BoneIndex]);
		}
	}
}

void FAbstractHierarchy::Compare(
	const FAbstractHierarchy& OtherHierarchy,
	TArray<FName>& OutMissingBones,
	TArray<FName>& OutBonesWithDifferentParent,
	int32& OutNumMatchingBones,
	float& OutPercentOfTemplateMatched) const
{
	OutMissingBones.Reset();
	OutBonesWithDifferentParent.Reset();
	OutNumMatchingBones = 0;
	OutPercentOfTemplateMatched = 0.f;
	
	if (OtherHierarchy.CleanBoneNames.IsEmpty())
	{
		// avoid divide by zero, empty hierarchy matches with anything
		return;
	}
	
	// calculate a simple score with equal weight for number of matching bones and parents
	// this score represents what percentage of the template is found in the input hierarchy
	FindBonesNotInOther(OtherHierarchy, OutMissingBones, OutBonesWithDifferentParent);
	const float NumBonesTotal = CleanBoneNames.Num();
	const float NumMatchingBones = NumBonesTotal - OutMissingBones.Num();
	const float NumMatchingParents = NumBonesTotal - OutBonesWithDifferentParent.Num();
	const float BoneMatchScore = NumMatchingBones / NumBonesTotal;
	const float ParentMatchScore = NumMatchingParents / NumBonesTotal;
	constexpr float InvNumScores = 1.0f / 2.f;
	OutPercentOfTemplateMatched = (BoneMatchScore + ParentMatchScore) * InvNumScores;
	
	// determine how many bones in other match with this template
	// this is an absolute score that indicates which template would maximize the number of retargeted bones
	OutNumMatchingBones = OtherHierarchy.GetNumMatchingBones(*this);
}

void FAbstractHierarchy::FindBonesNotInOther(
	const FAbstractHierarchy& OtherHierarchy,
	TArray<FName>& OutMissingBones,
	TArray<FName>& OutBonesWithDifferentParent) const
{
	OutMissingBones.Reset();
	OutBonesWithDifferentParent.Reset();
	
	for (const FName BoneName : CleanBoneNames)
	{
		bool bExists;
		bool bHasSameParent;
		CheckBoneExistsAndHasSameParent(BoneName, OtherHierarchy, bExists, bHasSameParent);

		if (!bExists)
		{
			OutMissingBones.Add(BoneName);
			continue;
		}

		if (!bHasSameParent)
		{
			OutBonesWithDifferentParent.Add(BoneName);
		}
	}
}

int32 FAbstractHierarchy::GetNumMatchingBones(const FAbstractHierarchy& OtherHierarchy) const
{
	int32 NumMatchingBones = 0;
	
	for (const FName BoneName : CleanBoneNames)
	{
		bool bExists;
		bool bHasSameParent;
		CheckBoneExistsAndHasSameParent(BoneName, OtherHierarchy, bExists, bHasSameParent);

		// increment score when bone exists in both with same parent in both
		NumMatchingBones += bExists && bHasSameParent ? 1 : 0;
	}
	
	return NumMatchingBones;
}

void FAbstractHierarchy::CheckBoneExistsAndHasSameParent(
	const FName& CleanBoneName,
	const FAbstractHierarchy& OtherHierarchy,
	bool& OutExists,
	bool& OutSameParent) const
{
	OutExists = false;
	OutSameParent = false;
	
	const int32 OtherBoneIndex = OtherHierarchy.GetBoneIndex(CleanBoneName, ECleanOrFullName::Clean);
	if (OtherBoneIndex == INDEX_NONE)
	{
		return;
	}
	
	OutExists = true;
	
	// check if parents are the same
	const int32 ThisParentIndex = GetParentIndex(CleanBoneName, ECleanOrFullName::Clean);
	const int32 OtherParentIndex = OtherHierarchy.GetParentIndex(CleanBoneName, ECleanOrFullName::Clean);
	const FName ThisParentName = (ThisParentIndex != INDEX_NONE) ? CleanBoneNames[ThisParentIndex] : NAME_None;
	const FName OtherParentName = (OtherParentIndex != INDEX_NONE) ? OtherHierarchy.CleanBoneNames[OtherParentIndex] : NAME_None;
	OutSameParent = ThisParentName == OtherParentName;
}

void FAbstractHierarchy::GenerateCleanBoneNames()
{
	// copy full names to make clean names from
	CleanBoneNames = FullBoneNames;
	
	const FString CommonPrefix = FindLargestCommonPrefix(CleanBoneNames);
	if (CommonPrefix.Len() == 0)
	{
		return;
	}

	// skeleton was prefixed, so remove the prefix from all the clean bone names
	for (int32 BoneIndex=0; BoneIndex<CleanBoneNames.Num(); ++BoneIndex)
	{
		FString BoneNameStr = CleanBoneNames[BoneIndex].ToString();
		BoneNameStr.RemoveFromStart(CommonPrefix);
		CleanBoneNames[BoneIndex] = FName(BoneNameStr);
	}
}

FString FAbstractHierarchy::FindLargestCommonPrefix(const TArray<FName>& ArrayOfNames)
{
	if (ArrayOfNames.Num() == 0)
	{
		return TEXT("");
	}

	// convert to strings
	TArray<FString> StrArray;
	for (const FName& Name : ArrayOfNames)
	{
		StrArray.Add(Name.ToString());
	}

	// sort the array to compare the first and last strings
	StrArray.Sort();

	FString FirstStr = StrArray[0];
	FString LastStr = StrArray.Last();
	FString CommonPrefix = TEXT("");
	for (int32 i = 0; i < FirstStr.Len(); ++i)
	{
		if (i < LastStr.Len() && FirstStr[i] == LastStr[i])
		{
			CommonPrefix.AppendChar(FirstStr[i]);
		}
		else
		{
			break;
		}
	}

	return CommonPrefix;
}

FKnownTemplateHierarchies::FKnownTemplateHierarchies()
{
	// TODO in the future we can make templates extensible.
	
	// UE4 Mannequin
	static FName UE4_Mannequin = "UE4 Mannequin";
	static TArray<FName> UE4_Mannequin_Bones = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "index_01_l", "index_02_l", "index_03_l", "middle_01_l", "middle_02_l", "middle_03_l", "pinky_01_l", "pinky_02_l", "pinky_03_l", "ring_01_l", "ring_02_l", "ring_03_l", "thumb_01_l", "thumb_02_l", "thumb_03_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "index_01_r", "index_02_r", "index_03_r", "middle_01_r", "middle_02_r", "middle_03_r", "pinky_01_r", "pinky_02_r", "pinky_03_r", "ring_01_r", "ring_02_r", "ring_03_r", "thumb_01_r", "thumb_02_r", "thumb_03_r", "neck_01", "head", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"};
	static TArray<int32> UE4_Mannequin_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 8, 12, 13, 8, 15, 16, 8, 18, 19, 8, 21, 22, 4, 24, 25, 26, 27, 28, 29, 27, 31, 32, 27, 34, 35, 27, 37, 38, 27, 40, 41, 4, 43, 1, 45, 46, 47, 1, 49, 50, 51};
	FRetargetDefinition UE4_Mannequin_Retarget;
	// core
	UE4_Mannequin_Retarget.RootBone = FName("pelvis");
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("root"), FName("root"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("spine_01"), FName("spine_02"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neck_01"), FName("neck_01"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("head"), FName("head"));
	// left
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("thigh_l"), FName("ball_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("clavicle_l"), FName("clavicle_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("upperarm_l"), FName("hand_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("thumb_01_l"), FName("thumb_03_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("index_01_l"), FName("index_03_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("middle_01_l"), FName("middle_03_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("ring_01_l"), FName("ring_03_l"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("pinky_01_l"), FName("pinky_03_l"));
	// right
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("thigh_r"), FName("ball_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("clavicle_r"), FName("clavicle_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("upperarm_r"), FName("hand_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("thumb_01_r"), FName("thumb_03_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("index_01_r"), FName("index_03_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("middle_01_r"), FName("middle_03_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("ring_01_r"), FName("ring_03_r"));
	UE4_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("pinky_01_r"), FName("pinky_03_r"));
	AddTemplateHierarchy(UE4_Mannequin, UE4_Mannequin_Bones, UE4_Mannequin_ParentIndices, UE4_Mannequin_Retarget);

	// UE5 Mannequin
	static FName UE5_Mannequin = "UE5 Mannequin";
	static TArray<FName> UE5_Mannequin_Bones = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "neck_02", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "index_metacarpal_l", "index_01_l", "index_02_l", "index_03_l", "middle_metacarpal_l", "middle_01_l", "middle_02_l", "middle_03_l", "thumb_01_l", "thumb_02_l", "thumb_03_l", "pinky_metacarpal_l", "pinky_01_l", "pinky_02_l", "pinky_03_l", "ring_metacarpal_l", "ring_01_l", "ring_02_l", "ring_03_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "pinky_metacarpal_r", "pinky_01_r", "pinky_02_r", "pinky_03_r", "ring_metacarpal_r", "ring_01_r", "ring_02_r", "ring_03_r", "middle_metacarpal_r", "middle_01_r", "middle_02_r", "middle_03_r", "index_metacarpal_r", "index_01_r", "index_02_r", "index_03_r", "thumb_01_r", "thumb_02_r", "thumb_03_r", "thigh_r", "calf_r", "foot_r", "ball_r", "thigh_l", "calf_l", "foot_l", "ball_l"};
	static TArray<int32> UE5_Mannequin_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 6, 10, 11, 12, 13, 14, 15, 16, 13, 18, 19, 20, 13, 22, 23, 13, 25, 26, 27, 13, 29, 30, 31, 6, 33, 34, 35, 36, 37, 38, 39, 36, 41, 42, 43, 36, 45, 46, 47, 36, 49, 50, 51, 36, 53, 54, 1, 56, 57, 58, 1, 60, 61, 62};
	FRetargetDefinition UE5_Mannequin_Retarget;
	// core
	UE5_Mannequin_Retarget.RootBone = FName("pelvis");
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("root"), FName("root"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("spine_01"), FName("spine_02"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neck_01"), FName("neck_02"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("head"), FName("head"));
	// left
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("thigh_l"), FName("ball_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("clavicle_l"), FName("clavicle_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("upperarm_l"), FName("hand_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexMetacarpal, FName("index_metacarpal_l"), FName("index_metacarpal_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddleMetacarpal, FName("middle_metacarpal_l"), FName("middle_metacarpal_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftRingMetacarpal, FName("ring_metacarpal_l"), FName("ring_metacarpal_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyMetacarpal, FName("pinky_metacarpal_l"), FName("pinky_metacarpal_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("thumb_01_l"), FName("thumb_03_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("index_01_l"), FName("index_03_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("middle_01_l"), FName("middle_03_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("ring_01_l"), FName("ring_03_l"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("pinky_01_l"), FName("pinky_03_l"));
	// right
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("thigh_r"), FName("ball_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("clavicle_r"), FName("clavicle_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("upperarm_r"), FName("hand_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexMetacarpal, FName("index_metacarpal_r"), FName("index_metacarpal_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddleMetacarpal, FName("middle_metacarpal_r"), FName("middle_metacarpal_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightRingMetacarpal, FName("ring_metacarpal_r"), FName("ring_metacarpal_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyMetacarpal, FName("pinky_metacarpal_r"), FName("pinky_metacarpal_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("thumb_01_r"), FName("thumb_03_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("index_01_r"), FName("index_03_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("middle_01_r"), FName("middle_03_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("ring_01_r"), FName("ring_03_r"));
	UE5_Mannequin_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("pinky_01_r"), FName("pinky_03_r"));
	AddTemplateHierarchy(UE5_Mannequin, UE5_Mannequin_Bones, UE5_Mannequin_ParentIndices, UE5_Mannequin_Retarget);
	
	// Daz 3d
	static FName Daz_3d = "Daz_3d";
	static TArray<FName> Daz_3d_Bones = {"hip", "pelvis", "lThighBend", "lThighTwist", "lShin", "lFoot", "lMetatarsals", "lToe", "lSmallToe4", "lSmallToe4_2", "lSmallToe3", "lSmallToe3_2", "lSmallToe2", "lSmallToe2_2", "lSmallToe1", "lSmallToe1_2", "lBigToe", "lBigToe_2", "lHeel", "rThighBend", "rThighTwist", "rShin", "rFoot", "rMetatarsals", "rToe", "rSmallToe4", "rSmallToe4_2", "rSmallToe3", "rSmallToe3_2", "rSmallToe2", "rSmallToe2_2", "rSmallToe1", "rSmallToe1_2", "rBigToe", "rBigToe_2", "rHeel", "abdomenLower", "abdomenUpper", "chestLower", "chestUpper", "lCollar", "lShldrBend", "lShldrTwist", "lForearmBend", "lForearmTwist", "lHand", "lThumb1", "lThumb2", "lThumb3", "lCarpal1", "lIndex1", "lIndex2", "lIndex3", "lCarpal2", "lMid1", "lMid2", "lMid3", "lCarpal3", "lRing1", "lRing2", "lRing3", "lCarpal4", "lPinky1", "lPinky2", "lPinky3", "rCollar", "rShldrBend", "rShldrTwist", "rForearmBend", "rForearmTwist", "rHand", "rThumb1", "rThumb2", "rThumb3", "rCarpal1", "rIndex1", "rIndex2", "rIndex3", "rCarpal2", "rMid1", "rMid2", "rMid3", "rCarpal3", "rRing1", "rRing2", "rRing3", "rCarpal4", "rPinky1", "rPinky2", "rPinky3", "neckLower", "neckUpper", "head"};
	static TArray<int32> Daz_3d_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 6, 8, 6, 10, 6, 12, 6, 14, 6, 16, 5, 1, 19, 20, 21, 22, 23, 23, 25, 23, 27, 23, 29, 23, 31, 23, 33, 22, 0, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 45, 49, 50, 51, 45, 53, 54, 55, 45, 57, 58, 59, 45, 61, 62, 63, 39, 65, 66, 67, 68, 69, 70, 71, 72, 70, 74, 75, 76, 70, 78, 79, 80, 70, 82, 83, 84, 70, 86, 87, 88, 39, 90, 91};
	FRetargetDefinition Daz_3d_Retarget;
	// core
	Daz_3d_Retarget.RootBone = FName("hip");
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("abdomenLower"), FName("chestUpper"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neckLower"), FName("neckUpper"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("head"), FName("head"));
	// left
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("lThighBend"), FName("lToe"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("lCollar"), FName("lCollar"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("lShldrBend"), FName("lHand"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexMetacarpal, FName("lCarpal1"), FName("lCarpal1"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddleMetacarpal, FName("lCarpal2"), FName("lCarpal2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftRingMetacarpal, FName("lCarpal3"), FName("lCarpal3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyMetacarpal, FName("lCarpal4"), FName("lCarpal4"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("lThumb1"), FName("lThumb3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("lIndex1"), FName("lIndex3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("lMid1"), FName("lMid3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("lRing1"), FName("lRing3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("lPinky1"), FName("lPinky3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftBigToe, FName("lBigToe"), FName("lBigToe_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexToe, FName("lSmallToe1"), FName("lSmallToe1_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddleToe, FName("lSmallToe2"), FName("lSmallToe2_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftRingToe, FName("lSmallToe3"), FName("lSmallToe3_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyToe, FName("lSmallToe4"), FName("lSmallToe4_2"));
	// right
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("rThighBend"), FName("rToe"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("rCollar"), FName("rCollar"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("rShldrBend"), FName("rHand"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexMetacarpal, FName("rCarpal1"), FName("rCarpal1"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddleMetacarpal, FName("rCarpal2"), FName("rCarpal2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightRingMetacarpal, FName("rCarpal3"), FName("rCarpal3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyMetacarpal, FName("rCarpal4"), FName("rCarpal4"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("rThumb1"), FName("rThumb3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("rIndex1"), FName("rIndex3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("rMid1"), FName("rMid3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("rRing1"), FName("rRing3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("rPinky1"), FName("rPinky3"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightBigToe, FName("rBigToe"), FName("rBigToe_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexToe, FName("rSmallToe1"), FName("rSmallToe1_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddleToe, FName("rSmallToe2"), FName("rSmallToe2_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightRingToe, FName("rSmallToe3"), FName("rSmallToe3_2"));
	Daz_3d_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyToe, FName("rSmallToe4"), FName("rSmallToe4_2"));
	AddTemplateHierarchy(Daz_3d, Daz_3d_Bones, Daz_3d_ParentIndices, Daz_3d_Retarget);

	// Mixamo
	static FName Mixamo = "Mixamo";
	static TArray<FName> Mixamo_Bones = {"Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "HeadTop_End", "RightShoulder", "RightArm", "RightForeArm", "RightHand", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3", "RightHandIndex4", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky1", "RightHandPinky2", "RightHandPinky3", "RightHandPinky4", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandIndex4", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3", "LeftHandPinky4", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase", "RightToe_End", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "LeftToe_End"};
	static TArray<int32> Mixamo_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 3, 7, 8, 9, 10, 11, 12, 13, 10, 15, 16, 17, 10, 19, 20, 21, 10, 23, 24, 25, 10, 27, 28, 29, 3, 31, 32, 33, 34, 35, 36, 37, 34, 39, 40, 41, 34, 43, 44, 45, 34, 47, 48, 49, 34, 51, 52, 53, 0, 55, 56, 57, 58, 0, 60, 61, 62, 63};
	FRetargetDefinition Mixamo_Retarget;
	// core
	Mixamo_Retarget.RootBone = FName("Hips");
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("Spine"), FName("Spine2"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("Neck"), FName("Neck"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head"), FName("Head"));
	// left
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("LeftUpLeg"), FName("LeftToeBase"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("LeftShoulder"), FName("LeftShoulder"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("LeftArm"), FName("LeftHand"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("LeftHandThumb1"), FName("LeftHandThumb3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("LeftHandIndex1"), FName("LeftHandIndex3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("LeftHandMiddle1"), FName("LeftHandMiddle3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("LeftHandRing1"), FName("LeftHandRing3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("LeftHandPinky1"), FName("LeftHandPinky3"));
	// right
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("RightUpLeg"), FName("RightToeBase"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("RightShoulder"), FName("RightShoulder"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("RightArm"), FName("RightHand"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("RightHandThumb1"), FName("RightHandThumb3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("RightHandIndex1"), FName("RightHandIndex3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("RightHandMiddle1"), FName("RightHandMiddle3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("RightHandRing1"), FName("RightHandRing3"));
	Mixamo_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("RightHandPinky1"), FName("RightHandPinky3"));
	AddTemplateHierarchy(Mixamo, Mixamo_Bones, Mixamo_ParentIndices, Mixamo_Retarget);
	
	// Reallusions Character Creator 4
	static FName ReallusionCC4 = "Reallusion Character Creator 4";
	static TArray<FName> ReallusionCC4_Bones = {"BoneRoot", "Hip", "Pelvis", "L_Thigh", "L_Calf", "L_Foot", "L_ToeBase", "L_PinkyToe1", "L_RingToe1", "L_MidToe1", "L_IndexToe1", "L_BigToe1", "L_ThighTwist01", "L_ThighTwist02", "R_Thigh", "R_Calf", "R_Foot", "R_ToeBase", "R_BigToe1", "R_PinkyToe1", "R_RingToe1", "R_IndexToe1", "R_MidToe1", "Waist", "Spine01", "Spine02", "NeckTwist01", "NeckTwist02", "Head", "L_Clavicle", "L_Upperarm", "L_Forearm", "L_Hand", "L_Pinky1", "L_Pinky2", "L_Pinky3", "L_Ring1", "L_Ring2", "L_Ring3", "L_Mid1", "L_Mid2", "L_Mid3", "L_Index1", "L_Index2", "L_Index3", "L_Thumb1", "L_Thumb2", "L_Thumb3", "R_Clavicle", "R_Upperarm", "R_Forearm", "R_Hand", "R_Ring1", "R_Ring2", "R_Ring3", "R_Mid1", "R_Mid2", "R_Mid3", "R_Thumb1", "R_Thumb2", "R_Thumb3", "R_Index1", "R_Index2", "R_Index3", "R_Pinky1", "R_Pinky2", "R_Pinky3"};
	static TArray<int32> ReallusionCC4_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 6, 3, 12, 2, 14, 15, 16, 17, 17, 17, 17, 17, 1, 23, 24, 25, 26, 27, 25, 29, 30, 31, 32, 33, 34, 32, 36, 37, 32, 39, 40, 32, 42, 43, 32, 45, 46, 25, 48, 49, 50, 51, 52, 53, 51, 55, 56, 51, 58, 59, 51, 61, 62, 51, 64, 65};
	FRetargetDefinition ReallusionCC4_Retarget;
	// core
	ReallusionCC4_Retarget.RootBone = FName("Hip");
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("BoneRoot"), FName("BoneRoot"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("Waist"), FName("Spine02"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("NeckTwist01"), FName("NeckTwist02"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head"), FName("Head"));
	// left
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("L_Thigh"), FName("L_ToeBase"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("L_Clavicle"), FName("L_Clavicle"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("L_Upperarm"), FName("L_Hand"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("L_Thumb1"), FName("L_Thumb3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("L_Index1"), FName("L_Index3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("L_Mid1"), FName("L_Mid3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("L_Ring1"), FName("L_Ring3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("L_Pinky1"), FName("L_Pinky3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftBigToe, FName("L_BigToe1"), FName("L_BigToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexToe, FName("L_IndexToe1"), FName("L_IndexToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddleToe, FName("L_MidToe1"), FName("L_MidToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftRingToe, FName("L_RingToe1"), FName("L_RingToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyToe, FName("L_PinkyToe1"), FName("L_PinkyToe1"));
	// right
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("R_Thigh"), FName("R_ToeBase"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("R_Clavicle"), FName("R_Clavicle"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("R_Upperarm"), FName("R_Hand"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("R_Thumb1"), FName("R_Thumb3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("R_Index1"), FName("R_Index3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("R_Mid1"), FName("R_Mid3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("R_Ring1"), FName("R_Ring3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("R_Pinky1"), FName("R_Pinky3"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightBigToe, FName("R_BigToe1"), FName("R_BigToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexToe, FName("R_IndexToe1"), FName("R_IndexToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddleToe, FName("R_MidToe1"), FName("R_MidToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightRingToe, FName("R_RingToe1"), FName("R_RingToe1"));
	ReallusionCC4_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyToe, FName("R_PinkyToe1"), FName("R_PinkyToe1"));
	AddTemplateHierarchy(ReallusionCC4, ReallusionCC4_Bones, ReallusionCC4_ParentIndices, ReallusionCC4_Retarget);

	// Xsens
	static FName Xsens = "Xsens";
	static TArray<FName> Xsens_Bones = {"Reference", "Hips", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "Spine", "Spine1", "Spine2", "Spine3", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3", "RightShoulder", "RightArm", "RightForeArm", "RightHand", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandPinky1", "RightHandPinky2", "RightHandPinky3", "Neck", "Head"};
	static TArray<int32> Xsens_ParentIndices = {-1, 0, 1, 2, 3, 4, 1, 6, 7, 8, 1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 17, 21, 22, 17, 24, 25, 17, 27, 28, 17, 30, 31, 13, 33, 34, 35, 36, 37, 38, 36, 40, 41, 36, 43, 44, 36, 46, 47, 36, 49, 50, 13, 52};
	FRetargetDefinition Xsens_Retarget;
	// core
	Xsens_Retarget.RootBone = FName("Hips");
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("Reference"), FName("Reference"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("Spine"), FName("Spine3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("Neck"), FName("Neck"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head"), FName("Head"));
	// left
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("LeftUpLeg"), FName("LeftToeBase"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("LeftShoulder"), FName("LeftShoulder"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("LeftArm"), FName("LeftHand"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("LeftHandThumb1"), FName("LeftHandThumb3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("LeftHandIndex1"), FName("LeftHandIndex3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("LeftHandMiddle1"), FName("LeftHandMiddle3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("LeftHandRing1"), FName("LeftHandRing3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("LeftHandPinky1"), FName("LeftHandPinky3"));
	// right
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("RightUpLeg"), FName("RightToeBase"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("RightShoulder"), FName("RightShoulder"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("RightArm"), FName("RightHand"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("RightHandThumb1"), FName("RightHandThumb3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("RightHandIndex1"), FName("RightHandIndex3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("RightHandMiddle1"), FName("RightHandMiddle3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("RightHandRing1"), FName("RightHandRing3"));
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("RightHandPinky1"), FName("RightHandPinky3"));
	AddTemplateHierarchy(Xsens, Xsens_Bones, Xsens_ParentIndices, Xsens_Retarget);
	
	// mGear
	static FName mGear = "mGear";
	static TArray<FName> mGear_Bones = {"root_C0_0_jnt", "spine_C0_pelvis_jnt", "spine_C0_01_jnt", "spine_C0_02_jnt", "spine_C0_03_jnt", "spine_C0_04_jnt", "spine_C0_05_jnt", "neck_C0_01_jnt", "neck_C0_02_jnt", "neck_C0_head_jnt", "headBendNose_C0_0_jnt", "headBendJaw_C0_0_jnt", "jaw_C0_0_jnt", "shoulder_L0_0_jnt", "arm_L0_upperarm_jnt", "arm_L0_lowerarm_jnt", "arm_L0_hand_jnt", "thumb_L0_0_jnt", "thumb_L0_1_jnt", "thumb_L0_2_jnt", "meta_L0_0_jnt", "finger_L0_0_jnt", "finger_L0_1_jnt", "finger_L0_2_jnt", "meta_L0_1_jnt", "finger_L1_0_jnt", "finger_L1_1_jnt", "finger_L1_2_jnt", "meta_L0_2_jnt", "finger_L2_0_jnt", "finger_L2_1_jnt", "finger_L2_2_jnt", "meta_L0_3_jnt", "finger_L3_0_jnt", "finger_L3_1_jnt", "finger_L3_2_jnt", "shoulder_R0_0_jnt", "arm_R0_upperarm_jnt", "arm_R0_lowerarm_jnt", "arm_R0_hand_jnt", "thumb_R0_0_jnt", "thumb_R0_1_jnt", "thumb_R0_2_jnt", "meta_R0_0_jnt", "finger_R0_0_jnt", "finger_R0_1_jnt", "finger_R0_2_jnt", "meta_R0_1_jnt", "finger_R1_0_jnt", "finger_R1_1_jnt", "finger_R1_2_jnt", "meta_R0_2_jnt", "finger_R2_0_jnt", "finger_R2_1_jnt", "finger_R2_2_jnt", "meta_R0_3_jnt", "finger_R3_0_jnt", "finger_R3_1_jnt", "finger_R3_2_jnt", "leg_L0_thigh_jnt", "leg_L0_calf_jnt", "leg_L0_foot_jnt", "foot_L0_ball_jnt", "leg_R0_thigh_jnt", "leg_R0_calf_jnt", "leg_R0_foot_jnt", "foot_R0_ball_jnt", "ik_hand_root_C0_0_jnt", "ik_hand_gun_C0_0_jnt", "ik_hand_L0_0_jnt", "ik_hand_R0_0_jnt", "ik_foot_root_C0_0_jnt", "ik_foot_L0_0_jnt", "ik_foot_R0_0_jnt"};
	static TArray<int32> mGear_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 6, 13, 14, 15, 16, 17, 18, 16, 20, 21, 22, 16, 24, 25, 26, 16, 28, 29, 30, 16, 32, 33, 34, 6, 36, 37, 38, 39, 40, 41, 39, 43, 44, 45, 39, 47, 48, 49, 39, 51, 52, 53, 39, 55, 56, 57, 1, 59, 60, 61, 1, 63, 64, 65, 0, 67, 68, 68, 0, 71, 71};
	FRetargetDefinition mGear_Retarget;
	// core
	mGear_Retarget.RootBone = FName("spine_C0_pelvis_jnt");
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("Reference"), FName("Reference"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("spine_C0_01_jnt"), FName("spine_C0_05_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neck_C0_01_jnt"), FName("neck_C0_02_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("neck_C0_head_jnt"), FName("neck_C0_head_jnt"));
	// left
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("leg_L0_thigh_jnt"), FName("foot_L0_ball_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("shoulder_L0_0_jnt"), FName("shoulder_L0_0_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("arm_L0_upperarm_jnt"), FName("arm_L0_hand_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("thumb_L0_0_jnt"), FName("thumb_L0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("finger_L0_0_jnt"), FName("finger_L0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("finger_L1_0_jnt"), FName("finger_L1_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("finger_L2_0_jnt"), FName("finger_L2_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("finger_L3_0_jnt"), FName("finger_L3_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexMetacarpal, FName("meta_L0_0_jnt"), FName("meta_L0_0_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddleMetacarpal, FName("meta_L0_1_jnt"), FName("meta_L0_1_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftRingMetacarpal, FName("meta_L0_2_jnt"), FName("meta_L0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyMetacarpal, FName("meta_L0_3_jnt"), FName("meta_L0_3_jnt"));
	// right
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("leg_R0_thigh_jnt"), FName("foot_R0_ball_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("shoulder_R0_0_jnt"), FName("shoulder_R0_0_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("arm_R0_upperarm_jnt"), FName("arm_R0_hand_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("thumb_R0_0_jnt"), FName("thumb_R0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("finger_R0_0_jnt"), FName("finger_R0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("finger_R1_0_jnt"), FName("finger_R1_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("finger_R2_0_jnt"), FName("finger_R2_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("finger_R3_0_jnt"), FName("finger_R3_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexMetacarpal, FName("meta_R0_0_jnt"), FName("meta_R0_0_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddleMetacarpal, FName("meta_R0_1_jnt"), FName("meta_R0_1_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightRingMetacarpal, FName("meta_R0_2_jnt"), FName("meta_R0_2_jnt"));
	mGear_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyMetacarpal, FName("meta_R0_3_jnt"), FName("meta_R0_3_jnt"));
	AddTemplateHierarchy(Xsens, Xsens_Bones, Xsens_ParentIndices, mGear_Retarget);
	
	// Motionbuilder / Human IK
	static FName HumanIK = "HumanIK / Motionbuilder";
	static TArray<FName> HumanIK_Bones = {"Hips", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "Spine", "Spine1", "Spine2", "Spine3", "Neck", "Head", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandAttach", "LeftFingerBase", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3", "RightShoulder", "RightArm", "RightForeArm", "RightHand", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandAttach", "RightFingerBase", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandPinky1", "RightHandPinky2", "RightHandPinky3", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"};
	static TArray<int32> HumanIK_ParentIndices = {-1, 0, 1, 2, 3, 0, 5, 6, 7, 8, 9, 8, 11, 12, 13, 14, 15, 16, 14, 14, 19, 20, 21, 19, 23, 24, 19, 26, 27, 19, 29, 30, 8, 32, 33, 34, 35, 36, 37, 35, 35, 40, 41, 42, 40, 44, 45, 40, 47, 48, 40, 50, 51, 0, 53, 54, 55};
	FRetargetDefinition HumanIK_Retarget;
	// core
	HumanIK_Retarget.RootBone = FName("Hips");
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("Spine"), FName("Spine3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("Neck"), FName("Neck1"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head"), FName("Head"));
	// left
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("LeftUpLeg"), FName("LeftToeBase"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("LeftShoulder"), FName("LeftShoulder"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("LeftArm"), FName("LeftHand"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("LeftHandThumb1"), FName("LeftHandThumb3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("LeftHandIndex1"), FName("LeftHandIndex3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("LeftHandMiddle1"), FName("LeftHandMiddle3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("LeftHandRing1"), FName("LeftHandRing3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("LeftHandPinky1"), FName("LeftHandPinky3"));
	// right
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("RightUpLeg"), FName("RightToeBase"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("RightShoulder"), FName("RightShoulder"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("RightArm"), FName("RightHand"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("RightHandThumb1"), FName("RightHandThumb3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("RightHandIndex1"), FName("RightHandIndex3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("RightHandMiddle1"), FName("RightHandMiddle3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("RightHandRing1"), FName("RightHandRing3"));
	HumanIK_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("RightHandPinky1"), FName("RightHandPinky3"));
	AddTemplateHierarchy(HumanIK, HumanIK_Bones, HumanIK_ParentIndices, HumanIK_Retarget);

	// Vicon
	static FName Vicon = "Vicon";
	static TArray<FName> Vicon_Bones = {"Hips", "Spine", "Spine1", "Spine2", "Spine3", "Neck", "Neck1", "Head", "HeadEnd", "RightShoulder", "RightArm", "RightForeArm", "RightHand", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "RightHandRing", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky", "RightHandPinky1", "RightHandPinky2", "RightHandPinky3", "RightHandPinky4", "RightHandIndex", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3", "RightHandIndex4", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftHandRing", "LeftHandRing1", "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3", "LeftHandPinky4", "LeftHandIndex", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandIndex4", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase", "RightToeBaseEnd", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "LeftToeBaseEnd"};
	static TArray<int32> Vicon_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 4, 9, 10, 11, 12, 13, 14, 15, 12, 17, 18, 19, 20, 17, 22, 23, 24, 25, 12, 27, 28, 29, 30, 27, 32, 33, 34, 4, 36, 37, 38, 39, 40, 41, 42, 39, 44, 45, 46, 47, 44, 49, 50, 51, 52, 39, 54, 55, 56, 57, 54, 59, 60, 61, 0, 63, 64, 65, 66, 0, 68, 69, 70, 71};
	FRetargetDefinition Vicon_Retarget;
	// core
	Vicon_Retarget.RootBone = FName("Hips");
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("Spine"), FName("Spine3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("Neck"), FName("Neck1"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head"), FName("Head"));
	// left
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("LeftUpLeg"), FName("LeftToeBase"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("LeftShoulder"), FName("LeftShoulder"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("LeftArm"), FName("LeftHand"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("LeftHandThumb1"), FName("LeftHandThumb3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("LeftHandIndex1"), FName("LeftHandIndex3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("LeftHandMiddle1"), FName("LeftHandMiddle3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("LeftHandRing1"), FName("LeftHandRing3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("LeftHandPinky1"), FName("LeftHandPinky3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndexMetacarpal, FName("LeftHandIndex"), FName("LeftHandIndex"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyMetacarpal, FName("LeftHandPinky"), FName("LeftHandPinky"));
	// right
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("RightUpLeg"), FName("RightToeBase"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("RightShoulder"), FName("RightShoulder"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("RightArm"), FName("RightHand"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("RightHandThumb1"), FName("RightHandThumb3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("RightHandIndex1"), FName("RightHandIndex3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("RightHandMiddle1"), FName("RightHandMiddle3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("RightHandRing1"), FName("RightHandRing3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("RightHandPinky1"), FName("RightHandPinky3"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightIndexMetacarpal, FName("RightHandIndex"), FName("RightHandIndex"));
	Vicon_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyMetacarpal, FName("RightHandPinky"), FName("RightHandPinky"));
	AddTemplateHierarchy(Vicon, Vicon_Bones, Vicon_ParentIndices, Vicon_Retarget);

	// Optitrack / Motive
	static FName Optitrak = "OptiTrack / Motive";
	static TArray<FName> Optitrak_Bones = {"root", "pelvis", "spine_01", "spine_02", "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"};
	static TArray<int32> Optitrak_ParentIndices = {-1, 0, 1, 2, 3, 4, 3, 6, 7, 8, 3, 10, 11, 12, 1, 14, 15, 16, 1, 18, 19, 20};
	FRetargetDefinition Optitrak_Retarget;
	// core
	Optitrak_Retarget.RootBone = FName("pelvis");
	Xsens_Retarget.AddBoneChain(FCharacterizationStandard::Root, FName("root"), FName("root"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("spine_01"), FName("spine_02"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neck_01"), FName("neck_01"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("head"), FName("head"));
	// left
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("thigh_l"), FName("ball_l"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("clavicle_l"), FName("clavicle_l"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("upperarm_l"), FName("hand_l"));
	// right
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("thigh_r"), FName("ball_r"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("clavicle_r"), FName("clavicle_r"));
	Optitrak_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("upperarm_r"), FName("hand_r"));
	AddTemplateHierarchy(Optitrak, Optitrak_Bones, Optitrak_ParentIndices, Optitrak_Retarget);

	// Sony Mocopi
	static FName Mocopi = "Sony Mocopi";
	static TArray<FName> Mocopi_Bones = {"root", "torso_1", "torso_2", "torso_3", "torso_4", "torso_5", "torso_6", "torso_7", "neck_1", "neck_2", "head", "head_End", "l_shoulder", "l_up_arm", "l_low_arm", "l_hand", "l_hand_End", "r_shoulder", "r_up_arm", "r_low_arm", "r_hand", "r_hand_End", "l_up_leg", "l_low_leg", "l_foot", "l_toes", "l_toes_End", "r_up_leg", "r_low_leg", "r_foot", "r_toes", "r_toes_End"};
	static TArray<int32> Mocopi_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 7, 12, 13, 14, 15, 7, 17, 18, 19, 20, 0, 22, 23, 24, 25, 0, 27, 28, 29, 30};
	FRetargetDefinition Mocopo_Retarget;
	Mocopo_Retarget.RootBone = FName("root");
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("torso_1"), FName("torso_7"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("neck_1"), FName("neck_2"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("head"), FName("head"));
	// left
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("l_up_leg"), FName("l_toes"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("l_shoulder"), FName("l_shoulder"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("l_up_arm"), FName("l_hand"));
	// right
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("r_up_leg"), FName("r_toes"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("r_shoulder"), FName("r_shoulder"));
	Mocopo_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("r_up_arm"), FName("r_hand"));
	AddTemplateHierarchy(Mocopi, Mocopi_Bones, Mocopi_ParentIndices, Mocopo_Retarget);
	
	// Advanced skeleton
	static FName AdvancedSkeleton = "Advanced Skeleton";
	static TArray<FName> AdvancedSkeleton_Bones = {"Root_M", "Hip_R", "HipPart1_R", "HipPart2_R", "Knee_R", "Ankle_R", "Toes_R", "RootPart1_M", "RootPart2_M", "Spine1_M", "Spine1Part1_M", "Spine1Part2_M", "Chest_M", "Neck_M", "NeckPart1_M", "NeckPart2_M", "Head_M", "Scapula_R", "Shoulder_R", "ShoulderPart1_R", "ShoulderPart2_R", "Elbow_R", "ElbowPart1_R", "ElbowPart2_R", "Wrist_R", "MiddleFinger1_R", "MiddleFinger2_R", "MiddleFinger3_R", "MiddleFinger4_R", "ThumbFinger1_R", "ThumbFinger2_R", "ThumbFinger3_R", "ThumbFinger4_R", "IndexFinger1_R", "IndexFinger2_R", "IndexFinger3_R", "IndexFinger4_R", "Cup_R", "PinkyFinger1_R", "PinkyFinger2_R", "PinkyFinger3_R", "PinkyFinger4_R", "RingFinger1_R", "RingFinger2_R", "RingFinger3_R", "RingFinger4_R", "Scapula_L", "Shoulder_L", "ShoulderPart1_L", "ShoulderPart2_L", "Elbow_L", "ElbowPart1_L", "ElbowPart2_L", "Wrist_L", "MiddleFinger1_L", "MiddleFinger2_L", "MiddleFinger3_L", "MiddleFinger4_L", "ThumbFinger1_L", "ThumbFinger2_L", "ThumbFinger3_L", "ThumbFinger4_L", "IndexFinger1_L", "IndexFinger2_L", "IndexFinger3_L", "IndexFinger4_L", "Cup_L", "PinkyFinger1_L", "PinkyFinger2_L", "PinkyFinger3_L", "PinkyFinger4_L", "RingFinger1_L", "RingFinger2_L", "RingFinger3_L", "RingFinger4_L", "Hip_L", "HipPart1_L", "HipPart2_L", "Knee_L", "Ankle_L", "Toes_L"};
	static TArray<int32> AdvancedSkeleton_ParentIndices = {-1, 0, 1, 2, 3, 4, 5, 0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 12, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 24, 29, 30, 31, 24, 33, 34, 35, 24, 37, 38, 39, 40, 37, 42, 43, 44, 12, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 53, 58, 59, 60, 53, 62, 63, 64, 53, 66, 67, 68, 69, 66, 71, 72, 73, 0, 75, 76, 77, 78, 79};
	FRetargetDefinition AdvSkel_Retarget;
	// core
	AdvSkel_Retarget.RootBone = FName("Root_M");
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::Spine, FName("RootPart1_M"), FName("Chest_M"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::Neck, FName("Neck_M"), FName("NeckPart2_M"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::Head, FName("Head_M"), FName("Head_M"));
	// left
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftLeg, FName("Hip_L"), FName("Toes_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftClavicle, FName("Scapula_L"), FName("Scapula_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftArm, FName("Shoulder_L"), FName("Wrist_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftThumb, FName("ThumbFinger2_L"), FName("ThumbFinger4_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftIndex, FName("IndexFinger1_L"), FName("IndexFinger3_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftMiddle, FName("MiddleFinger1_L"), FName("MiddleFinger3_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftRing, FName("RingFinger1_L"), FName("RingFinger3_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinky, FName("PinkyFinger1_L"), FName("PinkyFinger3_L"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::LeftPinkyMetacarpal, FName("Cup_L"), FName("Cup_L"));
	// right
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightLeg, FName("Hip_R"), FName("Toes_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightClavicle, FName("Scapula_R"), FName("Scapula_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightArm, FName("Shoulder_R"), FName("Wrist_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightThumb, FName("ThumbFinger2_R"), FName("ThumbFinger4_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightIndex, FName("IndexFinger1_R"), FName("IndexFinger3_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightMiddle, FName("MiddleFinger1_R"), FName("MiddleFinger3_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightRing, FName("RingFinger1_R"), FName("RingFinger3_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightPinky, FName("PinkyFinger1_R"), FName("PinkyFinger3_R"));
	AdvSkel_Retarget.AddBoneChain(FCharacterizationStandard::RightPinkyMetacarpal, FName("Cup_R"), FName("Cup_R"));
	AddTemplateHierarchy(AdvancedSkeleton, AdvancedSkeleton_Bones, AdvancedSkeleton_ParentIndices, AdvSkel_Retarget);

	// ToDo skeletons to add:
	// MoveAI 
	// Fortnite skeleton
}

void FKnownTemplateHierarchies::GetClosestMatchingKnownHierarchy(
	const FAbstractHierarchy& InHierarchy,
	FAutoCharacterizeResults& Results) const
{
	Results.BestNumMatchingBones = 0;
	Results.BestPercentageOfTemplateScore = 0.f;
	Results.BestTemplateName = NAME_None;

	TArray<FName> MissingBones;
	TArray<FName> BonesWithMissingParent;
	for (const FTemplateHierarchy& KnownHierarchy : KnownHierarchies)
	{
		int32 NumMatchingBones = 0;
		float PercentageOfTemplateFound  = 0.f;
		KnownHierarchy.Hierarchy.Compare(InHierarchy, MissingBones, BonesWithMissingParent, NumMatchingBones, PercentageOfTemplateFound);
		if (NumMatchingBones > Results.BestNumMatchingBones)
		{
			Results.BestNumMatchingBones = NumMatchingBones;
			Results.BestPercentageOfTemplateScore = PercentageOfTemplateFound;
			Results.BestTemplateName = KnownHierarchy.Name;
			Results.MissingBones = MissingBones;
			Results.BonesWithMissingParent = BonesWithMissingParent;
		}
	}
}

const FTemplateHierarchy* FKnownTemplateHierarchies::GetKnownHierarchyByName(const FName InName) const
{
	for (const FTemplateHierarchy& KnownHierarchy : KnownHierarchies)
	{
		if (KnownHierarchy.Name == InName)
		{
			return &KnownHierarchy;
		}
	}

	return nullptr;
}

void FKnownTemplateHierarchies::AddTemplateHierarchy(
	const FName& Label,
	const TArray<FName>& BoneNames,
	const TArray<int32>& ParentIndices,
	const FRetargetDefinition& RetargetDefinition)
{
	KnownHierarchies.Emplace(Label, FAbstractHierarchy(BoneNames, ParentIndices), RetargetDefinition);
}

void FAutoCharacterizer::GenerateRetargetDefinitionFromMesh(USkeletalMesh* Mesh, FAutoCharacterizeResults& Results) const
{
	// reset
	Results = FAutoCharacterizeResults();
	
	if (!Mesh)
	{
		return;
	}
	
	// first check to see if there are any known popular skeleton formats that closely match the input mesh
	const FAbstractHierarchy TargetAbstractHierarchy(Mesh);
	KnownHierarchies.GetClosestMatchingKnownHierarchy(TargetAbstractHierarchy, Results);

	// if we found a known hierarchy with high enough certainty, then use it!
	constexpr float MinScoreThreshold = 0.7f;
	if (Results.BestTemplateName != NAME_None && Results.BestPercentageOfTemplateScore > MinScoreThreshold)
	{
		// adapt the retarget definition from the best matching template to the target hierarchy
		const FTemplateHierarchy* ClosestTemplateHierarchy = KnownHierarchies.GetKnownHierarchyByName(Results.BestTemplateName);
		check(ClosestTemplateHierarchy);
		AdaptTemplateToHierarchy(*ClosestTemplateHierarchy, TargetAbstractHierarchy, Results);
		// record that a template was indeed used
		Results.bUsedTemplate = true;
		return;
	}

	// TODO
	// skeleton does not match closely enough to any known skeleton,
	// so we fall back on analyzing the skeleton and procedurally generating a retarget definition
	Results.bUsedTemplate = false;
}

void FAutoCharacterizer::AdaptTemplateToHierarchy(
	const FTemplateHierarchy& Template,
	const FAbstractHierarchy& TargetHierarchy,
	FAutoCharacterizeResults& Results) const
{
	Results.RetargetDefinition = FRetargetDefinition();

	// adapt the retarget root bone
	const int32 RetargetRootIndex = TargetHierarchy.GetBoneIndex(Template.RetargetDefinition.RootBone, ECleanOrFullName::Clean);
	if (RetargetRootIndex != INDEX_NONE)
	{
		// convert to the full, non-clean name to use for the actual bone chain
		const FName RootBoneName =  TargetHierarchy.GetBoneName(RetargetRootIndex, ECleanOrFullName::Full);
		Results.RetargetDefinition.RootBone = RootBoneName;
	}

	// adapt the bone chains by shrinking from start and end until a valid chain is found on the target hierarchy
	for (const FBoneChain& BoneChain : Template.RetargetDefinition.BoneChains)
	{
		// get the bones that are expected to be in this chain according to the template (ordered root to leaf)
		TArray<FName> BonesInChainInTemplate;
		Template.Hierarchy.GetBonesInChain(
			BoneChain.StartBone.BoneName,
			BoneChain.EndBone.BoneName,
			ECleanOrFullName::Clean,
			BonesInChainInTemplate);

		// find closest start bone in target hierarchy
		int32 StartBoneIndexInTarget = INDEX_NONE;
		for (const FName& BoneInChain : BonesInChainInTemplate)
		{
			const int32 BoneInTargetIndex = TargetHierarchy.GetBoneIndex(BoneInChain, ECleanOrFullName::Clean);
			if (BoneInTargetIndex != INDEX_NONE)
			{
				StartBoneIndexInTarget = BoneInTargetIndex;
				break;
			}
		}

		// find closest end bone in target hierarchy
		int32 EndBoneIndexInTarget = INDEX_NONE;
		for (int32 ChainIndex=BonesInChainInTemplate.Num()-1; ChainIndex>=0; --ChainIndex)
		{
			const FName& BoneInChain = BonesInChainInTemplate[ChainIndex];
			const int32 BoneInTargetIndex = TargetHierarchy.GetBoneIndex(BoneInChain, ECleanOrFullName::Clean);
			if (BoneInTargetIndex != INDEX_NONE)
			{
				EndBoneIndexInTarget = BoneInTargetIndex;
				break;
			}
		}

		// couldn't find both a start AND end bone in the target hierarchy
		if (StartBoneIndexInTarget == INDEX_NONE || EndBoneIndexInTarget == INDEX_NONE)
		{
			continue;
		}

		// chain must be either a single bone, OR end must be a child of start
		const FName StartBoneFullName = TargetHierarchy.GetBoneName(StartBoneIndexInTarget, ECleanOrFullName::Full);
		const FName EndBoneFullName = TargetHierarchy.GetBoneName(EndBoneIndexInTarget, ECleanOrFullName::Full);
		const bool bIsEndChildOfStart = TargetHierarchy.IsChildOf(StartBoneFullName, EndBoneFullName, ECleanOrFullName::Full);
		if (StartBoneIndexInTarget == EndBoneIndexInTarget || bIsEndChildOfStart)
		{
			// convert to the full, non-clean name to use for the actual bone chain
			Results.RetargetDefinition.AddBoneChain(BoneChain.ChainName, StartBoneFullName, EndBoneFullName);
			continue;
		}
	}

	
	// since the templates usually only account for 1-3 neck/spine bones, but some target skeletons have more,
	// we need to grow these chains or risk leaving some of the bones out of the retarget chain
	// grow the spine chain
	FBoneChain* SpineChain = Results.RetargetDefinition.GetEditableBoneChainByName(FCharacterizationStandard::Spine);
	Results.NumBonesAddedToSpineChain = ExpandChain(SpineChain, TargetHierarchy);
	// grow the neck chain
	FBoneChain* NeckChain = Results.RetargetDefinition.GetEditableBoneChainByName(FCharacterizationStandard::Neck);
	Results.NumBonesAddedToNeckChain = ExpandChain(NeckChain, TargetHierarchy);
}

int32 FAutoCharacterizer::ExpandChain(
	FBoneChain* ChainToExpand, 
	const FAbstractHierarchy& Hierarchy) const
{
	if (!ChainToExpand)
	{
		return 0;
	}
	
	// get all bones in spine
	TArray<FName> FullNamesOfBonesInSpine;
	Hierarchy.GetBonesInChain(ChainToExpand->StartBone.BoneName, ChainToExpand->EndBone.BoneName, ECleanOrFullName::Full, FullNamesOfBonesInSpine);

	// convert to clean strings for searching
	TArray<FName> BonesInSpine;
	for (const FName& FullName : FullNamesOfBonesInSpine)
	{
		const int32 BoneIndex = Hierarchy.GetBoneIndex(FullName, ECleanOrFullName::Full);
		BonesInSpine.Add(Hierarchy.GetBoneName(BoneIndex, ECleanOrFullName::Clean));
	}

	// keep searching down the hierarchy for more joint that belong to this chain
	FName NewSpineEnd = BonesInSpine.Last();
	int32 NumBonesExpanded = 0;
	while(NewSpineEnd != NAME_None)
	{
		TArray<FName> Children;
		Hierarchy.GetImmediateChildren(NewSpineEnd, ECleanOrFullName::Clean, Children);
		FName BestMatchingName;
		float Score;
		FindClosestNameInArray(NewSpineEnd, Children, BestMatchingName, Score);
		constexpr float MinScoreThreshold = 0.9f;
		if (Score < MinScoreThreshold)
		{
			break;
		}
		NewSpineEnd = BestMatchingName;
		++NumBonesExpanded;
	}

	// assign the new end bone
	const int32 NewSpineEndIndex = Hierarchy.GetBoneIndex(NewSpineEnd, ECleanOrFullName::Clean);
	ChainToExpand->EndBone = Hierarchy.GetBoneName(NewSpineEndIndex, ECleanOrFullName::Full);

	return NumBonesExpanded;
}

void FAutoCharacterizer::FindClosestNameInArray(
	const FName& NameToMatch,
	const TArray<FName>& NamesToCheck,
	FName& OutClosestName,
	float& OutBestScore) const
{
	OutClosestName = NAME_None;
	OutBestScore = -1.0f;
	
	if (NamesToCheck.IsEmpty())
	{
		return;
	}
	
	const FString NameToMatchStr = NameToMatch.ToString().ToLower();
	for (const FName BoneName : NamesToCheck)
	{
		FString CurrentNameStr = BoneName.ToString().ToLower();
		float WorstCase = NameToMatchStr.Len() + CurrentNameStr.Len();
		WorstCase = WorstCase < 1.0f ? 1.0f : WorstCase;
		const float Score = 1.0f - (Algo::LevenshteinDistance(NameToMatchStr, CurrentNameStr) / WorstCase);
		if (Score > OutBestScore)
		{
			OutBestScore = Score;
			OutClosestName = BoneName;
		}
	}
}

#undef LOCTEXT_NAMESPACE
