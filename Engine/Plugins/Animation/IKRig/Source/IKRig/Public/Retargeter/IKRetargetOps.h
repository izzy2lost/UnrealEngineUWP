// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Retargeter/IKRetargetProcessor.h"

#include "IKRetargetOps.generated.h"

#if WITH_EDITOR
class FIKRetargetEditorController;
#endif

UCLASS(abstract, hidecategories = UObject)
class IKRIG_API URetargetOpBase : public UObject
{
	GENERATED_BODY()

public:

	URetargetOpBase() { SetFlags(RF_Transactional); }
	
	// override to cache internal data when initializing the processor
	virtual bool Initialize(
		const FRetargetSkeleton& SourceSkeleton,
		const FTargetSkeleton& TargetSkeleton,
		const UIKRetargetProcessor* Processor,
		FIKRigLogger& Log) PURE_VIRTUAL(, return false;);

	// override to evaluate this operation and modify the output pose
	virtual void Run(
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose) PURE_VIRTUAL(,);

	UPROPERTY()
	bool bIsEnabled = true;
	
#if WITH_EDITOR
	// override to automate initial setup after being added to the stack
	virtual void OnAddedToStack(const UIKRetargeter* Asset) {};
	// override to give your operation a nice name to display in the UI
	virtual FText GetNiceName() const { checkNoEntry() return FText::GetEmpty(); };
	// override to display a warning message in the op stack
	virtual FText WarningMessage() const { return FText::GetEmpty(); };
#endif
};

// wrapper around a TArray of Retarget Ops for display in details panel
UCLASS(BlueprintType)
class IKRIG_API URetargetOpStack: public UObject
{
	GENERATED_BODY()
	
public:

	// stack of operations to run AFTER the main retarget phases (Order is: Root/IK/FK/Post) 
	UPROPERTY()
	TArray<TObjectPtr<URetargetOpBase>> RetargetOps;

	// pointer to editor for details customization
#if WITH_EDITOR
	TSharedPtr<FIKRetargetEditorController> EditorController;
#endif
};