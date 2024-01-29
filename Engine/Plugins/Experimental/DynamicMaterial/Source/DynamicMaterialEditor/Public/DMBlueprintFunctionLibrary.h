// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "DMEDefs.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DMBlueprintFunctionLibrary.generated.h"

class AActor;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageBlend;
class UDMMaterialStageExpression;
class UDMMaterialStageGradient;
class UDMMaterialStageInputValue;
class UDMMaterialValue;
class UDynamicMaterialModel;
class UTexture;
struct FDMObjectMaterialProperty;

/**
 * Material Designer Blueprint Function Library
 */
UCLASS()
class DYNAMICMATERIALEDITOR_API UDMBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDMMaterialStageInputValue* FindDefaultStageOpacityInputValue(UDMMaterialStage* InStage);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static void SetDefaultStageSourceTexture(UDMMaterialStage* InStage, UTexture* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static TArray<FDMObjectMaterialProperty> GetActorDynamicMaterialSlots(AActor* InActor, const bool bInIncludeEmptySlots);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static TArray<FDMObjectMaterialProperty> GetActorMaterialProperties(AActor* InActor);
	
	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDynamicMaterialModel* FindFirstValidActorMaterialSlot(const TArray<AActor*>& InActors);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static FDMObjectMaterialProperty MakePrimitiveComponentSlot(UPrimitiveComponent* InComponent, int32 InSlot);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static FDMObjectMaterialProperty MakeObjectMaterialProperty(UObject* InObject, FName PropertyName);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static FDMObjectMaterialProperty MakeObjectMaterialPropertyArray(UObject* InObject, FName PropertyName, int32 ArrayIndex);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static UDynamicMaterialModel* CreateDynamicMaterialInObject(FDMObjectMaterialProperty& InMaterialProperty);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDynamicMaterialModel* GetObjectPropertyMaterialModel(const FDMObjectMaterialProperty& InMaterialProperty) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDynamicMaterialInstance* GetObjectPropertyMaterial(const FDMObjectMaterialProperty& InMaterialProperty) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetObjectPropertyMaterial(UPARAM(Ref) FDMObjectMaterialProperty& InMaterialProperty, UDynamicMaterialInstance* InDynamicMaterial);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool IsObjectPropertyValid(const FDMObjectMaterialProperty& InMaterialProperty) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	FText GetObjectPropertyName(const FDMObjectMaterialProperty& InMaterialProperty, bool bInIgnoreNewStatus) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static bool ExportMaterialInstance(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath);

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	static bool ExportGeneratedMaterial(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath);
};
