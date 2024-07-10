// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "DMObjectMaterialProperty.generated.h"

class FProperty;
class UDynamicMaterialInstance;
class UDynamicMaterialModel;
class UDynamicMaterialModelBase;
class UObject;
class UPrimitiveComponent;

/**
 * Defines a material property slot that can be a Material Designer Instance.
 */
USTRUCT(BlueprintType)
struct FDMObjectMaterialProperty
{
	GENERATED_BODY()

	FDMObjectMaterialProperty();

	/** UPrimitiveComponent Material Index */
	DYNAMICMATERIALEDITOR_API FDMObjectMaterialProperty(UPrimitiveComponent* InOuter, int32 InIndex);

	/** Class Property (including potential array index) */
	DYNAMICMATERIALEDITOR_API FDMObjectMaterialProperty(UObject* InOuter, FProperty* InProperty, int32 InIndex = INDEX_NONE);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	TWeakObjectPtr<UObject> OuterWeak;

	/** C++ version of property */
	FProperty* Property;

	/** Blueprint version of property */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	FName PropertyName;

	/** Component or array property index. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material")
	int32 Index;

	DYNAMICMATERIALEDITOR_API UDynamicMaterialModelBase* GetMaterialModelBase() const;

	DYNAMICMATERIALEDITOR_API UDynamicMaterialInstance* GetMaterial() const;

	DYNAMICMATERIALEDITOR_API void SetMaterial(UDynamicMaterialInstance* DynamicMaterial);

	DYNAMICMATERIALEDITOR_API bool IsValid() const;

	DYNAMICMATERIALEDITOR_API FText GetPropertyName(bool bInIgnoreNewStatus) const;

	DYNAMICMATERIALEDITOR_API void Reset();

	template<typename InClass>
	InClass* GetTypedOuter() const
	{
		if (UObject* Outer = OuterWeak.Get())
		{
			if (InClass* CastOuter = Cast<InClass>(Outer))
			{
				return CastOuter;
			}

			return Outer->GetTypedOuter<InClass>();
		}

		return nullptr;
	}
};
