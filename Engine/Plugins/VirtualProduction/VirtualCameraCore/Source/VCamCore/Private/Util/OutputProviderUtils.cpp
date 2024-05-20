// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/OutputProviderUtils.h"

#include "EngineUtils.h"
#include "VCamComponent.h"
#include "Output/VCamOutputProviderBase.h"

namespace UE::VCamCore
{
	UVCamOutputProviderBase* GetOtherOutputProviderByIndex(const UVCamOutputProviderBase& OutputProvider, int32 Index)
	{
		const UVCamComponent* OuterComponent = OutputProvider.GetTypedOuter<UVCamComponent>();
		return OuterComponent ? OuterComponent->GetOutputProviderByIndex(Index) : nullptr;
	}
	
	int32 FindOutputProviderIndex(const UVCamOutputProviderBase& OutputProvider)
	{
		const UVCamComponent* OuterComponent = OutputProvider.GetTypedOuter<UVCamComponent>();
		if (!OuterComponent)
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < OuterComponent->GetNumberOfOutputProviders(); ++Index)
		{
			if (OuterComponent->GetOutputProviderByIndex(Index) == &OutputProvider)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FString GenerateUniqueOutputProviderName(const UVCamOutputProviderBase& OutputProvider)
	{
		AActor* OwningActor = OutputProvider.GetTypedOuter<AActor>();
		UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
		if (!ensure(OwningActor && World))
		{
			return {};
		}

		const auto GenerateNameBasedOnFName = [OwningActor, &OutputProvider]()
		{
			return FString::Printf(TEXT("%s_%d"), *OwningActor->GetName(), OutputProvider.FindOwnIndexInOwner());
		};
		
#if WITH_EDITOR // There are no actor labels in non-editor builds
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			if (*ActorIt && *ActorIt != OwningActor
				&& ActorIt->FindComponentByClass<UVCamComponent>()
				&& ActorIt->GetActorLabel() == OwningActor->GetActorLabel())
			{
				return GenerateNameBasedOnFName();
			}
		}
		
		return FString::Printf(TEXT("%s_%d"), *OwningActor->GetActorLabel(), OutputProvider.FindOwnIndexInOwner());
#else
		return GenerateNameBasedOnFName();
#endif
	}
}
