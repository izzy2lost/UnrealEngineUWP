// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimusDataType.h"
#include "PropertyBag.h"
#include "OptimusValueContainerStruct.generated.h"

USTRUCT()
struct OPTIMUSCORE_API FOptimusValueContainerStruct
{
	GENERATED_BODY()

	static const TCHAR* ValuePropertyName;
	
	UPROPERTY(EditAnywhere, Category="Value", meta=(FixedLayout, ShowOnlyInnerProperties))
	FInstancedPropertyBag Value;

	bool IsInitialized() const;
	void SetType(FOptimusDataTypeRef InDataType);
	FShaderValueContainer GetShaderValue(FOptimusDataTypeRef InDataType) const;
	const FProperty* GetValueProperty() const;
	const uint8* GetValueMemory() const;
	
protected:
	friend class UOptimusValueContainer;
	
	uint8* GetMutableValueMemory();
	
};
