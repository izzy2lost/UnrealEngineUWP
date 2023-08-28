// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "PropertyBag.h"
#include "Templates/SubclassOf.h"
#include "ViewModel/MVVMInstancedViewModelGeneratedClass.h"
#include "MVVMBlueprintInstancedViewModel.generated.h"

/**
 *
 */
UCLASS(Abstract, Within = MVVMBlueprintView)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintInstancedViewModelBase : public UObject
{
	GENERATED_BODY()

public:
	UMVVMBlueprintInstancedViewModelBase();

	void GenerateClass();

	UClass* GetGeneratedClass() const
	{
		return GeneratedClass;
	}

	virtual const UStruct* GetSourceStruct() const PURE_VIRTUAL(GetSourceStruct, return nullptr;);
	virtual const uint8* GetSourceDefaults() const PURE_VIRTUAL(GetSourceDefaults, return nullptr;);

protected:
	virtual void PreAddProperties();
	virtual void AddProperty(const FProperty* FromProperty);
	virtual void PostAddProperties();

	virtual void PreSetDefaultValues();
	virtual void SetDefaultValue(const FProperty* FromProperty, void const* SourceValuePtr);
	virtual void PostSetDefaultValues();

	virtual bool IsValidFieldName(const FName NewPropertyName) const;

protected:
	struct FInitializePropertyArgs
	{
		FName PropertyName;
		FString DisplayName;
		bool bFieldNotify = true;
		bool bReadOnly = false;
		bool bNetwork = false;
		bool bPrivate = false;
	};
	void InitializeProperty(FProperty* NewProperty, FInitializePropertyArgs& Args);
	void LinkProperty(FProperty* NewProperty);
	FName AddOnRepFunction(FName PropertyName);

	TMap<const FProperty*, FProperty*> FromPropertyToCreatedProperty;

public:
	/** The base object of the generated class. */
	UPROPERTY(EditAnywhere, Category = "Viewmodel", meta=(AllowedClasses = "/Script/FieldNotification.NotifyFieldValueChanged", DisallowedClasses = "/Script/UMG.Widget"))
	TSubclassOf<UObject> ParentClass;

	UPROPERTY()
	TObjectPtr<UMVVMInstancedViewModelGeneratedClass> GeneratedClass;

protected:
	//~ if it changes, the GeneratedClass needs to be removed.
	UPROPERTY()
	TSubclassOf<UMVVMInstancedViewModelGeneratedClass> GeneratedClassType;
};

/**
 *
 */
UCLASS()
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintInstancedViewModel : public UMVVMBlueprintInstancedViewModelBase
{
	GENERATED_BODY()

public:
	virtual const UStruct* GetSourceStruct() const
	{
		return Variables.GetValue().GetScriptStruct();
}

	virtual const uint8* GetSourceDefaults() const
	{
		return Variables.GetValue().GetMemory();
	}

public:
	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	FInstancedPropertyBag Variables;	// todo, this should be a base and we have 2 implementation, one for the normal with a struct builder, and one for verse with the class ptr


public:
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#endif
