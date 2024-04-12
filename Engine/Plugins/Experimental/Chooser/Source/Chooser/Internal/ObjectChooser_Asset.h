// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IObjectChooser.h"
#include "ObjectChooser_Asset.generated.h"

USTRUCT(DisplayName = "Asset")
struct CHOOSER_API FAssetChooser : public FObjectChooserBase
{
	GENERATED_BODY()
	
	// FObjectChooserBase interface
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;

#if WITH_EDITOR
	virtual UObject* GetReferencedObject() const override { return Asset; }
#endif
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UObject> Asset;
};

USTRUCT(DisplayName = "Soft Asset")
struct CHOOSER_API FSoftAssetChooser : public FObjectChooserBase
{
	GENERATED_BODY()

	// FObjectChooserBase interface
	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const final override;
	
#if WITH_EDITOR
	virtual UObject* GetReferencedObject() const override { return Asset.LoadSynchronous(); }
#endif
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TSoftObjectPtr<UObject> Asset;
};

// deprecated class for upgrading old data
UCLASS(ClassGroup = "LiveLink", deprecated)
class CHOOSER_API UDEPRECATED_ObjectChooser_Asset : public UObject, public IObjectChooser
{
	GENERATED_BODY()
	
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const override
	{
		OutInstancedStruct.InitializeAs(FAssetChooser::StaticStruct());
		FAssetChooser& AssetChooser = OutInstancedStruct.GetMutable<FAssetChooser>();
		AssetChooser.Asset = Asset;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UObject> Asset;
};