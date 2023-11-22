// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDataprepPipeline.h"

//#include "Nodes/InterchangeFactoryBaseNode.h"
#include "InterchangeActorFactoryNode.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "LevelSequence.h"

void UInterchangeDataprepLevelPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* NodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	if (!NodeContainer)
	{
		return;
	}

	Super::ExecutePipeline(NodeContainer, InSourceDatas, ContentBasePath);

	static TMap<UClass*, FString> SubPathsPerClass{
		{ UTexture::StaticClass(), "Textures" },
		{ UMaterialInterface::StaticClass(), "Materials" },
		{ UStaticMesh::StaticClass(), "Geometries" },
		{ ULevelSequence::StaticClass(), TEXT("Animations") },
	};

	// Compute unique prefix based on file path
	FString UniquePrefix = FString::FromInt(GetTypeHash(InSourceDatas[0]->GetFilename()));

	auto UpdateFactoryNodes = [this,&UniquePrefix](const FString& NodeUid, UInterchangeFactoryBaseNode* FactoryNode)
	{
		for (TPair<UClass*, FString>& Entry : SubPathsPerClass)
		{
			if (FactoryNode->GetObjectClass() && FactoryNode->GetObjectClass()->IsChildOf(Entry.Key))
			{
				FactoryNode->SetCustomSubPath(Entry.Value);
				break;
			}
		}

		// Append prefix to prevent actor's name collision in Dataprep world
		if (UInterchangeActorFactoryNode* ActorFactoryNode = Cast< UInterchangeActorFactoryNode>(FactoryNode))
		{
			FString NewLabel = UniquePrefix + TEXT("_") + ActorFactoryNode->GetDisplayLabel();
			ActorFactoryNode->SetDisplayLabel(NewLabel);
		}

		//FactoryNode->SetEnabled(true);
	};

	//Find all factory node we need for this pipeline
	NodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>(UpdateFactoryNodes);
}

void UInterchangeDataprepLevelPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* NodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	if (!NodeContainer || !CreatedAsset)
	{
		return;
	}

	Super::ExecutePostImportPipeline(NodeContainer, FactoryNodeKey, CreatedAsset, bIsAReimport);

	CreatedAsset->ClearFlags(RF_Public/* | RF_Standalone*/);
	CreatedAsset->SetFlags(RF_Transient);
}
