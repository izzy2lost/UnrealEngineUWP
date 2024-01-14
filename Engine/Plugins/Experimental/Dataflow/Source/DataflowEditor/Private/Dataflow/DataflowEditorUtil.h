// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"
#include "Templates/SharedPointer.h"

class UDataflow;
class UObject;
class UDataflowEditorContent;
class USkeletalMesh;
class USkeleton;
class UAnimationAsset;
class UMaterial;

namespace Private
{
	UDataflow* GetDataflowAssetFrom(UObject* InObject);

	USkeletalMesh* GetSkeletalMeshFrom(UObject* InObject);

	USkeleton* GetSkeletonFrom(UObject* InObject);

	UAnimationAsset* GetAnimationAssetFrom(UObject* InObject);

	FString GetDataflowTerminalFrom(UObject* InObject);
};

namespace UE
{
	namespace Material
	{
		UMaterial* LoadMaterialFromPath( const FName& Path, UObject* Outer);
	}
}

namespace Dataflow
{
	TSharedPtr<::Dataflow::FEngineContext> GetContext(UDataflowEditorContent* Content);
}