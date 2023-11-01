// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterSourceRef.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

const IAnimNextParameterSourceInterface* FAnimNextParameterSourceRef::Get(const UObject* InContextObject) const
{
	switch (Type)
	{
	case EAnimNextParameterSourceRefType::Self:
		return Cast<IAnimNextParameterSourceInterface>(InContextObject);
	case EAnimNextParameterSourceRefType::Asset: 
		return Asset.GetInterface();
	case EAnimNextParameterSourceRefType::Component:
	case EAnimNextParameterSourceRefType::Actor:
		if (const UActorComponent* ActorComponent = Cast<UActorComponent>(InContextObject))
		{
			if (AActor* Actor = ActorComponent->GetOwner())
			{
				if (Type == EAnimNextParameterSourceRefType::Actor)
				{
					return Cast<IAnimNextParameterSourceInterface>(Actor);
				}
				else
				{
					return Cast<IAnimNextParameterSourceInterface>(Component.GetComponent(Actor));
				}
			}
		}
		break;
	case EAnimNextParameterSourceRefType::Inline:
		return this;
	}

	return nullptr;
}

UE::AnimNext::FParamStackLayerHandle FAnimNextParameterSourceRef::CacheLayer() const
{
	return UE::AnimNext::FParamStack::MakeValueLayer(InlineParameters);
}