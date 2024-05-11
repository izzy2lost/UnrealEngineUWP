// Copyright Epic Games, Inc. All Rights Reserved.

#include "WireInterfaceImpl.h"

#include "Modules/ModuleManager.h"

#ifdef USE_OPENMODEL
#include "CADOptions.h"
#include "Containers/List.h"
#include "DatasmithImportOptions.h"
#include "DatasmithPayload.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "GenericPlatform/GenericPlatformTLS.h"
#include "HAL/ConsoleManager.h"
#include "IDatasmithSceneElements.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Utility/DatasmithMeshHelper.h"

#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"

#if WITH_EDITOR
#include "Editor.h"
#include "IMessageLogListing.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#endif

#include "AliasModelToCADKernelConverter.h"
#include "AliasModelToTechSoftConverter.h" // requires Techsoft as public dependency
#include "CADInterfacesModule.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <AlChannel.h>
#include <AlDagNode.h>
#include <AlGroupNode.h>
#include <AlLayer.h>
#include <AlLinkItem.h>
#include <AlList.h>
#include <AlMesh.h>
#include <AlMeshNode.h>
#include <AlPersistentID.h>
#include <AlRetrieveOptions.h>
#include <AlSet.h>
#include <AlSetMember.h>
#include <AlShader.h>
#include <AlShadingFieldItem.h>
#include <AlShell.h>
#include <AlShellNode.h>
#include <AlSurface.h>
#include <AlSurfaceNode.h>
#include <AlTesselate.h>
#include <AlTrimRegion.h>
#include <AlTM.h>
#include <AlUniverse.h>

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWireInterface, Log, All);

#define LOCTEXT_NAMESPACE "WireInterface"

#define WRONG_VERSION_TEXT "Unsupported version of Alias detected. Please upgrade to Alias 2021.3 (or later version)."

#define TRACK_MESHELEMENT 0

#if TRACK_MESHELEMENT
#include "CompGeom/FitOrientedBox3.h"
#endif

#ifdef OPEN_MODEL_2023_0

static bool bGAliasSewByMaterial = false;
FAutoConsoleVariableRef GAliasSewByMaterial(
	TEXT("ds.CADTranslator.Alias.SewByMaterial"),
	bGAliasSewByMaterial,
	TEXT("Enable Sew action merges BReps according to their material i.e. only BReps associated with same material can be merged together.\
Default is disable\n"),
ECVF_Default);

#endif

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{
#if TRACK_MESHELEMENT
	void MakeMeshVisible(FMeshDescription& MeshDescription);
#endif

	static bool bGSewByMaterial = false;

	static const FColor DefaultColor = FColor(200, 200, 200);

	const uint64 LibAliasNext_Version = 0xffffffffffffffffull;
	const uint64 LibAlias2023_1_0_Version = 8162778619576619;
	const uint64 LibAlias2023_0_0_Version = 8162774324609149;
	const uint64 LibAlias2022_2_0_Version = 7881307937833405;
	const uint64 LibAlias2022_1_0_Version = 7881303642865885;
	const uint64 LibAlias2022_0_1_Version = 7881299347964005;
	const uint64 LibAlias2021_3_2_Version = 7599833027117059;
	const uint64 LibAlias2021_3_1_Version = 7599824433840131;
	const uint64 LibAlias2021_3_0_Version = 7599824424206339;
	const uint64 LibAlias2021_Version = 7599824377020416;
	const uint64 LibAlias2020_Version = 7318349414924288;
	const uint64 LibAlias2019_Version = 5000000000000000;

#if defined(OPEN_MODEL_2020)
	const uint64 LibAliasVersionMin = LibAlias2019_Version;
	const uint64 LibAliasVersionMax = LibAlias2021_3_0_Version;
	const FString AliasSdkVersion   = TEXT("2020");
#elif defined(OPEN_MODEL_2021_3)
	const uint64 LibAliasVersionMin = LibAlias2021_3_0_Version;
	const uint64 LibAliasVersionMax = LibAlias2022_0_1_Version;
	const FString AliasSdkVersion   = TEXT("2021.3");
#elif defined(OPEN_MODEL_2022)
	const uint64 LibAliasVersionMin = LibAlias2022_0_1_Version;
	const uint64 LibAliasVersionMax = LibAlias2022_1_0_Version;
	const FString AliasSdkVersion   = TEXT("2022");
#elif defined(OPEN_MODEL_2022_1)
	const uint64 LibAliasVersionMin = LibAlias2022_1_0_Version;
	const uint64 LibAliasVersionMax = LibAlias2022_2_0_Version;
	const FString AliasSdkVersion   = TEXT("2022.1");
#elif defined(OPEN_MODEL_2022_2)
	const uint64 LibAliasVersionMin = LibAlias2022_2_0_Version;
	const uint64 LibAliasVersionMax = LibAlias2023_0_0_Version;
	const FString AliasSdkVersion   = TEXT("2022.2");
#elif defined(OPEN_MODEL_2023_0)
	const uint64 LibAliasVersionMin = LibAlias2023_0_0_Version;
	const uint64 LibAliasVersionMax = LibAlias2023_1_0_Version;
	const FString AliasSdkVersion   = TEXT("2023.0");
#elif defined(OPEN_MODEL_2023_1)      
	const uint64 LibAliasVersionMin = LibAlias2023_1_0_Version;
	const uint64 LibAliasVersionMax = LibAliasNext_Version;
	const FString AliasSdkVersion   = TEXT("2023.1");
#endif

	// Alias material management (to allow sew of BReps of different materials):
	// To be compatible with "Retessellate" function, Alias material management as to be the same than CAD (TechSoft) import. 
	// As a reminder: the name and slot of UE Material from CAD is based on CAD material/color data i.e. RGBA Color components => "UE Material slot" (int32) and "UE Material name" (FString = FString::FromInt("UE Material slot"))
	// UE Material Label is free.
	// 
	// During Retessellate step, Color/Material of each CAD Faces are known, so "UE Material slot" can be deduced.
	// 
	// For Alias import:
	// Alias BRep is exported in CAD modeler (CADKernel, TechSoft, ...)
	// Material is build in UE, 
	// From an Alias Material, a unique Color is generated.
	// This Color is associated to the BRep Shell/face in the CAD modeler
	// The name and slot of the associated UE Material is defined from this color
	// So at the retessellate step, nothing changes from the CAD Retessellate process
	// 
	// The unique Color of an Alias Material is defined as follows:
	// TypeHash(Alias Material Name) => uint24 == 3 uint8 => RGB components of the color  

	FColor CreateShaderColorFromShaderName(const FString& ShaderName)
	{
		const uint32 ShaderHash = FCrc::Strihash_DEPRECATED(*ShaderName);
		const uint32 Red = (ShaderHash & 0xff000000) >> 24;
		const uint32 Green = (ShaderHash & 0x00ff0000) >> 16;
		const uint32 Blue = (ShaderHash & 0x0000ff00) >> 8;
		return FColor(Red, Green, Blue);
	}

	int32 CreateShaderId(const FColor& ShaderColor)
	{
		return FMath::Abs((int32)GetTypeHash(ShaderColor));
	}

	uint32 GetSceneFileHash(const FString& FullPath, const FString& FileName)
	{
		FFileStatData FileStatData = IFileManager::Get().GetStatData(*FullPath);

		int64 FileSize = FileStatData.FileSize;
		FDateTime ModificationTime = FileStatData.ModificationTime;

		uint32 FileHash = GetTypeHash(FileName);
		FileHash = HashCombine(FileHash, GetTypeHash(FileSize));
		FileHash = HashCombine(FileHash, GetTypeHash(ModificationTime));

		return FileHash;
	}

	bool GetConsoleBoolValue(const TCHAR* CVarName, bool bDefault)
	{
		const IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(CVarName);
		return ConsoleVariable ? ConsoleVariable->GetBool() : bDefault;
	}

	bool FWireTranslatorImpl::Initialize(const TCHAR* InSceneFullName)
	{
		// #wire_import: Check file can be loaded with current SDK

		if (InSceneFullName)
		{
			SceneFullPath = InSceneFullName;
		}

		// #wire_import: Check whether this implementation can load it
		return true;
	}

	FWireTranslatorImpl::~FWireTranslatorImpl()
	{
		if (bSceneLoaded)
		{
			AlUniverse::deleteAll();
		}
		bSceneLoaded = false;
	}

	void FWireTranslatorImpl::SetImportSettings(const FWireSettings& Settings)
	{
		WireSettings = Settings;
		if (CADModelConverter)
		{
			CADModelConverter->SetImportParameters(Settings.ChordTolerance, Settings.MaxEdgeLength, Settings.NormalTolerance, (CADLibrary::EStitchingTechnique)Settings.StitchingTechnique);
		}
	}

	// Wire file parsing
	bool FWireTranslatorImpl::Load(TSharedPtr<IDatasmithScene> InScene)
	{
		DatasmithScene = InScene;

		const FString AliasProductVersion = FString::Printf(TEXT("Alias %s"), *AliasSdkVersion);
		DatasmithScene->SetHost(TEXT("Alias"));
		DatasmithScene->SetVendor(TEXT("Autodesk"));
		DatasmithScene->SetProductName(TEXT("Alias Tools"));
		DatasmithScene->SetExporterSDKVersion(*AliasSdkVersion);
		DatasmithScene->SetProductVersion(*AliasProductVersion);

		WireSettings.bAliasUseNative = GetConsoleBoolValue(TEXT("ds.Wiretranslator.UseNative"), false);
		if (!WireSettings.bAliasUseNative)
		{
			CADLibrary::FImportParameters ImportParameters;
			if (CADLibrary::FImportParameters::bGDisableCADKernelTessellation)
			{
				CADModelConverter = MakeShared<FAliasModelToTechSoftConverter>(ImportParameters);
			}
			else
			{
				CADModelConverter = MakeShared<FAliasModelToCADKernelConverter>(WireSettings, ImportParameters);
			}
		}
		else
		{
			// Merge by group when using Alias' tessellator
			WireSettings.bMergeGeometryByGroup = false;
		}

		bGSewByMaterial = GetConsoleBoolValue(TEXT("ds.CADTranslator.Alias.SewByMaterial"), false);

		// Initialize Alias.
		AlUniverse::initialize();

		if (AlUniverse::retrieve(TCHAR_TO_UTF8(*SceneFullPath)) != sSuccess)
		{
			return false;
		}

		AlRetrieveOptions options;
		AlUniverse::retrieveOptions(options);

		bSceneLoaded = true;

		return TraverseModel();
	}

	bool FWireTranslatorImpl::TraverseModel()
	{
		TAlDagNodePtr<AlDagNode> DagNode(AlUniverse::firstDagNode());
		while (DagNode)
		{
			TSharedPtr<IDatasmithActorElement> ActorElement = TraverseDag(DagNode);
			if (ActorElement)
			{
				DatasmithScene->AddActor(ActorElement);
			}

			DagNode = TAlDagNodePtr<AlDagNode>(DagNode->nextNode());
		}

		TAlObjectPtr<AlSet> Set(AlUniverse::firstSet());
		while (Set)
		{
			// #wire_import: Add an actor to represent the set.
			TAlObjectPtr<AlSetMember> SetMember(Set->firstMember());
			while (SetMember)
			{
				TAlDagNodePtr<AlDagNode> DagNodeInSet(SetMember->object() ? SetMember->object()->asDagNodePtr() : nullptr);
				if (DagNodeInSet)
				{
					TSharedPtr<IDatasmithActorElement> ActorElement = TraverseDag(DagNodeInSet);
					if (ActorElement)
					{
						DatasmithScene->AddActor(ActorElement);
					}
				}

				SetMember = TAlObjectPtr<AlSetMember>(SetMember->nextSetMember());
			}

			Set = TAlObjectPtr<AlSet>(Set->nextSet());
		}

		return true;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::TraverseDag(const TAlDagNodePtr<AlDagNode>& RootNode)
	{
		TAlDagNodePtr<AlGroupNode> GroupNode(RootNode->asGroupNodePtr());
		if (GroupNode)
		{
			return WireSettings.bMergeGeometryByGroup ? ProcessGroupNode(GroupNode) : TraverseGroupNode(GroupNode);
		}
		else if (OpenModelUtils::IsGeometryValid(RootNode))
		{
			return ProcessGeometryNode(RootNode);
		}

		return TSharedPtr<IDatasmithActorElement>();
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::TraverseGroupNode(const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer)
	{
		TAlDagNodePtr<AlDagNode> ChildNode(GroupNode->childNode());
		int32 ChildrenCount = 0;
		while (ChildNode)
		{
			ChildrenCount++;
			ChildNode = TAlDagNodePtr<AlDagNode>(ChildNode->nextNode());
		}

		if (ChildrenCount == 0)
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TArray<TSharedPtr<IDatasmithActorElement>> ChildActors;
		ChildActors.Reserve(ChildrenCount);

		ChildNode = TAlDagNodePtr<AlDagNode>(GroupNode->childNode());
		while (ChildNode)
		{
			TAlDagNodePtr<AlGroupNode> ChildGroupNode(ChildNode->asGroupNodePtr());
			if (ChildGroupNode)
			{
				if (TSharedPtr<IDatasmithActorElement> ActorElement = TraverseGroupNode(ChildGroupNode, GroupNode->layer()))
				{
					ChildActors.Emplace(MoveTemp(ActorElement));
				}

			}
			else if (OpenModelUtils::IsGeometryValid(ChildNode))
			{
				if (TSharedPtr<IDatasmithActorElement> ActorElement = ProcessGeometryNode(ChildNode, ParentLayer))
				{
					ChildActors.Emplace(MoveTemp(ActorElement));
				}
			}

			ChildNode = TAlDagNodePtr<AlDagNode>(ChildNode->nextNode());
		}

		if (ChildActors.Num() == 0)
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithActorElement> ActorElement = FDatasmithSceneFactory::CreateActor(*GroupNode.GetUniqueID(GROUPNODE_TYPE));
		if (!ActorElement.IsValid())
		{
			return ActorElement;
		}

		FString Label = GroupNode.GetName();
		ActorElement->SetLabel(Label.Len() > 0 ? *Label : TEXT("UnnamedGroup"));

		TAlObjectPtr<AlLayer> Layer(GroupNode->layer());
		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(Layer, CsvLayerString))
		{
			ActorElement->SetLayer(*CsvLayerString);
		}

		OpenModelUtils::SetActorTransform(*ActorElement, GroupNode.Get());

		for (TSharedPtr<IDatasmithActorElement>& ChildActor : ChildActors)
		{
			if (OpenModelUtils::ActorHasContent(ChildActor))
			{
				ActorElement->AddChild(ChildActor);
			}
		}

		if (WireSettings.bUseLayerAsActor && Layer != ParentLayer)
		{
			if (OpenModelUtils::ActorHasContent(ActorElement))
			{
				if (TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(Layer))
				{
					LayerActor->AddChild(ActorElement);
				}
			}

			return TSharedPtr<IDatasmithActorElement>();
		}

		return ActorElement;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::ProcessGroupNode(const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer)
	{
		TAlDagNodePtr<AlDagNode> ChildNode(GroupNode->childNode());
		int32 ChildrenCount = 0;
		while (ChildNode)
		{
			ChildrenCount++;
			ChildNode = TAlDagNodePtr<AlDagNode>(ChildNode->nextNode());
		}

		if (ChildrenCount == 0)
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TArray<TSharedPtr<IDatasmithActorElement>> ChildActors;
		ChildActors.Reserve(ChildrenCount);

		TSharedPtr<FBodyNode> BodyNode = MakeShared<FBodyNode>(GroupNode.GetName() + TEXT("_surf"), GroupNode->layer(), ChildrenCount);
		TSharedPtr<FPatchMesh> PatchMesh = MakeShared<FPatchMesh>(GroupNode.GetName() + TEXT("_mesh"), GroupNode->layer(), ChildrenCount);

		ChildNode = TAlDagNodePtr<AlDagNode>(GroupNode->childNode());
		while (ChildNode)
		{
			TAlDagNodePtr<AlGroupNode> ChildGroupNode(ChildNode->asGroupNodePtr());
			if (ChildGroupNode)
			{
				if (TSharedPtr<IDatasmithActorElement> ActorElement = ProcessGroupNode(ChildGroupNode, GroupNode->layer()))
				{
					ChildActors.Emplace(MoveTemp(ActorElement));
				}

			}
			else if (OpenModelUtils::IsGeometryValid(ChildNode))
			{
				TAlDagNodePtr<AlMeshNode> MeshNode(ChildNode->asMeshNodePtr());
				if (MeshNode && MeshNode->mesh())
				{
					PatchMesh->AddMeshNode(MeshNode);
				}
				TAlDagNodePtr<AlShellNode> ShellNode(ChildNode->asShellNodePtr());
				if (ShellNode && ShellNode->shell())
				{
					BodyNode->AddShellNode(ShellNode);
				}
				else
				{
					TAlDagNodePtr<AlSurfaceNode> SurfaceNode(ChildNode->asSurfaceNodePtr());
					if (SurfaceNode && SurfaceNode->surface())
					{
						BodyNode->AddSurfaceNode(SurfaceNode);
					}
				}
			}

			ChildNode = TAlDagNodePtr<AlDagNode>(ChildNode->nextNode());
		}

		if (ChildActors.IsEmpty() && !BodyNode->Initialize() && !PatchMesh->Initialize())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		if (TSharedPtr<IDatasmithActorElement> ActorElement = ProcessBodyNode(BodyNode, GroupNode, GroupNode->layer()))
		{
			if (OpenModelUtils::ActorHasContent(ActorElement))
			{
				ChildActors.Emplace(MoveTemp(ActorElement));
			}
		}

		if (TSharedPtr<IDatasmithActorElement> ActorElement = ProcessPatchMesh(PatchMesh, GroupNode, GroupNode->layer()))
		{
			if (OpenModelUtils::ActorHasContent(ActorElement))
			{
				ChildActors.Emplace(MoveTemp(ActorElement));
			}
		}

		if (ChildActors.Num() == 0)
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithActorElement> ActorElement;
		if (WireSettings.bMergeGeometryByGroup && ChildActors.Num() == 1)
		{
			ActorElement = ChildActors[0];
		}
		else
		{
			ActorElement = FDatasmithSceneFactory::CreateActor(*GroupNode.GetUniqueID(GROUPNODE_TYPE));
		}

		if (!ActorElement.IsValid())
		{
			return ActorElement;
		}

		FString Label = GroupNode.GetName();
		ActorElement->SetLabel(Label.Len() > 0 ? *Label : TEXT("UnnamedGroup"));

		TAlObjectPtr<AlLayer> Layer(GroupNode->layer());
		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(Layer, CsvLayerString))
		{
			ActorElement->SetLayer(*CsvLayerString);
		}

		if (WireSettings.bMergeGeometryByGroup && ChildActors.Num() > 1)
		{
			OpenModelUtils::SetActorTransform(*ActorElement, GroupNode.Get());

			for (TSharedPtr<IDatasmithActorElement>& ChildActor : ChildActors)
			{
				if (OpenModelUtils::ActorHasContent(ChildActor))
				{
					ActorElement->AddChild(ChildActor);
				}
			}

		}

		if (WireSettings.bUseLayerAsActor && Layer != ParentLayer)
		{
			if (OpenModelUtils::ActorHasContent(ActorElement))
			{
				if (TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(Layer))
				{
					LayerActor->AddChild(ActorElement);
				}
			}

			return TSharedPtr<IDatasmithActorElement>();
		}

		return ActorElement;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::ProcessGeometryNode(const TAlDagNodePtr<AlDagNode>& GeomNode, const TAlObjectPtr<AlLayer>& ParentLayer)
	{
		TAlObjectPtr<AlLayer> Layer(GeomNode->layer());
		if (Layer && Layer->invisible())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FindOrAddMeshElement(GeomNode);
		if (!MeshElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithMeshActorElement> ActorElement = FDatasmithSceneFactory::CreateMeshActor(*GeomNode.GetUniqueID(MESHNODE_TYPE));
		if (!ActorElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		FString Label = GeomNode.GetName();
		ActorElement->SetLabel(Label.Len() > 0 ? *Label : TEXT("NoName"));
		ActorElement->SetStaticMeshPathName(MeshElement->GetName());

		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(Layer, CsvLayerString))
		{
			ActorElement->SetLayer(*CsvLayerString);
		}

		OpenModelUtils::SetActorTransform(*ActorElement, GeomNode);

		TAlDagNodePtr<AlShellNode> ShellNode(GeomNode->asShellNodePtr());
		if (ShellNode && ShellNode->shell())
		{
			TAlObjectPtr<AlShell> Shell(ShellNode->shell());
			TAlObjectPtr<AlShader> Shader(Shell->firstShader());
			int32 SlotIndex = 0;
			while (Shader.IsValid())
			{
				TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = FindOrAddMaterial(Shader);
				MaterialIDElement->SetId(SlotIndex++);
				ActorElement->AddMaterialOverride(MaterialIDElement);
				Shader = Shell->nextShader(Shader.Get());
			}
		}
		else
		{
			TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement;

			TAlDagNodePtr<AlSurfaceNode> SurfaceNode(GeomNode->asSurfaceNodePtr());
			if (SurfaceNode && SurfaceNode->surface())
			{
				MaterialIDElement = FindOrAddMaterial(SurfaceNode->surface()->firstShader());
			}
			else
			{
				TAlDagNodePtr<AlMeshNode> MeshNode(GeomNode->asMeshNodePtr());
				if (MeshNode && MeshNode->mesh())
				{
					MaterialIDElement = FindOrAddMaterial(MeshNode->mesh()->firstShader());
				}
			}

			if (MaterialIDElement)
			{
				MaterialIDElement->SetId(0);
				ActorElement->AddMaterialOverride(MaterialIDElement);
			}
		}

		if (WireSettings.bUseLayerAsActor && Layer != ParentLayer)
		{
			if (TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(Layer))
			{
				LayerActor->AddChild(ActorElement);
				return TSharedPtr<IDatasmithActorElement>();
			}

			ensure(false);
		}

		return ActorElement;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::ProcessBodyNode(TSharedPtr<FBodyNode>& BodyNode, const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer)
	{
		if (!BodyNode->HasContent())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		if (BodyNode->HasSingleContent())
		{
			return ProcessGeometryNode(BodyNode->GetSingleContent(), ParentLayer);
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FindOrAddMeshElement(BodyNode);
		if (!MeshElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithMeshActorElement> ActorElement = FDatasmithSceneFactory::CreateMeshActor(*BodyNode->GetUniqueID());
		if (!ActorElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		FString Label = BodyNode->GetName();
		ActorElement->SetLabel(Label.Len() > 0 ? *Label : TEXT("NoName"));
		ActorElement->SetStaticMeshPathName(MeshElement->GetName());

		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(BodyNode->GetLayer(), CsvLayerString))
		{
			ActorElement->SetLayer(*CsvLayerString);
		}

		if (!BodyNode->GetLayer()->isSymmetric())
		{
			OpenModelUtils::SetActorTransform(*ActorElement, GroupNode.Get());
		}

		if (WireSettings.bUseLayerAsActor && BodyNode->GetLayer() != ParentLayer)
		{
			if (TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(BodyNode->GetLayer()))
			{
				LayerActor->AddChild(ActorElement);
				return TSharedPtr<IDatasmithActorElement>();
			}

			ensure(false);
		}

		return ActorElement;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::ProcessPatchMesh(TSharedPtr<FPatchMesh>& PatchMesh, const TAlDagNodePtr<AlGroupNode>& GroupNode, const TAlObjectPtr<AlLayer>& ParentLayer)
	{
		if (!PatchMesh->HasContent())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TAlObjectPtr<AlLayer> Layer(GroupNode->layer());
		if (Layer && Layer->invisible())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		if (PatchMesh->HasSingleContent())
		{
			return ProcessGeometryNode(PatchMesh->GetSingleContent().Get(), ParentLayer);
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FindOrAddMeshElement(PatchMesh);
		if (!MeshElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithMeshActorElement> ActorElement = FDatasmithSceneFactory::CreateMeshActor(*PatchMesh->GetUniqueID());
		if (!ActorElement.IsValid())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		FString Label = PatchMesh->GetName();
		ActorElement->SetLabel(Label.Len() > 0 ? *Label : TEXT("NoName"));
		ActorElement->SetStaticMeshPathName(MeshElement->GetName());

		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(PatchMesh->GetLayer(), CsvLayerString))
		{
			ActorElement->SetLayer(*CsvLayerString);
		}

		OpenModelUtils::SetActorTransform(*ActorElement, GroupNode.Get());

		if (WireSettings.bUseLayerAsActor && PatchMesh->GetLayer() != ParentLayer)
		{
			if (TSharedPtr<IDatasmithActorElement> LayerActor = FindOrAddLayerActor(PatchMesh->GetLayer()))
			{
				LayerActor->AddChild(ActorElement);
				return TSharedPtr<IDatasmithActorElement>();
			}

			ensure(false);
		}

		return ActorElement;
	}

	TSharedPtr<IDatasmithMeshElement> FWireTranslatorImpl::FindOrAddMeshElement(const TAlDagNodePtr<AlDagNode>& GeomNode)
	{
		// Look if geometry has not been already processed, return it if found
		if (TSharedPtr<IDatasmithMeshElement>* MeshElementPtr = GeomNodeToMeshElement.Find(GeomNode.GetHash()))
		{
			return *MeshElementPtr;
		}

		if (!OpenModelUtils::IsGeometryValid(GeomNode))
		{
			// #wire_import: Log an error
			return TSharedPtr<IDatasmithMeshElement>();
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*GeomNode.GetUniqueID(MESH_TYPE));

		MeshElement->SetLabel(*GeomNode.GetName());
		MeshElement->SetLightmapSourceUV(-1);
#if TRACK_MESHELEMENT
		{
			//static const FString ToTrack(TEXT("shell_30181"));
			//static const FString ToTrack(TEXT("Blend_srf_1395"));
			static const FString ToTrack(TEXT("M0047942"));
			if (!ToTrack.Equals(MeshElement->GetLabel()))
			{
				return TSharedPtr<IDatasmithMeshElement>();
			}
		}
#endif
		auto ApplyMaterial = [this, &MeshElement](const TAlObjectPtr<AlShader>& Shader, int32 SlotIndex)
			{
				if (TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = this->FindOrAddMaterial(Shader))
				{
					MaterialIDElement->SetId(SlotIndex);
					MeshElement->SetMaterial(MaterialIDElement->GetName(), SlotIndex);
				}
			};


		TAlDagNodePtr<AlShellNode> ShellNode(GeomNode->asShellNodePtr());
		if (ShellNode)
		{
			TAlObjectPtr<AlShell> Shell(ShellNode->shell());

			TAlObjectPtr<AlShader> Shader(Shell->firstShader());
			int32 SlotIndex = 0;
			while (Shader)
			{
				ApplyMaterial(Shader, SlotIndex++);
				Shader = TAlObjectPtr<AlShader>(Shell->nextShader(Shader.Get()));
			}
			// #wire_import: Check There are as many shaders as trim regions
			MeshElementToParametricNode.Add(MeshElement, GeomNode.Get());
		}
		else
		{
			TAlDagNodePtr<AlSurfaceNode> SurfaceNode(GeomNode->asSurfaceNodePtr());
			if (SurfaceNode && AlIsValid(SurfaceNode->surface()))
			{
				// #wire_import: Check for trim regions
				ApplyMaterial(SurfaceNode->surface()->firstShader(), 0);
				MeshElementToParametricNode.Add(MeshElement, GeomNode.Get());
			}
			else
			{
				TAlDagNodePtr<AlMeshNode> MeshNode(GeomNode->asMeshNodePtr());
				if (MeshNode && AlIsValid(MeshNode->mesh()))
				{
					ApplyMaterial(MeshNode->mesh()->firstShader(), 0);
					MeshElementToMeshNode.Add(MeshElement, MeshNode.Get());
				}
				else
				{
					// #wire_import: Log an error
					return TSharedPtr<IDatasmithMeshElement>();
				}
			}
		}

		DatasmithScene->AddMesh(MeshElement);
		GeomNodeToMeshElement.Add(GeomNode.GetHash(), MeshElement);

		return MeshElement;
	}

	TSharedPtr<IDatasmithMeshElement> FWireTranslatorImpl::FindOrAddMeshElement(TSharedPtr<FBodyNode>& BodyNode)
	{
		// Look if geometry has not been already processed, return it if found
		if (TSharedPtr<IDatasmithMeshElement>* MeshElementPtr = BodyNodeToMeshElement.Find(BodyNode->GetHash()))
		{
			return *MeshElementPtr;
		}

		if (!BodyNode->HasContent())
		{
			// #wire_import: Log an error
			return TSharedPtr<IDatasmithMeshElement>();
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*BodyNode->GetUniqueID());

		MeshElement->SetLabel(*BodyNode->GetName());
		MeshElement->SetLightmapSourceUV(-1);
#if TRACK_MESHELEMENT
		{
			//static const FString ToTrack(TEXT("shell_30181"));
			//static const FString ToTrack(TEXT("Blend_srf_1395"));
			static const FString ToTrack(TEXT("M0047942"));
			if (!ToTrack.Equals(MeshElement->GetLabel()))
			{
				return TSharedPtr<IDatasmithMeshElement>();
			}
		}
#endif

		auto ApplyMaterial = [this, &MeshElement](int32 SlotIndex, const TAlObjectPtr<AlShader>& Shader)
			{
				if (TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = this->FindOrAddMaterial(Shader))
				{
					MaterialIDElement->SetId(SlotIndex);
					MeshElement->SetMaterial(MaterialIDElement->GetName(), SlotIndex);
				}
			};


		BodyNode->IterateOnSlotIndices(ApplyMaterial);

		DatasmithScene->AddMesh(MeshElement);
		BodyNodeToMeshElement.Add(BodyNode->GetHash(), MeshElement);
		MeshElementToBodyNode.Add(MeshElement, BodyNode);

		return MeshElement;
	}

	TSharedPtr<IDatasmithMeshElement> FWireTranslatorImpl::FindOrAddMeshElement(TSharedPtr<FPatchMesh>& PatchMesh)
	{
		// Look if geometry has not been already processed, return it if found
		if (TSharedPtr<IDatasmithMeshElement>* MeshElementPtr = PatchMeshToMeshElement.Find(PatchMesh->GetHash()))
		{
			return *MeshElementPtr;
		}

		if (!PatchMesh->HasContent())
		{
			// #wire_import: Log an error
			return TSharedPtr<IDatasmithMeshElement>();
		}

		TSharedPtr<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*PatchMesh->GetUniqueID());

		MeshElement->SetLabel(*PatchMesh->GetName());
		MeshElement->SetLightmapSourceUV(-1);

		auto ApplyMaterial = [this, &MeshElement](const TAlObjectPtr<AlShader>& Shader, int32 SlotIndex)
			{
				if (TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = this->FindOrAddMaterial(Shader))
				{
					MaterialIDElement->SetId(SlotIndex);
					MeshElement->SetMaterial(MaterialIDElement->GetName(), SlotIndex);
				}
			};


		int32 SlotIndex = 0;
		PatchMesh->IterateOnMeshNodes([&ApplyMaterial, &SlotIndex](const TAlDagNodePtr<AlMeshNode>& MeshNode)
			{
				if (MeshNode->mesh())
				{
					ApplyMaterial(MeshNode->mesh()->firstShader(), SlotIndex++);
				}
			});

		DatasmithScene->AddMesh(MeshElement);
		BodyNodeToMeshElement.Add(PatchMesh->GetHash(), MeshElement);
		MeshElementToPatchMesh.Add(MeshElement, PatchMesh);

		return MeshElement;
	}

	TSharedPtr<IDatasmithActorElement> FWireTranslatorImpl::FindOrAddLayerActor(const TAlObjectPtr<AlLayer>& Layer)
	{
		if (!WireSettings.bUseLayerAsActor || !Layer || Layer.GetName().IsEmpty())
		{
			return TSharedPtr<IDatasmithActorElement>();
		}

		TSharedPtr<IDatasmithActorElement>* LayerActorPtr = LayerToActor.Find(Layer.GetHash());
		if (LayerActorPtr)
		{
			return *LayerActorPtr;
		}

		TSharedPtr<IDatasmithActorElement> ParentLayerActor;
		TAlObjectPtr<AlLayer> ParentLayer(Layer->parentLayer());
		if (ParentLayer)
		{
			ParentLayerActor = FindOrAddLayerActor(ParentLayer);
		}

		TSharedPtr<IDatasmithActorElement> LayerActor = FDatasmithSceneFactory::CreateActor(*Layer.GetUniqueID(LAYER_TYPE));

		FString LayerName = Layer.GetName();
		LayerActor->SetLabel(*LayerName);

		FString CsvLayerString;
		if (OpenModelUtils::GetCsvLayerString(Layer, CsvLayerString))
		{
			LayerActor->SetLayer(*CsvLayerString);
		}

		if (ParentLayerActor)
		{
			ParentLayerActor->AddChild(LayerActor);
		}
		else
		{
			DatasmithScene->AddActor(LayerActor);
		}

		LayerToActor.Add(Layer.GetHash(), LayerActor);

		return LayerActor;
	}

	// Geometry retrieval

	bool FWireTranslatorImpl::LoadStaticMesh(const TSharedPtr<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions)
	{
		CADLibrary::FMeshParameters MeshParameters;

		if (TOptional<FMeshDescription> Mesh = GetMeshDescription(MeshElement, MeshParameters))
		{
#if TRACK_MESHELEMENT
			FMeshDescription& MeshDescription = OutMeshPayload.LodMeshes.Add_GetRef(MoveTemp(Mesh.GetValue()));
			//MakeMeshVisible(MeshDescription);
#else
			OutMeshPayload.LodMeshes.Add(MoveTemp(Mesh.GetValue()));
#endif
			const TCHAR* MeshFilename = MeshElement->GetFile();
			if (!WireSettings.bAliasUseNative && FPaths::FileExists(MeshFilename))
			{
				CADModelConverter->AddSurfaceDataForMesh(MeshFilename, MeshParameters, InTessellationOptions, OutMeshPayload);

				// Remove the file because it is temporary since caching is disabled.
				if (!CADLibrary::FImportParameters::bGEnableCADCache)
				{
					IFileManager::Get().Delete(MeshFilename);
				}
			}

			return true;
		}

		return false;
	}

	TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescription(TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters)
	{
		if (WireSettings.bAliasUseNative)
		{
			if (AlDagNode** GeomNodePtr = MeshElementToParametricNode.Find(MeshElement))
			{
				TAlDagNodePtr<AlDagNode> GeomNode(*GeomNodePtr);

				// #wire_import: Check whether parametric geometry with symmetry keeps the symmetry
				// #wire_import: the best way, should be to don't have to apply inverse global transform to the generated mesh
				TAlDagNodePtr<AlMeshNode> MeshNode = OpenModelUtils::TesselateDagLeaf(*GeomNode, ETesselatorType::Fast, WireSettings.ChordTolerance);

				if (MeshNode && MeshNode->mesh())
				{
					AlMatrix4x4 AlMatrix;
					GeomNode->inverseGlobalTransformationMatrix(AlMatrix);
					MeshNode->mesh()->transform(AlMatrix);

					// Get the meshes from the dag nodes. Note that removing the mesh's DAG.
					// will also removes the meshes, so we have to do it later.
					return GetMeshDescriptionFromMeshNode(MeshNode, MeshElement, OutMeshParameters);
				}
			}
			
			if (AlMeshNode** MeshNodePtr = MeshElementToMeshNode.Find(MeshElement))
			{
				TAlDagNodePtr<AlMeshNode> MeshNode(*MeshNodePtr);
				if (MeshNode)
				{
					return GetMeshDescriptionFromMeshNode(MeshNode, MeshElement, OutMeshParameters);
				}
			}

			return TOptional<FMeshDescription>();
		}

		if (TSharedPtr<FBodyNode>* BodyNodePtr = MeshElementToBodyNode.Find(MeshElement))
		{
			return GetMeshDescriptionFromBodyNode(*BodyNodePtr, MeshElement, OutMeshParameters);
		}

		if (AlDagNode** GeomNodePtr = MeshElementToParametricNode.Find(MeshElement))
		{
			return GetMeshDescriptionFromParametricNode(*GeomNodePtr, MeshElement, OutMeshParameters);
		}

		if (TSharedPtr<FPatchMesh>* PatchMeshPtr = MeshElementToPatchMesh.Find(MeshElement))
		{
			return GetMeshDescriptionFromPatchMesh(*PatchMeshPtr, MeshElement, OutMeshParameters);
		}

		if (AlMeshNode** MeshNodePtr = MeshElementToMeshNode.Find(MeshElement))
		{
			return GetMeshDescriptionFromMeshNode(*MeshNodePtr, MeshElement, OutMeshParameters);
		}

		return TOptional<FMeshDescription>();
	}

	TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescriptionFromBodyNode(TSharedPtr<FBodyNode>& BodyNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters)
	{
		OutMeshParameters = OpenModelUtils::GetMeshParameters(BodyNode->GetLayer());

		TSharedPtr<CADLibrary::ICADModelConverter> ModelConverter = GetModelConverter();

		ModelConverter->InitializeProcess();

		EAliasObjectReference ObjectReference = EAliasObjectReference::LocalReference;
		if (OutMeshParameters.bIsSymmetric)
		{
			// All actors of a Alias symmetric layer are defined in the world Reference i.e. they have identity transform. So Mesh actor has to be defined in the world reference.
			ObjectReference = EAliasObjectReference::WorldReference;
		}
		else if (WireSettings.bMergeGeometryByGroup)
		{
			// In the case of StitchingSew, AlDagNode children of a GroupNode are merged together. To be merged, they have to be defined in the reference of parent GroupNode.
			ObjectReference = EAliasObjectReference::ParentReference;
		}

		const FBodyNodeGeometry BodyNodeGeometry( (int32)ECADModelGeometryType::BodyNode, ObjectReference, BodyNode);
		ModelConverter->AddGeometry(BodyNodeGeometry);

		ModelConverter->RepairTopology();

		ModelConverter->SaveModel(*OutputPath, MeshElement);

		FMeshDescription MeshDescription;
		DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

		if (ModelConverter->Tessellate(OutMeshParameters, MeshDescription))
		{
			return TOptional<FMeshDescription>(MoveTemp(MeshDescription));
		}

		const TCHAR* StaticMeshLabel = MeshElement->GetLabel();
		const TCHAR* StaticMeshName = MeshElement->GetName();
		UE_LOG(LogWireInterface, Warning, TEXT("Failed to generate the mesh of \"%s\" (%s) StaticMesh."), StaticMeshLabel, StaticMeshName);

		return TOptional<FMeshDescription>();
	}

	TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescriptionFromPatchMesh(TSharedPtr<FPatchMesh>& PatchMesh, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters)
	{
		OutMeshParameters = OpenModelUtils::GetMeshParameters(PatchMesh->GetLayer());

		FMeshDescription MeshDescription;
		DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);
		MeshDescription.Empty();

		constexpr bool bMerge = true;
		PatchMesh->IterateOnMeshNodes([&](const TAlDagNodePtr<AlMeshNode>& MeshNode)
			{
				TAlObjectPtr<AlMesh> Mesh(MeshNode.GetMesh());
				if (Mesh)
				{
					AlMatrix4x4 AlMatrix;
					if (OutMeshParameters.bIsSymmetric)
					{
						MeshNode->globalTransformationMatrix(AlMatrix);
					}
					else
					{
						MeshNode->localTransformationMatrix(AlMatrix);
					}

					Mesh->transform(AlMatrix);

					TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = MeshElement->GetMaterialSlotAt(0);
					const FString SlotMaterialName = MaterialIDElement ? MaterialIDElement->GetName() : FString();

					OpenModelUtils::TransferAlMeshToMeshDescription(*Mesh, *SlotMaterialName, MeshDescription, OutMeshParameters, bMerge);
				}
			});

		// Build edge meta data
		FStaticMeshOperations::DetermineEdgeHardnessesFromVertexInstanceNormals(MeshDescription);

		return MoveTemp(MeshDescription);
	}

	// #wire_import: AlSurfaceNode can have trim regions. This should be handle at this stage
	TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescriptionFromParametricNode(const TAlDagNodePtr<AlDagNode>& DagNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters)
	{
		OutMeshParameters = OpenModelUtils::GetMeshParameters(DagNode.GetLayer());

		TSharedPtr<CADLibrary::ICADModelConverter> ModelConverter = GetModelConverter();

		ModelConverter->InitializeProcess();

		EAliasObjectReference ObjectReference = EAliasObjectReference::LocalReference;

		// All geometry are processed in world space if layers are converted to actors
		// All actors of a Alias symmetric layer are defined in the world Reference
		// i.e. they have identity transform. So Mesh actor has to be defined in the world reference.
		if (OutMeshParameters.bIsSymmetric)
		{
			ObjectReference = EAliasObjectReference::WorldReference;
		}

		ensure(MeshElement->GetMaterialSlotCount() == 1);

		const FDagNodeGeometry DagNodeGeometry( (int32)ECADModelGeometryType::DagNode, ObjectReference, DagNode );
		if (!ModelConverter->AddGeometry(DagNodeGeometry))
		{
			return TOptional<FMeshDescription>();
		}

		ModelConverter->RepairTopology();

		ModelConverter->SaveModel(*OutputPath, MeshElement);

		FMeshDescription MeshDescription;
		DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

		OutMeshParameters = OpenModelUtils::GetMeshParameters(*DagNode);

		if (ModelConverter->Tessellate(OutMeshParameters, MeshDescription))
		{
			return TOptional<FMeshDescription>(MoveTemp(MeshDescription));
		}

		const TCHAR* StaticMeshLabel = MeshElement->GetLabel();
		const TCHAR* StaticMeshName = MeshElement->GetName();
		UE_LOG(LogWireInterface, Warning, TEXT("Failed to generate the mesh of \"%s\" (%s) StaticMesh."), StaticMeshLabel, StaticMeshName);

		return TOptional<FMeshDescription>();
	}

	TOptional<FMeshDescription> FWireTranslatorImpl::GetMeshDescriptionFromMeshNode(const TAlDagNodePtr<AlMeshNode>& MeshNode, TSharedPtr<IDatasmithMeshElement> MeshElement, CADLibrary::FMeshParameters& OutMeshParameters)
	{
		if (!MeshNode)
		{
			return TOptional<FMeshDescription>();
		}

		TAlObjectPtr<AlMesh> Mesh(MeshNode->mesh());
		if (!Mesh)
		{
			return TOptional<FMeshDescription>();
		}

		OutMeshParameters = OpenModelUtils::GetMeshParameters(*MeshNode);

		if (OutMeshParameters.bIsSymmetric)
		{
			AlMatrix4x4 AlGlobalMatrix;
			MeshNode->globalTransformationMatrix(AlGlobalMatrix);
			Mesh->transform(AlGlobalMatrix);
		}

		FMeshDescription MeshDescription;
		DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

		TSharedPtr<IDatasmithMaterialIDElement> MaterialIDElement = MeshElement->GetMaterialSlotAt(0);
		const FString SlotMaterialName = MaterialIDElement ? MaterialIDElement->GetName() : FString();

		const bool bMerge = false;
		OpenModelUtils::TransferAlMeshToMeshDescription(*Mesh, *SlotMaterialName, MeshDescription, OutMeshParameters, bMerge);

		// Build edge meta data
		FStaticMeshOperations::DetermineEdgeHardnessesFromVertexInstanceNormals(MeshDescription);

		return MoveTemp(MeshDescription);
	}

	TSharedPtr<CADLibrary::ICADModelConverter> FWireTranslatorImpl::GetModelConverter() const
	{
		TSharedPtr<CADLibrary::ICADModelConverter> ModelConverter;

		CADLibrary::FImportParameters ImportParameters;
		if (CADLibrary::FImportParameters::bGDisableCADKernelTessellation)
		{
			ModelConverter = MakeShared<FAliasModelToTechSoftConverter>(ImportParameters);
		}
		else
		{
			ModelConverter = MakeShared<FAliasModelToCADKernelConverter>(WireSettings, ImportParameters);
		}

		if (ModelConverter)
		{
			ModelConverter->SetImportParameters(WireSettings.ChordTolerance, WireSettings.MaxEdgeLength, WireSettings.NormalTolerance, (CADLibrary::EStitchingTechnique)WireSettings.StitchingTechnique);
		}

		return ModelConverter;
	}

	// Material creation

	TSharedPtr<IDatasmithMaterialIDElement> FWireTranslatorImpl::FindOrAddMaterial(const TAlObjectPtr<AlShader>& Shader)
	{
		const FString ShaderName = Shader.GetName();

		if (TSharedPtr<IDatasmithBaseMaterialElement>* MaterialElementPtr = ShaderNameToMaterial.Find(ShaderName))
		{
			return FDatasmithSceneFactory::CreateMaterialId((*MaterialElementPtr)->GetName());
		}

		const FString ShaderModelName = Shader->shadingModel();

		//const FColor Color = CreateShaderColorFromShaderName(ShaderName);
		//ShaderNameToColor.Add(ShaderName, Color);

		TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement = FDatasmithSceneFactory::CreateUEPbrMaterial(*Shader.GetUniqueID(SHADER_TYPE));
		MaterialElement->SetLabel(*ShaderName);

		if (ShaderModelName.Equals(TEXT("BLINN")))
		{
			AddAlBlinnParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("LAMBERT")))
		{
			AddAlLambertParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("LIGHTSOURCE")))
		{
			AddAlLightSourceParameters(Shader, MaterialElement);
		}
		else if (ShaderModelName.Equals(TEXT("PHONG")))
		{
			AddAlPhongParameters(Shader, MaterialElement);
		}

		DatasmithScene->AddMaterial(MaterialElement);
		ShaderNameToMaterial.Add(*ShaderName, MaterialElement);

		return FDatasmithSceneFactory::CreateMaterialId(MaterialElement->GetName());
	}

	bool FWireTranslatorImpl::GetCommonParameters(int32 Field, double Value, FColor& Color, FColor& TransparencyColor, FColor& IncandescenceColor, double GlowIntensity)
	{
		switch (AlShadingFields(Field))
		{
		case AlShadingFields::kFLD_SHADING_COMMON_COLOR_R:
			Color.R = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_COLOR_G:
			Color.G = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_COLOR_B:
			Color.B = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_R:
			IncandescenceColor.R = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_G:
			IncandescenceColor.G = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_INCANDESCENCE_B:
			IncandescenceColor.B = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_R:
			TransparencyColor.R = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_G:
			TransparencyColor.G = (uint8)Value;
			return true;
		case  AlShadingFields::kFLD_SHADING_COMMON_TRANSPARENCY_B:
			TransparencyColor.B = (uint8)Value;
			return true;
		case AlShadingFields::kFLD_SHADING_COMMON_GLOW_INTENSITY:
			GlowIntensity = Value;
			return true;
		default:
			return false;
		}
	}

	void FWireTranslatorImpl::AddAlBlinnParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement)
	{
		// Default values for a Blinn material
		FColor Color(145, 148, 153);
		FColor TransparencyColor(0, 0, 0);
		FColor IncandescenceColor(0, 0, 0);
		FColor SpecularColor(38, 38, 38);
		double Diffuse = 1.0;
		double GlowIntensity = 0.0;
		double Gloss = 0.8;
		double Eccentricity = 0.35;
		double Specularity = 1.0;
		double Reflectivity = 0.5;
		double SpecularRolloff = 0.5;

		AlList* List = Shader->fields();
		for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
		{
			double Value = 0.0f;
			statusCode ErrorCode = Shader->parameter(Item->field(), Value);
			if (ErrorCode != 0)
			{
				continue;
			}

			if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
			{
				continue;
			}

			switch (Item->field())
			{
			case AlShadingFields::kFLD_SHADING_BLINN_DIFFUSE:
				Diffuse = Value;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_GLOSS_:
				Gloss = Value;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_R:
				SpecularColor.R = (uint8)(255.f * Value);
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_G:
				SpecularColor.G = (uint8)(255.f * Value);;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_B:
				SpecularColor.B = (uint8)(255.f * Value);;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_SPECULARITY_:
				Specularity = Value;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_SPECULAR_ROLLOFF:
				SpecularRolloff = Value;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_ECCENTRICITY:
				Eccentricity = Value;
				break;
			case AlShadingFields::kFLD_SHADING_BLINN_REFLECTIVITY:
				Reflectivity = Value;
				break;
			}
		}

		bool bIsTransparent = IsTransparent(TransparencyColor);

		// Construct parameter expressions
		IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseExpression->GetScalar() = Diffuse;
		DiffuseExpression->SetName(TEXT("Diffuse"));

		IDatasmithMaterialExpressionScalar* GlossExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlossExpression->GetScalar() = Gloss;
		GlossExpression->SetName(TEXT("Gloss"));

		IDatasmithMaterialExpressionColor* SpecularColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		SpecularColorExpression->SetName(TEXT("SpecularColor"));
		SpecularColorExpression->GetColor() = FLinearColor::FromSRGBColor(SpecularColor);

		IDatasmithMaterialExpressionScalar* SpecularityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		SpecularityExpression->GetScalar() = Specularity * 0.3;
		SpecularityExpression->SetName(TEXT("Specularity"));

		IDatasmithMaterialExpressionScalar* SpecularRolloffExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		SpecularRolloffExpression->GetScalar() = SpecularRolloff;
		SpecularRolloffExpression->SetName(TEXT("SpecularRolloff"));

		IDatasmithMaterialExpressionScalar* EccentricityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		EccentricityExpression->GetScalar() = Eccentricity;
		EccentricityExpression->SetName(TEXT("Eccentricity"));

		IDatasmithMaterialExpressionScalar* ReflectivityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ReflectivityExpression->GetScalar() = Reflectivity;
		ReflectivityExpression->SetName(TEXT("Reflectivity"));

		IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		ColorExpression->SetName(TEXT("Color"));
		ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

		IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
		IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

		IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
		TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

		IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlowIntensityExpression->GetScalar() = GlowIntensity;
		GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

		// Create aux expressions
		IDatasmithMaterialExpressionGeneric* ColorSpecLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ColorSpecLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* ColorSpecLerpValue = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ColorSpecLerpValue->GetScalar() = 0.96f;

		IDatasmithMaterialExpressionGeneric* ColorMetallicLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ColorMetallicLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpA->GetScalar() = 0.04f;

		IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpB->GetScalar() = 1.0f;

		IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorAdd->SetExpressionName(TEXT("Add"));

		IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		IncandescenceScale->GetScalar() = 100.0f;

		IDatasmithMaterialExpressionGeneric* EccentricityMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		EccentricityMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* EccentricityOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		EccentricityOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionGeneric* RoughnessOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		RoughnessOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionScalar* FresnelExponent = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		FresnelExponent->GetScalar() = 4.0f;

		IDatasmithMaterialExpressionFunctionCall* FresnelFunc = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
		FresnelFunc->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Fresnel_Function.Fresnel_Function"));

		IDatasmithMaterialExpressionGeneric* FresnelLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		FresnelLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* FresnelLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		FresnelLerpA->GetScalar() = 1.0f;

		IDatasmithMaterialExpressionScalar* SpecularPowerExp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		SpecularPowerExp->GetScalar() = 0.5f;

		IDatasmithMaterialExpressionGeneric* Power = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		Power->SetExpressionName(TEXT("Power"));

		IDatasmithMaterialExpressionGeneric* FresnelMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		FresnelMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
		IDatasmithMaterialExpressionGeneric* Divide = nullptr;
		IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
		if (bIsTransparent)
		{
			BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
			BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

			AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRG->SetExpressionName(TEXT("Add"));

			AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRGB->SetExpressionName(TEXT("Add"));

			Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			Divide->SetExpressionName(TEXT("Divide"));

			DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			DivideConstant->GetScalar() = 3.0f;
		}

		// Connect expressions
		SpecularColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(0));
		ColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(1));
		ColorSpecLerpValue->ConnectExpression(*ColorSpecLerp->GetInput(2));

		ColorExpression->ConnectExpression(*ColorMetallicLerp->GetInput(0));
		ColorSpecLerp->ConnectExpression(*ColorMetallicLerp->GetInput(1));
		GlossExpression->ConnectExpression(*ColorMetallicLerp->GetInput(2));

		DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
		DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
		DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

		ColorMetallicLerp->ConnectExpression(*BaseColorMultiply->GetInput(0));
		DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

		BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
		IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

		BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
		TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

		GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
		IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

		BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
		IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

		EccentricityExpression->ConnectExpression(*EccentricityOneMinus->GetInput(0));

		EccentricityOneMinus->ConnectExpression(*EccentricityMultiply->GetInput(0));
		SpecularityExpression->ConnectExpression(*EccentricityMultiply->GetInput(1));

		EccentricityMultiply->ConnectExpression(*RoughnessOneMinus->GetInput(0));

		FresnelExponent->ConnectExpression(*FresnelFunc->GetInput(3));

		SpecularRolloffExpression->ConnectExpression(*Power->GetInput(0));
		SpecularPowerExp->ConnectExpression(*Power->GetInput(1));

		FresnelLerpA->ConnectExpression(*FresnelLerp->GetInput(0));
		FresnelFunc->ConnectExpression(*FresnelLerp->GetInput(1));
		Power->ConnectExpression(*FresnelLerp->GetInput(2));

		FresnelLerp->ConnectExpression(*FresnelMultiply->GetInput(0));
		ReflectivityExpression->ConnectExpression(*FresnelMultiply->GetInput(1));

		TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

		if (bIsTransparent)
		{
			TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

			BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
			BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

			AddRG->ConnectExpression(*AddRGB->GetInput(0));
			BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

			AddRGB->ConnectExpression(*Divide->GetInput(0));
			DivideConstant->ConnectExpression(*Divide->GetInput(1));
		}

		// Connect material outputs
		MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
		MaterialElement->GetMetallic().SetExpression(GlossExpression);
		MaterialElement->GetSpecular().SetExpression(FresnelMultiply);
		MaterialElement->GetRoughness().SetExpression(RoughnessOneMinus);
		MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);

		if (bIsTransparent)
		{
			MaterialElement->GetOpacity().SetExpression(Divide);
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasBlinnTransparent"));
		}
		else
		{
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasBlinn"));
		}

	}

	void FWireTranslatorImpl::AddAlLambertParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement)
	{
		// Default values for a Lambert material
		FColor Color(145, 148, 153);
		FColor TransparencyColor(0, 0, 0);
		FColor IncandescenceColor(0, 0, 0);
		double Diffuse = 1.0;
		double GlowIntensity = 0.0;

		AlList* List = Shader->fields();
		for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
		{
			double Value = 0.0f;
			statusCode ErrorCode = Shader->parameter(Item->field(), Value);
			if (ErrorCode != 0)
			{
				continue;
			}

			if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
			{
				continue;
			}

			switch (Item->field())
			{
			case AlShadingFields::kFLD_SHADING_LAMBERT_DIFFUSE:
				Diffuse = Value;
				break;
			}
		}

		bool bIsTransparent = IsTransparent(TransparencyColor);

		// Construct parameter expressions
		IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseExpression->GetScalar() = Diffuse;
		DiffuseExpression->SetName(TEXT("Diffuse"));

		IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		ColorExpression->SetName(TEXT("Color"));
		ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

		IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
		IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

		IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
		TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

		IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlowIntensityExpression->GetScalar() = GlowIntensity;
		GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

		// Create aux expressions
		IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpA->GetScalar() = 0.04f;

		IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpB->GetScalar() = 1.0f;

		IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorAdd->SetExpressionName(TEXT("Add"));

		IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		IncandescenceScale->GetScalar() = 100.0f;

		IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
		IDatasmithMaterialExpressionGeneric* Divide = nullptr;
		IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
		if (bIsTransparent)
		{
			BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
			BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

			AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRG->SetExpressionName(TEXT("Add"));

			AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRGB->SetExpressionName(TEXT("Add"));

			Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			Divide->SetExpressionName(TEXT("Divide"));

			DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			DivideConstant->GetScalar() = 3.0f;
		}

		// Connect expressions
		DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
		DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
		DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

		ColorExpression->ConnectExpression(*BaseColorMultiply->GetInput(0));
		DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

		BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
		IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

		BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
		TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

		GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
		IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

		BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
		IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

		TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

		if (bIsTransparent)
		{
			TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

			BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
			BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

			AddRG->ConnectExpression(*AddRGB->GetInput(0));
			BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

			AddRGB->ConnectExpression(*Divide->GetInput(0));
			DivideConstant->ConnectExpression(*Divide->GetInput(1));
		}

		// Connect material outputs
		MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
		MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);
		if (bIsTransparent)
		{
			MaterialElement->GetOpacity().SetExpression(Divide);
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLambertTransparent"));
		}
		else {
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLambert"));
		}

	}

	void FWireTranslatorImpl::AddAlLightSourceParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement)
	{
		// Default values for a LightSource material
		FColor Color(145, 148, 153);
		FColor TransparencyColor(0, 0, 0);
		FColor IncandescenceColor(0, 0, 0);
		double GlowIntensity = 0.0;

		AlList* List = Shader->fields();
		for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
		{
			double Value = 0.0f;
			statusCode ErrorCode = Shader->parameter(Item->field(), Value);
			if (ErrorCode != 0)
			{
				continue;
			}

			GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity);
		}

		bool bIsTransparent = IsTransparent(TransparencyColor);

		// Construct parameter expressions
		IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		ColorExpression->SetName(TEXT("Color"));
		ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

		IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
		IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

		IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
		TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

		IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlowIntensityExpression->GetScalar() = GlowIntensity;
		GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

		// Create aux expressions
		IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorAdd->SetExpressionName(TEXT("Add"));

		IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		IncandescenceScale->GetScalar() = 100.0f;

		IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
		IDatasmithMaterialExpressionGeneric* Divide = nullptr;
		IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
		if (bIsTransparent)
		{
			BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
			BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

			AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRG->SetExpressionName(TEXT("Add"));

			AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRGB->SetExpressionName(TEXT("Add"));

			Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			Divide->SetExpressionName(TEXT("Divide"));

			DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			DivideConstant->GetScalar() = 3.0f;
		}

		// Connect expressions
		ColorExpression->ConnectExpression(*BaseColorAdd->GetInput(0));
		IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

		BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
		TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

		GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
		IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

		BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
		IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

		TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

		if (bIsTransparent)
		{
			TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

			BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
			BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

			AddRG->ConnectExpression(*AddRGB->GetInput(0));
			BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

			AddRGB->ConnectExpression(*Divide->GetInput(0));
			DivideConstant->ConnectExpression(*Divide->GetInput(1));
		}

		// Connect material outputs
		MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
		MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);

		if (bIsTransparent)
		{
			MaterialElement->GetOpacity().SetExpression(Divide);
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLightSourceTransparent"));
		}
		else {
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasLightSource"));
		}
	}

	void FWireTranslatorImpl::AddAlPhongParameters(const TAlObjectPtr<AlShader>& Shader, TSharedPtr<IDatasmithUEPbrMaterialElement> MaterialElement)
	{
		// Default values for a Phong material
		FColor Color(145, 148, 153);
		FColor TransparencyColor(0, 0, 0);
		FColor IncandescenceColor(0, 0, 0);
		FColor SpecularColor(38, 38, 38);
		double Diffuse = 1.0;
		double GlowIntensity = 0.0;
		double Gloss = 0.8;
		double Shinyness = 20.0;
		double Specularity = 1.0;
		double Reflectivity = 0.5;

		AlList* List = Shader->fields();
		for (AlShadingFieldItem* Item = static_cast<AlShadingFieldItem*>(List->first()); Item; Item = Item->nextField())
		{
			double Value = 0.0f;
			statusCode ErrorCode = Shader->parameter(Item->field(), Value);
			if (ErrorCode != 0)
			{
				continue;
			}

			if (GetCommonParameters(Item->field(), Value, Color, TransparencyColor, IncandescenceColor, GlowIntensity))
			{
				continue;
			}

			switch (Item->field())
			{
			case AlShadingFields::kFLD_SHADING_PHONG_DIFFUSE:
				Diffuse = Value;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_GLOSS_:
				Gloss = Value;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_R:
				SpecularColor.R = (uint8)(255.f * Value);;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_G:
				SpecularColor.G = (uint8)(255.f * Value);;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_SPECULAR_B:
				SpecularColor.B = (uint8)(255.f * Value);;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_SPECULARITY_:
				Specularity = Value;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_SHINYNESS:
				Shinyness = Value;
				break;
			case AlShadingFields::kFLD_SHADING_PHONG_REFLECTIVITY:
				Reflectivity = Value;
				break;
			}
		}

		bool bIsTransparent = IsTransparent(TransparencyColor);

		// Construct parameter expressions
		IDatasmithMaterialExpressionScalar* DiffuseExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseExpression->GetScalar() = Diffuse;
		DiffuseExpression->SetName(TEXT("Diffuse"));

		IDatasmithMaterialExpressionScalar* GlossExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlossExpression->GetScalar() = Gloss;
		GlossExpression->SetName(TEXT("Gloss"));

		IDatasmithMaterialExpressionColor* SpecularColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		SpecularColorExpression->SetName(TEXT("SpecularColor"));
		SpecularColorExpression->GetColor() = FLinearColor::FromSRGBColor(SpecularColor);

		IDatasmithMaterialExpressionScalar* SpecularityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		SpecularityExpression->GetScalar() = Specularity * 0.3;
		SpecularityExpression->SetName(TEXT("Specularity"));

		IDatasmithMaterialExpressionScalar* ShinynessExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ShinynessExpression->GetScalar() = Shinyness;
		ShinynessExpression->SetName(TEXT("Shinyness"));

		IDatasmithMaterialExpressionScalar* ReflectivityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ReflectivityExpression->GetScalar() = Reflectivity;
		ReflectivityExpression->SetName(TEXT("Reflectivity"));

		IDatasmithMaterialExpressionColor* ColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		ColorExpression->SetName(TEXT("Color"));
		ColorExpression->GetColor() = FLinearColor::FromSRGBColor(Color);

		IDatasmithMaterialExpressionColor* IncandescenceColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		IncandescenceColorExpression->SetName(TEXT("IncandescenceColor"));
		IncandescenceColorExpression->GetColor() = FLinearColor::FromSRGBColor(IncandescenceColor);

		IDatasmithMaterialExpressionColor* TransparencyColorExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionColor>();
		TransparencyColorExpression->SetName(TEXT("TransparencyColor"));
		TransparencyColorExpression->GetColor() = FLinearColor::FromSRGBColor(TransparencyColor);

		IDatasmithMaterialExpressionScalar* GlowIntensityExpression = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		GlowIntensityExpression->GetScalar() = GlowIntensity;
		GlowIntensityExpression->SetName(TEXT("GlowIntensity"));

		// Create aux expressions
		IDatasmithMaterialExpressionGeneric* ColorSpecLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ColorSpecLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* ColorSpecLerpValue = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ColorSpecLerpValue->GetScalar() = 0.96f;

		IDatasmithMaterialExpressionGeneric* ColorMetallicLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ColorMetallicLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionGeneric* DiffuseLerp = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		DiffuseLerp->SetExpressionName(TEXT("LinearInterpolate"));

		IDatasmithMaterialExpressionScalar* DiffuseLerpA = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpA->GetScalar() = 0.04f;

		IDatasmithMaterialExpressionScalar* DiffuseLerpB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		DiffuseLerpB->GetScalar() = 1.0f;

		IDatasmithMaterialExpressionGeneric* BaseColorMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* BaseColorAdd = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorAdd->SetExpressionName(TEXT("Add"));

		IDatasmithMaterialExpressionGeneric* BaseColorTransparencyMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		BaseColorTransparencyMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* IncandescenceScaleMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		IncandescenceScaleMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionScalar* IncandescenceScale = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		IncandescenceScale->GetScalar() = 100.0f;

		IDatasmithMaterialExpressionGeneric* ShinynessSubtract = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ShinynessSubtract->SetExpressionName(TEXT("Subtract"));

		IDatasmithMaterialExpressionScalar* ShinynessSubtract2 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ShinynessSubtract2->GetScalar() = 2.0f;

		IDatasmithMaterialExpressionGeneric* ShinynessDivide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		ShinynessDivide->SetExpressionName(TEXT("Divide"));

		IDatasmithMaterialExpressionScalar* ShinynessDivide98 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
		ShinynessDivide98->GetScalar() = 98.0f;

		IDatasmithMaterialExpressionGeneric* SpecularityMultiply = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		SpecularityMultiply->SetExpressionName(TEXT("Multiply"));

		IDatasmithMaterialExpressionGeneric* RoughnessOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		RoughnessOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionGeneric* TransparencyOneMinus = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
		TransparencyOneMinus->SetExpressionName(TEXT("OneMinus"));

		IDatasmithMaterialExpressionFunctionCall* BreakFloat3 = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRG = nullptr;
		IDatasmithMaterialExpressionGeneric* AddRGB = nullptr;
		IDatasmithMaterialExpressionGeneric* Divide = nullptr;
		IDatasmithMaterialExpressionScalar* DivideConstant = nullptr;
		if (bIsTransparent)
		{
			BreakFloat3 = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionFunctionCall>();
			BreakFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakFloat3Components.BreakFloat3Components"));

			AddRG = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRG->SetExpressionName(TEXT("Add"));

			AddRGB = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			AddRGB->SetExpressionName(TEXT("Add"));

			Divide = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionGeneric>();
			Divide->SetExpressionName(TEXT("Divide"));

			DivideConstant = MaterialElement->AddMaterialExpression<IDatasmithMaterialExpressionScalar>();
			DivideConstant->GetScalar() = 3.0f;
		}

		// Connect expressions
		SpecularColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(0));
		ColorExpression->ConnectExpression(*ColorSpecLerp->GetInput(1));
		ColorSpecLerpValue->ConnectExpression(*ColorSpecLerp->GetInput(2));

		ColorExpression->ConnectExpression(*ColorMetallicLerp->GetInput(0));
		ColorSpecLerp->ConnectExpression(*ColorMetallicLerp->GetInput(1));
		GlossExpression->ConnectExpression(*ColorMetallicLerp->GetInput(2));

		DiffuseLerpA->ConnectExpression(*DiffuseLerp->GetInput(0));
		DiffuseLerpB->ConnectExpression(*DiffuseLerp->GetInput(1));
		DiffuseExpression->ConnectExpression(*DiffuseLerp->GetInput(2));

		ColorMetallicLerp->ConnectExpression(*BaseColorMultiply->GetInput(0));
		DiffuseLerp->ConnectExpression(*BaseColorMultiply->GetInput(1));

		BaseColorMultiply->ConnectExpression(*BaseColorAdd->GetInput(0));
		IncandescenceColorExpression->ConnectExpression(*BaseColorAdd->GetInput(1));

		BaseColorAdd->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(0));
		TransparencyOneMinus->ConnectExpression(*BaseColorTransparencyMultiply->GetInput(1));

		GlowIntensityExpression->ConnectExpression(*IncandescenceScaleMultiply->GetInput(0));
		IncandescenceScale->ConnectExpression(*IncandescenceScaleMultiply->GetInput(1));

		BaseColorTransparencyMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(0));
		IncandescenceScaleMultiply->ConnectExpression(*IncandescenceMultiply->GetInput(1));

		ShinynessExpression->ConnectExpression(*ShinynessSubtract->GetInput(0));
		ShinynessSubtract2->ConnectExpression(*ShinynessSubtract->GetInput(1));

		ShinynessSubtract->ConnectExpression(*ShinynessDivide->GetInput(0));
		ShinynessDivide98->ConnectExpression(*ShinynessDivide->GetInput(1));

		ShinynessDivide->ConnectExpression(*SpecularityMultiply->GetInput(0));
		SpecularityExpression->ConnectExpression(*SpecularityMultiply->GetInput(1));

		SpecularityMultiply->ConnectExpression(*RoughnessOneMinus->GetInput(0));

		TransparencyColorExpression->ConnectExpression(*TransparencyOneMinus->GetInput(0));

		if (bIsTransparent)
		{
			TransparencyOneMinus->ConnectExpression(*BreakFloat3->GetInput(0));

			BreakFloat3->ConnectExpression(*AddRG->GetInput(0), 0);
			BreakFloat3->ConnectExpression(*AddRG->GetInput(1), 1);

			AddRG->ConnectExpression(*AddRGB->GetInput(0));
			BreakFloat3->ConnectExpression(*AddRGB->GetInput(1), 2);

			AddRGB->ConnectExpression(*Divide->GetInput(0));
			DivideConstant->ConnectExpression(*Divide->GetInput(1));
		}

		// Connect material outputs
		MaterialElement->GetBaseColor().SetExpression(BaseColorTransparencyMultiply);
		MaterialElement->GetMetallic().SetExpression(GlossExpression);
		MaterialElement->GetSpecular().SetExpression(ReflectivityExpression);
		MaterialElement->GetRoughness().SetExpression(RoughnessOneMinus);
		MaterialElement->GetEmissiveColor().SetExpression(IncandescenceMultiply);
		if (bIsTransparent)
		{
			MaterialElement->GetOpacity().SetExpression(Divide);
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasPhongTransparent"));
		}
		else {
			MaterialElement->SetParentLabel(TEXT("M_DatasmithAliasPhong"));
		}
	}

	class FWireInterfaceModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			uint64 AliasVersion = IWireInterface::GetRequiredAliasVersion();

#ifdef OPEN_MODEL_2020
			// Check installed version of Alias Tools because binaries before 2021.3 are not compatible with Alias 2022
			if (LibAlias2020_Version < AliasVersion && AliasVersion < LibAlias2021_Version)
			{
				static const bool bIsDisplay = []() -> bool
					{
						UE_LOG(LogWireInterface, Warning, TEXT(WRONG_VERSION_TEXT));
						return true;
					}();
				return;
			}
#endif

			if (LibAliasVersionMin <= AliasVersion && AliasVersion < LibAliasVersionMax)
			{
				auto MakeInterfaceFunc = []() -> TSharedPtr<IWireInterface>
					{
						return MakeShared<FWireTranslatorImpl>();
					};
				IWireInterface::RegisterInterface(UE_OPENMODEL_MAJOR_VERSION, UE_OPENMODEL_MAJOR_VERSION, MoveTemp(MakeInterfaceFunc));
			}
		}

		virtual void ShutdownModule() override
		{
		}
	};

#if TRACK_MESHELEMENT
#pragma optimize("", off)
	void MakeMeshVisible(FMeshDescription& MeshDescription)
	{
		using namespace UE::Geometry;

		TVertexAttributesRef<FVector3f> VertexPositions = MeshDescription.GetVertexPositions();
		
		TArrayView<FVector3f> Positions = VertexPositions.GetRawArray();
		TOrientedBox3<float> OBox = FitOrientedBox3Points<float>(Positions);
		
		FVertexArray& Vertices = MeshDescription.Vertices();
		const TMatrix3<float> Matrix = OBox.Frame.Rotation.ToRotationMatrix();

		constexpr float MinSize = 1.f;
		constexpr float MaxSize = 20.f;

		const float ScaleX = OBox.Extents.X < MinSize ? 2.f / OBox.Extents.X : OBox.Extents.X > MaxSize ? MaxSize / OBox.Extents.X : 1.f;
		const float ScaleY = OBox.Extents.Y < MinSize ? 2.f / OBox.Extents.Y : OBox.Extents.Y > MaxSize ? MaxSize / OBox.Extents.Y : 1.f;
		const float ScaleZ = OBox.Extents.Z < MinSize ? 2.f / OBox.Extents.Z : OBox.Extents.Z > MaxSize ? MaxSize / OBox.Extents.Z : 1.f;
		UE_LOG(LogWireInterface, Warning, TEXT("Scaling factor: %.3f %.3f %.3f"), ScaleX, ScaleY, ScaleZ);
		const FVector3f AxisX = OBox.AxisX();
		const FVector3f AxisY = OBox.AxisY();
		const FVector3f AxisZ = OBox.AxisZ();

		for (const FVertexID& VertexID : MeshDescription.Vertices().GetElementIDs())
		{
			FVector3f P = VertexPositions[VertexID] - OBox.Frame.Origin;
			VertexPositions[VertexID] = ((P | AxisX) * ScaleX) * AxisX + ((P | AxisY) * ScaleY) * AxisY + ((P | AxisZ) * ScaleZ) * AxisZ;
		}
	}
#endif

} // namespace

#undef LOCTEXT_NAMESPACE // "DatasmithWireTranslator"

#else // USE_OPENMODEL
namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{
	class FWireInterfaceModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
		}

		virtual void ShutdownModule() override
		{
		}
	};
}
#endif

// need this macro wrapper to expand the UE_DATASMITHWIRETRANSLATOR_MODULE_NAME macro and not create symbols with "UE_DATASMITHWIRETRANSLATOR_MODULE_NAME" in the token
#define IMPLEMENT_MODULE_WRAPPER(ModuleName) IMPLEMENT_MODULE(UE_DATASMITHWIRETRANSLATOR_NAMESPACE::FWireInterfaceModule, ModuleName);
IMPLEMENT_MODULE_WRAPPER(UE_DATASMITHWIRETRANSLATOR_MODULE_NAME)
