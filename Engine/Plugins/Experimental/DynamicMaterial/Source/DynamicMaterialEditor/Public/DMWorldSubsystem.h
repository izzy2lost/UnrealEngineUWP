// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "DynamicMaterialEditorModule.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"
#include "DMWorldSubsystem.generated.h"

class AActor;
class IDetailKeyframeHandler;
class UDynamicMaterialInstance;
class UDynamicMaterialModel;
struct FDMObjectMaterialProperty;

DECLARE_DELEGATE_RetVal_OneParam(bool, FDMIsValidDelegate, UDynamicMaterialModel*)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FDMSetMaterialValueDelegate, const FDMObjectMaterialProperty&, UDynamicMaterialInstance*)
DECLARE_DELEGATE(FDMInvokeTabDelegate)
 
UCLASS()
class DYNAMICMATERIALEDITOR_API UDMWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
 
public:
	UDMWorldSubsystem();
 
	const TSharedPtr<IDetailKeyframeHandler>& GetKeyframeHandler() const { return KeyframeHandler; }
	void SetKeyframeHandler(const TSharedPtr<IDetailKeyframeHandler>& InKeyframeHandler);

	/** Sets material in a custom editor tab. */
	FDMSetMaterialModelDelegate& GetSetCustomEditorModelDelegate() { return CustomModelEditorDelegate; }

	/** Sets the object property in custom editor tab. */
	FDMSetMaterialObjectPropertyDelegate& GetCustomObjectPropertyEditorDelegate() { return CustomObjectPropertyEditorDelegate; }

	/** Sets actor in a custom editor tab. */
	FDMSetMaterialActorDelegate& GetSetCustomEditorActorDelegate() { return CustomActorEditorDelegate; }

	/** Returns true if the supplied material is valid for this world. */
	FDMIsValidDelegate& GetIsValidDelegate() { return IsValidDelegate; }

	/** Used to redirect SetMaterial to different objects/paths. */
	FDMSetMaterialValueDelegate& GetMaterialValueSetterDelegate() { return SetMaterialValueDelegate; }

	/** Used to show the tab to the user. */
	FDMInvokeTabDelegate& GetInvokeTabDelegate() { return InvokeTabDelegate; }

protected:
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler;
	FDMSetMaterialModelDelegate CustomModelEditorDelegate;
	FDMSetMaterialObjectPropertyDelegate CustomObjectPropertyEditorDelegate;
	FDMSetMaterialActorDelegate CustomActorEditorDelegate;
	FDMIsValidDelegate IsValidDelegate;
	FDMSetMaterialValueDelegate SetMaterialValueDelegate;
	FDMInvokeTabDelegate InvokeTabDelegate;
};
