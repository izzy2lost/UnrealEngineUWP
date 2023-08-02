// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActorCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"


FChaosVDParticleActorCustomization::FChaosVDParticleActorCustomization()
{
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDCategoryName);
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDVisualizationCategoryName);
}

TSharedRef<IDetailCustomization> FChaosVDParticleActorCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDParticleActorCustomization );
}

void FChaosVDParticleActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory(ChaosVDVisualizationCategoryName, FText::GetEmpty(), ECategoryPriority::Important);

	FChaosVDDetailsCustomizationUtils::HideAllCategories(DetailBuilder, AllowedCategories);

	DetailBuilder.EditCategory(ChaosVDCategoryName).InitiallyCollapsed(false);
}
