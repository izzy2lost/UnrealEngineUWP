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

	if (UDynamicMaterialModelBase* MaterialModelBase = MaterialModelBaseWeak.Get())
	{
		if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBase))
		{
			MaterialModel->GetOnValueUpdateDelegate().RemoveAll(this);
			MaterialModel->GetOnTextureUVUpdateDelegate().RemoveAll(this);
		}
		else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBase))
		{
			MaterialModelDynamic->GetOnValueDynamicUpdateDelegate().RemoveAll(this);
			MaterialModelDynamic->GetOnTextureUVDynamicUpdateDelegate().RemoveAll(this);
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
		MaterialModel->GetOnValueUpdateDelegate().AddSP(this, &SDMMaterialComponentPreview::OnValueUpdated);
		MaterialModel->GetOnTextureUVUpdateDelegate().AddSP(this, &SDMMaterialComponentPreview::OnTextureUVUpdated);
		MaterialModel->ApplyComponents(PreviewMaterialDynamicWeak.Get());
	}
	else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBaseWeak.Get()))
	{
		MaterialModelDynamic->GetOnValueDynamicUpdateDelegate().AddSP(this, &SDMMaterialComponentPreview::OnValueDynamicUpdated);
		MaterialModelDynamic->GetOnTextureUVDynamicUpdateDelegate().AddSP(this, &SDMMaterialComponentPreview::OnTextureUVDynamicUpdated);
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
		OnComponentUpdated(InComponent, EDMUpdateType::Structure);
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

void SDMMaterialComponentPreview::OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent))
	{
		if (Stage == ComponentWeak.Get() && IsValid(Stage) && Stage->IsComponentValid())
		{
			if (TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin())
			{
				UMaterial* PreviewMaterialBase = PreviewMaterialBaseWeak.Get();

				if (!PreviewMaterialBase)
				{
					PreviewMaterialBase = EditorWidget->GetPreviewMaterialManager()->CreatePreviewMaterial(Stage);
				}

				if (EnumHasAnyFlags(InUpdateType, EDMUpdateType::Structure))
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
		}
	}
}

void SDMMaterialComponentPreview::OnValueUpdated(UDynamicMaterialModel* InMaterialModel, UDMMaterialValue* InValue)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InValue->SetMIDParameter(PreviewMaterialDynamic);
	}
}

void SDMMaterialComponentPreview::OnTextureUVUpdated(UDynamicMaterialModel* InMaterialModel, UDMTextureUV* InTextureUV)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InTextureUV->SetMIDParameters(PreviewMaterialDynamic);
	}
}

void SDMMaterialComponentPreview::OnValueDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMMaterialValueDynamic* InValueDynamic)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InValueDynamic->SetMIDParameter(PreviewMaterialDynamic);
	}
}

void SDMMaterialComponentPreview::OnTextureUVDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMTextureUVDynamic* InTextureUVDynamic)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InTextureUVDynamic->SetMIDParameters(PreviewMaterialDynamic);
	}
}
