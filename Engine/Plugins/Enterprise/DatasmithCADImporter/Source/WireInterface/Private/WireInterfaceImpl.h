// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifdef USE_OPENMODEL

#include "AliasBrepConverter.h"
#include "OpenModelUtils.h"

#include "IWireInterface.h"

#include "Templates/SharedPointer.h"

class AlDagNode;
class AlLayer;
class AlMesh;
class AlShader;
class AlShaderNode;
class IDatasmithActorElement;
class IDatasmithBaseMaterialElement;
class IDatasmithMaterialIDElement;
class IDatasmithUEPbrMaterialElement;

struct FMeshDescription;

namespace CADLibrary
{
	struct FMeshParameters;
	class ICADModelConverter;
}

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{
	class FBodyData;
	struct FDagNodeInfo;
	class IAliasBRepConverter;

	class FWireTranslatorImpl : public IWireInterface
	{
	public:
		FWireTranslatorImpl() {}

		~FWireTranslatorImpl();

		/** Begin IWrieInterface */
		virtual bool Initialize(const TCHAR* InSceneFullName) override;
		virtual bool Load(TSharedPtr<IDatasmithScene> InScene) override;
		virtual void SetImportSettings(const FWireSettings& Options) override;
		virtual void SetOutputPath(const FString& Path) { OutputPath = Path; }

		bool LoadStaticMesh(const TSharedPtr<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions);
		/** End IWrieInterface */

	private:
		/** Model traversal */
		bool TraverseModel();
		TSharedPtr<IDatasmithActorElement> TraverseDag(const TAlDagNodePtr<AlDagNode>& DagNode);

		TSharedPtr<IDatasmithActorElement> ProcessGeometryNode(const TAlDagNodePtr<AlDagNode>& GeomNode, const TAlObjectPtr<AlLayer>& ParentLayer = TAlObjectPtr<AlLayer>());
		TSharedPtr<IDatasmithActorElement> TraverseGroupNode(const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer = TAlObjectPtr<AlLayer>());
		TSharedPtr<IDatasmithActorElement> ProcessGroupNode(const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer = TAlObjectPtr<AlLayer>());
		TSharedPtr<IDatasmithActorElement> ProcessBodyNode(TSharedPtr<FBodyNode>& BodyNode, const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer = TAlObjectPtr<AlLayer>());
		TSharedPtr<IDatasmithActorElement> ProcessPatchMesh(TSharedPtr<FPatchMesh>& PatchMesh, const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer = TAlObjectPtr<AlLayer>());

		TSharedPtr<IDatasmithActorElement> FindOrAddLayerActor(const TAlObjectPtr<AlLayer>& Layer);
		TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(const TAlDagNodePtr<AlDagNode>& GeomNode);
		TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(TSharedPtr<FBodyNode>& BodyNode);
		TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(TSharedPtr<FPatchMesh>& PatchMesh);

		/** Material creation */
		bool IsTransparent(FColor& TransparencyColor)
		{
			float Opacity = 1.0f - ((float)(TransparencyColor.R + TransparencyColor.G + TransparencyColor.B)) / 765.0f;
			return !FMath::IsNearlyEqual(Opacity, 1.0f);
		}

		bool GetCommonParameters(int32 Field, double Value, FColor& Color, FColor& TransparencyColor, FColor& IncandescenceColor, double GlowIntensity);

		void AddAlBlinnParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement);
		void AddAlLambertParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement);
		void AddAlLightSourceParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement);
		void AddAlPhongParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement);
		
		TSharedPtr<IDatasmithMaterialIDElement> FindOrAddMaterial(const TAlObjectPtr<AlShader>& Shader);

		/** Geometry retrieval */
		TOptional<FMeshDescription> GetMeshDescription(TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters);
		TSharedPtr<CADLibrary::ICADModelConverter> GetModelConverter(IAliasBRepConverter*& BRepConverter) const;
		TOptional<FMeshDescription> TessellateParametricNode(AlDagNode& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshDescriptionFromBodyNode(TSharedPtr<FBodyNode>& BodyNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshDescriptionFromPatchMesh(TSharedPtr<FPatchMesh>& PatchMesh, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshDescriptionFromParametricNode(AlDagNode& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshDescriptionFromMeshNode(const TAlDagNodePtr<AlMeshNode>& MeshNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, AlMatrix4x4* AlMeshInvGlobalMatrix = nullptr);

	private:
		TSharedPtr<IDatasmithScene> DatasmithScene;
		FString OutputPath;
		FString SceneFullPath;

		FWireSettings WireSettings;

		TSharedPtr<CADLibrary::ICADModelConverter> CADModelConverter;
		TSharedPtr<IAliasBRepConverter> AliasBRepConverter;

		bool bSceneLoaded = false;

		TMap<FString, TSharedPtr<IDatasmithBaseMaterialElement>> ShaderNameToMaterial;

		TMap<uint32, TSharedPtr<IDatasmithMeshElement>> GeomNodeToMeshElement;
		TMap<TSharedPtr<IDatasmithMeshElement>, AlDagNode*> MeshElementToGeomNode;

		TMap<uint32, TSharedPtr<IDatasmithMeshElement>> BodyNodeToMeshElement;
		TMap<TSharedPtr<IDatasmithMeshElement>, TSharedPtr<FBodyNode>> MeshElementToBodyNode;

		TMap<uint32, TSharedPtr<IDatasmithMeshElement>> PatchMeshToMeshElement;
		TMap<TSharedPtr<IDatasmithMeshElement>, TSharedPtr<FPatchMesh>> MeshElementToPatchMesh;

		TMap<uint32, TSharedPtr<IDatasmithActorElement>> LayerToActor;
	};
} // namespace
#endif
