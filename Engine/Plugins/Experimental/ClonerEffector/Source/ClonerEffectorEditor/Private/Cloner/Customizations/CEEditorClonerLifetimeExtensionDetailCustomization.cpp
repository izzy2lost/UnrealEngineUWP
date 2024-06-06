// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Customizations/CEEditorClonerLifetimeExtensionDetailCustomization.h"

#include "Cloner/Extensions/CEClonerLifetimeExtension.h"
#include "DetailBuilderTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataInterfaceCurve.h"
#include "PropertyHandle.h"

void FCEEditorClonerLifetimeExtensionDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	const TArray<TWeakObjectPtr<UCEClonerLifetimeExtension>> LifetimeExtensionsWeak = InDetailBuilder.GetObjectsOfTypeBeingCustomized<UCEClonerLifetimeExtension>();

	FAddPropertyParams Params;
	Params.HideRootObjectNode(true);
	Params.CreateCategoryNodes(false);

	for (const TWeakObjectPtr<UCEClonerLifetimeExtension>& LifetimeExtensionWeak : LifetimeExtensionsWeak)
	{
		const UCEClonerLifetimeExtension* LifetimeExtension = LifetimeExtensionWeak.Get();

		if (!LifetimeExtension)
		{
			continue;
		}

		const FName CategoryName = LifetimeExtension->GetExtensionName();

		IDetailCategoryBuilder& CategoryBuilder = InDetailBuilder.EditCategory(CategoryName);
		CategoryBuilder.SetShowAdvanced(true);

		/**
		 * UNiagaraDataInterfaceCurve cannot display simultaneously multiple curves, so we need to add them separately
		 */
		if (IDetailPropertyRow* Row = CategoryBuilder.AddExternalObjects({LifetimeExtension->GetLifetimeScaleCurveDI()}, EPropertyLocation::Advanced, Params))
		{
			const TAttribute<EVisibility> VisibilityAttr = TAttribute<EVisibility>::CreateSP(this, &FCEEditorClonerLifetimeExtensionDetailCustomization::GetCurveVisibility, LifetimeExtensionWeak);
			Row->Visibility(VisibilityAttr);
		}
	}
}

EVisibility FCEEditorClonerLifetimeExtensionDetailCustomization::GetCurveVisibility(TWeakObjectPtr<UCEClonerLifetimeExtension> InExtensionWeak) const
{
	const UCEClonerLifetimeExtension* Extension = InExtensionWeak.Get();

	if (!Extension)
	{
		return EVisibility::Collapsed;
	}

	return Extension->GetLifetimeEnabled()
		&& Extension->GetLifetimeScaleEnabled()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}
