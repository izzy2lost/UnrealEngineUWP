// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Customizations/CEEditorClonerLifetimeExtensionDetailCustomization.h"

#include "Cloner/Extensions/CEClonerLifetimeExtension.h"
#include "Cloner/Sequencer/MovieSceneClonerTrackEditor.h"
#include "DetailBuilderTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataInterfaceCurve.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CEEditorClonerLifetimeExtensionDetailCustomization"

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

		UNiagaraDataInterfaceCurve* CurveDI = LifetimeExtension->GetLifetimeScaleCurveDI();

		if (!CurveDI)
		{
			continue;
		}

		const FName CategoryName = LifetimeExtension->GetExtensionName();
		IDetailCategoryBuilder& CurveCategoryBuilder = InDetailBuilder.EditCategory(FName(CategoryName.ToString() + TEXT("Curve")), FText::GetEmpty(), ECategoryPriority::Uncommon);

		// Hide other properties, only curve will be shown instead of tree
		for (FProperty* Property : TFieldRange<FProperty>(CurveDI->GetClass()))
		{
			if (Property)
			{
				Property->SetMetaData(TEXT("EditCondition"), TEXT("false"));
				Property->SetMetaData(TEXT("EditConditionHides"), TEXT("true"));
			}
		}

		// UNiagaraDataInterfaceCurve cannot display simultaneously multiple curves, so we need to add them separately
		if (IDetailPropertyRow* Row = CurveCategoryBuilder.AddExternalObjects({CurveDI}, EPropertyLocation::Common, Params))
		{
			const TAttribute<EVisibility> VisibilityAttr = TAttribute<EVisibility>::CreateSP(this, &FCEEditorClonerLifetimeExtensionDetailCustomization::GetCurveVisibility, LifetimeExtensionWeak);
			Row->Visibility(VisibilityAttr);
		}

		if (LifetimeExtensionsWeak.Num() == 1)
		{
			const FText ButtonLabel = LOCTEXT("AddSequencerTrack", "Add Sequencer Tracks");

			IDetailCategoryBuilder& LifetimeCategoryBuilder = InDetailBuilder.EditCategory(CategoryName);

			LifetimeCategoryBuilder.AddCustomRow(ButtonLabel)
			.WholeRowContent()
			.HAlign(HAlign_Left)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.f, 3.f)
				.AutoHeight()
				[
					SNew(SButton)
					.Text(ButtonLabel)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					.IsEnabled(this, &FCEEditorClonerLifetimeExtensionDetailCustomization::CanAddSequencerTracks, LifetimeExtensionWeak)
					.OnClicked(this, &FCEEditorClonerLifetimeExtensionDetailCustomization::OnAddSequencerTracks, LifetimeExtensionWeak)
				]
			];
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

bool FCEEditorClonerLifetimeExtensionDetailCustomization::CanAddSequencerTracks(TWeakObjectPtr<UCEClonerLifetimeExtension> InClonerLifetimeExtension) const
{
	uint32 TrackCount = 0;

	if (const UCEClonerLifetimeExtension* LifetimeExtension = InClonerLifetimeExtension.Get())
	{
		FMovieSceneClonerTrackEditor::OnClonerTrackExists.Broadcast(LifetimeExtension->GetClonerComponent(), TrackCount);
	}

	// Lifecycle + cache tracks
	constexpr uint32 ExpectedTrackCount = 2;
	return TrackCount < ExpectedTrackCount;
}

FReply FCEEditorClonerLifetimeExtensionDetailCustomization::OnAddSequencerTracks(TWeakObjectPtr<UCEClonerLifetimeExtension> InClonerLifetimeExtension)
{
	if (const UCEClonerLifetimeExtension* LifetimeExtension = InClonerLifetimeExtension.Get())
	{
		FMovieSceneClonerTrackEditor::OnAddClonerTrack.Broadcast(LifetimeExtension->GetClonerComponent());
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
