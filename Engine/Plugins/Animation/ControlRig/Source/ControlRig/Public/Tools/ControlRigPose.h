// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
/**
*  Data To Store and Apply the Control Rig Pose
*/

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TransformNoScale.h"
#include "Engine/SkeletalMesh.h"
#include "ControlRigToolAsset.h"
#include "Tools/ControlRigPoseMirrorTable.h"
#include "Rigs/RigControlHierarchy.h"
#include "ControlRigPose.generated.h"

class UBaseControlRig;
/**
* The Data Stored For Each Control in A Pose.
*/
USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigControlCopy
{
	GENERATED_BODY()

		FRigControlCopy()
		: Name(NAME_None)
		, ControlType(ERigControlType::Transform)
		, Value()
		, ParentKey()
		, OffsetTransform(FTransform::Identity)
		, ParentTransform(FTransform::Identity)
		, LocalTransform(FTransform::Identity)
		, GlobalTransform(FTransform::Identity)

	{
	}

	FRigControlCopy(FRigControlElement* InControlElement, URigHierarchy* InHierarchy)
	{
		Name = InControlElement->GetFName();
		ControlType = InControlElement->Settings.ControlType;
		Value = InHierarchy->GetControlValue(InControlElement, ERigControlValueType::Current);
		ParentKey = InHierarchy->GetFirstParent(InControlElement->GetKey());
		OffsetTransform = InHierarchy->GetControlOffsetTransform(InControlElement, ERigTransformType::CurrentLocal);

		ParentTransform = InHierarchy->GetParentTransform(InControlElement, ERigTransformType::CurrentGlobal);
		LocalTransform = InHierarchy->GetTransform(InControlElement, ERigTransformType::CurrentLocal);
		GlobalTransform = InHierarchy->GetTransform(InControlElement, ERigTransformType::CurrentGlobal);
	}
	virtual ~FRigControlCopy() {}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Names")
	FName Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Type")
	ERigControlType ControlType;

	UPROPERTY()
	FRigControlValue Value;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Names")
	FRigElementKey ParentKey;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	FTransform OffsetTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	FTransform ParentTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	FTransform LocalTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transforms")
	FTransform GlobalTransform;

};

/**
* The Data Stored For Each Pose and associated Functions to Store and Paste It
*/
USTRUCT(BlueprintType)
struct CONTROLRIG_API FControlRigControlPose
{
	GENERATED_USTRUCT_BODY()

	FControlRigControlPose() {};
	FControlRigControlPose(UBaseControlRig* InControlRig, bool bUseAll)
	{
		SavePose(InControlRig, bUseAll);
	}
	~FControlRigControlPose() {};

	void SavePose(UBaseControlRig* ControlRig, bool bUseAll);
	void PastePose(UBaseControlRig* ControlRig, bool bDoKey, bool bDoMirror);
	void SetControlMirrorTransform(bool bDoLocalSpace, UBaseControlRig* ControlRig, const FName& Name, bool bIsMatched,
		const FTransform& GlobalTrnnsform, const FTransform& LocalTransform, bool bNotify, const  FRigControlModifiedContext& Context, bool bSetupUndo);
	void PastePoseInternal(UBaseControlRig* ControlRig, bool bDoKey, bool bDoMirror, const TArray<FRigControlCopy>& ControlsToPaste);
	void BlendWithInitialPoses(FControlRigControlPose& InitialPose, UBaseControlRig* ControlRig, bool bDoKey, bool bDoMirror, float BlendValue);

	bool ContainsName(const FName& Name) const;
	void ReplaceControlName(const FName& Name, const FName& NewName);
	TArray<FName> GetControlNames() const;

	void SetUpControlMap();
	TArray<FRigControlCopy> GetPoses() const {return CopyOfControls;};

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Controls")
	TArray<FRigControlCopy> CopyOfControls;

	//Cache of the Map, Used to make pasting faster.
	TMap<FName, int32>  CopyOfControlsNameToIndex;
};


/**
* An individual Pose made of Control Rig Controls
*/
UCLASS(BlueprintType)
class CONTROLRIG_API UControlRigPoseAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	//UOBJECT
	virtual void PostLoad() override;

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void SavePose(UBaseControlRig* InControlRig, bool bUseAll);

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void PastePose(UBaseControlRig* InControlRig, bool bDoKey = false, bool bDoMirror = false);
	
	UFUNCTION(BlueprintCallable, Category = "Pose")
	void SelectControls(UBaseControlRig* InControlRig, bool bDoMirror = false);

	TArray<FRigControlCopy> GetCurrentPose(UBaseControlRig* InControlRig);

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void GetCurrentPose(UBaseControlRig* InControlRig, FControlRigControlPose& OutPose);

	UFUNCTION(BlueprintPure, Category = "Pose")
	TArray<FName> GetControlNames() const;

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void ReplaceControlName(const FName& CurrentName, const FName& NewName);

	UFUNCTION(BlueprintPure, Category = "Pose")
	bool DoesMirrorMatch(UBaseControlRig* ControlRig, const FName& ControlName) const;

	void BlendWithInitialPoses(FControlRigControlPose& InitialPose, UBaseControlRig* ControlRig, bool bDoKey, bool bdoMirror, float BlendValue);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pose")
	FControlRigControlPose Pose;

};
