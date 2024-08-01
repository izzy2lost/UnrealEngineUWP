// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/DynamicMaterialInstance.h"

#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"

#if WITH_EDITOR
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#endif

UDynamicMaterialInstance::UDynamicMaterialInstance()
{
	MaterialModelBase = nullptr;

	bOutputTranslucentVelocity = true;
}

UDynamicMaterialModelBase* UDynamicMaterialInstance::GetMaterialModelBase()
{
	return MaterialModelBase;
}

UDynamicMaterialModel* UDynamicMaterialInstance::GetMaterialModel()
{
	if (IsValid(MaterialModelBase))
	{
		return MaterialModelBase->ResolveMaterialModel();
	}

	return nullptr;
}

#if WITH_EDITOR
void UDynamicMaterialInstance::SetMaterialModel(UDynamicMaterialModelBase* InMaterialModel)
{
	MaterialModelBase = InMaterialModel;

	if (InMaterialModel)
	{
		InMaterialModel->Rename(nullptr, this, UE::DynamicMaterial::RenameFlags);
	}
}

void UDynamicMaterialInstance::InitializeMIDPublic()
{
	check(MaterialModelBase);

	UDynamicMaterialModel* MaterialModel = MaterialModelBase->ResolveMaterialModel();
	check(MaterialModel);

	SetParentInternal(MaterialModel->GetGeneratedMaterial(), false);
	ClearParameterValues();
	UpdateCachedData();
}

void UDynamicMaterialInstance::PostDuplicate(bool bInDuplicateForPIE)
{
	Super::PostDuplicate(bInDuplicateForPIE);

	if (MaterialModelBase)
	{
		MaterialModelBase->SetDynamicMaterialInstance(this);

		if (UDynamicMaterialModel* MaterialModel = MaterialModelBase->ResolveMaterialModel())
		{
			if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
			{
				ModelEditorOnlyData->RequestMaterialBuild();
			}
		}
	}
}

void UDynamicMaterialInstance::PostEditImport()
{
	Super::PostEditImport();

	if (MaterialModelBase)
	{
		MaterialModelBase->SetDynamicMaterialInstance(this);

		if (UDynamicMaterialModel* MaterialModel = MaterialModelBase->ResolveMaterialModel())
		{
			if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
			{
				ModelEditorOnlyData->RequestMaterialBuild();
			}
		}
	}
}

void UDynamicMaterialInstance::OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModel)
{
	if (MaterialModelBase != InMaterialModel)
	{
		return;
	}

	InitializeMIDPublic();
}
#endif