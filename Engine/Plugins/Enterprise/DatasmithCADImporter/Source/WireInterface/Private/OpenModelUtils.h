// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

//#include "CADModelToTechSoftConverterBase.h"
#include "CADModelConverter.h"
#include "CADOptions.h"

#ifdef USE_OPENMODEL

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "AlShadingFields.h"
#include "AlDagNode.h"
#include "AlPersistentID.h"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class IDatasmithActorElement;
class AlMesh;

struct FMeshDescription;

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{
	//enum class EAliasObjectReference : uint8
	//{
	//	LocalReference,
	//	ParentReference,
	//	WorldReference,
	//};

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

		TAlObjectPtr<T> operator=(TAlObjectPtr<T> Src)
		{
			Ptr = Src.IsValid() ? Src.Get() : nullptr;
			return *this;
		}

		operator bool() const
		{
			return IsValid();
		}

	private:
		T* Ptr = nullptr;
	};

	namespace OpenModelUtils
	{
		void SetActorTransform(TSharedPtr<IDatasmithActorElement>& OutActorElement, const AlDagNode& InDagNode);

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
			FString Label = UTF8_TO_TCHAR(DagNode.name());
			return GetTypeHash(Label);
		}

		bool TransferAlMeshToMeshDescription(const AlMesh& Mesh, const TCHAR* SlotMaterialName, FMeshDescription& MeshDescription, CADLibrary::FMeshParameters& SymmetricParameters, const bool bMerge = false);

		TAlObjectPtr<AlDagNode> TesselateDagLeaf(const AlDagNode& DagLeaf, ETesselatorType TessType, double Tolerance);
	}

}

#endif


