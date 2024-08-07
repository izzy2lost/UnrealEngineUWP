// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDInfoCache.h"

#include "USDGeomMeshConversion.h"
#include "USDLog.h"
#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Async/ParallelFor.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/pcp/layerStack.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/collectionAPI.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primCompositionQuery.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/pointInstancer.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/subset.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/root.h"
#include "USDIncludesEnd.h"
#endif	  // USE_USD_SDK

static int32 GMaxNumVerticesCollapsedMesh = 5000000;
static FAutoConsoleVariableRef CVarMaxNumVerticesCollapsedMesh(
	TEXT("USD.MaxNumVerticesCollapsedMesh"),
	GMaxNumVerticesCollapsedMesh,
	TEXT("Maximum number of vertices that a combined Mesh can have for us to collapse it into a single StaticMesh")
);

// Can toggle on/off to compare performance with StaticMesh instead of GeometryCache
static bool GUseGeometryCacheUSD = true;
static FAutoConsoleVariableRef CVarUsdUseGeometryCache(
	TEXT("USD.GeometryCache.Enable"),
	GUseGeometryCacheUSD,
	TEXT("Use GeometryCache instead of static meshes for loading animated meshes")
);

static int32 GGeometryCacheMaxDepth = 15;
static FAutoConsoleVariableRef CVarGeometryCacheMaxDepth(
	TEXT("USD.GeometryCache.MaxDepth"),
	GGeometryCacheMaxDepth,
	TEXT("Maximum distance between an animated mesh prim to its collapsed geometry cache root")
);

static int32 GNumPerPrimLocks = 32;
static FAutoConsoleVariableRef CVarNumPerPrimLocks(
	TEXT("USD.NumPerPrimLocks"),
	GNumPerPrimLocks,
	TEXT(
		"Maximum number of locks that are distributed between all the prim info structs that the USDInfoCache keeps internally. More locks can imply better performance for the info cache build, but the total number of locks available on the system is finite"
	)
);

namespace UE::UsdInfoCache::Private
{
	// Flags to hint at the state of a prim for the purpose of geometry cache
	enum class EGeometryCachePrimState : uint8
	{
		None = 0x00,
		Uncollapsible = 0x01,		   // prim cannot be collapsed as part of a geometry cache
		Mesh = 0x02,				   // prim is a mesh, animated or not
		Xform = 0x04,				   // prim is a xform, animated or not
		Collapsible = Mesh | Xform,	   // only meshes and xforms can be collapsed into a geometry cache
		ValidRoot = 0x08			   // prim can collapse itself and its children into a geometry cache
	};
	ENUM_CLASS_FLAGS(EGeometryCachePrimState)

	struct FUsdPrimInfo
	{
		int32 PrimLockIndex = INDEX_NONE;

		// Points to our collapsing root
		// - Optional is not set: Prim wasn't visited on cache build. It's collapsed, but we don't know the root (yet);
		// - Optional contains empty path: Prim was visited on build, we know it doesn't collapse and it isn't collapsed;
		// - Optional contains prim's own path: This prim is a collapse root itself: It CollapsesChildren();
		// - Optional contain other prim's path: This prim is collapsed, and that other prim is our collapse root (it CollapsesChildren());
		TOptional<UE::FSdfPath> AssetCollapsedRoot;

		// Whether this prim can be collapsed or not, according to its schema translator
		// - Optional is not set: Prim wasn't visited yet, we don't know
		// - Optional has value: Whether the prim can be collapsed or not
		TOptional<bool> bXformSubtreeCanBeCollapsed;

		uint64 ExpectedVertexCountForSubtree = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeMaterialSlots;

		int32 GeometryCacheDepth = -1;
		EGeometryCachePrimState GeometryCacheState = EGeometryCachePrimState::None;

		// Paths to material prims to the mesh prims they are bound to in the scene, given the current settings for
		// render context, material purpose, variant selections, etc.
		TSet<UE::FSdfPath> MaterialUsers;

		// Maps from prims, to all the prims that require also reading this prim to be translated into an asset.
		// Mainly used to update these assets whenever the depencency prim is updated.
		TSet<UE::FSdfPath> MainPrims;
		TSet<UE::FSdfPath> AuxPrims;

		FUsdPrimInfo() = default;
		FUsdPrimInfo(int InPrimLockIndex)
			: PrimLockIndex(InPrimLockIndex)
		{
		}
	};
}	 // namespace UE::UsdInfoCache::Private

FArchive& operator<<(FArchive& Ar, UE::UsdInfoCache::Private::FUsdPrimInfo& Info)
{
	Ar << Info.PrimLockIndex;

	Ar << Info.AssetCollapsedRoot;

	Ar << Info.ExpectedVertexCountForSubtree;
	Ar << Info.SubtreeMaterialSlots;

	Ar << Info.GeometryCacheDepth;
	Ar << Info.GeometryCacheState;

	Ar << Info.MaterialUsers;

	Ar << Info.MainPrims;
	Ar << Info.AuxPrims;

	return Ar;
}

struct FUsdInfoCacheImpl
{
	FUsdInfoCacheImpl()
		: AllowedExtensionsForGeometryCacheSource(UnrealUSDWrapper::GetNativeFileFormats())
	{
		AllowedExtensionsForGeometryCacheSource.Add(TEXT("abc"));

		FScopedUnrealAllocs Allocs;
		PrimLocks = new FRWLock[GNumPerPrimLocks]();
	}

	FUsdInfoCacheImpl(const FUsdInfoCacheImpl& Other)
		: FUsdInfoCacheImpl()
	{
		FReadScopeLock ScopedInfoMapLock(Other.InfoMapLock);

		FWriteScopeLock ThisScopedInfoMapLock(InfoMapLock);

		InfoMap = Other.InfoMap;

		AllowedExtensionsForGeometryCacheSource = Other.AllowedExtensionsForGeometryCacheSource;
	}

	FUsdInfoCacheImpl& operator=(const FUsdInfoCacheImpl& Other)
	{
		FReadScopeLock OtherScopedInfoMapLock(Other.InfoMapLock);

		FWriteScopeLock ThisScopedInfoMapLock(InfoMapLock);

		InfoMap = Other.InfoMap;
		AllowedExtensionsForGeometryCacheSource = Other.AllowedExtensionsForGeometryCacheSource;
		TempStage = Other.TempStage;
		return *this;
	}

	~FUsdInfoCacheImpl()
	{
		delete[] PrimLocks;
	}

	// Information we must have about all prims on the stage
	TMap<UE::FSdfPath, UE::UsdInfoCache::Private::FUsdPrimInfo> InfoMap;
	mutable FRWLock InfoMapLock;

	// Temporarily used during the info cache build, as we need to do another pass on point instancers afterwards
	TArray<FString> PointInstancerPaths;
	mutable FRWLock PointInstancerPathsLock;

	// This is used to keep track of which prototypes are already being translated within this "translation session",
	// so that the schema translators can early out if they're trying to translate multiple instances of the same
	// prototype
	TSet<UE::FSdfPath> TranslatedPrototypes;
	mutable FRWLock TranslatedPrototypesLock;

	// Geometry cache can come from a reference or payload of these file types
	TArray<FString> AllowedExtensionsForGeometryCacheSource;

	// Valid only during the main info cache build
	UE::FUsdStageWeak TempStage;

	// Individual locks distributed across the FUsdPrimInfo in the InfoMap
	FRWLock* PrimLocks = nullptr;

public:
	void RegisterAuxiliaryPrims(const UE::FSdfPath& MainPrimPath, const TSet<UE::FSdfPath>& AuxPrimPaths)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RegisterAuxiliaryPrims);

		if (AuxPrimPaths.Num() == 0)
		{
			return;
		}

		FReadScopeLock ScopeLock{InfoMapLock};

		if (UE::UsdInfoCache::Private::FUsdPrimInfo* MainPrim = InfoMap.Find(MainPrimPath))
		{
			FWriteScopeLock PrimLock{PrimLocks[MainPrim->PrimLockIndex]};
			MainPrim->AuxPrims.Append(AuxPrimPaths);
		}

		for (const UE::FSdfPath& AuxPrimPath : AuxPrimPaths)
		{
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* AuxPrim = InfoMap.Find(AuxPrimPath))
			{
				FWriteScopeLock PrimLock{PrimLocks[AuxPrim->PrimLockIndex]};
				AuxPrim->MainPrims.Add(MainPrimPath);
			}

			UE_LOG(LogUsd, Verbose, TEXT("Registering main prim '%s' and aux prim '%s'"), *MainPrimPath.GetString(), *AuxPrimPath.GetString());
		}
	}
};

FUsdInfoCache::FUsdInfoCache()
{
	Impl = MakeUnique<FUsdInfoCacheImpl>();
}

FUsdInfoCache::~FUsdInfoCache()
{
}

void FUsdInfoCache::CopyImpl(const FUsdInfoCache& Other)
{
	*Impl = *Other.Impl;
}

bool FUsdInfoCache::Serialize(FArchive& Ar)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			Ar << ImplPtr->InfoMap;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->TranslatedPrototypesLock);
			Ar << ImplPtr->TranslatedPrototypes;
		}
	}

	return true;
}

bool FUsdInfoCache::ContainsInfoAboutPrim(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.Contains(Path);
	}

	return false;
}

TSet<UE::FSdfPath> FUsdInfoCache::GetKnownPrims() const
{
	TSet<UE::FSdfPath> Result;

	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		ImplPtr->InfoMap.GetKeys(Result);
		return Result;
	}

	return Result;
}

bool FUsdInfoCache::IsPathCollapsed(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			// See comment on the AssetCollapsedRoot member
			return !FoundInfo->AssetCollapsedRoot.IsSet() || (!FoundInfo->AssetCollapsedRoot->IsEmpty() && FoundInfo->AssetCollapsedRoot != Path);
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

bool FUsdInfoCache::DoesPathCollapseChildren(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			// We store our own Path in there when we collapse children.
			// Otherwise we hold the path of our collapse root, or empty (in case nothing is collapsed up to here)
			return FoundInfo->AssetCollapsedRoot == Path;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

namespace UE::USDInfoCache::Private
{
#if USE_USD_SDK
	bool RecursiveQueryCanBeCollapsed(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCacheImpl& Impl,
		FUsdSchemaTranslatorRegistry& Registry
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCache::Private::RecursiveQueryCanBeCollapsed);

		UE::FSdfPath UsdPrimPath = UE::FSdfPath{UsdPrim.GetPrimPath()};

		// If we already have a value for our prim then we can just return it right now. We only fill these bCanBeCollapsed values
		// through here, so if we know e.g. that UsdPrim can be collapsed, we know its entire subtree can too.
		UE::UsdInfoCache::Private::FUsdPrimInfo* MainPrimInfo = Impl.InfoMap.Find(UsdPrimPath);
		if (MainPrimInfo)
		{
			FReadScopeLock PrimLock{Impl.PrimLocks[MainPrimInfo->PrimLockIndex]};
			if (MainPrimInfo->bXformSubtreeCanBeCollapsed.IsSet())
			{
				return MainPrimInfo->bXformSubtreeCanBeCollapsed.GetValue();
			}
		}

		// If we're here, we don't know whether UsdPrim CanBeCollapsed or not.
		// Since these calls are usually cheap, let's just query it for ourselves right now
		bool bCanBeCollapsed = true;
		if (TSharedPtr<FUsdSchemaTranslator> SchemaTranslator = Registry.CreateTranslatorForSchema(Context.AsShared(), UE::FUsdTyped(UsdPrim)))
		{
			bCanBeCollapsed = SchemaTranslator->CanBeCollapsed(ECollapsingType::Assets);
		}

		// If we can be collapsed ourselves we're not still done, because this is about the subtree. If any of our
		// children can't be collapsed, we actually can't either
		if (bCanBeCollapsed)
		{
			TArray<pxr::UsdPrim> Children;
			for (pxr::UsdPrim Child : UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate)))
			{
				// We don't care about non-xformable prims (materials, stuff we don't have schema translators for, etc.)
				if (Child.IsA<pxr::UsdGeomXformable>())
				{
					Children.Emplace(Child);
				}
			}

			TArray<bool> ChildrenCanBeCollapsed;
			ChildrenCanBeCollapsed.SetNumZeroed(Children.Num());

			const int32 MinBatchSize = 1;
			ParallelFor(
				TEXT("RecursiveQueryCanBeCollapsed"),
				Children.Num(),
				MinBatchSize,
				[&](int32 Index)
				{
					ChildrenCanBeCollapsed[Index] = RecursiveQueryCanBeCollapsed(Children[Index], Context, Impl, Registry);
				}
			);

			for (bool bChildCanBeCollapsed : ChildrenCanBeCollapsed)
			{
				if (!bChildCanBeCollapsed)
				{
					bCanBeCollapsed = false;
					break;
				}
			}
		}

		// Record what we found about our main prim
		{
			FWriteScopeLock PrimLock{Impl.PrimLocks[MainPrimInfo->PrimLockIndex]};
			MainPrimInfo->bXformSubtreeCanBeCollapsed = bCanBeCollapsed;
		}

		// Before we return though, what we can do here is that if we know that we can't be collapsed ourselves,
		// then none of our ancestors can either! So let's quickly paint upwards to make future queries faster
		if (!bCanBeCollapsed)
		{
			UE::FSdfPath TraversalPath = UsdPrimPath.GetParentPath();
			while (!TraversalPath.IsAbsoluteRootPath())
			{
				if (UE::UsdInfoCache::Private::FUsdPrimInfo* AncestorInfo = Impl.InfoMap.Find(TraversalPath))
				{
					FWriteScopeLock PrimLock{Impl.PrimLocks[AncestorInfo->PrimLockIndex]};

					// We found something that was already filled out: Let's stop traversing here
					if (AncestorInfo->bXformSubtreeCanBeCollapsed.IsSet())
					{
						// If we can't collapse ourselves then like we mentioned above none of our ancestors should
						// be able to collapse either
						ensure(AncestorInfo->bXformSubtreeCanBeCollapsed.GetValue() == false);
						break;
					}
					else
					{
						AncestorInfo->bXformSubtreeCanBeCollapsed = false;
					}
				}

				TraversalPath = TraversalPath.GetParentPath();
			}
		}

		return bCanBeCollapsed;
	}
#endif	  // USE_USD_SDK
}	 // namespace UE::USDInfoCache::Private

UE::FSdfPath FUsdInfoCache::UnwindToNonCollapsedPath(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		TOptional<UE::FSdfPath> CollapseRoot;
		UE::UsdInfoCache::Private::FUsdPrimInfo* MainFoundInfo = ImplPtr->InfoMap.Find(Path);
		if (MainFoundInfo)
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[MainFoundInfo->PrimLockIndex]};
			CollapseRoot = MainFoundInfo->AssetCollapsedRoot;
		}

		// We never visited this prim before. We know it's collapsed, let's find our collapse root
		if (!CollapseRoot.IsSet())
		{
			TArray<UE::UsdInfoCache::Private::FUsdPrimInfo*> InfosToUpdate = {MainFoundInfo};

			UE::FSdfPath TraversalPath = Path.GetParentPath();
			while (!TraversalPath.IsAbsoluteRootPath())
			{
				if (UE::UsdInfoCache::Private::FUsdPrimInfo* AncestorInfo = ImplPtr->InfoMap.Find(TraversalPath))
				{
					FReadScopeLock PrimLock{ImplPtr->PrimLocks[AncestorInfo->PrimLockIndex]};

					// We found an ancestor that has this filled in: We're collapsed, so whatever is its
					// collapse root is also our collapse root
					if (AncestorInfo->AssetCollapsedRoot.IsSet())
					{
						// If our original Path doesn't have anything filled in, then we *must* be a child of a
						// collapsed prim (i.e. something that has a non-empty path in its AssetCollapsedRoot)
						ensure(!AncestorInfo->AssetCollapsedRoot->IsEmpty());

						CollapseRoot = AncestorInfo->AssetCollapsedRoot;
						break;
					}
					// Still nothing. Let's keep track of this info so that we can update it later
					else
					{
						InfosToUpdate.Add(AncestorInfo);
					}
				}

				TraversalPath = TraversalPath.GetParentPath();
			}

			// Fill in all visited infos with what we found on our ancestor
			if (CollapseRoot.IsSet())
			{
				for (UE::UsdInfoCache::Private::FUsdPrimInfo* Info : InfosToUpdate)
				{
					FWriteScopeLock PrimLock{ImplPtr->PrimLocks[Info->PrimLockIndex]};
					Info->AssetCollapsedRoot = CollapseRoot;
				}
			}
		}

		// We have visited this prim during the info cache build (or another Unwind, or just now within this function)
		if (CollapseRoot.IsSet())
		{
			// An empty path here means that we are not collapsed at all
			if (CollapseRoot->IsEmpty())
			{
				return Path;
			}
			// Otherwise we have our own path in there (in case we collapse children) or the path to the prim that collapsed us
			else
			{
				return CollapseRoot.GetValue();
			}
		}

		// This should never happen: We should have cached the entire tree
		ensureAlwaysMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return Path;
}

TSet<UE::FSdfPath> FUsdInfoCache::GetMainPrims(const UE::FSdfPath& AuxPrimPath) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(AuxPrimPath))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			TSet<UE::FSdfPath> Result = FoundInfo->MainPrims;
			Result.Add(AuxPrimPath);
			return Result;
		}
	}

	return {AuxPrimPath};
}

TSet<UE::FSdfPath> FUsdInfoCache::GetAuxiliaryPrims(const UE::FSdfPath& MainPrimPath) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(MainPrimPath))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			TSet<UE::FSdfPath> Result = FoundInfo->AuxPrims;
			Result.Add(MainPrimPath);
			return Result;
		}
	}

	return {MainPrimPath};
}

TSet<UE::FSdfPath> FUsdInfoCache::GetMaterialUsers(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->MaterialUsers;
		}
	}

	return {};
}

bool FUsdInfoCache::IsMaterialUsed(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->MaterialUsers.Num() > 0;
		}
	}

	return false;
}

namespace UE::USDInfoCache::Private
{
#if USE_USD_SDK
	void GetPrimVertexCountAndSlots(
		const pxr::UsdPrim& UsdPrim,
		const FUsdSchemaTranslationContext& Context,
		const FUsdInfoCacheImpl& Impl,
		uint64& OutVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutMaterialSlots
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetPrimVertexCountAndSlots);

		FScopedUsdAllocs Allocs;

		if (UsdPrim.IsA<pxr::UsdGeomGprim>() || UsdPrim.IsA<pxr::UsdGeomSubset>())
		{
			OutVertexCount = UsdUtils::GetGprimVertexCount(pxr::UsdGeomGprim{UsdPrim}, Context.Time);

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if (!Context.RenderContext.IsNone())
			{
				RenderContextToken = UnrealToUsd::ConvertToken(*Context.RenderContext.ToString()).Get();
			}

			pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
			if (!Context.MaterialPurpose.IsNone())
			{
				MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context.MaterialPurpose.ToString()).Get();
			}

			const bool bProvideMaterialIndices = false;
			UsdUtils::FUsdPrimMaterialAssignmentInfo LocalInfo = UsdUtils::GetPrimMaterialAssignments(
				UsdPrim,
				Context.Time,
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);

			OutMaterialSlots.Append(MoveTemp(LocalInfo.Slots));
		}
		else if (pxr::UsdGeomPointInstancer PointInstancer{UsdPrim})
		{
			const pxr::UsdRelationship& Prototypes = PointInstancer.GetPrototypesRel();

			pxr::SdfPathVector PrototypePaths;
			if (Prototypes.GetTargets(&PrototypePaths))
			{
				TArray<uint64> PrototypeVertexCounts;
				PrototypeVertexCounts.SetNumZeroed(PrototypePaths.size());

				{
					FReadScopeLock ScopeLock(Impl.InfoMapLock);
					for (uint32 PrototypeIndex = 0; PrototypeIndex < PrototypePaths.size(); ++PrototypeIndex)
					{
						const pxr::SdfPath& PrototypePath = PrototypePaths[PrototypeIndex];

						// Skip invisible prototypes here to mirror how they're skipped within
						// USDGeomMeshConversion.cpp, in the RecursivelyCollapseChildMeshes function. Those two
						// traversals have to match at least with respect to the material slots, so that we can use
						// the data collected here to apply material overrides to the meshes generated for the point
						// instancers when they're collapsed
						pxr::UsdPrim PrototypePrim = UsdPrim.GetStage()->GetPrimAtPath(PrototypePath);
						if (pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable(PrototypePrim))
						{
							if (pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr())
							{
								pxr::TfToken VisibilityToken;
								if (VisibilityAttr.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
								{
									continue;
								}
							}
						}

						// If we're calling this for a point instancer we should have parsed the results for our
						// prototype subtrees already
						if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = Impl.InfoMap.Find(UE::FSdfPath{PrototypePath}))
						{
							FReadScopeLock PrimLock{Impl.PrimLocks[FoundInfo->PrimLockIndex]};

							PrototypeVertexCounts[PrototypeIndex] = FoundInfo->ExpectedVertexCountForSubtree;
							OutMaterialSlots.Append(FoundInfo->SubtreeMaterialSlots);
						}
					}
				}

				if (pxr::UsdAttribute ProtoIndicesAttr = PointInstancer.GetProtoIndicesAttr())
				{
					pxr::VtArray<int> ProtoIndicesArr;
					if (ProtoIndicesAttr.Get(&ProtoIndicesArr, pxr::UsdTimeCode::EarliestTime()))
					{
						for (int ProtoIndex : ProtoIndicesArr)
						{
							OutVertexCount += PrototypeVertexCounts[ProtoIndex];
						}
					}
				}
			}
		}
	}

	void RepopulateInfoMap(const pxr::UsdPrim& UsdPrim, FUsdInfoCacheImpl& Impl)
	{
		using namespace UE::UsdInfoCache::Private;

		TRACE_CPUPROFILER_EVENT_SCOPE(RepopulateInfoMap);

		FWriteScopeLock ScopeLock(Impl.InfoMapLock);

		uint64 PrimCounter = 0;

		pxr::UsdPrimRange PrimRange{UsdPrim, pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate)};
		for (pxr::UsdPrim Child : PrimRange)
		{
			Impl.InfoMap.Emplace(Child.GetPrimPath(), FUsdPrimInfo{static_cast<int32>(PrimCounter++ % GNumPerPrimLocks)});
		}
	}

	void RecursivePropagateVertexAndMaterialSlotCounts(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		const pxr::TfToken& MaterialPurposeToken,
		FUsdInfoCacheImpl& Impl,
		FUsdSchemaTranslatorRegistry& Registry,
		uint64& OutSubtreeVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutSubtreeSlots,
		bool bPossibleInheritedBindings
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RecursivePropagateVertexAndMaterialSlotCounts);

		if (!UsdPrim)
		{
			return;
		}

		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};
		pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();

		TFunction<void(const UE::FSdfPath&, TSet<UE::FSdfPath>&)> TryAddMaterialUser = [&Stage](const UE::FSdfPath& Path, TSet<UE::FSdfPath>& Users)
		{
			pxr::UsdPrim UserPrim = Stage->GetPrimAtPath(Path);

			if (UserPrim.IsA<pxr::UsdGeomImageable>())
			{
				// Do this filtering here because Collection.ComputeIncludedPaths() can be very aggressive and return
				// literally *all prims* below an included prim path. That's fine and it really does mean that any Mesh prim
				// in there could use the collection-based material binding, but nevertheless we don't want to register that
				// e.g. Shader prims or SkelAnimation prims are "material users"
				Users.Add(Path);
			}
			else if (UserPrim.IsA<pxr::UsdGeomSubset>())
			{
				// If a UsdGeomSubset is a material user, make its Mesh parent prim into a user too.
				// Our notice handling is somewhat stricter now, and we have no good way of upgrading a simple material info change
				// into a resync change of the StaticMeshComponent when we change a material that is bound directly to a
				// UsdGeomSubset, since the GeomMesh translator doesn't collapse. We'll unwind this path later when fetching material
				// users, so collapsed static meshes are handled OK, skeletal meshes are handled OK, we just need this one exception
				// for handling uncollapsed static meshes, because by default Mesh prims don't "collapse" their child UsdGeomSubsets
				Users.Add(Path.GetParentPath());
			}
		};

		// Material bindings are inherited down to child prims, so if we detect a binding on a parent Xform,
		// we should register the child Mesh prims as users of the material too (regardless of collapsing).
		// Note that we only consider this for direct bindings: Collection-based bindings will already provide the exhaustive
		// list of all the prims that they should apply to when we call ComputeIncludedPaths
		bool bPrimHasInheritableMaterialBindings = false;

		// Register material users
		if (!UsdPrim.IsPseudoRoot())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CheckingMaterialUsers);

			TMap<UE::FSdfPath, TSet<UE::FSdfPath>> NewMaterialUsers;

			pxr::UsdShadeMaterialBindingAPI BindingAPI{UsdPrim};
			if (BindingAPI || bPossibleInheritedBindings)
			{
				// Check for material users via collections-based material bindings
				{
					// When retrieving the relationships directly we'll always need to check the universal render context
					// manually, as it won't automatically "compute the fallback" for us like when we ComputeBoundMaterial()
					std::unordered_set<pxr::TfToken, pxr::TfHash> MaterialPurposeTokens{
						MaterialPurposeToken,
						pxr::UsdShadeTokens->universalRenderContext};
					for (const pxr::TfToken& SomeMaterialPurposeToken : MaterialPurposeTokens)
					{
						// Each one of those relationships must have two targets: A collection, and a material
						for (const pxr::UsdRelationship& Rel : BindingAPI.GetCollectionBindingRels(SomeMaterialPurposeToken))
						{
							const pxr::SdfPath* CollectionPath = nullptr;
							const pxr::SdfPath* MaterialPath = nullptr;

							std::vector<pxr::SdfPath> PathVector;
							if (Rel.GetTargets(&PathVector))
							{
								for (const pxr::SdfPath& Path : PathVector)
								{
									if (Path.IsPrimPath())
									{
										MaterialPath = &Path;
									}
									else if (Path.IsPropertyPath())
									{
										CollectionPath = &Path;
									}
								}
							}

							if (!CollectionPath || !MaterialPath || PathVector.size() != 2)
							{
								// Emit this warning here as USD doesn't seem to and just seems to just ignores this relationship instead
								UE_LOG(
									LogUsd,
									Warning,
									TEXT("Prim '%s' describes a collection-based material binding, but the relationship '%s' is invalid: It should "
										 "contain exactly one Material path and one path to a collection relationship"),
									*PrimPath.GetString(),
									*UsdToUnreal::ConvertToken(Rel.GetName())
								);
								continue;
							}

							if (pxr::UsdCollectionAPI Collection = pxr::UsdCollectionAPI::Get(Stage, *CollectionPath))
							{
								TSet<UE::FSdfPath>& MaterialUsers = NewMaterialUsers.FindOrAdd(UE::FSdfPath{*MaterialPath});

								std::set<pxr::SdfPath> IncludedPaths = Collection.ComputeIncludedPaths(Collection.ComputeMembershipQuery(), Stage);
								for (const pxr::SdfPath& IncludedPath : IncludedPaths)
								{
									TryAddMaterialUser(UE::FSdfPath{IncludedPath}, MaterialUsers);
								}
							}
							else
							{
								UE_LOG(
									LogUsd,
									Warning,
									TEXT("Failed to find collection at path '%s' when processing collection-based material bindings on prim '%s'"),
									*UsdToUnreal::ConvertPath(CollectionPath->GetPrimPath()),
									*PrimPath.GetString()
								);
							}
						}
					}
				}

				// Check for material bindings directly for this prim
				if (pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial(MaterialPurposeToken))
				{
					bPrimHasInheritableMaterialBindings = true;

					TSet<UE::FSdfPath>& MaterialUsers = NewMaterialUsers.FindOrAdd(UE::FSdfPath{ShadeMaterial.GetPrim().GetPath()});
					TryAddMaterialUser(UE::FSdfPath{UsdPrimPath}, MaterialUsers);
				}
			}
			// Temporary fallback for prims that don't have the MaterialBindingAPI but do have the relationship.
			// USD will emit a warning for these though
			else if (pxr::UsdRelationship Relationship = UsdPrim.GetRelationship(pxr::UsdShadeTokens->materialBinding))
			{
				pxr::SdfPathVector Targets;
				Relationship.GetTargets(&Targets);

				if (Targets.size() > 0)
				{
					const pxr::SdfPath& TargetMaterialPrimPath = Targets[0];
					pxr::UsdPrim MaterialPrim = Stage->GetPrimAtPath(TargetMaterialPrimPath);
					if (pxr::UsdShadeMaterial ShadeMaterial = pxr::UsdShadeMaterial{MaterialPrim})
					{
						bPrimHasInheritableMaterialBindings = true;

						TSet<UE::FSdfPath>& MaterialUsers = NewMaterialUsers.FindOrAdd(UE::FSdfPath{TargetMaterialPrimPath});
						TryAddMaterialUser(UE::FSdfPath{UsdPrimPath}, MaterialUsers);
					}
				}
			}

			for (const TPair<UE::FSdfPath, TSet<UE::FSdfPath>>& NewMaterialToUsers : NewMaterialUsers)
			{
				FReadScopeLock ScopeLock(Impl.InfoMapLock);
				if (UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(NewMaterialToUsers.Key))
				{
					FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};
					Info->MaterialUsers.Append(NewMaterialToUsers.Value);
				}
			}
		}

		TArray<pxr::UsdPrim> Prims;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CollectingChildren);
			pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

			for (pxr::UsdPrim Child : PrimChildren)
			{
				Prims.Emplace(Child);
			}
		}

		const uint32 NumChildren = Prims.Num();

		TArray<uint64> ChildSubtreeVertexCounts;
		ChildSubtreeVertexCounts.SetNumUninitialized(NumChildren);

		TArray<TArray<UsdUtils::FUsdPrimMaterialSlot>> ChildSubtreeMaterialSlots;
		ChildSubtreeMaterialSlots.SetNum(NumChildren);

		const int32 MinBatchSize = 1;
		ParallelFor(
			TEXT("RecursivePropagateVertexAndMaterialSlotCounts"),
			Prims.Num(),
			MinBatchSize,
			[&](int32 Index)
			{
				RecursivePropagateVertexAndMaterialSlotCounts(
					Prims[Index],
					Context,
					MaterialPurposeToken,
					Impl,
					Registry,
					ChildSubtreeVertexCounts[Index],
					ChildSubtreeMaterialSlots[Index],
					bPrimHasInheritableMaterialBindings || bPossibleInheritedBindings
				);
			}
		);

		OutSubtreeVertexCount = 0;
		OutSubtreeSlots.Empty();

		// We will still step into invisible prims to collect all info we can, but we won't count their material slots
		// or vertex counts: The main usage of those counts is to handle collapsed meshes, and during collapse we just
		// early out whenever we encounter an invisible prim
		bool bIsPointInstancer = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GettingVertexCountAndSlots);

			bool bPrimIsInvisible = false;
			if (pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable(UsdPrim))
			{
				if (pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr())
				{
					pxr::TfToken VisibilityToken;
					if (VisibilityAttr.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
					{
						bPrimIsInvisible = true;
					}
				}
			}

			// If the mesh prim has an unselected geometry purpose, it is also essentially invisible
			if (!EnumHasAllFlags(Context.PurposesToLoad, IUsdPrim::GetPurpose(UsdPrim)))
			{
				bPrimIsInvisible = true;
			}

			if (pxr::UsdGeomPointInstancer PointInstancer{UsdPrim})
			{
				bIsPointInstancer = true;
			}
			else if (!bPrimIsInvisible)
			{
				GetPrimVertexCountAndSlots(UsdPrim, Context, Impl, OutSubtreeVertexCount, OutSubtreeSlots);

				for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
				{
					OutSubtreeVertexCount += ChildSubtreeVertexCounts[ChildIndex];
					OutSubtreeSlots.Append(ChildSubtreeMaterialSlots[ChildIndex]);
				}
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(StoringCounts);

			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath{UsdPrimPath}))
			{
				// For point instancers we can't guarantee we parsed the prototypes yet because they
				// could technically be anywhere, so store them here for a later pass
				if (bIsPointInstancer)
				{
					FWriteScopeLock PointInstancerLock(Impl.PointInstancerPathsLock);
					Impl.PointInstancerPaths.Emplace(UE::FSdfPath{UsdPrimPath}.GetString());
				}
				// While we will compute the totals for any and all children normally, don't just append the regular
				// traversal vertex count to the point instancer prim itself just yet, as that doesn't really represent
				// what will happen. We'll later do another pass to handle point instancers where we'll properly instance
				// stuff, and then we'll updadte all ancestors
				else
				{
					FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};
					Info->ExpectedVertexCountForSubtree = OutSubtreeVertexCount;
					Info->SubtreeMaterialSlots.Append(OutSubtreeSlots);
				}
			}
		}
	}

	/**
	 * Updates the subtree counts with point instancer instancing info.
	 *
	 * This has to be done outside of the main recursion because point instancers may reference any prim in the
	 * stage to be their prototypes (including other point instancers), so we must first parse the entire
	 * stage (forcing point instancer vertex/material slot counts to zero), and only then use the parsed counts
	 * of prim subtrees all over to build the final counts of point instancers that use them as prototypes, and
	 * then update their parents.
	 */
	void UpdateInfoForPointInstancers(const FUsdSchemaTranslationContext& Context, FUsdInfoCacheImpl& Impl)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateInfoForPointInstancers);

		pxr::UsdStageWeakPtr UsdStageWeak = Impl.TempStage;
		pxr::UsdStageRefPtr Stage = UsdStageWeak;
		if (!Stage)
		{
			return;
		}

		// We must sort point instancers in a particular order in case they depend on each other.
		// At least we know that an ordering like this should be possible, because A with B as a prototype and B with A
		// as a prototype leads to an invalid USD stage.
		TFunction<bool(const FString&, const FString&)> SortFunction = [Stage](const FString& LHS, const FString& RHS)
		{
			FScopedUsdAllocs Allocs;

			pxr::SdfPath LPath = UnrealToUsd::ConvertPath(*LHS).Get();
			pxr::SdfPath RPath = UnrealToUsd::ConvertPath(*RHS).Get();

			pxr::UsdGeomPointInstancer LPointInstancer = pxr::UsdGeomPointInstancer{Stage->GetPrimAtPath(LPath)};
			pxr::UsdGeomPointInstancer RPointInstancer = pxr::UsdGeomPointInstancer{Stage->GetPrimAtPath(RPath)};
			if (LPointInstancer && RPointInstancer)
			{
				const pxr::UsdRelationship& LPrototypes = LPointInstancer.GetPrototypesRel();
				pxr::SdfPathVector LPrototypePaths;
				if (LPrototypes.GetTargets(&LPrototypePaths))
				{
					for (const pxr::SdfPath& LPrototypePath : LPrototypePaths)
					{
						// Consider RPointInstancer at RPath "/LPointInstancer/Prototypes/Nest/RPointInstancer", and
						// LPointInstancer has prototype "/LPointInstancer/Prototypes/Nest". If RPath has the LPrototypePath as prefix,
						// we should have R come before L in the sort order.
						// Of course, in this scenario we could get away with just sorting by length, but that wouldn't help if the
						// point instancers were not inside each other (e.g. siblings).
						if (RPath.HasPrefix(LPrototypePath))
						{
							return false;
						}
					}

					// Give it the benefit of the doubt here and say that if R doesn't *need* to come before L, let's ensure L
					// goes before R just in case
					return true;
				}
			}

			return LHS < RHS;
		};
		{
			FWriteScopeLock PointInstancerLock{Impl.PointInstancerPathsLock};
			Impl.PointInstancerPaths.Sort(SortFunction);
		}

		FReadScopeLock PointInstancerLock{Impl.PointInstancerPathsLock};
		for (const FString& PointInstancerPath : Impl.PointInstancerPaths)
		{
			UE::FSdfPath UsdPointInstancerPath{*PointInstancerPath};

			if (pxr::UsdPrim PointInstancer = Stage->GetPrimAtPath(UnrealToUsd::ConvertPath(*PointInstancerPath).Get()))
			{
				uint64 PointInstancerVertexCount = 0;
				TArray<UsdUtils::FUsdPrimMaterialSlot> PointInstancerMaterialSlots;

				GetPrimVertexCountAndSlots(PointInstancer, Context, Impl, PointInstancerVertexCount, PointInstancerMaterialSlots);

				FReadScopeLock Lock{Impl.InfoMapLock};
				if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UsdPointInstancerPath))
				{
					{
						FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};
						Info->ExpectedVertexCountForSubtree = PointInstancerVertexCount;
						Info->SubtreeMaterialSlots.Append(PointInstancerMaterialSlots);
					}

					// Now that we have info on the point instancer itself, update the counts of all ancestors.
					// Note: The vertex/material slot count for the entire point instancer subtree are just the counts
					// for the point instancer itself, as we stop regular traversal when we hit them
					UE::FSdfPath ParentPath = UsdPointInstancerPath.GetParentPath();
					pxr::UsdPrim Prim = Stage->GetPrimAtPath(ParentPath);
					while (Prim)
					{
						// If our ancestor is a point instancer itself, just abort as we'll only get the actual counts
						// when we handle that ancestor directly. We don't want to update the ancestor point instancer's
						// ancestors with incorrect values
						if (Prim.IsA<pxr::UsdGeomPointInstancer>())
						{
							break;
						}

						if (UE::UsdInfoCache::Private::FUsdPrimInfo* ParentInfo = Impl.InfoMap.Find(ParentPath))
						{
							FWriteScopeLock ParentPrimLock{Impl.PrimLocks[ParentInfo->PrimLockIndex]};
							ParentInfo->ExpectedVertexCountForSubtree += PointInstancerVertexCount;
							ParentInfo->SubtreeMaterialSlots.Append(PointInstancerMaterialSlots);
						}

						// Break only here so we update the pseudoroot too
						if (Prim.IsPseudoRoot())
						{
							break;
						}

						ParentPath = ParentPath.GetParentPath();
						Prim = Stage->GetPrimAtPath(ParentPath);
					}
				}
			}
		}
	}

	/**
	 * Condenses our collected material slots for all subtress (SubtreeMaterialSlots) into just material slot counts,
	 * (OutMaterialSlotCounts), according to bMergeIdenticalSlots.
	 *
	 * We do this after the main pass because then the main material slot collecting code on
	 * the main recursive pass just adds them to arrays, and we're allowed to handle bMergeIdenticalSlots
	 * only here.
	 */
	void CollectMaterialSlotCounts(FUsdInfoCacheImpl& Impl, bool bContextMergeIdenticalSlots)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CollectMaterialSlotCounts);

		if (!bContextMergeIdenticalSlots)
		{
			return;
		}

		FReadScopeLock Lock{Impl.InfoMapLock};
		for (TPair<UE::FSdfPath, UE::UsdInfoCache::Private::FUsdPrimInfo>& Pair : Impl.InfoMap)
		{
			const UE::FSdfPath& PrimPath = Pair.Key;
			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Pair.Value;

			bool bCanMergeSlotsForThisPrim = false;

			// Check whether we merge slots for this prim or not
			{
				FReadScopeLock ReadPrimLock{Impl.PrimLocks[Info.PrimLockIndex]};

				// We only merge slots in the context of collapsing
				const TOptional<UE::FSdfPath>& CollapsedRoot = Info.AssetCollapsedRoot;
				const bool bPrimIsCollapsedOrCollapseRoot = !CollapsedRoot.IsSet() || (!CollapsedRoot->IsEmpty() || PrimPath.IsAbsoluteRootPath());

				const bool bPrimIsPotentialGeometryCacheRoot = Info.GeometryCacheState
															   == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;
				bCanMergeSlotsForThisPrim = bPrimIsCollapsedOrCollapseRoot && !bPrimIsPotentialGeometryCacheRoot;
			}

			// Actually update the slot count
			// For now we only ever merge material slots when collapsing
			if (bCanMergeSlotsForThisPrim)
			{
				FWriteScopeLock WritePrimLock{Impl.PrimLocks[Info.PrimLockIndex]};
				Info.SubtreeMaterialSlots = TSet<UsdUtils::FUsdPrimMaterialSlot>{Info.SubtreeMaterialSlots}.Array();
			}
		}
	}

	bool CanMeshSubtreeBeCollapsed(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCacheImpl& Impl,
		const TSharedPtr<FUsdSchemaTranslator>& Translator
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CanMeshSubtreeBeCollapsed);

		if (!UsdPrim)
		{
			return false;
		}

		// We should never be able to collapse SkelRoots because the UsdSkelSkeletonTranslator doesn't collapse
		if (UsdPrim.IsA<pxr::UsdSkelRoot>())
		{
			return false;
		}

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();

		FReadScopeLock ScopeLock(Impl.InfoMapLock);
		if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath{UsdPrimPath}))
		{
			FReadScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};

			if (Info->ExpectedVertexCountForSubtree > GMaxNumVerticesCollapsedMesh)
			{
				return false;
			}
		}

		return true;
	}

	void RecursiveQueryCollapsesChildren(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCacheImpl& Impl,
		FUsdSchemaTranslatorRegistry& Registry
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCache::Private::RecursiveQueryCollapsesChildren);
		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};

		bool bCollapsesChildren = false;

		TSharedPtr<FUsdSchemaTranslator> SchemaTranslator = Registry.CreateTranslatorForSchema(Context.AsShared(), UE::FUsdTyped(UsdPrim));
		if (SchemaTranslator)
		{
			bool bIsPotentialGeometryCacheRoot = false;
			{
				FReadScopeLock ScopeLock{Impl.InfoMapLock};
				if (const UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath{UsdPrimPath}))
				{
					bIsPotentialGeometryCacheRoot = Info->GeometryCacheState == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;
				}
			}

			// The potential geometry cache root is checked first since the FUsdGeometryCacheTranslator::CollapsesChildren
			// has no logic of its own
			if (bIsPotentialGeometryCacheRoot
				|| (SchemaTranslator->CollapsesChildren(ECollapsingType::Assets)
					&& CanMeshSubtreeBeCollapsed(UsdPrim, Context, Impl, SchemaTranslator)))
			{
				bCollapsesChildren = true;
			}
		}

		// We only need to visit our children if we don't collapse. We'll leave the AssetCollapsedRoot fields unset on
		// the InfoMap, and whenever we query info about a particular prim will fill that in on-demand by just traveling
		// upwards until we run into our collapse root
		if (!bCollapsesChildren)
		{
			TArray<pxr::UsdPrim> Prims;
			for (pxr::UsdPrim Child : UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate)))
			{
				Prims.Emplace(Child);
			}

			const int32 MinBatchSize = 1;
			ParallelFor(
				TEXT("RecursiveQueryCollapsesChildren"),
				Prims.Num(),
				MinBatchSize,
				[&](int32 Index)
				{
					RecursiveQueryCollapsesChildren(Prims[Index], Context, Impl, Registry);
				}
			);
		}

		// Record our collapse root
		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath{UsdPrimPath}))
			{
				FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};
				Info->AssetCollapsedRoot.Emplace(bCollapsesChildren ? UsdPrimPath : pxr::SdfPath::EmptyPath());
			}
		}

		// This really should be a separate pass, but it does no harm here and we have so many passes already...
		// This needs to happen after we set things into the FUsdPrimInfo for this prim right above this, as it may query
		// whether this prim (or any of its children) collapse
		//
		// We only do this for uncollapsed prims or collapse roots. This because whenever the collapse root
		// registers its auxiliary prims here, it will already account for all of the relevant child prims in the entire subtree,
		// according to the translator type. The links between prims inside of a collapsed subtree aren't really useful,
		// because if anything inside the collapsed subtree updates, we'll always just need to update from the collapsed
		// root anyway
		if (SchemaTranslator)
		{
			Impl.RegisterAuxiliaryPrims(PrimPath, SchemaTranslator->CollectAuxiliaryPrims());
		}
	}

	// Returns the paths to all prims on the same local layer stack, that are used as sources for composition
	// arcs that are non-root (i.e. the arcs that are either reference, payload, inherits, etc.).
	// In other words, "instanceable composition arcs from local prims"
	TSet<UE::FSdfPath> GetLocalNonRootCompositionArcSourcePaths(const pxr::UsdPrim& UsdPrim)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetLocalNonRootCompositionArcSourcePaths);

		TSet<UE::FSdfPath> Result;

		if (!UsdPrim)
		{
			return Result;
		}

		pxr::PcpLayerStackRefPtr RootLayerStack;

		pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery(UsdPrim);
		std::vector<pxr::UsdPrimCompositionQueryArc> Arcs = PrimCompositionQuery.GetCompositionArcs();
		Result.Reserve(Arcs.size());
		for (const pxr::UsdPrimCompositionQueryArc& Arc : Arcs)
		{
			pxr::PcpNodeRef TargetNode = Arc.GetTargetNode();

			if (Arc.GetArcType() == pxr::PcpArcTypeRoot)
			{
				RootLayerStack = TargetNode.GetLayerStack();
			}
			// We use this function to collect aux/main prim links for instanceables, and we don't have
			// to track instanceable arcs to outside the local layer stack because those don't generate
			// source prims on the stage that the user could edit anyway!
			else if (TargetNode.GetLayerStack() == RootLayerStack)
			{
				Result.Add(UE::FSdfPath{Arc.GetTargetPrimPath()});
			}
		}

		return Result;
	}

	void RegisterInstanceableAuxPrims(FUsdSchemaTranslationContext& Context, FUsdInfoCacheImpl& Impl)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCache::Private::RegisterInstanceableAuxPrims);
		FScopedUsdAllocs Allocs;

		pxr::UsdStageWeakPtr UsdStageWeak = Impl.TempStage;
		pxr::UsdStageRefPtr Stage = UsdStageWeak;
		if (!Stage)
		{
			return;
		}

		FReadScopeLock ScopeLock{Impl.InfoMapLock};

		std::vector<pxr::UsdPrim> Prototypes = Stage->GetPrototypes();
		const int32 MinBatchSize = 1;
		ParallelFor(
			TEXT("RegisterInstanceableAuxPrimsPrototypes"),
			Prototypes.size(),
			MinBatchSize,
			[&Prototypes, &Context, &Impl, &Stage](int32 Index)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::Prototype);

				FScopedUsdAllocs Allocs;

				const pxr::UsdPrim& Prototype = Prototypes[Index];
				if (!Prototype)
				{
					return;
				}

				std::vector<pxr::UsdPrim> Instances = Prototype.GetInstances();
				if (Instances.size() < 1)
				{
					return;
				}

				TArray<UE::FSdfPath> InstancePaths;
				InstancePaths.SetNum(Instances.size());

				// Really what we want is to find the source prim that generated this prototype though. Instances always work
				// through some kind of composition arc, so here we collect all references/payloads/inherits/specializes/etc.
				// There's a single source prim shared across all instances, so just fetch it from the first one
				TSet<UE::FSdfPath> SourcePaths = GetLocalNonRootCompositionArcSourcePaths(Instances[0]);
				if (SourcePaths.Num() == 0)
				{
					return;
				}

				// Step into every instance of this prototype on the stage
				ParallelFor(
					TEXT("RegisterInstanceableAuxPrimsInstances"),
					Instances.size(),
					MinBatchSize,
					[&Instances, &InstancePaths, &Context, &Impl, &Stage, &SourcePaths](int32 Index)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::PrototypeInstance);

						FScopedUsdAllocs Allocs;

						const pxr::UsdPrim& Instance = Instances[Index];

						UE::FSdfPath InstancePath{Instance.GetPrimPath()};
						InstancePaths[Index] = InstancePath;

						if (UE::UsdInfoCache::Private::FUsdPrimInfo* MainPrim = Impl.InfoMap.Find(InstancePath))
						{
							FWriteScopeLock PrimLock{Impl.PrimLocks[MainPrim->PrimLockIndex]};
							MainPrim->AuxPrims.Append(SourcePaths);
						}

						// Here we'll traverse the entire subtree of the instance
						pxr::UsdPrimRange PrimRange(Instance, pxr::UsdTraverseInstanceProxies());
						for (pxr::UsdPrimRange::iterator InstanceChildIt = ++PrimRange.begin(); InstanceChildIt != PrimRange.end(); ++InstanceChildIt)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::InstanceChild);

							pxr::SdfPath SdfChildPrimPath = InstanceChildIt->GetPrimPath();
							UE::FSdfPath ChildPrimPath{SdfChildPrimPath};

							// Register a dependency from child prim to analogue prims on the sources used for the instance.
							// We have to do some path surgery to discover what the analogue paths on the source prims are though
							pxr::SdfPath RelativeChildPath = SdfChildPrimPath.MakeRelativePath(InstancePath);
							for (const UE::FSdfPath& SourcePath : SourcePaths)
							{
								pxr::SdfPath ChildOnSourcePath = pxr::SdfPath{SourcePath}.AppendPath(RelativeChildPath);
								if (pxr::UsdPrim ChildOnSource = Stage->GetPrimAtPath(ChildOnSourcePath))
								{
									Impl.RegisterAuxiliaryPrims(ChildPrimPath, {UE::FSdfPath{ChildOnSourcePath}});
								}
							}
						}
					}
				);

				// Append all the instance paths in one go for these source paths
				for (const UE::FSdfPath& AuxPrimPath : SourcePaths)
				{
					if (UE::UsdInfoCache::Private::FUsdPrimInfo* AuxPrim = Impl.InfoMap.Find(AuxPrimPath))
					{
						FWriteScopeLock PrimLock{Impl.PrimLocks[AuxPrim->PrimLockIndex]};
						AuxPrim->MainPrims.Append(InstancePaths);
					}
				}
			}
		);
	}

	void FindValidGeometryCacheRoot(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCacheImpl& Impl,
		UE::UsdInfoCache::Private::EGeometryCachePrimState& OutState
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindValidGeometryCacheRoot);

		using namespace UE::UsdInfoCache::Private;

		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};
		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath(UsdPrim.GetPrimPath())))
			{
				FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};

				// A prim is considered a valid root if its subtree has no uncollapsible branch and a valid depth.
				// A valid depth is positive, meaning it has an animated mesh, and doesn't exceed the limit.
				bool bIsValidDepth = Info->GeometryCacheDepth > -1 && Info->GeometryCacheDepth <= GGeometryCacheMaxDepth;
				if (!EnumHasAnyFlags(Info->GeometryCacheState, EGeometryCachePrimState::Uncollapsible) && bIsValidDepth)
				{
					OutState = EGeometryCachePrimState::ValidRoot;
					Info->GeometryCacheState = EGeometryCachePrimState::ValidRoot;
					return;
				}
				// The prim is not a valid root so it's flagged as uncollapsible since the root will be among its children
				// and the eventual geometry cache cannot be collapsed.
				else
				{
					OutState = EGeometryCachePrimState::Uncollapsible;
					Info->GeometryCacheState = EGeometryCachePrimState::Uncollapsible;
				}
			}
		}

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

		// Continue the search for a valid root among the children
		TArray<pxr::UsdPrim> Prims;
		for (pxr::UsdPrim Child : PrimChildren)
		{
			bool bIsCollapsible = false;
			{
				FReadScopeLock ScopeLock(Impl.InfoMapLock);
				const UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath(Child.GetPrimPath()));
				bIsCollapsible = EnumHasAnyFlags(Info.GeometryCacheState, EGeometryCachePrimState::Collapsible);
			}

			// A subtree is considered only if it has anything collapsible in the first place
			if (bIsCollapsible)
			{
				FindValidGeometryCacheRoot(Child, Context, Impl, OutState);
			}
		}

		OutState = EGeometryCachePrimState::Uncollapsible;
	}

	void RecursiveCheckForGeometryCache(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCacheImpl& Impl,
		bool bIsInsideSkelRoot,
		int32& OutDepth,
		UE::UsdInfoCache::Private::EGeometryCachePrimState& OutState
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RecursiveCheckForGeometryCache);

		using namespace UE::UsdInfoCache::Private;

		FScopedUsdAllocs Allocs;

		// With this recursive check for geometry cache, we want to find branches with an animated mesh at the leaf and find the root where they can
		// meet. This root prim will collapses the static and animated meshes under it into a single geometry cache.

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

		TArray<pxr::UsdPrim> Prims;
		for (pxr::UsdPrim Child : PrimChildren)
		{
			Prims.Emplace(Child);
		}

		TArray<int32> Depths;
		Depths.SetNum(Prims.Num());

		TArray<EGeometryCachePrimState> States;
		States.SetNum(Prims.Num());

		const int32 MinBatchSize = 1;
		ParallelFor(
			TEXT("RecursiveCheckForGeometryCache"),
			Prims.Num(),
			MinBatchSize,
			[&Prims, &Context, &Impl, bIsInsideSkelRoot, &Depths, &States](int32 Index)
			{
				RecursiveCheckForGeometryCache(
					Prims[Index],
					Context,
					Impl,
					bIsInsideSkelRoot || Prims[Index].IsA<pxr::UsdSkelRoot>(),
					Depths[Index],
					States[Index]
				);
			}
		);

		static IConsoleVariable* ForceImportCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("USD.GeometryCache.ForceImport"));
		const bool bIsImporting = Context.bIsImporting || (ForceImportCvar && ForceImportCvar->GetBool());

		bool bIsAnimatedMesh = UsdUtils::IsAnimatedMesh(UsdPrim);
		if (!bIsImporting)
		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath(UsdPrimPath)))
			{
				FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};

				// When loading on the stage, the GeometryCache root can only be the animated mesh prim itself
				// and there's no collapsing involved since each animated mesh will become a GeometryCache.
				// The depth is irrelevant here.
				Info->GeometryCacheDepth = -1;
				Info->GeometryCacheState = bIsAnimatedMesh ? EGeometryCachePrimState::ValidRoot : EGeometryCachePrimState::Uncollapsible;
			}

			return;
		}

		// A geometry cache "branch" starts from an animated mesh prim for which we assign a depth of 0
		// Other branches, without any animated mesh, we don't care about and will remain at -1
		int32 Depth = -1;
		if (bIsAnimatedMesh)
		{
			Depth = 0;
		}
		else
		{
			// The depth is propagated from children to parent, incremented by 1 at each level,
			// with the parent depth being the deepest of its children depth
			int32 ChildDepth = -1;
			for (int32 Index = 0; Index < Depths.Num(); ++Index)
			{
				if (Depths[Index] > -1)
				{
					ChildDepth = FMath::Max(ChildDepth, Depths[Index] + 1);
				}
			}
			Depth = ChildDepth;
		}

		// Along with the depth, we want some hints on the content of the subtree of the prim as this will tell us
		// if the prim can serve as a root and collapse its children into a GeometryCache. The sole condition for
		// being a valid root is that all the branches of the subtree are collapsible.
		EGeometryCachePrimState ChildrenState = EGeometryCachePrimState::None;
		for (EGeometryCachePrimState ChildState : States)
		{
			ChildrenState |= ChildState;
		}

		EGeometryCachePrimState PrimState = EGeometryCachePrimState::None;
		const bool bIsMesh = !!pxr::UsdGeomMesh(UsdPrim);
		const bool bIsXform = !!pxr::UsdGeomXform(UsdPrim);
		if (bIsMesh)
		{
			// A skinned mesh can never be considered part of a geometry cache.
			// Now that we use the UsdSkelSkeletonTranslator instead of the old UsdSkelRootTranslator we may run into these
			// skinned meshes that were already handled by a SkeletonTranslator elsewhere, and need to manually skip them
			if (GIsEditor && bIsInsideSkelRoot && UsdPrim.HasAPI<pxr::UsdSkelBindingAPI>())
			{
				PrimState = EGeometryCachePrimState::Uncollapsible;
			}
			else
			{
				// Animated or static mesh. Static meshes could potentially be animated by transforms in their hierarchy.
				// A mesh prim should be a leaf, but it can have GeomSubset prims as children, but those don't
				// affect the collapsibility status.
				PrimState = EGeometryCachePrimState::Mesh;
			}
		}
		else if (bIsXform)
		{
			// An xform prim is considered collapsible since it could have a mesh prim under it. It has to bubble up its children state.
			PrimState = ChildrenState != EGeometryCachePrimState::None ? ChildrenState | EGeometryCachePrimState::Xform
																	   : EGeometryCachePrimState::Xform;
		}
		else
		{
			// This prim is not considered collapsible with some exception
			// Like a Scope could have some meshes under it, so it has to bubble up its children state
			const bool bIsException = !!pxr::UsdGeomScope(UsdPrim);
			if (bIsException && EnumHasAnyFlags(ChildrenState, EGeometryCachePrimState::Mesh))
			{
				PrimState = ChildrenState;
			}
			else
			{
				PrimState = EGeometryCachePrimState::Uncollapsible;
			}
		}

		// A prim could be a potential root if it has a reference or payload to an allowed file type for GeometryCache
		bool bIsPotentialRoot = false;
		{
			pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery::GetDirectReferences(UsdPrim);
			for (const pxr::UsdPrimCompositionQueryArc& CompositionArc : PrimCompositionQuery.GetCompositionArcs())
			{
				if (CompositionArc.GetArcType() == pxr::PcpArcTypeReference)
				{
					pxr::SdfReferenceEditorProxy ReferenceEditor;
					pxr::SdfReference UsdReference;

					if (CompositionArc.GetIntroducingListEditor(&ReferenceEditor, &UsdReference))
					{
						FString FilePath = UsdToUnreal::ConvertString(UsdReference.GetAssetPath());
						FString Extension = FPaths::GetExtension(FilePath);

						if (Impl.AllowedExtensionsForGeometryCacheSource.Contains(Extension))
						{
							bIsPotentialRoot = true;
							break;
						}
					}
				}
				else if (CompositionArc.GetArcType() == pxr::PcpArcTypePayload)
				{
					pxr::SdfPayloadEditorProxy PayloadEditor;
					pxr::SdfPayload UsdPayload;

					if (CompositionArc.GetIntroducingListEditor(&PayloadEditor, &UsdPayload))
					{
						FString FilePath = UsdToUnreal::ConvertString(UsdPayload.GetAssetPath());
						FString Extension = FPaths::GetExtension(FilePath);

						if (Impl.AllowedExtensionsForGeometryCacheSource.Contains(Extension))
						{
							bIsPotentialRoot = true;
							break;
						}
					}
				}
			}
		}

		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			if (UE::UsdInfoCache::Private::FUsdPrimInfo* Info = Impl.InfoMap.Find(UE::FSdfPath(UsdPrimPath)))
			{
				FWriteScopeLock PrimLock{Impl.PrimLocks[Info->PrimLockIndex]};

				Info->GeometryCacheDepth = Depth;
				Info->GeometryCacheState = PrimState;
			}
		}

		// We've encountered a potential root and the subtree has a geometry cache branch, so find its root
		if (bIsPotentialRoot && Depth > -1)
		{
			if (Depth > GGeometryCacheMaxDepth)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Prim '%s' is potentially a geometry cache %d levels deep, which exceeds the limit of %d. "
						 "This could affect its imported animation. The limit can be increased with the cvar USD.GeometryCache.MaxDepth if needed."),
					*PrimPath.GetString(),
					Depth,
					GGeometryCacheMaxDepth
				);
			}
			FindValidGeometryCacheRoot(UsdPrim, Context, Impl, PrimState);
			Depth = -1;
		}

		OutDepth = Depth;
		OutState = PrimState;
	}

	void CheckForGeometryCache(const pxr::UsdPrim& UsdPrim, FUsdSchemaTranslationContext& Context, FUsdInfoCacheImpl& Impl)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CheckForGeometryCache);

		using namespace UE::UsdInfoCache::Private;

		if (!GUseGeometryCacheUSD)
		{
			return;
		}

		// If the stage doesn't contain any animated mesh prims, then don't bother doing a full check
		bool bHasAnimatedMesh = false;
		{
			FScopedUsdAllocs UsdAllocs;
			TArray<TUsdStore<pxr::UsdPrim>> ChildPrims = UsdUtils::GetAllPrimsOfType(UsdPrim, pxr::TfType::Find<pxr::UsdGeomMesh>());
			for (const TUsdStore<pxr::UsdPrim>& ChildPrim : ChildPrims)
			{
				if (UsdUtils::IsAnimatedMesh(ChildPrim.Get()))
				{
					bHasAnimatedMesh = true;
					break;
				}
			}
		}

		if (!bHasAnimatedMesh)
		{
			return;
		}

		const bool bIsInsideSkelRoot = static_cast<bool>(UsdUtils::GetClosestParentSkelRoot(UsdPrim));

		int32 Depth = -1;
		EGeometryCachePrimState State = EGeometryCachePrimState::None;
		RecursiveCheckForGeometryCache(UsdPrim, Context, Impl, bIsInsideSkelRoot, Depth, State);

		// If we end up with a positive depth, it means the check found an animated mesh somewhere
		// but no potential root before reaching the pseudoroot, so find one
		if (Depth > -1)
		{
			if (Depth > GGeometryCacheMaxDepth)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("The stage has a geometry cache %d levels deep, which exceeds the limit of %d. "
						 "This could affect its imported animation. The limit can be increased with the cvar USD.GeometryCache.MaxDepth if needed."),
					Depth,
					GGeometryCacheMaxDepth
				);
			}

			FScopedUsdAllocs UsdAllocs;

			pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

			// The pseudoroot itself cannot be a root for the geometry cache so start from its children
			TArray<pxr::UsdPrim> Prims;
			for (pxr::UsdPrim Child : PrimChildren)
			{
				FindValidGeometryCacheRoot(Child, Context, Impl, State);
			}
		}
	}
#endif	  // USE_SD_SDK
}	 // namespace UE::USDInfoCache::Private

bool FUsdInfoCache::IsPotentialGeometryCacheRoot(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->GeometryCacheState == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

void FUsdInfoCache::ResetTranslatedPrototypes()
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FWriteScopeLock ScopeLock(ImplPtr->TranslatedPrototypesLock);
		return ImplPtr->TranslatedPrototypes.Reset();
	}
}

bool FUsdInfoCache::IsPrototypeTranslated(const UE::FSdfPath& PrototypePath)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->TranslatedPrototypesLock);
		return ImplPtr->TranslatedPrototypes.Contains(PrototypePath);
	}

	return false;
}

void FUsdInfoCache::MarkPrototypeAsTranslated(const UE::FSdfPath& PrototypePath)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FWriteScopeLock ScopeLock(ImplPtr->TranslatedPrototypesLock);
		ImplPtr->TranslatedPrototypes.Add(PrototypePath);
	}
}

TOptional<uint64> FUsdInfoCache::GetSubtreeVertexCount(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->ExpectedVertexCountForSubtree;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return {};
}

TOptional<uint64> FUsdInfoCache::GetSubtreeMaterialSlotCount(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->SubtreeMaterialSlots.Num();
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return {};
}

TOptional<TArray<UsdUtils::FUsdPrimMaterialSlot>> FUsdInfoCache::GetSubtreeMaterialSlots(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};

			return FoundInfo->SubtreeMaterialSlots;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return {};
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FUsdInfoCache::LinkAssetToPrim(const UE::FSdfPath& Path, UObject* Asset)
{
}

void FUsdInfoCache::UnlinkAssetFromPrim(const UE::FSdfPath& Path, UObject* Asset)
{
}

TArray<TWeakObjectPtr<UObject>> FUsdInfoCache::RemoveAllAssetPrimLinks(const UE::FSdfPath& Path)
{
	return {};
}

TArray<UE::FSdfPath> FUsdInfoCache::RemoveAllAssetPrimLinks(const UObject* Asset)
{
	return {};
}

void FUsdInfoCache::RemoveAllAssetPrimLinks()
{
}

TArray<TWeakObjectPtr<UObject>> FUsdInfoCache::GetAllAssetsForPrim(const UE::FSdfPath& Path) const
{
	return {};
}

TArray<UE::FSdfPath> FUsdInfoCache::GetPrimsForAsset(const UObject* Asset) const
{
	return {};
}

TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> FUsdInfoCache::GetAllAssetPrimLinks() const
{
	return {};
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FUsdInfoCache::RebuildCacheForSubtree(const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context)
{
#if USE_USD_SDK
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdInfoCache::RebuildCacheForSubtree);

	using namespace UE::USDInfoCache::Private;

	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	// We can't deallocate our info cache pointer with the Usd allocator
	FScopedUnrealAllocs UEAllocs;

	TGuardValue<bool> Guard{Context.bIsBuildingInfoCache, true};
	{
		pxr::UsdPrim UsdPrim{Prim};
		if (!UsdPrim)
		{
			return;
		}

		ImplPtr->TempStage = UE::FUsdStageWeak{UsdPrim.GetStage()};

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked<IUsdSchemasModule>(TEXT("USDSchemas"));
		FUsdSchemaTranslatorRegistry& Registry = UsdSchemasModule.GetTranslatorRegistry();

		pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
		if (!Context.MaterialPurpose.IsNone())
		{
			MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context.MaterialPurpose.ToString()).Get();
		}

		// We don't call FUsdInfoCache::Clear() here as we don't want to get rid of PrimPathToAssets because
		// we're rebuilding the cache, as that info is also linked to the asset cache
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			ImplPtr->InfoMap.Reset();
		}
		{
			FWriteScopeLock PointInstancerLock(ImplPtr->PointInstancerPathsLock);
			ImplPtr->PointInstancerPaths.Reset();
		}

		// This should be the first step as all future functions will expect to find one entry per prim in the cache
		RepopulateInfoMap(UsdPrim, *ImplPtr);

		// Propagate vertex and material slot counts before we query CollapsesChildren because the Xformable
		// translator needs to know when it would generate too large a static mesh
		uint64 SubtreeVertexCount = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeSlots;
		const bool bPossibleInheritedBindings = false;
		RecursivePropagateVertexAndMaterialSlotCounts(
			UsdPrim,
			Context,
			MaterialPurposeToken,
			*ImplPtr,
			Registry,
			SubtreeVertexCount,
			SubtreeSlots,
			bPossibleInheritedBindings
		);

		UpdateInfoForPointInstancers(Context, *ImplPtr);

		CheckForGeometryCache(UsdPrim, Context, *ImplPtr);

		RecursiveQueryCollapsesChildren(UsdPrim, Context, *ImplPtr, Registry);

		RegisterInstanceableAuxPrims(Context, *ImplPtr);

		CollectMaterialSlotCounts(*ImplPtr, Context.bMergeIdenticalMaterialSlots);
	}
#endif	  // USE_USD_SDK
}

void FUsdInfoCache::Clear()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AUsdStageActor::UnloadUsdStage);

	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(InfoMapEmpty);
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			ImplPtr->InfoMap.Empty();
		}
		{
			FWriteScopeLock PointInstancerLock(ImplPtr->PointInstancerPathsLock);
			ImplPtr->PointInstancerPaths.Empty();
		}

		ResetTranslatedPrototypes();
	}
}

bool FUsdInfoCache::IsEmpty()
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.IsEmpty();
	}

	return true;
}

TOptional<bool> FUsdInfoCache::CanXformableSubtreeBeCollapsed(const UE::FSdfPath& RootPath, FUsdSchemaTranslationContext& Context) const
{
#if USE_USD_SDK
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdInfoCache::CanSubtreeBeCollapsed);

	// The only reason this function exists is that FUsdGeomXformableTranslator::CollapsesChildren() needs to check if all
	// GeomXformable prims in its subtree return true for CanBeCollapsed().
	//
	// We don't want to compute this for the entire stage on the main info cache build, because it may not be needed.
	// However, we definitely do not want each call to FUsdGeomXformableTranslator::CollapsesChildren() to traverse its entire
	// subtree of prims calling CanBeCollapsed() on their own: That would be a massive waste since the output is going to
	// be the same regardless of the caller.
	//
	// This is the awkward compromise where the first call to FUsdGeomXformableTranslator::CollapsesChildren() will traverse
	// its entire subtree and fill this in, and subsequent calls can just use those results, or fill in additional subtrees, etc.

	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(RootPath))
		{
			FReadScopeLock PrimLock{ImplPtr->PrimLocks[FoundInfo->PrimLockIndex]};
			if (FoundInfo->bXformSubtreeCanBeCollapsed.IsSet())
			{
				return FoundInfo->bXformSubtreeCanBeCollapsed.GetValue();
			}
		}

		TOptional<bool> bCanBeCollapsed;

		// Fill in missing entries for CanBeCollapsed on-demand and compute the value for the prim at RootPath,
		// if we can still access our stage
		pxr::UsdStageWeakPtr UsdStageWeak = ImplPtr->TempStage;
		pxr::UsdStageRefPtr Stage = UsdStageWeak;
		if (Stage)
		{
			if (pxr::UsdPrim Prim = Stage->GetPrimAtPath(RootPath))
			{
				IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked<IUsdSchemasModule>(TEXT("USDSchemas"));
				FUsdSchemaTranslatorRegistry& Registry = UsdSchemasModule.GetTranslatorRegistry();

				bCanBeCollapsed = UE::USDInfoCache::Private::RecursiveQueryCanBeCollapsed(Prim, Context, *ImplPtr, Registry);
			}
		}

		// We can potentially still fail to find this here, in case our stage reference is broken (i.e. called outside of the
		// main infocache build callstack).
		//
		// There shouldn't be any point in checking our FoundInfo again though: If we didn't return anything valid from
		// our call to RecursiveQueryCanBeCollapsed, then we didn't put anything new on the InfoMap either
		if (bCanBeCollapsed.IsSet())
		{
			return bCanBeCollapsed;
		}

		UE_LOG(
			LogUsd,
			Warning,
			TEXT(
				"Failed to find whether subtree '%s' can be collapsed or not. Note: This function is meant to be used only during the main FUsdInfoCache build!"
			),
			*RootPath.GetString()
		);
	}
#endif	  // USE_USD_SDK

	return {};
}
