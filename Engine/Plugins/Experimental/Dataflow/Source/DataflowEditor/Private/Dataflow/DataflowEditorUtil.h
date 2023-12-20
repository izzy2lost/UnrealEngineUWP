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
class UAnimationAsset;
class UMaterial;

namespace Private
{
	UDataflow* GetDataflowAssetFrom(UObject* InObject);

	USkeletalMesh* GetSkeletalMeshFrom(UObject* InObject);

	UAnimationAsset* GetAnimationAssetFrom(UObject* InObject);

	FString GetDataflowTerminalFrom(UObject* InObject);
};

namespace UE
{
	namespace Material
	{
		UMaterial* LoadMaterialFromPath( const FName& Path, UObject* Outer);
	}

	namespace Conversion
	{
		// Convert a rendering facade to a dynamic mesh
		void RenderingFacadeToDynamicMesh(const GeometryCollection::Facades::FRenderingFacade& Facade, UE::Geometry::FDynamicMesh3& DynamicMesh);

		// Convert a dataflow component to a dynamic mesh
		void DataflowToDynamicMesh(TSharedPtr<::Dataflow::FEngineContext> DataflowContext, UObject* Asset, UDataflow* Dataflow, UE::Geometry::FDynamicMesh3& DynamicMesh);

		// Convert a dynamic mesh to a rendering facade
		void DynamicMeshToRenderingFacade(const UE::Geometry::FDynamicMesh3& DynamicMesh, GeometryCollection::Facades::FRenderingFacade& Facade);

		// Convert a dynamic mesh to a dataflow component
		void DynamicMeshToDataflow(const UE::Geometry::FDynamicMesh3& DynamicMesh, UDataflow* Dataflow);
	}
}

namespace Dataflow
{
	TSharedPtr<::Dataflow::FEngineContext> GetContext(UDataflowEditorContent* Content);
}