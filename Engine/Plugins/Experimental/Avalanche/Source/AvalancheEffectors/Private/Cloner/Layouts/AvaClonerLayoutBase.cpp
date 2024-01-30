// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/AvaClonerLayoutBase.h"
#include "Cloner/AvaClonerActor.h"
#include "Cloner/AvaClonerComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaClonerLayoutBase, Log, All);

bool UAvaClonerLayoutBase::IsLayoutValid() const
{
	if (LayoutName.IsNone() || LayoutAssetPath.IsEmpty())
	{
		return false;
	}

	static const FString LayoutBaseAssetPath = TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerBase.NS_ClonerBase'");

	// Get the template niagara asset
	const UNiagaraSystem* TemplateNiagaraSystem = LoadSystemPath(LayoutAssetPath);

	// Get the base niagara asset
	const UNiagaraSystem* BaseNiagaraSystem = LoadSystemPath(LayoutBaseAssetPath);

	if (!TemplateNiagaraSystem || !BaseNiagaraSystem)
	{
		UE_LOG(LogAvaClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) or base system (%s) is invalid"), *LayoutName.ToString(), *LayoutAssetPath, *LayoutBaseAssetPath);
		return false;
	}

	// Compare parameters : template should have base parameters
	bool bIsSystemBasedOnBaseAsset = true;

	{
		TArray<FNiagaraVariable> TemplateSystemParameters;
		TemplateNiagaraSystem->GetExposedParameters().GetParameters(TemplateSystemParameters);

		TArray<FNiagaraVariable> BaseSystemParameters;
		BaseNiagaraSystem->GetExposedParameters().GetParameters(BaseSystemParameters);

		for (const FNiagaraVariable& SystemParameter : BaseSystemParameters)
		{
			if (!TemplateSystemParameters.Contains(SystemParameter))
			{
				bIsSystemBasedOnBaseAsset = false;
				UE_LOG(LogAvaClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) missing parameter (%s) from base system (%s)"), *LayoutName.ToString(), *LayoutAssetPath, *SystemParameter.ToString(), *LayoutBaseAssetPath);
				break;
			}
		}
	}

	if (!bIsSystemBasedOnBaseAsset)
	{
		UE_LOG(LogAvaClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) is not based off base system (%s)"), *LayoutName.ToString(), *LayoutAssetPath, *LayoutBaseAssetPath);
	}
	
	return bIsSystemBasedOnBaseAsset;
}

bool UAvaClonerLayoutBase::IsLayoutLoaded() const
{
	return !IsTemplate() && NiagaraSystem && MeshRenderer && DataInterfaces.IsValid();
}

bool UAvaClonerLayoutBase::LoadLayout()
{
	if (IsLayoutLoaded())
	{
		return true;
	}
	
	if (LayoutAssetPath.IsEmpty())
	{
		return false;
	}
	
	// Get the template niagara asset
	const UNiagaraSystem* TemplateNiagaraSystem = FindObject<UNiagaraSystem>(nullptr, *LayoutAssetPath);
	if (!TemplateNiagaraSystem)
	{
		TemplateNiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *LayoutAssetPath);
	}

	if (!TemplateNiagaraSystem)
	{
		return false;
	}

	// Copy template asset since we will modify it, and we do not want other cloner instances to have the new modified asset
	FObjectDuplicationParameters Parameters = InitStaticDuplicateObjectParams(TemplateNiagaraSystem, GetTransientPackage(), NAME_None, RF_Transient, nullptr, EDuplicateMode::Normal);
	NiagaraSystem = Cast<UNiagaraSystem>(StaticDuplicateObjectEx(Parameters));

	if (!NiagaraSystem)
	{
		return false;
	}

	for (FNiagaraEmitterHandle& SystemEmitterHandle : NiagaraSystem->GetEmitterHandles())
	{
		if (const FVersionedNiagaraEmitterData* EmitterData = SystemEmitterHandle.GetEmitterData())
		{
			for (UNiagaraRendererProperties* EmitterRenderer : EmitterData->GetRenderers())
			{
				if (UNiagaraMeshRendererProperties* EmitterMeshRenderer = Cast<UNiagaraMeshRendererProperties>(EmitterRenderer))
				{
					EmitterMeshRenderer->Meshes.Empty();
#if WITH_EDITORONLY_DATA
					EmitterMeshRenderer->OnMeshChanged();
#endif

					MeshRenderer = EmitterMeshRenderer;
					DataInterfaces = FAvaClonerEffectorDataInterfaces(NiagaraSystem);

					OnLayoutLoaded();
					
					return true;
				}
			}
		}
	}
	
	return false;
}

bool UAvaClonerLayoutBase::UnloadLayout()
{
	if (!IsLayoutLoaded())
	{
		return false;
	}

	// Cannot unload while active
	if (IsLayoutActive())
	{
		return false;
	}
	
	DataInterfaces = FAvaClonerEffectorDataInterfaces();
	MeshRenderer = nullptr;
	NiagaraSystem = nullptr;
	
	OnLayoutUnloaded();

	return true;
}

bool UAvaClonerLayoutBase::IsLayoutActive() const
{
	const UAvaClonerComponent* Component = GetClonerComponent();

	if (!Component)
	{
		return false;	
	}
	
	return IsLayoutLoaded() && Component->GetAsset() == NiagaraSystem;
}

bool UAvaClonerLayoutBase::ActivateLayout()
{
	if (IsLayoutActive())
	{
		return false;
	}

	// Load layout first
	if (!IsLayoutLoaded())
	{
		return false;
	}

	UAvaClonerComponent* ClonerComponent = GetClonerComponent();

	if (!ClonerComponent)
	{
		return false;
	}

	ClonerComponent->SetAsset(NiagaraSystem);

	OnLayoutActive();

	return true;
}

bool UAvaClonerLayoutBase::DeactivateLayout()
{
	if (!IsLayoutActive())
	{
		return false;
	}

	UAvaClonerComponent* ClonerComponent = GetClonerComponent();

	if (!ClonerComponent)
	{
		return false;
	}

	ClonerComponent->SetAsset(nullptr);

	OnLayoutInactive();

	return true;
}

bool UAvaClonerLayoutBase::CopyTo(UAvaClonerLayoutBase* InOtherLayout) const
{
	if (!InOtherLayout || InOtherLayout == this)
	{
		return false;
	}

	if (!IsLayoutLoaded())
	{
		return false;
	}

	const UNiagaraSystem* OtherSystem = InOtherLayout->NiagaraSystem;
	if (!OtherSystem)
	{
		return false;
	}
	
	DataInterfaces.CopyTo(InOtherLayout->DataInterfaces);

	return true;
}

void UAvaClonerLayoutBase::OnLayoutPropertyChanged()
{
	UpdateLayoutParameters();
}

UNiagaraSystem* UAvaClonerLayoutBase::LoadSystemPath(const FString& InPath) const
{
	UNiagaraSystem* LoadedNiagaraSystem = FindObject<UNiagaraSystem>(nullptr, *InPath);
	
	if (!LoadedNiagaraSystem)
	{
		LoadedNiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *InPath);
	}

	return LoadedNiagaraSystem;
}

AAvaClonerActor* UAvaClonerLayoutBase::GetClonerActor() const
{
	return GetTypedOuter<AAvaClonerActor>();
}

UAvaClonerComponent* UAvaClonerLayoutBase::GetClonerComponent() const
{
	if (const AAvaClonerActor* Cloner = GetClonerActor())
	{
		return Cloner->GetClonerComponent();
	}

	return nullptr;
}

void UAvaClonerLayoutBase::UpdateLayoutParameters(bool bInUpdateCloner, bool bInImmediate)
{
	if (!IsLayoutActive())
	{
		return;	
	}
	
	if (UAvaClonerComponent* ClonerComponent = GetClonerComponent())
	{
		OnLayoutParametersChanged(ClonerComponent);

		if (bInUpdateCloner)
		{
			RequestClonerUpdate(bInImmediate);
		}
	}
}

void UAvaClonerLayoutBase::RequestClonerUpdate(bool bInImmediate) const
{
	if (AAvaClonerActor* Cloner = GetClonerActor())
	{
		Cloner->RequestClonerUpdate(bInImmediate);
	}
}
