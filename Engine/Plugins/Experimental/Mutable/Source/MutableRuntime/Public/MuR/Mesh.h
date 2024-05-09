// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "Math/Transform.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "MuR/Layout.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Skeleton.h"
#include "Templates/Tuple.h"

class FString;

namespace mu
{

	// Forward references
	class Layout;
    class Skeleton;
    class PhysicsBody;

	class Mesh;
    typedef Ptr<Mesh> MeshPtr;
    typedef Ptr<const Mesh> MeshPtrConst;

	struct FMeshSurface
	{
		int32 FirstVertex=0;
		int32 VertexCount=0;
		int32 FirstIndex=0;
		int32 IndexCount=0;
		uint32 Id=0;
		
		uint32 BoneMapIndex=0;
		uint32 BoneMapCount=0;

		bool bCastShadow=false;

		inline bool operator==(const FMeshSurface& o) const
		{
			return FirstVertex == o.FirstVertex
				&& VertexCount == o.VertexCount
				&& FirstIndex == o.FirstIndex
				&& IndexCount == o.IndexCount
				&& Id == o.Id
				&& BoneMapIndex == o.BoneMapIndex
				&& BoneMapCount == o.BoneMapCount
				&& bCastShadow == o.bCastShadow;
		}

		inline void Serialise(OutputArchive& arch) const;
		inline void Unserialise(InputArchive& arch);

	};


	enum class EBoneUsageFlags : uint32
	{
		None		   = 0,
		Root		   = 1 << 1,
		Skinning	   = 1 << 2,
		SkinningParent = 1 << 3,
		Physics	       = 1 << 4,
		PhysicsParent  = 1 << 5,
		Deform         = 1 << 6,
		DeformParent   = 1 << 7,
		Reshaped       = 1 << 8	
	};

	ENUM_CLASS_FLAGS(EBoneUsageFlags);

	//!
	enum class EMeshBufferType
	{
		None,
		SkeletonDeformBinding,
		PhysicsBodyDeformBinding,
		PhysicsBodyDeformSelection,
		PhysicsBodyDeformOffsets,
		MeshLaplacianData,
		MeshLaplacianOffsets,
		UniqueVertexMap
	};

	//!
	enum class EShapeBindingMethod : uint32
	{
		ReshapeClosestProject = 0,
		ClipDeformClosestProject = 1,
		ClipDeformClosestToSurface = 2,
		ClipDeformNormalProject = 3	
	};

	enum class EVertexColorUsage : uint32
	{
		None = 0,
		ReshapeMaskWeight = 1,
		ReshapeClusterId = 2
	};

	enum class EMeshCopyFlags : uint32
	{
		None = 0,
		WithSkeletalMesh = 1 << 1,
		WithSurfaces = 1 << 2,
		WithSkeleton = 1 << 3,
		WithPhysicsBody = 1 << 4,
		WithFaceGroups = 1 << 5,
		WithTags = 1 << 6,
		WithVertexBuffers = 1 << 7,
		WithIndexBuffers = 1 << 8,
		// deprecated WithFaceBuffers = 1 << 9,
		WithAdditionalBuffers = 1 << 10,
		WithLayouts = 1 << 11,
		WithPoses = 1 << 12,
		WithBoneMap = 1 << 13,
		WithSkeletonIDs = 1 << 14,
		WithAdditionalPhysics = 1 << 15,
		WithStreamedResources = 1 << 16,

		AllFlags = 0xFFFFFFFF
	};
	
	ENUM_CLASS_FLAGS(EMeshCopyFlags);

    //! \brief Mesh object containing any number of buffers with any number of channels.
    //! The buffers can be per-index or per-vertex.
    //! The mesh also includes layout information for every texture channel for internal usage, and
    //! it can be ignored.
    //! The meshes are always assumed to be triangle list primitives.
    //! \ingroup runtime
    class MUTABLERUNTIME_API Mesh : public Resource
    {
	public:

		static constexpr uint64 InvalidVertexId = TNumericLimits<uint64>::Max();

	public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Deep clone this mesh.
        Ptr<Mesh> Clone() const;
		
		// Clone with flags allowing to not include some parts in the cloned mesh
		Ptr<Mesh> Clone(EMeshCopyFlags Flags) const;

		// Copy form another mesh.
		void CopyFrom(const Mesh& From, EMeshCopyFlags Flags = EMeshCopyFlags::AllFlags);

        //! Serialisation
        static void Serialise( const Mesh* p, OutputArchive& arch );
        static Ptr<Mesh> StaticUnserialise( InputArchive& arch );

		// Resource interface
		int32 GetDataSize() const override;

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------


        //! \name Buffers
        //! \{

        //!
        int32 GetIndexCount() const;

        //! Index buffers. They are owned by this mesh.
        FMeshBufferSet& GetIndexBuffers();
        const FMeshBufferSet& GetIndexBuffers() const;

        //
        int32 GetVertexCount() const;

        //! Vertex buffers. They are owned by this mesh.
        FMeshBufferSet& GetVertexBuffers();
        const FMeshBufferSet& GetVertexBuffers() const;

        //
        int32 GetFaceCount() const;

        //! Get the number of surfaces defined in this mesh. Surfaces are buffer-contiguous mesh
        //! fragments that share common properties (usually material)
        int GetSurfaceCount() const;
        void GetSurface( int32 surfaceIndex,
                         int32* FirstVertex, int32* VertexCount,
                         int32* FirstIndex, int32* IndexCount,
						 int32* FirstBone, int32* BoneCount,
						 bool* bCastShadow) const;

        //! Return an internal id that can be used to match mesh surfaces and instance surfaces.
        //! Only valid for meshes that are part of instances.
        uint32 GetSurfaceId( int surfaceIndex ) const;

        //! \}

		/** Return true if the mesh has unique vertex IDs and they stored in an implicit way. 
		* This is relevant for some mesh operations that will need to make them explicit so that the result is still correct.
		*/
		bool AreVertexIdsImplicit() const;
		bool AreVertexIdsExplicit() const;

		/** Create an explicit vertex buffer for vertex IDs if they are implicit. */
		void MakeVertexIdsRelative();
		void MakeIdsExplicit();

        //! \name Texture layouts
        //! \{

        //!
        void AddLayout( Ptr<const Layout> pLayout );

        //!
        int GetLayoutCount() const;

        //!
        const Layout* GetLayout( int i ) const;

        //!
        void SetLayout( int i, Ptr<const Layout> );
        //! \}

        //! \name Skeleton information
        //! \{

        void SetSkeleton( Ptr<const Skeleton> );
        Ptr<const Skeleton> GetSkeleton() const;

        //! \}

        //! \name PhysicsBody information
        //! \{

        void SetPhysicsBody( Ptr<const PhysicsBody> );
        Ptr<const PhysicsBody> GetPhysicsBody() const;

		int32 AddAdditionalPhysicsBody(Ptr<const PhysicsBody> Body);
		Ptr<const PhysicsBody> GetAdditionalPhysicsBody(int32 I) const;
		//int32 GetAdditionalPhysicsBodyExternalId(int32 I) const;

        //! \}

        //! \name Tags
        //! \{

        //!
        void SetTagCount( int count );

        //!
        int GetTagCount() const;

        //!
        const FString& GetTag( int tagIndex ) const;

        //!
        void SetTag( int tagIndex, const FString& Name );

		//!
		void AddStreamedResource(uint64 ResourceId);

		//!
		const TArray<uint64>& GetStreamedResources() const;

		//!
		int32 FindBonePose(const FBoneName& BoneName) const;
		
		//!
		void SetBonePoseCount(int32 count);

		//!
		int32 GetBonePoseCount() const;

		//!
		void SetBonePose(int32 Index, const FBoneName& BoneName, FTransform3f Transform, EBoneUsageFlags BoneUsageFlags);

		//! @return - Bone identifier of the pose at 'Index'.
		const FBoneName& GetBonePoseId(int32 BoneIndex) const;

		//! Return a matrix stored per bone. It is a set of 16-float values.
		void GetBonePoseTransform(int32 BoneIndex, FTransform3f& Transform) const;

		//! 
		EBoneUsageFlags GetBoneUsageFlags(int32 BoneIndex) const;

		//! Set the bonemap of this mesh
		void SetBoneMap(const TArray<FBoneName>& InBoneMap);

		//! Return an array containing the bonemaps of all surfaces in the mesh.
		const TArray<FBoneName>& GetBoneMap() const;

		//!
		int32 GetSkeletonIDsCount() const;

		//!
		int32 GetSkeletonID(int32 SkeletonIndex) const;

		//!
		void AddSkeletonID(int32 SkeletonID);

        //! \}


        //! Get an internal identifier used to reference this mesh in operations like deferred
        //! mesh building, or instance updating.
        uint32 GetId() const;


    protected:

        //! Forbidden. Manage with the Ptr<> template.
		~Mesh() {}

    
	public:

		template<typename Type>
		using TMemoryTrackedArray = TArray<Type, FDefaultMemoryTrackingAllocator<MemoryCounters::FMeshMemoryCounter>>;

		/** Non-persistent internal id unique for a mesh generated for a specific state and parameter values. */
		mutable uint32 InternalId = 0;

		//! This is bit-mask on the STATIC_MESH_FORMATS enumeration, marking what static formats
		//! are compatible with this one. Usually precalculated at model compilation time.
		//! It should be reset after any operation that modifies the format.
		mutable uint32 StaticFormatFlags = 0;

		/** Prefix for the unique IDs related to this mesh (vertices and layout blocks). Useful if the mesh stores them in an implicit, or relative way. 
		* See MeshVertexIdIterator for details.
		*/
		uint32 MeshIDPrefix = 0;

		//!
		FMeshBufferSet VertexBuffers;

		//!
		FMeshBufferSet IndexBuffers;

		//! Additional buffers used for temporary or custom data in different algorithms.
		TArray<TPair<EMeshBufferType, FMeshBufferSet>> AdditionalBuffers;

		TArray<FMeshSurface> Surfaces;

		// Externally provided SkeletonIDs of the skeletons required by this mesh.
		TArray<uint32> SkeletonIDs;

		//! This skeleton and physics body are not owned and may be used by other meshes, so it cannot be modified
		//! once the mesh has been fully created.
		Ptr<const Skeleton> Skeleton;
		Ptr<const PhysicsBody> PhysicsBody;

		//! Additional physics bodies referenced by the mesh that don't merge.
		TArray<Ptr<const mu::PhysicsBody>> AdditionalPhysicsBodies;

		//! Texture Layout blocks attached to this mesh. They are const because they could be shared with
		//! other meshes, so they need to be cloned and replaced if a modification is needed.
		TArray<Ptr<const Layout>> Layouts;		

		//!
		TArray<FString> Tags;

		// Opaque handle to external resources.
		TArray<uint64> StreamedResources;

		struct FBonePose
		{
			// Identifier built from the bone FName.
			FBoneName BoneId;

			EBoneUsageFlags BoneUsageFlags = EBoneUsageFlags::None;
			FTransform3f BoneTransform;

			inline void Serialise(OutputArchive& arch) const;
			inline void Unserialise(InputArchive& arch);

			//!
			inline bool operator==(const FBonePose& Other) const
			{
				return BoneUsageFlags == Other.BoneUsageFlags && BoneId == Other.BoneId;
			}
		};
		// This is the pose used by this mesh fragment, used to update the transforms of the final skeleton
		// taking into consideration the meshes being used.
		TMemoryTrackedArray<FBonePose> BonePoses;

		// Array containing the bonemaps of all surfaces in the mesh.
		TArray<FBoneName> BoneMap;

		//!
		inline void Serialise(OutputArchive& arch) const;

        //!
		inline void Unserialise(InputArchive& arch);


        //!
		inline bool operator==(const Mesh& o) const
		{
			bool bEqual = true;

			if (bEqual) bEqual = (MeshIDPrefix == o.MeshIDPrefix);
			if (bEqual) bEqual = (IndexBuffers == o.IndexBuffers);
			if (bEqual) bEqual = (VertexBuffers == o.VertexBuffers);
			if (bEqual) bEqual = (Layouts.Num() == o.Layouts.Num());
			if (bEqual) bEqual = (BonePoses.Num() == o.BonePoses.Num());
			if (bEqual) bEqual = (BoneMap.Num() == o.BoneMap.Num());
			if (bEqual && Skeleton != o.Skeleton)
			{
				if (Skeleton && o.Skeleton)
				{
					bEqual = (*Skeleton == *o.Skeleton);
				}
				else
				{
					bEqual = false;
				}
			}
			if (bEqual) bEqual = (StreamedResources == o.StreamedResources);
			if (bEqual) bEqual = (Surfaces == o.Surfaces);
			if (bEqual) bEqual = (Tags == o.Tags);
			if (bEqual) bEqual = (SkeletonIDs == o.SkeletonIDs);

			for (int32 i = 0; bEqual && i < Layouts.Num(); ++i)
			{
				bEqual &= (*Layouts[i]) == (*o.Layouts[i]);
			}

			bEqual &= AdditionalBuffers.Num() == o.AdditionalBuffers.Num();
			for (int32 i = 0; bEqual && i < AdditionalBuffers.Num(); ++i)
			{
				bEqual &= AdditionalBuffers[i] == o.AdditionalBuffers[i];
			}

			bEqual &= BonePoses.Num() == o.BonePoses.Num();
			for (int32 i = 0; bEqual && i < BonePoses.Num(); ++i)
			{
				bEqual &= BonePoses[i] == o.BonePoses[i];
			}

			if (bEqual) bEqual = BoneMap == o.BoneMap;

			bEqual &= AdditionalPhysicsBodies.Num() == o.AdditionalPhysicsBodies.Num();
			for (int32 i = 0; bEqual && i < AdditionalPhysicsBodies.Num(); ++i)
			{
				bEqual &= *AdditionalPhysicsBodies[i] == *o.AdditionalPhysicsBodies[i];
			}

			return bEqual;
		}

		//! Compare the mesh with another one, but ignore internal data like generated vertex
		//! indices.
		bool IsSimilar(const Mesh& o, bool bCompareLayouts) const;


		//! Make a map from the vertices in this mesh to thefirst matching vertex of the given
		//! mesh. If non is found, the index is set to -1.
		struct FVertexMatchMap
		{
			//! One for every vertex
			TArray<int32> FirstMatch;

			//! The matches of every vertex in a sequence
			TArray<int32> Matches;

			//!
			bool DoMatch(int32 v, int32 ov) const;
		};

		void GetVertexMap( const Mesh& other, FVertexMatchMap& vertexMap, float tolerance = 1e-3f) const;

		//! Compare the vertex attributes to check if they match.
		UE::Math::TIntVector3<uint32> GetFaceVertexIndices(int f) const;

		//! Return true if the given mesh has the same vertex and index formats, and in the same
		//! buffer structure.
		bool HasCompatibleFormat(const Mesh* pOther) const;

		//! Update the flags identifying the mesh format as some of the optimised formats.
		void ResetStaticFormatFlags() const;

		//! Create the surface data if not present.
		void EnsureSurfaceData();

		//! Check mesh buffer data for possible inconsistencies
		void CheckIntegrity() const;

		//! Change the buffer descriptions so that all buffer indices start at 0 and are in the
		//! same order than memory.
		void ResetBufferIndices();

		//! Debug: get a text representation of the mesh
		void Log(FString& out, int32 VertrexLimit);
    };


	MUTABLE_DEFINE_ENUM_SERIALISABLE(EBoneUsageFlags)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMeshBufferType)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EShapeBindingMethod)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EVertexColorUsage)
}

