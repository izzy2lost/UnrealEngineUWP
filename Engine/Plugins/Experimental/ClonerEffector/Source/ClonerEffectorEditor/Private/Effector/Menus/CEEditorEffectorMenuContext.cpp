// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/Menus/CEEditorEffectorMenuContext.h"

#include "Effector/CEEffectorComponent.h"
#include "GameFramework/Actor.h"

FCEEditorEffectorMenuContext::FCEEditorEffectorMenuContext(const TSet<UObject*>& InObjects)
{
	for (UObject* Object : InObjects)
	{
		if (!IsValid(Object))
		{
			continue;
		}

		if (const AActor* Actor = Cast<AActor>(Object))
		{
			TArray<UCEEffectorComponent*> EffectorComponents;
			Actor->GetComponents(EffectorComponents, /** IncludeChildren */false);

			for (UCEEffectorComponent* Component : EffectorComponents)
			{
				if (IsValid(Component))
				{
					ContextComponents.Add(Component);
				}
			}
		}
		else if (UCEEffectorComponent* Component = Cast<UCEEffectorComponent>(Object))
		{
			ContextComponents.Add(Component);
		}
	}
}

const TSet<UCEEffectorComponent*>& FCEEditorEffectorMenuContext::GetComponents() const
{
	return ContextComponents;
}

TSet<UCEEffectorComponent*> FCEEditorEffectorMenuContext::GetDisabledEffectors() const
{
	return GetStateEffectors(/** IsEnabled */ false);
}

TSet<UCEEffectorComponent*> FCEEditorEffectorMenuContext::GetEnabledEffectors() const
{
	return GetStateEffectors(/** IsEnabled */ true);
}

UWorld* FCEEditorEffectorMenuContext::GetWorld() const
{
	for (const UCEEffectorComponent* Component : ContextComponents)
	{
		if (IsValid(Component))
		{
			return Component->GetWorld();
		}
	}

	return nullptr;
}

bool FCEEditorEffectorMenuContext::IsEmpty() const
{
	return ContextComponents.IsEmpty();
}

bool FCEEditorEffectorMenuContext::ContainsAnyComponent() const
{
	return !ContextComponents.IsEmpty();
}

bool FCEEditorEffectorMenuContext::ContainsAnyDisabledEffectors() const
{
	return ContainsEffectorState(/** IsEnabled */ false);
}

bool FCEEditorEffectorMenuContext::ContainsAnyEnabledEffectors() const
{
	return ContainsEffectorState(/** IsEnabled */ true);
}

bool FCEEditorEffectorMenuContext::ContainsEffectorState(bool bInState) const
{
	for (const UCEEffectorComponent* Component : ContextComponents)
	{
		if (IsValid(Component) && Component->GetEnabled() == bInState)
		{
			return true;
		}
	}

	return false;
}

TSet<UCEEffectorComponent*> FCEEditorEffectorMenuContext::GetStateEffectors(bool bInState) const
{
	TSet<UCEEffectorComponent*> Effectors;
	Effectors.Reserve(ContextComponents.Num());

	for (UCEEffectorComponent* Component : ContextComponents)
	{
		if (IsValid(Component) && Component->GetEnabled() == bInState)
		{
			Effectors.Add(Component);
		}
	}

	return Effectors;
}