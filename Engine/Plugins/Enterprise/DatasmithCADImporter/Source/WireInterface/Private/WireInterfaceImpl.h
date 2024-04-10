// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifdef USE_OPENMODEL

#include "AliasBrepConverter.h"
#include "OpenModelUtils.h"

#include "IWireInterface.h"

#include "DatasmithImportOptions.h"

#include "Templates/SharedPointer.h"

class AlDagNode;
class AlMesh;
class AlShaderNode;
class IDatasmithActorElement;
class IDatasmithMaterialIDElement;
class IDatasmithUEPbrMaterialElement;

struct FMeshDescription;
struct FDatasmithTessellationOptions;

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
		virtual void SetTessellationOptions(const FDatasmithTessellationOptions& Options) override;
		virtual void SetOutputPath(const FString& Path) { OutputPath = Path; }

		bool LoadStaticMesh(const TSharedPtr<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions);
		/** End IWrieInterface */

	private:
		TOptional<FMeshDescription> GetMeshDescription(TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters);
		TOptional<FMeshDescription> GetMeshDescription(TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, TSharedPtr<FBodyData> BodyTemp);


	private:

		bool GetDagLeaves();
		bool GetShader();
		void RecurseDagForLeaves(const TAlObjectPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
		void RecurseDagForLeavesNoMerge(const TAlObjectPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
		void DagForLeavesNoMerge(const TAlObjectPtr<AlDagNode>& DagNode, FDagNodeInfo& ParentInfo);
		bool ProcessAlGroupNode(AlDagNode& GroupNode, FDagNodeInfo& ParentInfo);
		void ProcessAlShellNode(const TAlObjectPtr<AlDagNode>& ShellNode, FDagNodeInfo& ParentInfo, const FString& ShaderName);
		void ProcessBodyNode(const TSharedPtr<FBodyData>& Body, FDagNodeInfo& ParentInfo);
		TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(const TSharedPtr<FBodyData>& Body, const FDagNodeInfo& ParentInfo);
		TSharedPtr<IDatasmithMeshElement> FindOrAddMeshElement(const TAlObjectPtr<AlDagNode>& ShellNode, const FDagNodeInfo& ParentInfo, const FString& ShaderName);
		TSharedPtr<IDatasmithActorElement> FindOrAddParentActor(FDagNodeInfo& ParentInfo, const FString& LayerName);
		TSharedPtr<IDatasmithActorElement> FindOrAddLayerActor(const FString& LayerName);

		void RecurseDeleteEmptyActor(TSharedPtr<IDatasmithActorElement> Actor);

		TOptional<FMeshDescription> GetMeshOfShellNode(AlDagNode& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshOfNodeMesh(AlDagNode& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters, AlMatrix4x4* AlMeshInvGlobalMatrix = nullptr);
		TOptional<FMeshDescription> GetMeshOfShellBody(TSharedPtr<FBodyData> DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> GetMeshOfMeshBody(TSharedPtr<FBodyData> DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

		void AddNodeInBodyGroup(TAlObjectPtr<AlDagNode>& DagNode, const FString& ShaderName, TMap<uint32, TSharedPtr<FBodyData>>& ShellToProcess, bool bIsAPatch, uint32 MaxSize);

		TSharedPtr<CADLibrary::ICADModelConverter> GetModelConverter(IAliasBRepConverter*& BRepConverter) const;

		TOptional<FMeshDescription> MeshDagNodeWithExternalMesher(AlDagNode& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);
		TOptional<FMeshDescription> MeshDagNodeWithExternalMesher(TSharedPtr<FBodyData> DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

		TOptional<FMeshDescription> ImportMesh(AlMesh& Mesh, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& MeshParameters);

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

	private:
		TSharedPtr<IDatasmithScene> DatasmithScene;
		FString SceneName;
		FString CurrentPath;
		FString OutputPath;
		FString SceneFullPath;

		// Hash value of the scene file used to check if the file has been modified for re-import
		uint32 SceneFileHash = 0;

		TAlObjectPtr<AlDagNode> AlRootNode;

		/** Table of correspondence between mesh identifier and associated Datasmith mesh element */
		TMap<uint32, TSharedPtr<IDatasmithMeshElement>> ShellUuidToMeshElementMap;
		TMap<uint32, TSharedPtr<IDatasmithMeshElement>> BodyUuidToMeshElementMap;

		/** Datasmith mesh elements to OpenModel objects */
		TMap<IDatasmithMeshElement*, TAlObjectPtr<AlDagNode>> MeshElementToAlDagNodeMap;

		TMap<IDatasmithMeshElement*, TSharedPtr<FBodyData>> MeshElementToBodyMap;

		TMap<IDatasmithMeshElement*, FString> MeshElementToShaderName;
		TMap<FString, FColor> ShaderNameToColor;
		TMap<FString, TSharedPtr<IDatasmithMaterialIDElement>> ShaderNameToUEMaterialId;

		TMap<FString, TSharedPtr<IDatasmithActorElement>> LayerNameToActor;

		FDatasmithTessellationOptions TessellationOptions;

		TSharedPtr<CADLibrary::ICADModelConverter> CADModelConverter;
		TSharedPtr<IAliasBRepConverter> AliasBRepConverter;

		bool bAliasUseNative = false;
		bool bSceneLoaded = false;
	};
} // namespace
#endif
