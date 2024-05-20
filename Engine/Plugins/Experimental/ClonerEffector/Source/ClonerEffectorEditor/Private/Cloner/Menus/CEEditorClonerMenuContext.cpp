// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/Menus/CEEditorClonerMenuContext.h"

#include "GameFramework/Actor.h"
#include "Cloner/CEClonerComponent.h"

FCEEditorClonerMenuContext::FCEEditorClonerMenuContext(const TSet<UObject*>& InObjects)
{
	for (UObject* Object : InObjects)
	{
		if (!IsValid(Object))
		{
			continue;
		}

		if (const AActor* Actor = Cast<AActor>(Object))
		{
			TArray<UCEClonerComponent*> ClonerComponents;
			Actor->GetComponents(ClonerComponents, /** IncludeChildren */false);

			for (UCEClonerComponent* Component : ClonerComponents)
			{
				if (IsValid(Component))
				{
					ContextComponents.Add(Component);
				}
			}
		}
		else if (UCEClonerComponent* Component = Cast<UCEClonerComponent>(Object))
		{
			ContextComponents.Add(Component);
		}
	}
}

const TSet<UCEClonerComponent*>& FCEEditorClonerMenuContext::GetComponents() const
{
	return ContextComponents;
}

TSet<UCEClonerComponent*> FCEEditorClonerMenuContext::GetDisabledCloners() const
{
	return GetStateCloners(/** IsEnabled */false);
}

TSet<UCEClonerComponent*> FCEEditorClonerMenuContext::GetEnabledCloners() const
{
	return GetStateCloners(/** IsEnabled */true);
}

UWorld* FCEEditorClonerMenuContext::GetWorld() const
{
	for (const UCEClonerComponent* Component : ContextComponents)
	{
		if (IsValid(Component))
		{
			return Component->GetWorld();
		}
	}

	return nullptr;
}

bool FCEEditorClonerMenuContext::IsEmpty() const
{
	return ContextComponents.IsEmpty();
}

bool FCEEditorClonerMenuContext::ContainsAnyComponent() const
{
	return !ContextComponents.IsEmpty();
}

bool FCEEditorClonerMenuContext::ContainsAnyDisabledCloner() const
{
	return ContainsClonerState(/** IsEnabled */false);
}

bool FCEEditorClonerMenuContext::ContainsAnyEnabledCloner() const
{
	return ContainsClonerState(/** IsEnabled */true);
}

bool FCEEditorClonerMenuContext::ContainsClonerState(bool bInState) const
{
	for (UCEClonerComponent* Component : ContextComponents)
	{
		if (IsValid(Component) && Component->GetEnabled() == bInState)
		{
			return true;
		}
	}

	return false;
}

TSet<UCEClonerComponent*> FCEEditorClonerMenuContext::GetStateCloners(bool bInState) const
{
	TSet<UCEClonerComponent*> Cloners;
	Cloners.Reserve(ContextComponents.Num());

	for (UCEClonerComponent* Component : ContextComponents)
	{
		if (IsValid(Component) && Component->GetEnabled() == bInState)
		{
			Cloners.Add(Component);
		}
	}

	return Cloners;
}