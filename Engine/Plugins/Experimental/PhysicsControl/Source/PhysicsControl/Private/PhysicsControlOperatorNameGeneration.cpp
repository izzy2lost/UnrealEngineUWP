// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlOperatorNameGeneration.h"

#include "ReferenceSkeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "AnimNode_RigidBodyWithControl.h"
#include "PhysicsControlLimbData.h"

//======================================================================================================================
template<typename TOperatorCreationType> FName FindUniqueOperatorName(const TOperatorCreationType& OperatorCreationSpecifier, const TSet<FName>& ExistingNames)
{
	return NAME_None;
}

//======================================================================================================================
template<> FName FindUniqueOperatorName<FRigidBodyModifierCreation>(const FRigidBodyModifierCreation& OperatorCreationSpecifier, const TSet<FName>& ExistingNames)
{
	return FindUniqueBodyModifierName(OperatorCreationSpecifier.Modifier.BoneName, ExistingNames);;
}

//======================================================================================================================
template<> FName FindUniqueOperatorName<FRigidBodyControlCreation>(const FRigidBodyControlCreation& OperatorCreationSpecifier, const TSet<FName>& ExistingNames)
{
	return FindUniqueControlName(OperatorCreationSpecifier.Control.ParentBoneName, OperatorCreationSpecifier.Control.ChildBoneName, ExistingNames);
}

//======================================================================================================================
FName FindParentBodyBoneName(const FName BoneName, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset)
{
	FName ParentBoneName;

	if (PhysicsAsset)
	{
		const int32 StartBoneIndex = RefSkeleton.FindBoneIndex(BoneName);

		if (StartBoneIndex != INDEX_NONE)
		{
			const int32 ParentBodyIndex = PhysicsAsset->FindParentBodyIndex(RefSkeleton, StartBoneIndex);

			if (PhysicsAsset->SkeletalBodySetups.IsValidIndex(ParentBodyIndex) && PhysicsAsset->SkeletalBodySetups[ParentBodyIndex])
			{
				ParentBoneName = PhysicsAsset->SkeletalBodySetups[ParentBodyIndex]->BoneName;
			}
		}
	}

	return ParentBoneName;
}

//======================================================================================================================
template<typename TOperatorFunctorType> void CreateAdditionalBodyModifiers(const TArray<FRigidBodyModifierCreation> CreationSpecifiers, FRigidBodyNameRecords& NameRecords, TOperatorFunctorType& OperatorFunctor)
{
	for (const FRigidBodyModifierCreation& Specifier : CreationSpecifiers)
	{
		const FName Name = OperatorFunctor(Specifier.Modifier.BoneName, Specifier.Modifier.ModifierData);
		if (Name.IsNone())
		{
			UE_LOG(LogRigidBodyWithControl, Warning,
				TEXT("CreateAdditionalControls: Failed to make body modifier for %s"), *Specifier.Modifier.BoneName.ToString());
		}
		else
		{
			UE_LOG(LogRigidBodyWithControl, Verbose,
				TEXT("Made modifier %s for %s"),
				*Name.ToString(),
				*Specifier.Modifier.BoneName.ToString());

			NameRecords.AddBodyModifier(Name, Specifier.Sets);
		}
	}
}

//======================================================================================================================
template<typename TOperatorFunctorType> void CreateAdditionalControls(const TArray<FRigidBodyControlCreation> CreationSpecifiers, FRigidBodyNameRecords& NameRecords, TOperatorFunctorType& OperatorFunctor)
{
	for (const FRigidBodyControlCreation& Specifier : CreationSpecifiers)
	{
		const FName Name = OperatorFunctor(Specifier.Control.ParentBoneName, Specifier.Control.ChildBoneName, Specifier.Control.ControlData);
		if (Name.IsNone())
		{
			UE_LOG(LogRigidBodyWithControl, Warning,
				TEXT("CreateAdditionalControls: Failed to make control between %s and %s"),
				*Specifier.Control.ParentBoneName.ToString(), *Specifier.Control.ChildBoneName.ToString());
		}
		else
		{
			UE_LOG(LogRigidBodyWithControl, Verbose,
				TEXT("Made control %s between %s and %s"),
				*Name.ToString(),
				*Specifier.Control.ParentBoneName.ToString(),
				*Specifier.Control.ChildBoneName.ToString());
			NameRecords.AddControl(Name, Specifier.Sets);
		}
	}
}


//======================================================================================================================
template<typename TFunctor> void CreateControlsFromLimbBones(
	const FName                   LimbName,
	const FPhysicsControlLimbBones& LimbBones,
	const EPhysicsControlType   ControlType,
	const FReferenceSkeleton& RefSkeleton,
	UPhysicsAsset* const PhysicsAsset,
	const FPhysicsControlData& ControlData,
	FRigidBodyNameRecords& NameRecords,
	TFunctor& CreateOperationLambda)
{
	for (const FName ChildBoneName : LimbBones.BoneNames)
	{
		FName ParentBoneName;

		if (ControlType == EPhysicsControlType::ParentSpace)
		{
			ParentBoneName = FindParentBodyBoneName(ChildBoneName, RefSkeleton, PhysicsAsset);

			if (ParentBoneName.IsNone())
			{
				// This happens for the pelvis, for example - we only create parent space
				// controls if there's a parent!
				continue;
			}
		}

		const FName ControlName = CreateOperationLambda(ParentBoneName, ChildBoneName, ControlData);

		if (!ControlName.IsNone())
		{
			NameRecords.AddControl(ControlName, LimbName);
			NameRecords.AddControl(ControlName, GetPhysicsControlTypeName(ControlType));
			NameRecords.AddControl(ControlName, FName(
				GetPhysicsControlTypeName(ControlType).ToString().Append("_").Append(LimbName.ToString())));
		}
		else if (!ParentBoneName.IsNone())
		{
			UE_LOG(LogRigidBodyWithControl, Warning,
				TEXT("Failed to find unique control name for %s and %s"), *ParentBoneName.ToString(), *ChildBoneName.ToString());
		}
		else
		{
			UE_LOG(LogRigidBodyWithControl, Warning,
				TEXT("Failed to find unique control name for %s"), *ChildBoneName.ToString());
		}
	}
}

//======================================================================================================================
template<typename TFunctor> void CreateBodyModifiersFromLimbBones(
	const FName                   LimbName,
	const FPhysicsControlLimbBones& LimbBones,
	const FPhysicsControlModifierData& ModifierData,
	FRigidBodyNameRecords& NameRecords,
	TFunctor& CreateOperationLambda)
{
	for (const FName BoneName : LimbBones.BoneNames)
	{
		const FName BodyModifierName = CreateOperationLambda(BoneName, ModifierData);

		if (!BodyModifierName.IsNone())
		{
			NameRecords.AddBodyModifier(BodyModifierName, LimbName);
		}
		else
		{
			UE_LOG(LogRigidBodyWithControl, Warning,
				TEXT("Failed to find unique body modifier name for %s"), *BoneName.ToString());
		}
	}
}

//======================================================================================================================
template<typename TControlFunctorType, typename TBodyModifierFunctorType> void ForEachPotentialOperator(const FAnimNode_RigidBodyWithControl* const Node, TMap<FName, FPhysicsControlLimbBones> AllLimbBones, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset, FRigidBodyNameRecords& NameRecords, TControlFunctorType& ControlFunctor, TBodyModifierFunctorType& BodyModifierFunctor)
{
	for (const TMap<FName, FPhysicsControlLimbBones>::ElementType& LimbBoneEntry : AllLimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& LimbBones = LimbBoneEntry.Value;
		if (LimbBones.bCreateWorldSpaceControls)
		{
			CreateControlsFromLimbBones(LimbName, LimbBones, EPhysicsControlType::WorldSpace, RefSkeleton, PhysicsAsset, Node->SetupData.DefaultWorldSpaceControlData, NameRecords, ControlFunctor);
		}
		if (LimbBones.bCreateParentSpaceControls)
		{
			CreateControlsFromLimbBones(LimbName, LimbBones, EPhysicsControlType::ParentSpace, RefSkeleton, PhysicsAsset, Node->SetupData.DefaultParentSpaceControlData, NameRecords, ControlFunctor);
		}
		if (LimbBones.bCreateBodyModifiers)
		{
			checkSlow(Node);
			CreateBodyModifiersFromLimbBones(LimbName, LimbBones, Node->SetupData.DefaultBodyModifierData, NameRecords, BodyModifierFunctor);
		}
	}

	if (Node != nullptr)
	{
		// Find names for any additional controls that have been requested	
		CreateAdditionalBodyModifiers(Node->AdditionalControlsAndBodyModifiers.Modifiers, NameRecords, BodyModifierFunctor);
		CreateAdditionalControls(Node->AdditionalControlsAndBodyModifiers.Controls, NameRecords, ControlFunctor);
	}
}

//======================================================================================================================
// Slightly annoying to have to add the names individually, but we want to check they exist
template<typename TBodyModifierNameContainerType, typename TControlNameContainerType> void CreateAdditionalSets_Implimentation(const FPhysicsControlSetUpdates& AdditionalSets, const TBodyModifierNameContainerType& BodyModifierNames, const TControlNameContainerType& ControlNames, FRigidBodyNameRecords& NameRecords)
{
	for (const FPhysicsControlSetUpdate& Set : AdditionalSets.ControlSetUpdates)
	{
		TArray<FName> Names = ExpandName(Set.Names, NameRecords.ControlSets);

		for (FName Name : Names)
		{
			if (ControlNames.Contains(Name))
			{
				NameRecords.AddControl(Name, Set.SetName);
			}
			else
			{
				UE_LOG(LogRigidBodyWithControl, Warning,
					TEXT("CreateAdditionalSets: Failed to find control with name %s to add to set %s"),
					*Name.ToString(), *Set.SetName.ToString());
			}
		}
	}

	for (const FPhysicsControlSetUpdate& Set : AdditionalSets.ModifierSetUpdates)
	{
		TArray<FName> Names = ExpandName(Set.Names, NameRecords.BodyModifierSets);

		for (FName Name : Names)
		{
			if (BodyModifierNames.Contains(Name))
			{
				NameRecords.AddBodyModifier(Name, Set.SetName);
			}
			else
			{
				UE_LOG(LogRigidBodyWithControl, Warning,
					TEXT("CreateAdditionalSets: Failed to find body modifier with name %s to add to set %s"),
					*Name.ToString(), *Set.SetName.ToString());
			}
		}
	}
}

//======================================================================================================================
FName FindUniqueName(const FString& NameBase, const TSet<FName>& Keys, const int32 MaxNmeaIndex)
{
	FString NameStr = NameBase;

	// If the number gets too large, almost certainly we're in some nasty situation where this is
	// getting called in a loop. Better to quit and fail, rather than allow the constraint set to
	// increase without bound. 
	for (int32 Index = 0; Index < MaxNmeaIndex; ++Index)
	{
		const FName Name(NameStr);
		if (!Keys.Find(Name))
		{
			return Name;
		}
		NameStr = FString::Format(TEXT("{0}_{1}"), { NameBase, Index });
	}

	return NAME_None;
}

//======================================================================================================================
FName FindUniqueBodyModifierName(const FName BodyName, const TSet<FName>& ExistingNames)
{
	FString NameBase = TEXT("");
	if (!BodyName.IsNone())
	{
		NameBase += BodyName.ToString();
	}
	else
	{
		NameBase = TEXT("Body");
	}

	return FindUniqueName(NameBase, ExistingNames, MaxNumControlsOrModifiersPerName);
}

//======================================================================================================================
FName FindUniqueControlName(const FName ParentBodyName, const FName ChildBodyName, const TSet<FName>& ExistingNames)
{
	FString NameBase = TEXT("");
	if (!ParentBodyName.IsNone())
	{
		NameBase += ParentBodyName.ToString() + TEXT("_");
	}
	if (!ChildBodyName.IsNone())
	{
		NameBase += ChildBodyName.ToString();
	}

	return FindUniqueName(NameBase, ExistingNames, MaxNumControlsOrModifiersPerName);
}

//======================================================================================================================
TMap<FName, FPhysicsControlLimbBones> GetLimbBones(
	const TArray<FPhysicsControlLimbSetupData>& LimbSetupData,
	const FReferenceSkeleton& RefSkeleton,
	UPhysicsAsset* const PhysicsAsset)
{
	// TODO - Output limb bones are not in the order specified in the skeleton - would be better if they were

	TMap<FName, FPhysicsControlLimbBones> Result;

	if (!PhysicsAsset)
	{
		UE_LOG(LogRigidBodyWithControl, Warning, TEXT("Physics asset missing"));
		return Result;
	}

	TSet<FName> AllBones;

	for (const FPhysicsControlLimbSetupData& LimbSetup : LimbSetupData)
	{
		FPhysicsControlLimbBones& LimbBones = Result.Add(LimbSetup.LimbName);

		LimbBones.bFirstBoneIsAdditional = false;
		LimbBones.bCreateWorldSpaceControls = LimbSetup.bCreateWorldSpaceControls;
		LimbBones.bCreateParentSpaceControls = LimbSetup.bCreateParentSpaceControls;
		LimbBones.bCreateBodyModifiers = LimbSetup.bCreateBodyModifiers;

		if (LimbSetup.bIncludeParentBone)
		{
			const FName ParentBoneName = FindParentBodyBoneName(LimbSetup.StartBone, RefSkeleton, PhysicsAsset);

			if (!ParentBoneName.IsNone())
			{
				LimbBones.BoneNames.Add(ParentBoneName);
				AllBones.Add(ParentBoneName);
				LimbBones.bFirstBoneIsAdditional = true;
			}
		}

		TArray<int32> ChildBodyIndices;
		PhysicsAsset->GetBodyIndicesBelow(ChildBodyIndices, LimbSetup.StartBone, RefSkeleton);

		for (int32 ChildBodyIndex : ChildBodyIndices)
		{
			const FName BoneName = PhysicsAsset->SkeletalBodySetups[ChildBodyIndex]->BoneName;

			if (!AllBones.Find(BoneName))
			{
				LimbBones.BoneNames.Add(BoneName);
				AllBones.Add(BoneName);
			}
		}
	}

	return Result;
}

void CollectOperatorNames(const FAnimNode_RigidBodyWithControl* const Node, TMap<FName, FPhysicsControlLimbBones> AllLimbBones, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset, TSet<FName>& BodyModifierNames, TSet<FName>& ControlNames, FRigidBodyNameRecords& NameRecords)
{
	auto CollectControlName = [&ControlNames](const FName ParentBoneName, const FName ChildBoneName, const FPhysicsControlData& Data) -> FName { const FName Name = FindUniqueControlName(ParentBoneName, ChildBoneName, ControlNames); ControlNames.Add(Name); return Name; };
	auto CollectBodyModifierName = [&BodyModifierNames](const FName BoneName, const FPhysicsControlModifierData& Data) -> FName { const FName Name = FindUniqueBodyModifierName(BoneName, BodyModifierNames); BodyModifierNames.Add(Name); return Name; };

	ForEachPotentialOperator(Node, AllLimbBones, RefSkeleton, PhysicsAsset, NameRecords, CollectControlName, CollectBodyModifierName);
}

void CreateOperatorsForNode(FAnimNode_RigidBodyWithControl* const Node, TMap<FName, FPhysicsControlLimbBones> AllLimbBones, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset, FRigidBodyNameRecords& NameRecords)
{
	auto CreateControl = [Node](const FName ParentBoneName, const FName ChildBoneName, const FPhysicsControlData& Data) -> FName { return Node->CreateControl(ParentBoneName, ChildBoneName, Data); };
	auto CreateBodyModifier = [Node](const FName BoneName, const FPhysicsControlModifierData& Data) -> FName { return Node->CreateBodyModifier(BoneName, Data); };

	ForEachPotentialOperator(Node, AllLimbBones, RefSkeleton, PhysicsAsset, NameRecords, CreateControl, CreateBodyModifier);
}

void CreateAdditionalSets(const FPhysicsControlSetUpdates& AdditionalSets, const TSet<FName>& BodyModifierNames, const TSet<FName>& ControlNames, FRigidBodyNameRecords& NameRecords)
{
	CreateAdditionalSets_Implimentation(AdditionalSets, BodyModifierNames, ControlNames, NameRecords);
}

void CreateAdditionalSets(const FPhysicsControlSetUpdates& AdditionalSets, const TMap<FName, FRigidBodyModifierRecord>& BodyModifierNames, const TMap<FName, FRigidBodyControlRecord>& ControlNames, FRigidBodyNameRecords& NameRecords)
{
	CreateAdditionalSets_Implimentation(AdditionalSets, BodyModifierNames, ControlNames, NameRecords);
}
