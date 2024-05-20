// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Extensions/CEClonerEffectorExtension.h"

#include "Cloner/CEClonerComponent.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Effector/CEEffectorActor.h"
#include "Effector/CEEffectorComponent.h"
#include "Subsystems/CEEffectorSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCEClonerEffectorExtension, Log, All);

bool UCEClonerEffectorExtension::LinkEffector(AActor* InEffectorActor)
{
	if (!IsValid(InEffectorActor)
		|| EffectorActorsWeak.Contains(InEffectorActor)
		|| !InEffectorActor->FindComponentByClass<UCEEffectorComponent>())
	{
		return false;
	}

	EffectorActorsWeak.Add(InEffectorActor);

	OnEffectorsChanged();

	UE_LOG(LogCEClonerEffectorExtension, Log, TEXT("%s : Effector %s linked to Cloner"), *GetClonerComponent()->GetOwner()->GetActorNameOrLabel(), *InEffectorActor->GetActorNameOrLabel());

	return true;
}

bool UCEClonerEffectorExtension::UnlinkEffector(AActor* InEffectorActor)
{
	if (!InEffectorActor)
	{
		return false;
	}

	if (EffectorActorsWeak.Remove(InEffectorActor) > 0)
	{
		OnEffectorsChanged();

		UE_LOG(LogCEClonerEffectorExtension, Log, TEXT("%s : Effector %s unlinked from Cloner"), *GetClonerComponent()->GetOwner()->GetActorNameOrLabel(), *InEffectorActor->GetActorNameOrLabel());
	}

	return true;
}

bool UCEClonerEffectorExtension::IsEffectorLinked(const AActor* InEffectorActor) const
{
	return InEffectorActor && EffectorActorsWeak.Contains(InEffectorActor);
}

int32 UCEClonerEffectorExtension::GetEffectorCount() const
{
	return EffectorActorsWeak.Num();
}

const FCEClonerEffectorDataInterfaces* UCEClonerEffectorExtension::GetEffectorDataInterfaces() const
{
	if (const UCEClonerLayoutBase* LayoutSystem = GetClonerLayout())
	{
		return &LayoutSystem->GetDataInterfaces();
	}

	return nullptr;
}

void UCEClonerEffectorExtension::OnExtensionActivated()
{
	Super::OnExtensionActivated();

	UCEEffectorSubsystem::OnEffectorIdentifierChanged().RemoveAll(this);
	UCEEffectorSubsystem::OnEffectorIdentifierChanged().AddUObject(this, &UCEClonerEffectorExtension::OnEffectorIdentifierChanged);

	OnEffectorsChanged();
}

void UCEClonerEffectorExtension::OnExtensionDeactivated()
{
	Super::OnExtensionDeactivated();

	UCEEffectorSubsystem::OnEffectorIdentifierChanged().RemoveAll(this);
}

void UCEClonerEffectorExtension::OnEffectorIdentifierChanged(UCEEffectorComponent* InEffector, int32 InOldIdentifier, int32 InNewIdentifier)
{
	if (EffectorActorsWeak.Contains(InEffector->GetOwner()))
	{
		OnEffectorsChanged();
	}
}

void UCEClonerEffectorExtension::OnEffectorsChanged()
{
	const FCEClonerEffectorDataInterfaces* EffectorDataInterfaces = GetEffectorDataInterfaces();

	if (!EffectorDataInterfaces || !EffectorDataInterfaces->GetIndexArray())
	{
		return;
	}

	// Remove duplicates
	TSet<TWeakObjectPtr<UCEEffectorComponent>> SetEffectorsWeak;

	for (TArray<TWeakObjectPtr<AActor>>::TIterator It(EffectorActorsWeak); It; ++It)
	{
		if (AActor* EffectorActor = It->Get())
		{
			TArray<UCEEffectorComponent*> EffectorComponents;
			EffectorActor->GetComponents(EffectorComponents, /** Recurse */false);

			for (UCEEffectorComponent* EffectorComponent : EffectorComponents)
			{
				if (EffectorComponent->GetChannelIdentifier() != INDEX_NONE)
				{
					SetEffectorsWeak.Add(EffectorComponent);
				}
			}

			if (SetEffectorsWeak.IsEmpty())
			{
#if WITH_EDITOR
				if (GUndo)
				{
					Modify();
				}
#endif

				It.RemoveCurrent();
			}
		}
	}

	// Removed effectors
	for (const TWeakObjectPtr<UCEEffectorComponent>& EffectorWeak : EffectorsInternalWeak.Difference(SetEffectorsWeak))
	{
		if (UCEEffectorComponent* Effector = EffectorWeak.Get())
		{
			Effector->OnClonerUnlinked(this);
		}
	}

	// Added effectors
	for (const TWeakObjectPtr<UCEEffectorComponent>& EffectorWeak : SetEffectorsWeak.Difference(EffectorsInternalWeak))
	{
		if (UCEEffectorComponent* Effector = EffectorWeak.Get())
		{
			Effector->OnClonerLinked(this);
		}
	}

	EffectorsInternalWeak = SetEffectorsWeak;

	TArray<int32> EffectorIndexes;
	EffectorIndexes.Reserve(SetEffectorsWeak.Num());

	for (const TWeakObjectPtr<UCEEffectorComponent>& EffectorWeak : SetEffectorsWeak)
	{
		if (UCEEffectorComponent* Effector = EffectorWeak.Get())
		{
			const int32 ChannelIdentifier = Effector->GetChannelIdentifier();

			if (ChannelIdentifier != INDEX_NONE)
			{
				EffectorIndexes.AddUnique(ChannelIdentifier);
			}
		}
	}

	TArray<int32>& EffectorIndexArray = EffectorDataInterfaces->GetIndexArray()->GetArrayReference();
	EffectorIndexArray.Empty(EffectorIndexes.Num());

	for (const int32 EffectorIndex : EffectorIndexes)
	{
		EffectorIndexArray.Add(EffectorIndex);
	}

	UpdateExtensionParameters(/** UpdateCloner */true);
}

void UCEClonerEffectorExtension::OnEffectorActorsChanged()
{
	for (TArray<TWeakObjectPtr<AActor>>::TIterator It(EffectorActorsWeak); It; ++It)
	{
		if (AActor* EffectorActor = It->Get())
		{
			if (!EffectorActor->FindComponentByClass<UCEEffectorComponent>())
			{
				It.RemoveCurrent();
			}
		}
	}

	OnEffectorsChanged();
}

#if WITH_EDITOR
void UCEClonerEffectorExtension::CreateLinkedEffector()
{
	UCEClonerComponent* ClonerComponent = GetClonerComponent();
	if (!IsValid(ClonerComponent))
	{
		return;
	}

	UWorld* ClonerWorld = ClonerComponent->GetWorld();
	if (!IsValid(ClonerWorld))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.bTemporaryEditorActor = false;

	const FVector ClonerLocation = ClonerComponent->GetComponentLocation();
	const FRotator ClonerRotation = ClonerComponent->GetComponentRotation();

	ACEEffectorActor* EffectorActor = ClonerWorld->SpawnActor<ACEEffectorActor>(ACEEffectorActor::StaticClass(), ClonerLocation, ClonerRotation, Params);
	if (!EffectorActor)
	{
		return;
	}

	FActorLabelUtilities::RenameExistingActor(EffectorActor, EffectorActor->GetDefaultActorLabel(), true);

	LinkEffector(EffectorActor);

	if (GEditor)
	{
		GEditor->SelectNone(true, true);
		GEditor->SelectActor(EffectorActor, true, true);
	}
}

const TCEPropertyChangeDispatcher<UCEClonerEffectorExtension> UCEClonerEffectorExtension::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(UCEClonerEffectorExtension, EffectorActorsWeak), &UCEClonerEffectorExtension::OnEffectorActorsChanged }
};

void UCEClonerEffectorExtension::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}

void UCEClonerEffectorExtension::PostEditUndo()
{
	Super::PostEditUndo();

	OnEffectorsChanged();
}
#endif
