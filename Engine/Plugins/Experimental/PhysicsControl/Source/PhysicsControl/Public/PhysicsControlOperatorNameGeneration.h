// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PhysicsControlLog.h"

class UPhysicsAsset;

struct FAnimNode_RigidBodyWithControl;
struct FPhysicsControlLimbBones;
struct FPhysicsControlLimbSetupData;
struct FPhysicsControlSetUpdates;
struct FReferenceSkeleton;
struct FRigidBodyControlRecord;
struct FRigidBodyModifierRecord;
struct FPhysicsControlNameRecords;

constexpr int32 MaxNumControlsOrModifiersPerName = 16;

// Parse the skeleton tree to figure out which bones are associated with which limbs. 
PHYSICSCONTROL_API TMap<FName, FPhysicsControlLimbBones> GetLimbBones(const TArray<FPhysicsControlLimbSetupData>& LimbSetupData, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset);

// Populates the supplied body modifier names, control names and name records structures with the names and sets that could be created for the supplied node, limb bones, skeleton and physics asset.
PHYSICSCONTROL_API void CollectOperatorNames(const FAnimNode_RigidBodyWithControl* const Node, TMap<FName, FPhysicsControlLimbBones> AllLimbBones, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset, TSet<FName>& BodyModifierNames, TSet<FName>& ControlNames, FPhysicsControlNameRecords& NameRecords);

// Creates the body modifiers, controls and sets for the supplied node, limb bones, skeleton and physics asset.
PHYSICSCONTROL_API void CreateOperatorsForNode(FAnimNode_RigidBodyWithControl* const Node, TMap<FName, FPhysicsControlLimbBones> AllLimbBones, const FReferenceSkeleton& RefSkeleton, UPhysicsAsset* const PhysicsAsset, FPhysicsControlNameRecords& NameRecords);

// Adds the specified additional sets to the supplied Name Records structure.
PHYSICSCONTROL_API void CreateAdditionalSets(const FPhysicsControlSetUpdates& AdditionalSets, const TSet<FName>& BodyModifierNames, const TSet<FName>& ControlNames, FPhysicsControlNameRecords& NameRecords);

// Adds the specified additional sets to the supplied Name Records structure.
PHYSICSCONTROL_API void CreateAdditionalSets(const FPhysicsControlSetUpdates& AdditionalSets, const TMap<FName, FRigidBodyModifierRecord>& BodyModifierNames, const TMap<FName, FRigidBodyControlRecord>& ControlNames, FPhysicsControlNameRecords& NameRecords);

FName FindUniqueName(const FString& NameBase, const TSet<FName>& Keys, const int32 MaxNmeaIndex);
FName FindUniqueBodyModifierName(const FName BodyName, const TSet<FName>& ExistingNames);
FName FindUniqueControlName(const FName ParentBodyName, const FName ChildBodyName, const TSet<FName>& ExistingNames);
