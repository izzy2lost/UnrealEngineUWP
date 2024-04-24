// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifdef USE_OPENMODEL
//#include "CADModelToTechSoftConverterBase.h"
#include "CADModelConverter.h"
#include "CADOptions.h"


#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "AlShadingFields.h"
#include "AlDagNode.h"
#include "AlMeshNode.h"
#include "AlShellNode.h"
#include "AlSurfaceNode.h"
#include "AlPersistentID.h"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class IDatasmithActorElement;

struct FMeshDescription;

#define LAYER_TYPE          TEXT("Layer")
#define GROUPNODE_TYPE      TEXT("GroupNode")
#define MESH_TYPE           TEXT("Mesh")
#define MESHNODE_TYPE       TEXT("MeshNode")
#define SHADER_TYPE         TEXT("Shader")
#define SHELLNODE_TYPE      TEXT("ShellNode")
#define SHELL_TYPE          TEXT("Shell")
#define SURFACE_TYPE        TEXT("Surface")
#define SURFACENODE_TYPE    TEXT("SurfaceNode")

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{
	typedef double AlMatrix4x4[4][4];

	enum class ETesselatorType : uint8
	{
		Fast,
		Accurate,
	};

	enum class EAlShaderModelType : uint8
	{
		BLINN,
		LAMBERT,
		LIGHTSOURCE,
		PHONG,
	};

	template<typename T>
	class TAlObjectPtr
	{
	public:
		TAlObjectPtr(T* AlObject = nullptr) : Ptr(AlObject) {}

		bool IsValid() const { return Ptr && AlIsValid(Ptr); }

		T* operator->() const
		{
			return Ptr;
		}

		T& operator*() const
		{
			return *Ptr;
		}

		T* Get() const
		{
			return Ptr;
		}

		FString GetName() const
		{
			return Ptr ? StringCast<TCHAR>(Ptr->name()).Get() : FString();
		}

		uint32 GetHash() const
		{
			return GetTypeHash(FString(StringCast<TCHAR>(Ptr->name()).Get()));
		}

		FString GetUniqueID(const TCHAR* TypeName = TEXT("Object")) const
		{
			return FString(TypeName) + FString::FromInt(GetHash());
		}

		TAlObjectPtr<T> operator=(const TAlObjectPtr<T>& Src)
		{
			Ptr = Src.IsValid() ? Src.Get() : nullptr;
			return *this;
		}

		operator bool() const
		{
			return IsValid();
		}

		friend uint32 GetTypeHash(const TAlObjectPtr<T>& Object)
		{
			return Object.GetHash()/*GetTypeHash(DateTime.Ticks)*/;
		}

	protected:
		T* Ptr = nullptr;
	};

	template<typename T>
	inline bool operator==(const TAlObjectPtr<T>& A, const TAlObjectPtr<T>& B)
	{
		return (bool)AlAreEqual((const AlObject*)A.Get(), (const AlObject*)B.Get());
	}

	template<typename T>
	inline bool operator!=(const TAlObjectPtr<T>& A, const TAlObjectPtr<T>& B)
	{
		return !((bool)AlAreEqual((const AlObject*)A.Get(), (const AlObject*)B.Get()));
	}

	template<>
	uint32 TAlObjectPtr<AlLayer>::GetHash() const;

	template<typename T>
	class TAlDagNodePtr : public TAlObjectPtr<T>
	{
	public:
		TAlDagNodePtr(T* AlDagNode = nullptr) : TAlObjectPtr<T>(AlDagNode) {}

		FString GetLayerName() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			T* LocalPtr = TAlObjectPtr<T>::Ptr;
			return LocalPtr && LocalPtr->layer() ? StringCast<FString,const char*>(LocalPtr->layer()->name()) : FString();
		}

		TAlObjectPtr<AlLayer> GetLayer() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			T* LocalPtr = TAlObjectPtr<T>::Ptr;
			return TAlObjectPtr<AlLayer>(LocalPtr && LocalPtr->layer() ? LocalPtr->layer() : nullptr);
		}

		bool HasSymmetry() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			T* LocalPtr = TAlObjectPtr<T>::Ptr;
			return (AlIsValid(LocalPtr) && LocalPtr->layer()) ? (bool)LocalPtr->layer()->isSymmetric() : false;
		}

		TAlObjectPtr<AlMesh> GetMesh() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			AlMeshNode* MeshNode = ((AlDagNode*)TAlObjectPtr<T>::Ptr)->asMeshNodePtr();
			return MeshNode ? TAlObjectPtr<AlMesh>(MeshNode->mesh()) : TAlObjectPtr<AlMesh>();
		}

		TAlObjectPtr<AlSurface> GetSurface() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			AlSurfaceNode* SurfaceNode = ((AlDagNode*)TAlObjectPtr<T>::Ptr)->asSurfaceNodePtr();
			return SurfaceNode ? TAlObjectPtr<AlSurface>(SurfaceNode->surface()) : TAlObjectPtr<AlSurface>();
		}

		TAlObjectPtr<AlShell> GetShell() const
		{
			static_assert(std::is_base_of<AlDagNode, T>::value);
			AlShellNode* ShellNode = ((AlDagNode*)TAlObjectPtr<T>::Ptr)->asShellNodePtr();
			return ShellNode ? TAlObjectPtr<AlShell>(ShellNode->shell()) : TAlObjectPtr<AlShell>();
		}
	};

	class FPatchMesh
	{
	public:
		FPatchMesh(const FString& InName, const TAlObjectPtr<AlLayer>& InLayer, int32 Count)
			: Name(InName)
			, Layer(InLayer)
		{
			MeshNodes.Reserve(Count);
		}

		bool HasContent()
		{
			return Initialize() && !MeshNodes.IsEmpty();
		}

		bool HasSingleContent() const
		{
			return MeshNodes.Num() == 1;
		}

		TAlDagNodePtr<AlMeshNode> GetSingleContent() const
		{
			if (MeshNodes.Num() == 1)
			{
				return MeshNodes[0];
			}

			return TAlDagNodePtr<AlMeshNode>();
		}

		const FString& GetName() const { return Name; }

		uint32 GetHash() const { return Hash; }

		const FString& GetUniqueID() const { return UniqueID; }

		const TAlObjectPtr<AlLayer> GetLayer() const { return Layer; }

		void AddMeshNode(const TAlDagNodePtr<AlMeshNode>& MeshNode)
		{
			ensure(Layer == MeshNode.GetLayer());
			ensure(MeshNode->mesh());
			MeshNodes.Add(MeshNode);
		}

		void IterateOnMeshNodes(const TFunction<void(const TAlDagNodePtr<AlMeshNode>& MeshNode)>& Callback) const
		{
			for (const TAlDagNodePtr<AlMeshNode>& MeshNode : MeshNodes)
			{
				Callback(MeshNode);
			}
		}

		bool Initialize();

	private:
		FString Name;
		TArray<TAlDagNodePtr<AlMeshNode>> MeshNodes;
		TAlObjectPtr<AlLayer> Layer;
		uint32 Hash;
		FString UniqueID;
		bool bInitialized = false;
	};

	class FBodyNode
	{
	public:
		FBodyNode(const FString& InName, const TAlObjectPtr<AlLayer>& InLayer, int32 Count)
			: Name(InName)
			, Layer(InLayer)
		{
			SurfaceNodes.Reserve(Count);
			ShellNodes.Reserve(Count);
		}

		bool HasContent()
		{
			return Initialize() && (!SurfaceNodes.IsEmpty() || !ShellNodes.IsEmpty());
		}
		 
		bool HasSingleContent() const
		{
			return (SurfaceNodes.Num() + ShellNodes.Num()) == 1;
		}

		TAlDagNodePtr<AlDagNode> GetSingleContent() const
		{
			if (!HasSingleContent())
			{
				return TAlDagNodePtr<AlDagNode>();
			}

			if (SurfaceNodes.Num() == 1)
			{
				return TAlDagNodePtr<AlDagNode>(SurfaceNodes[0].Get());
			}

			if (ShellNodes.Num() == 1)
			{
				return TAlDagNodePtr<AlDagNode>(ShellNodes[0].Get());
			}

			return TAlDagNodePtr<AlDagNode>();
		}

		const FString& GetName() const { return Name; }

		uint32 GetHash() const { return Hash; }

		const FString& GetUniqueID() const { return UniqueID; }

		const TAlObjectPtr<AlLayer> GetLayer() const { return Layer; }

		void AddSurfaceNode(const TAlDagNodePtr<AlSurfaceNode>& SurfaceNode);

		void AddShellNode(const TAlDagNodePtr<AlShellNode>& ShellNode);

		void IterateOnSurfaceNodes(const TFunction<void(const TAlDagNodePtr<AlSurfaceNode>& SurfaceNode)>& Callback) const
		{
			for (const TAlDagNodePtr<AlSurfaceNode>& SurfaceNode : SurfaceNodes)
			{
				Callback(SurfaceNode);
			}
		}

		void IterateOnShellNodes(const TFunction<void(const TAlDagNodePtr<AlShellNode>& ShellNode)>& Callback) const
		{
			for (const TAlDagNodePtr<AlShellNode>& ShellNode : ShellNodes)
			{
				Callback(ShellNode);
			}
		}

		void IterateOnSlotIndices(const TFunction<void(int SlotIndex, const TAlObjectPtr<AlShader>& Shader)>& Callback) const
		{
			for (const TPair<int, TAlObjectPtr<AlShader>>& Entry : SlotIndexToShader)
			{
				Callback(Entry.Key, Entry.Value);
			}
		}

		bool Initialize();

		int32 GetSlotIndex(const TAlDagNodePtr<AlDagNode>& DagNode);

	private:
		FString Name;
		TArray<TAlDagNodePtr<AlSurfaceNode>> SurfaceNodes;
		TArray<TAlDagNodePtr<AlShellNode>> ShellNodes;
		TAlObjectPtr<AlLayer> Layer;
		TMap<FString, int> ShaderNameToSlotIndex;
		TMap<int, TAlObjectPtr<AlShader>> SlotIndexToShader;
		uint32 Hash;
		FString UniqueID;
		bool bInitialized = false;
	};

	enum class ECADModelGeometryType : int32
	{
		DagNode,
		MeshNode,
		BodyNode,
		PatchMesh,
	};

	enum class EAliasObjectReference
	{
		LocalReference,  
		ParentReference, 
		WorldReference, 
	};
	
	struct FAliasGeometry : public CADLibrary::FCADModelGeometry
	{
		EAliasObjectReference Reference = EAliasObjectReference::LocalReference;
	};

	struct FDagNodeGeometry : public FAliasGeometry
	{
		TAlDagNodePtr<AlDagNode> DagNode;

		FDagNodeGeometry(int32 InType, EAliasObjectReference InReference, const TAlDagNodePtr<AlDagNode>& InDagNode)
		{
			Type = InType;
			Reference = InReference;
			DagNode = InDagNode;
		}
	};

	struct FBodyNodeGeometry : public FAliasGeometry
	{
		TSharedPtr<FBodyNode> BodyNode;

		FBodyNodeGeometry(int32 InType, EAliasObjectReference InReference, const TSharedPtr<FBodyNode>& InBodyNode)
		{
			Type = InType;
			Reference = InReference;
			BodyNode = InBodyNode;
		}
	};

	namespace OpenModelUtils
	{
		/** Following layer hierarchy, get list of layers an actor would be in as a csv string*/
		bool GetCsvLayerString(const TAlObjectPtr<AlLayer>& Layer, FString& CsvString);

		bool ActorHasContent(const TSharedPtr<IDatasmithActorElement>& ActorElement);

		void SetActorTransform(IDatasmithActorElement& OutActorElement, const TAlDagNodePtr<AlDagNode>& InDagNode);

		bool IsValidActor(const TSharedPtr<IDatasmithActorElement>& ActorElement);

		inline FString UuidToString(const uint32& Uuid)
		{
			return FString::Printf(TEXT("0x%08x"), Uuid);
		}

		inline uint32 GetTypeHash(AlPersistentID& GroupNodeId)
		{
			int IdA, IdB, IdC, IdD;
			GroupNodeId.id(IdA, IdB, IdC, IdD);
			return HashCombine(IdA, HashCombine(IdB, HashCombine(IdC, IdD)));
		}

		inline uint32 GetAlDagNodeUuid(AlDagNode& DagNode)
		{
			if (DagNode.hasPersistentID() == sSuccess)
			{
				AlPersistentID* PersistentID;
				DagNode.persistentID(PersistentID);
				return GetTypeHash(*PersistentID);
			}
			FString Label(StringCast<TCHAR>(DagNode.name()).Get());
			return GetTypeHash(Label);
		}

		bool TransferAlMeshToMeshDescription(const AlMesh& Mesh, const TCHAR* SlotMaterialName, FMeshDescription& MeshDescription, CADLibrary::FMeshParameters& SymmetricParameters, const bool bMerge = false);

		TAlDagNodePtr<AlMeshNode> TesselateDagLeaf(const AlDagNode& DagLeaf, ETesselatorType TessType, double Tolerance);

		/** Returns true if DagNode is supported and its content is valid */
		bool IsDagNodeValid(const TAlDagNodePtr<AlDagNode>& DagNode);

		/** Returns true if geometry node is supported and its content is valid */
		bool IsGeometryValid(const TAlDagNodePtr<AlDagNode>& GeomNode);

		CADLibrary::FMeshParameters GetMeshParameters(AlDagNode& DagNode);
		CADLibrary::FMeshParameters GetMeshParameters(const TAlObjectPtr<AlLayer>& Layer);
	}

}

#endif


