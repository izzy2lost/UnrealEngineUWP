// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMComponentPreview.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialValue.h"
#include "Components/DMTextureUV.h"
#include "Components/DMTextureUVDynamic.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Slate/SDMEditor.h"
#include "Widgets/Images/SImage.h"

SDMComponentPreview::SDMComponentPreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{
	Brush.SetUVRegion(FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector));
}

SDMComponentPreview::~SDMComponentPreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialComponent* Component = ComponentWeak.Get())
	{
		Component->GetOnUpdate().RemoveAll(this);

		if (TSharedPtr<SDMEditor> EditorWidget = EditorWidgetWeak.Pin())
		{
			EditorWidget->FreePreviewMaterial(Component);
		}
	}

	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBaseWeak.Get()))
	{
		MaterialModel->GetOnValueUpdateDelegate().RemoveAll(this);
		MaterialModel->GetOnTextureUVUpdateDelegate().RemoveAll(this);
	}
	else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBaseWeak.Get()))
	{
		MaterialModelDynamic->GetOnValueDynamicUpdateDelegate().RemoveAll(this);
		MaterialModelDynamic->GetOnTextureUVDynamicUpdateDelegate().RemoveAll(this);
	}
}

void SDMComponentPreview::Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditorWidget, UDMMaterialComponent* InComponent)
{
	EditorWidgetWeak = InEditorWidget;
	ComponentWeak = InComponent;

	PreviewMaterialBaseWeak = InEditorWidget->CreatePreviewMaterial(InComponent);
	PreviewMaterialDynamicWeak = InEditorWidget->CreateMID(PreviewMaterialBaseWeak.Get());
	MaterialModelBaseWeak = InEditorWidget->GetMaterialModelBase();

	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBaseWeak.Get()))
	{
		MaterialModel->GetOnValueUpdateDelegate().AddSP(this, &SDMComponentPreview::OnValueUpdated);
		MaterialModel->GetOnTextureUVUpdateDelegate().AddSP(this, &SDMComponentPreview::OnTextureUVUpdated);
		MaterialModel->ApplyComponents(PreviewMaterialDynamicWeak.Get());
	}
	else if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBaseWeak.Get()))
	{
		MaterialModelDynamic->GetOnValueDynamicUpdateDelegate().AddSP(this, &SDMComponentPreview::OnValueDynamicUpdated);
		MaterialModelDynamic->GetOnTextureUVDynamicUpdateDelegate().AddSP(this, &SDMComponentPreview::OnTextureUVDynamicUpdated);
		MaterialModelDynamic->ApplyComponents(PreviewMaterialDynamicWeak.Get());
	}
	else
	{
		return;
	}

	PreviewSize = InArgs._PreviewSize;

	SetCanTick(true);

	Brush.SetImageSize(PreviewSize.Get());

	if (ensure(IsValid(InComponent)))
	{
		InComponent->GetOnUpdate().AddSP(this, &SDMComponentPreview::OnComponentUpdated);
		OnComponentUpdated(InComponent, EDMUpdateType::Structure);
	}

	ChildSlot
	[
		SNew(SImage)
		.Image(&Brush)
		.DesiredSizeOverride(this, &SDMComponentPreview::GetPreviewSize)
	];
}

void SDMComponentPreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialBaseWeak.IsValid() || !PreviewMaterialDynamicWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}	
}

void SDMComponentPreview::OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent))
	{
		if (Stage == ComponentWeak.Get() && IsValid(Stage) && Stage->IsComponentValid())
		{
			if (TSharedPtr<SDMEditor> EditorWidget = EditorWidgetWeak.Pin())
			{
				UMaterial* PreviewMaterialBase = PreviewMaterialBaseWeak.Get();

				if (!PreviewMaterialBase)
				{
					PreviewMaterialBase = EditorWidget->CreatePreviewMaterial(Stage);
				}

				if (InUpdateType == EDMUpdateType::Structure)
				{
					Stage->GeneratePreviewMaterial(PreviewMaterialBase);

					EditorWidget->FreeMID(PreviewMaterialBase);
					PreviewMaterialDynamicWeak = EditorWidget->CreateMID(PreviewMaterialBase);

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

TOptional<FVector2D> SDMComponentPreview::GetPreviewSize() const
{
	return PreviewSize.Get();
}

void SDMComponentPreview::OnValueUpdated(UDynamicMaterialModel* InMaterialModel, UDMMaterialValue* InValue)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InValue->SetMIDParameter(PreviewMaterialDynamic);
	}
}

void SDMComponentPreview::OnTextureUVUpdated(UDynamicMaterialModel* InMaterialModel, UDMTextureUV* InTextureUV)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InTextureUV->SetMIDParameters(PreviewMaterialDynamic);
	}
}

void SDMComponentPreview::OnValueDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMMaterialValueDynamic* InValueDynamic)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InValueDynamic->SetMIDParameter(PreviewMaterialDynamic);
	}
}

void SDMComponentPreview::OnTextureUVDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMTextureUVDynamic* InTextureUVDynamic)
{
	if (UMaterialInstanceDynamic* PreviewMaterialDynamic = PreviewMaterialDynamicWeak.Get())
	{
		InTextureUVDynamic->SetMIDParameters(PreviewMaterialDynamic);
	}
}
