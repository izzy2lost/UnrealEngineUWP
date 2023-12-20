// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Input/TG_Expression_Material.h"

#if WITH_EDITOR
void UTG_Expression_Material::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// First catch if Material changes
	if (PropertyChangedEvent.GetPropertyName() == FName(TEXT("Material")))
	{
		UE_LOG(LogTextureGraph, Log, TEXT("Material Expression PostEditChangeProperty."));
		SetMaterialInternal(Material);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UTG_Expression_Material::SetMaterialInternal(UMaterialInterface* InMaterial)
{
	if (!InMaterial)
	{
		Material = nullptr;
	}
	else if (InMaterial->IsA<UMaterial>())
	{
		// create a material instance
		Material = InMaterial;
	}
	else if (InMaterial->IsA<UMaterialInstance>())
	{
		Material = InMaterial;
	}

	Super::SetMaterialInternal(InMaterial);
}


void UTG_Expression_Material::SetMaterial(UMaterialInterface* InMaterial)
{
	// This is the public setter of the material, 
	// This is NOT called if the Material is modified from the detail panel!!!
	// We catch that case in PostEditChangeProperty, which will call SetMaterialInternal 

	// If it is the same Material then avoid anymore work, we shoudl be good to go
	if (InMaterial == Material)
	{
		// Just check that the MaterialInstance is valid, if not reassign below
		if ((!Material) || (Material && MaterialInstance))
			return;
	}

	SetMaterialInternal(InMaterial);
}

bool UTG_Expression_Material::CanHandleAsset(UObject* Asset)
{
	const UMaterialInterface* Mat = Cast<UMaterialInterface>(Asset);
	
	return Mat !=nullptr;
}

void UTG_Expression_Material::SetAsset(UObject* Asset)
{
	if(UMaterialInterface* MaterialAsset = Cast<UMaterialInterface>(Asset); MaterialAsset != nullptr)
	{
		SetMaterial(MaterialAsset);
#if WITH_EDITOR
		// We need to find its property and trigger property change event manually.
		const auto SourcePin = GetParentNode()->GetInputPin("Material");

		check(SourcePin)
	
		if(SourcePin)
		{
			auto Property = SourcePin->GetExpressionProperty();
			PropertyChangeTriggered(Property, EPropertyChangeType::ValueSet);
		}
#endif
	}
}

void UTG_Expression_Material::SetTitleName(FName NewName)
{
	TitleName = NewName;
}

FName UTG_Expression_Material::GetTitleName() const
{
	return TitleName;
}



