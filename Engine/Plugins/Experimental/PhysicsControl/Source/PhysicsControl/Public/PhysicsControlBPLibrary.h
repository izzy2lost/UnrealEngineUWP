// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if 1

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimNodeReference.h"
#include "SkeletalControlLibrary.h"

#include "PhysicsControlBPLibrary.generated.h"

struct FRigidBodyControlParameters;
struct FAnimNode_RigidBodyWithControl;

USTRUCT(BlueprintType)
struct FRigidBodyWithControlReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_RigidBodyWithControl FInternalNodeType;
};


UCLASS()
class PHYSICSCONTROL_API UPhysicsControlBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Add a single control parameter Parameters to a set of parameter Parameters. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe, AutoCreateRefTerm = "LinearStrengthMultiplier, LinearExtraDampingMultiplier, AngularStrengthMultiplier, AngularExtraDampingMultiplier, bEnabled"))
	static void AddControlParameters(
		UPARAM(ref, DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& InParameters, 
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters,
		const FName                        Name,
		const FRigidBodyControlSparseData& ControlData);

	/** Add a single control parameter Parameters to a set of parameter Parameters. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe, AutoCreateRefTerm = "LinearStrengthMultiplier, LinearExtraDampingMultiplier, AngularStrengthMultiplier, AngularExtraDampingMultiplier, bEnabled"))
	static void AddMultipleControlParameters(
		UPARAM(ref, DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& InParameters, 
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters,
		const TArray<FName>&               Names,
		const FRigidBodyControlSparseData& ControlData);

	/** Add a single body modifier parameter Parameters to a set of parameter Parameters. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static void AddModifierParameters(
		UPARAM(ref, DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& InParameters,
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters,
		const FName                         Name, 
		const FRigidBodyModifierSparseData& ModifierData);

	/** Add a single body modifier parameter Parameters to a set of parameter Parameters. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static void AddMultipleModifierParameters(
		UPARAM(ref, DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& InParameters, 
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters,
		const TArray<FName>&                Names,
		const FRigidBodyModifierSparseData& ModifierData);

	/** Returns the linear interpolation of two sets of parameter Parameters. Any Parameters that exists in one of the input sets but not the other will be added to the output with a weight of 1. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static void BlendParameters(
		UPARAM(ref, DisplayName = "From Parameters") FRigidBodyControlAndModifierParameters& InParametersA,
		UPARAM(ref, DisplayName = "To Parameters") FRigidBodyControlAndModifierParameters& InParametersB,
		UPARAM(DisplayName = "Weight") const float InInterpolationWeight,
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters);
	
	/** Adds an Parameters to the output parameters for each control name in the supplied set. The values in each Parameters will be a linear interpolation of the two supplied Parameters, blending from the start Parameters to the end Parameters across the elements in the set. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static void BlendControlParametersThroughSet(
		UPARAM(ref, DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& InParameters,
		UPARAM(ref, DisplayName = "Start Parameters") const FRigidBodyNamedControlParameters& InStartControlParameters,
		UPARAM(ref, DisplayName = "End Parameters") const FRigidBodyNamedControlParameters& InEndControlParameters,
		const TArray<FName>& ControlNames,
		FRigidBodyControlAndModifierParameters& OutParameters);

	/** Adds an Parameters to the output parameters for each modifier name in the supplied set. The values in each Parameters will be a linear interpolation of the two supplied Parameters, blending from the start Parameters to the end Parameters across the elements in the set. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static void BlendModifierParametersThroughSet(
		UPARAM(ref) FRigidBodyControlAndModifierParameters& InParameters,
		UPARAM(ref) const FRigidBodyNamedModifierParameters& InStartModifierParameters,
		UPARAM(ref) const FRigidBodyNamedModifierParameters& InEndModifierParameters,
		const TArray<FName>& ModifierNames,
		UPARAM(DisplayName = "Parameters") FRigidBodyControlAndModifierParameters& OutParameters);

	/** Get a Rigid Body With Control node reference from an anim node reference */
	UFUNCTION(BlueprintCallable, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FRigidBodyWithControlReference ConvertToRigidBodyWithControl(
		const FAnimNodeReference& Node,
		EAnimNodeReferenceConversionResult& Result);

	/** Get a Rigid Body With Control node from an anim node (pure) */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe, DisplayName = "Convert to Rigid Body With Control"))
	static void ConvertToRigidBodyWithControlPure(
		const FAnimNodeReference& Node, 
		FRigidBodyWithControlReference& RigidBodyWithControl, 
		bool& Result)
	{
		EAnimNodeReferenceConversionResult ConversionResult;
		RigidBodyWithControl = ConvertToRigidBodyWithControl(Node, ConversionResult);
		Result = (ConversionResult == EAnimNodeReferenceConversionResult::Succeeded);
	}

	/** Get the names of all the controls in a specified set managed by this Rigid Body With Control node. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static TArray<FName> GetControlNamesInSet(
		UPARAM(DisplayName = "Node") const FRigidBodyWithControlReference& RigidBodyWithControl, 
		const FName SetName);

	/** Get the names of all the body modifiers in a specified set managed by this Rigid Body With Control node. */
	UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
	static TArray<FName> GetBodyModifierNamesInSet(
		UPARAM(DisplayName = "Node") const FRigidBodyWithControlReference& RigidBodyWithControl, 
		const FName SetName);
};

#endif