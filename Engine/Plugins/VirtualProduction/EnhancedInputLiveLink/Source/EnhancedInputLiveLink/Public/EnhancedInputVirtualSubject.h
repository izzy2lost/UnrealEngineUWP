// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"
#include "EnhancedInputSubsystemInterface.h"
#include "VirtualSubjects/LiveLinkBlueprintVirtualSubject.h"

#include "EnhancedInputVirtualSubject.generated.h"

class UInputComponent;
class UInputMappingContext;
class IEnhancedInputSubsystemInterface;
class UInputMappingContext;
class UMotionControllerComponent;

/**
 * 
 */
UCLASS(Blueprintable, Abstract)
class ENHANCEDINPUTLIVELINK_API UEnhancedInputVirtualSubject : public ULiveLinkBlueprintVirtualSubject
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, Category="Enhanced Input LiveLink", meta=(AllowedClasses="/Script/EnhancedInput.InputMappingContext"))
	FSoftObjectPath MappingContext;

	/** Returns the current InputComponent. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Input|Editor")
	UInputComponent* GetInputComponent() const
	{
		return InputComponent.Get();
	}
	
	void RegisterInputComponent();
	void UnregisterInputComponent();

protected:
	bool bIsInputRegistered = false;
	IEnhancedInputSubsystemInterface* GetEnhancedInputSubsystemInterface() const;
	
private:
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UInputComponent> InputComponent;
	
	virtual void Initialize(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, ILiveLinkClient* InLiveLinkClient) override;
	/** Creates the InputComponent if it does not already exist and registers all sub-object callbacks to it */
	void CreateEditorInput();

	/** Removes the InputComponent from this object */
	void RemoveEditorInput();
		
};
