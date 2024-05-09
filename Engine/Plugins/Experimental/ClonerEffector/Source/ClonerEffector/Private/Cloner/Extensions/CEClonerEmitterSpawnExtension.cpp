// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Extensions/CEClonerEmitterSpawnExtension.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "NiagaraSystem.h"
#include "NiagaraUserRedirectionParameterStore.h"

void UCEClonerEmitterSpawnExtension::SetSpawnLoopMode(ECEClonerSpawnLoopMode InMode)
{
	if (SpawnLoopMode == InMode)
	{
		return;
	}

	SpawnLoopMode = InMode;
	UpdateExtensionParameters();
}

void UCEClonerEmitterSpawnExtension::SetSpawnLoopIterations(int32 InIterations)
{
	if (SpawnLoopIterations == InIterations)
	{
		return;
	}

	if (InIterations < 1)
	{
		return;
	}

	SpawnLoopIterations = InIterations;
	UpdateExtensionParameters();
}

void UCEClonerEmitterSpawnExtension::SetSpawnLoopInterval(float InInterval)
{
	if (SpawnLoopInterval == InInterval)
	{
		return;
	}

	if (InInterval < 0.f)
	{
		return;
	}

	SpawnLoopInterval = InInterval;
	UpdateExtensionParameters();
}

void UCEClonerEmitterSpawnExtension::SetSpawnBehaviorMode(ECEClonerSpawnBehaviorMode InMode)
{
	if (SpawnBehaviorMode == InMode)
	{
		return;
	}

	SpawnBehaviorMode = InMode;
	UpdateExtensionParameters();
}

void UCEClonerEmitterSpawnExtension::SetSpawnRate(float InRate)
{
	if (SpawnRate == InRate)
	{
		return;
	}

	if (InRate < 0)
	{
		return;
	}

	SpawnRate = InRate;
	UpdateExtensionParameters();
}

void UCEClonerEmitterSpawnExtension::OnExtensionParametersChanged(UCEClonerComponent* InComponent)
{
	Super::OnExtensionParametersChanged(InComponent);

	if (const UCEClonerLayoutBase* ActiveSystem = GetClonerLayout())
	{
		FNiagaraUserRedirectionParameterStore& ExposedParameters = ActiveSystem->GetSystem()->GetExposedParameters();

		InComponent->SetFloatParameter(TEXT("SpawnLoopInterval"), SpawnLoopInterval);

		InComponent->SetIntParameter(TEXT("SpawnLoopIterations"), SpawnLoopIterations);

		InComponent->SetFloatParameter(TEXT("SpawnRate"), SpawnRate);

		const FNiagaraVariable SpawnBehaviorModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnBehaviorMode>()), TEXT("SpawnBehaviorMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnBehaviorMode), SpawnBehaviorModeVar);

		const FNiagaraVariable SpawnLoopModeVar(FNiagaraTypeDefinition(StaticEnum<ECEClonerSpawnLoopMode>()), TEXT("SpawnLoopMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnLoopMode), SpawnLoopModeVar);
	}
}

#if WITH_EDITOR
const TCEPropertyChangeDispatcher<UCEClonerEmitterSpawnExtension> UCEClonerEmitterSpawnExtension::PropertyChangeDispatcher =
{
	/** Spawn */
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEmitterSpawnExtension, SpawnLoopMode), &UCEClonerEmitterSpawnExtension::OnExtensionPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEmitterSpawnExtension, SpawnLoopInterval), &UCEClonerEmitterSpawnExtension::OnExtensionPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEmitterSpawnExtension, SpawnLoopIterations), &UCEClonerEmitterSpawnExtension::OnExtensionPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEmitterSpawnExtension, SpawnBehaviorMode), &UCEClonerEmitterSpawnExtension::OnExtensionPropertyChanged },
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEmitterSpawnExtension, SpawnRate), &UCEClonerEmitterSpawnExtension::OnExtensionPropertyChanged },
};

void UCEClonerEmitterSpawnExtension::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif
