// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterSourceRef.h"
#include "Component/AnimNextComponent.h"
#include "GameFramework/Actor.h"

const IAnimNextParameterSourceInterface* FAnimNextParameterSourceRef::Get(const UObject* InContextObject) const
{
	switch (Type)
	{
	case EAnimNextParameterSourceRefType::Self:
		return Cast<UAnimNextComponent>(InContextObject);
	case EAnimNextParameterSourceRefType::Asset:
		return Asset.GetInterface();
	case EAnimNextParameterSourceRefType::Component:
	case EAnimNextParameterSourceRefType::Actor:
		if (const UAnimNextComponent* AnimNextComponent = Cast<UAnimNextComponent>(InContextObject))
		{
			if (AActor* Actor = AnimNextComponent->GetOwner())
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