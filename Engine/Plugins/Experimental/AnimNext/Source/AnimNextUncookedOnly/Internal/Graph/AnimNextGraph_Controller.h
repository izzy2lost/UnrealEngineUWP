// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "RigVMModel/RigVMController.h"
#include "Param/AnimNextParamInstanceIdentifier.h"
#include "InstancedStruct.h"
#include "AnimNextGraph_Controller.generated.h"

struct FAnimNextParamType;

/**
  * Implements AnimNext RigVM controller extensions
  */
UCLASS(MinimalAPI)
class UAnimNextGraph_Controller : public URigVMController
{
	GENERATED_BODY()

public:
	// Adds a unit node with a dynamic number of pins
	URigVMUnitNode* AddUnitNodeWithPins(UScriptStruct* InScriptStruct, const FRigVMPinInfoArray& PinArray, const FName& InMethodName = TEXT("Execute"), const FVector2D& InPosition = FVector2D::ZeroVector, const FString& InNodeName = TEXT(""), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Sets the parameter referenced by a set/get parameter node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	ANIMNEXTUNCOOKEDONLY_API bool SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None, const UObject* ValueTypeObject = nullptr, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	ANIMNEXTUNCOOKEDONLY_API bool SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Creates a Get Parameter node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddGetAnimNextParameterNode(const FVector2D& InPosition, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None, const UObject* ValueTypeObject = nullptr, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddGetAnimNextParameterNode(const FVector2D& InPosition, FName ParameterName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Creates a Get Graph Parameter node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddGetAnimNextGraphParameterNode(const FVector2D& InPosition, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None, const UObject* ValueTypeObject = nullptr, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddGetAnimNextGraphParameterNode(const FVector2D& InPosition, FName ParameterName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

	// Creates a Set Graph Parameter node
	UFUNCTION(BlueprintCallable, Category = RigVMController)
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddSetAnimNextGraphParameterNode(const FVector2D& InPosition, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None, const UObject* ValueTypeObject = nullptr, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);
	ANIMNEXTUNCOOKEDONLY_API URigVMNode* AddSetAnimNextGraphParameterNode(const FVector2D& InPosition, FName ParameterName, const FAnimNextParamType& InType, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId = TInstancedStruct<FAnimNextParamInstanceIdentifier>(), bool bSetupUndoRedo = true, bool bPrintPythonCommand = false);

private:
	URigVMNode* AddGetAnimNextParameterNodeInternal(UScriptStruct* NodeStruct, const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand);
};
