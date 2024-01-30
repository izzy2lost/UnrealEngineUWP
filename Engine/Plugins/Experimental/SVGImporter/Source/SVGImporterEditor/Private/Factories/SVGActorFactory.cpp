// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGActorFactory.h"

#include "LevelEditorViewport.h"
#include "SVGActor.h"
#include "SVGData.h"

USVGActorFactory::USVGActorFactory(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	// AActor subclass this ActorFactory creates.
	NewActorClassName = FString("ASVGActor");
	NewActorClass = ASVGActor::StaticClass();
}

bool USVGActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	return (AssetData.IsValid() && AssetData.IsInstanceOf(USVGData::StaticClass()));
}

AActor* USVGActorFactory::GetDefaultActor(const FAssetData& AssetData)
{
	return NewActorClass->GetDefaultObject<ASVGActor>();
}

AActor* USVGActorFactory::SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams)
{
	if (!InAsset)
	{
		return nullptr;
	}

	USVGData* SVGData = Cast<USVGData>(InAsset);
	
	if (!SVGData)
	{
		return nullptr;
	}
	
	const AActor* const DefaultActor = GetDefaultActor(FAssetData(InAsset));
	
	if (DefaultActor && InLevel)
	{
		const ULevel* const LocalLevel = ValidateSpawnActorLevel(InLevel, InSpawnParams);
		
		ASVGActor* const SVGActor = LocalLevel->OwningWorld->SpawnActorDeferred<ASVGActor>(DefaultActor->GetClass(), InTransform);
		
		SVGActor->SVGData = SVGData;

		if (FLevelEditorViewportClient::IsDroppingPreviewActor() || InSpawnParams.ObjectFlags & RF_Transient)
		{
			SVGActor->RenderMode = ESVGRenderMode::Texture2D;
		}
		else
		{
			SVGActor->RenderMode = ESVGRenderMode::DynamicMesh3D;
		}
		
		SVGActor->FinishSpawning(InTransform);
		
		return SVGActor;
	}

	return nullptr;
}
