// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "InstancedStruct.h"
#include "Chooser.h"
#include "ChooserPropertyAccess.h"

#include "ChooserInitializer.generated.h"

USTRUCT()
struct FChooserInitializer
{
	GENERATED_BODY()
	virtual ~FChooserInitializer() {}
	virtual void Initialize(UChooserTable* Chooser) const {};
};


USTRUCT(DisplayName="Generic Chooser", Meta = (ToolTip="A ChooserTable for use in Blueprint, which can return an arbitrary Asset type or Class, and can take any number of Objects or Structs as Parameters.") )
struct FGenericChooserInitializer : public FChooserInitializer
{
	GENERATED_BODY()

	virtual void Initialize(UChooserTable* Chooser) const override;
	
	UPROPERTY(EditAnywhere, DisplayName= "Result Class", Category="Result", Meta = (AllowAbstract=true))
	TObjectPtr<UClass> OutputObjectType;
	
	UPROPERTY(EditAnywhere, DisplayName = "Result Type", Category="Result")
	EObjectChooserResultType ResultType = EObjectChooserResultType::ObjectResult;
	
	UPROPERTY(EditAnywhere, DisplayName = "Parameters", NoClear, Meta = (ExpandByDefault, ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ContextObjectTypeBase"), Category = "Parameters")
	TArray<FInstancedStruct> ContextData;
};

USTRUCT(DisplayName="Animation Chooser", meta = (ToolTip="A ChooserTable for use with the ChooserPlayer AnimGraph node.\nReturns an AnimAsset, and takes an AnimInstance, and a ChooserPlayerSettings struct as parameters."))
struct FChooserPlayerInitializer : public FChooserInitializer
{
	GENERATED_BODY()
	virtual void Initialize(UChooserTable* Chooser) const override;

	UPROPERTY(EditAnywhere, Category="AnimClass")
	TObjectPtr<UClass> AnimClass;
};