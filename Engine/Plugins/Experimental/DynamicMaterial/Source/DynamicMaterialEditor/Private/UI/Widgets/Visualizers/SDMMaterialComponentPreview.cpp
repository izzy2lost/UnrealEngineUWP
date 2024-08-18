// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Visualizers/SDMMaterialComponentPreview.h"

#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialValue.h"
#include "Components/DMTextureUV.h"
#include "Components/DMTextureUVDynamic.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "UI/Utils/DMPreviewMaterialManager.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "Widgets/Images/SImage.h"

SDMMaterialComponentPreview::SDMMaterialComponentPreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{
	Brush.SetUVRegion(FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector));
}

SDMMaterialComponentPreview::~SDMMaterialComponentPreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialComponent* Component = ComponentWeak.Get())
	{
		Component->GetOnUpdate().RemoveAll(this);

		if (TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin())
		{
			EditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterial(Component);
		}
	}
}

void SDMMaterialComponentPreview::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget, UDMMaterialComponent* InComponent)
{
	EditorWidgetWeak = InEditorWidget;
	ComponentWeak = InComponent;

	PreviewMaterialBaseWeak = InEditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterial(InComponent);
	PreviewMaterialDynamicWeak = InEditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterialDynamic(PreviewMaterialBaseWeak.Get());
	MaterialModelBaseWeak = InEditorWidget->GetMaterialModelBase();

	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBaseWeak.Get()))
	{
		MaterialModel->ApplyComponents(PreviewMaterialDynamicWeak.Get());
	}
	else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBaseWeak.Get()))
	{
		MaterialModelDynamic->ApplyComponents(PreviewMaterialDynamicWeak.Get());
	}
	else
	{
		return;
	}

	SetCanTick(true);

	if (ensure(IsValid(InComponent)))
	{
		InComponent->GetOnUpdate().AddSP(this, &SDMMaterialComponentPreview::OnComponentUpdated);
		OnComponentUpdated(InComponent, InComponent, EDMUpdateType::Structure);
	}

	ChildSlot
	[
		SNew(SImage)
		.Image(&Brush)
		.DesiredSizeOverride(InArgs._PreviewSize)
	];
}

void SDMMaterialComponentPreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialBaseWeak.IsValid() || !PreviewMaterialDynamicWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}	
}

void SDMMaterialComponentPreview::OnComponentUpdated(UDMMaterialComponent* InComponent, UDMMaterialComponent* InSource, EDMUpdateType InUpdateType)
{
	UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent);

	if (!Stage)
	{
		return;
	}

	if (Stage != ComponentWeak.Get() || !IsValid(Stage) || !Stage->IsComponentValid())
	{
		return;
	}

	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UMaterial* PreviewMaterialBase = PreviewMaterialBaseWeak.Get();

	if (!PreviewMaterialBase)
	{
		PreviewMaterialBase = EditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterial(Stage);
	}

	UMaterialInstanceDynamic* MID = PreviewMaterialDynamicWeak.Get();

	if (!MID || !EnumHasAnyFlags(InUpdateType, EDMUpdateType::Structure))
	{
		if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(InSource))
		{
			Value->SetMIDParameter(MID);
		}
		else if (UDMMaterialValueDynamic* ValueDynamic = Cast<UDMMaterialValueDynamic>(InSource))
		{
			ValueDynamic->SetMIDParameter(MID);
		}
		else if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(InSource))
		{
			TextureUV->SetMIDParameters(MID);
		}
		else if (UDMTextureUVDynamic* TextureUVDynamic = Cast<UDMTextureUVDynamic>(InSource))
		{
			TextureUVDynamic->SetMIDParameters(MID);
		}
	}
	else
	{
		Stage->GeneratePreviewMaterial(PreviewMaterialBase);

		EditorWidget->GetPreviewMaterialManager()->FreePreviewMaterialDynamic(PreviewMaterialBase);
		PreviewMaterialDynamicWeak = EditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterialDynamic(PreviewMaterialBase);

		UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase();

		if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBase))
		{
			MaterialModel->ApplyComponents(PreviewMaterialDynamicWeak.Get());
		}
		else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBase))
		{
			MaterialModelDynamic->ApplyComponents(PreviewMaterialDynamicWeak.Get());
		}

		Brush.SetMaterial(PreviewMaterialDynamicWeak.Get());
	}
}
