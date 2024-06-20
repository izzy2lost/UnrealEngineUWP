// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementEditorAssetDataInterface.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"

TArray<FAssetData> UActorElementEditorAssetDataInterface::GetAllReferencedAssetDatas(const FTypedElementHandle& InElementHandle)
{
	TArray<FAssetData> AssetDatas;

	if (AActor* RawActorPtr = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		TArray<UObject*> ReferencedContentObjects;
		RawActorPtr->GetReferencedContentObjects(ReferencedContentObjects);
		for (const UObject* ContentObject : ReferencedContentObjects)
		{
			FAssetData ObjectAssetData = FAssetData(ContentObject);
			if (ObjectAssetData.IsValid())
			{
				AssetDatas.Emplace(ObjectAssetData);
			}
		}

		TArray<FSoftObjectPath> SoftObjects;
		RawActorPtr->GetSoftReferencedContentObjects(SoftObjects);
		if (SoftObjects.Num())
		{
			IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

			for (const FSoftObjectPath& SoftObject : SoftObjects)
			{
				FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftObject);

				if (AssetData.IsValid())
				{
					AssetDatas.Add(AssetData);
				}
			}
		}
	}

	return AssetDatas;
}
