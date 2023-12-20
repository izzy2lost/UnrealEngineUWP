// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression_MaterialBase.h"
#include "Materials/Material.h"
#include "TG_Texture.h"

#include "TG_Expression_Material.generated.h"

typedef std::weak_ptr<class Job>		JobPtrW;

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Material : public UTG_Expression_MaterialBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// The input material to employ for rendering
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Category = NoCategory, meta = (TGType = "TG_Setting"))
	TObjectPtr<UMaterialInterface> Material;
	
	void SetMaterial(UMaterialInterface* InMaterial);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting"))
	EDrawMaterialAttributeTarget RenderedAttributeId = EDrawMaterialAttributeTarget::BaseColor;	// The attribute identifier among all the attributes of the material that is rendered in the output
	
	virtual bool CanHandleAsset(UObject* Asset) override;
	virtual void SetAsset(UObject* Asset) override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Renders a material into a quad and makes it available. It is automatically exposed as a graph input parameter.")); } 

protected:
	
	virtual void SetMaterialInternal(UMaterialInterface* InMaterial) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FName TitleName = TEXT("Material");

public:
	
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;

	virtual FName GetCategory() const override { return TG_Category::Input;}
	
protected:
	virtual TObjectPtr<UMaterialInterface> GetMaterial() const override { return Material;};
	virtual EDrawMaterialAttributeTarget GetRenderedAttributeId()  override { return RenderedAttributeId;}
};

