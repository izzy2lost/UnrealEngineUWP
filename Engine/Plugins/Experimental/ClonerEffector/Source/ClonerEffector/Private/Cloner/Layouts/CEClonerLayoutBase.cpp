// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Layouts/CEClonerLayoutBase.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/CEClonerComponent.h"
#include "Cloner/Extensions/CEClonerExtensionBase.h"
#include "Misc/PackageName.h"
#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "Subsystems/CEClonerSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCEClonerLayoutBase, Log, All);

bool UCEClonerLayoutBase::IsLayoutValid() const
{
	if (LayoutName.IsNone() || LayoutAssetPath.IsEmpty())
	{
		return false;
	}

	// Get the template niagara asset
	const UNiagaraSystem* TemplateNiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *LayoutAssetPath);

	// Get the base niagara asset
	const UNiagaraSystem* BaseNiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, LayoutBaseAssetPath);

	if (!TemplateNiagaraSystem || !BaseNiagaraSystem)
	{
		UE_LOG(LogCEClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) or base system (%s) is invalid"), *LayoutName.ToString(), *LayoutAssetPath, LayoutBaseAssetPath);
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
				UE_LOG(LogCEClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) missing parameter (%s) from base system (%s)"), *LayoutName.ToString(), *LayoutAssetPath, *SystemParameter.ToString(), LayoutBaseAssetPath);
				break;
			}
		}
	}

	if (!bIsSystemBasedOnBaseAsset)
	{
		UE_LOG(LogCEClonerLayoutBase, Warning, TEXT("Cloner layout %s : Template system (%s) is not based off base system (%s)"), *LayoutName.ToString(), *LayoutAssetPath, LayoutBaseAssetPath);
	}

	return bIsSystemBasedOnBaseAsset;
}

bool UCEClonerLayoutBase::IsLayoutLoaded() const
{
	return !IsTemplate() && NiagaraSystem && MeshRenderer;
}

void UCEClonerLayoutBase::LoadLayout()
{
	if (IsLayoutLoaded())
	{
		return;
	}

	// Already being loaded
	if (LoadRequestIdentifier != INDEX_NONE)
	{
		return;
	}

	if (LayoutAssetPath.IsEmpty())
	{
		return;
	}

	const UCEClonerComponent* ClonerComponent = GetClonerComponent();

	if (!IsValid(ClonerComponent))
	{
		return;
	}

	// Extract package path
	FString PackagePath = LayoutAssetPath;

	int32 FirstQuoteIndex;
	PackagePath.FindChar('\'', FirstQuoteIndex);

	int32 LastQuoteIndex;
	PackagePath.FindLastChar('\'', LastQuoteIndex);

	if (FirstQuoteIndex != LastQuoteIndex)
	{
		PackagePath = PackagePath.Mid(FirstQuoteIndex + 1, LastQuoteIndex - FirstQuoteIndex - 1);
	}

	PackagePath = FPackageName::ObjectPathToPackageName(PackagePath);

	FLoadPackageAsyncOptionalParams Params;
	Params.CustomPackageName = FName(TEXT("/") + FString::FromInt(ClonerComponent->GetUniqueID()) + TEXT("_") + GetLayoutName().ToString());
	Params.CompletionDelegate = MakeUnique<FLoadPackageAsyncDelegate>(FLoadPackageAsyncDelegate::CreateUObject(this, &UCEClonerLayoutBase::OnSystemPackageLoaded));

	UE_LOG(LogCEClonerLayoutBase, Verbose, TEXT("%s : Cloner layout load requested %s - Template system %s - Package %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString(), *LayoutAssetPath, *Params.CustomPackageName.ToString())

	LoadRequestIdentifier = LoadPackageAsync(PackagePath, MoveTemp(Params));
}

bool UCEClonerLayoutBase::UnloadLayout()
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

	MeshRenderer = nullptr;
	NiagaraSystem = nullptr;

	UE_LOG(LogCEClonerLayoutBase, Verbose, TEXT("%s : Cloner layout unloaded %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString())

	OnLayoutUnloaded();

	return true;
}

bool UCEClonerLayoutBase::IsLayoutActive() const
{
	const UCEClonerComponent* Component = GetClonerComponent();

	if (!Component)
	{
		return false;
	}

	return IsLayoutLoaded() && Component->GetAsset() == NiagaraSystem;
}

bool UCEClonerLayoutBase::ActivateLayout()
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

	UCEClonerComponent* ClonerComponent = GetClonerComponent();

	if (!ClonerComponent)
	{
		return false;
	}

	ClonerComponent->SetAsset(NiagaraSystem);

	UE_LOG(LogCEClonerLayoutBase, Verbose, TEXT("%s : Cloner layout activated %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString())

	OnLayoutActive();

	return true;
}

bool UCEClonerLayoutBase::DeactivateLayout()
{
	if (!IsLayoutActive())
	{
		return false;
	}

	UCEClonerComponent* ClonerComponent = GetClonerComponent();

	if (!ClonerComponent)
	{
		return false;
	}

	ClonerComponent->SetAsset(nullptr);

	UE_LOG(LogCEClonerLayoutBase, Verbose, TEXT("%s : Cloner layout deactivated %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString())

	OnLayoutInactive();

	return true;
}

TSet<TSubclassOf<UCEClonerExtensionBase>> UCEClonerLayoutBase::GetSupportedExtensions() const
{
	TSet<TSubclassOf<UCEClonerExtensionBase>> ExtensionSupported;

	if (const UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get())
	{
		for (const TSubclassOf<UCEClonerExtensionBase>& ExtensionClass : ClonerSubsystem->GetExtensionClasses())
		{
			const UCEClonerExtensionBase* Extension = ExtensionClass.GetDefaultObject();

			if (!Extension)
			{
				continue;
			}

			// Does the layout supports this extension
			if (!IsExtensionSupported(Extension))
			{
				continue;
			}

			// Does the extension supports this layout
			if (!Extension->IsLayoutSupported(this))
			{
				continue;
			}

			ExtensionSupported.Add(ExtensionClass);
		}
	}

	return ExtensionSupported;
}

bool UCEClonerLayoutBase::IsLayoutDirty() const
{
	return EnumHasAnyFlags(LayoutStatus, ECEClonerSystemStatus::ParametersDirty);
}

void UCEClonerLayoutBase::PostEditImport()
{
	Super::PostEditImport();

	MarkLayoutDirty();
}

#if WITH_EDITOR
void UCEClonerLayoutBase::PostEditUndo()
{
	Super::PostEditUndo();

	MarkLayoutDirty();
}
#endif

void UCEClonerLayoutBase::OnLayoutPropertyChanged()
{
	MarkLayoutDirty();
}

void UCEClonerLayoutBase::OnSystemPackageLoaded(const FName& InName, UPackage* InPackage, EAsyncLoadingResult::Type InResult)
{
	NiagaraSystem = InPackage ? Cast<UNiagaraSystem>(InPackage->FindAssetInPackage()) : nullptr;
	LoadRequestIdentifier = INDEX_NONE;

	if (NiagaraSystem)
	{
		// Change outer to avoid GC leak
		InPackage->SetFlags(RF_Transient);
		NiagaraSystem->RemoveFromRoot();
		NiagaraSystem->Rename(NULL, this, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		InPackage->MarkAsGarbage();

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

						UE_LOG(LogCEClonerLayoutBase, Verbose, TEXT("%s : Cloner layout loaded %s - Template system %s - Package %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString(), *LayoutAssetPath, *InName.ToString())

						OnLayoutLoaded();
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogCEClonerLayoutBase, Warning, TEXT("%s : Cloner layout load failed %s - Template system %s - Package %s"), *GetClonerActor()->GetActorNameOrLabel(), *LayoutName.ToString(), *LayoutAssetPath, *InName.ToString())
	}

	OnClonerLayoutLoadedDelegate.Broadcast(this, IsLayoutLoaded());
	OnClonerLayoutLoadedDelegate.Clear();
}

UCEClonerComponent* UCEClonerLayoutBase::GetClonerComponent() const
{
	return GetTypedOuter<UCEClonerComponent>();
}

AActor* UCEClonerLayoutBase::GetClonerActor() const
{
	if (const UCEClonerComponent* ClonerComponent = GetClonerComponent())
	{
		return ClonerComponent->GetOwner();
	}

	return nullptr;
}

void UCEClonerLayoutBase::UpdateLayoutParameters()
{
	if (!IsLayoutActive())
	{
		return;
	}

	if (UCEClonerComponent* ClonerComponent = GetClonerComponent())
	{
		if (!ClonerComponent->GetEnabled())
		{
			return;
		}

		OnLayoutParametersChanged(ClonerComponent);

		if (EnumHasAnyFlags(LayoutStatus, ECEClonerSystemStatus::SimulationDirty))
		{
			ClonerComponent->RequestClonerUpdate(/*Immediate*/false);
		}
	}

	LayoutStatus = ECEClonerSystemStatus::UpToDate;
}

void UCEClonerLayoutBase::MarkLayoutDirty(bool bInUpdateCloner)
{
	EnumAddFlags(LayoutStatus, ECEClonerSystemStatus::ParametersDirty);

	if (bInUpdateCloner)
	{
		EnumAddFlags(LayoutStatus, ECEClonerSystemStatus::SimulationDirty);
	}
}
