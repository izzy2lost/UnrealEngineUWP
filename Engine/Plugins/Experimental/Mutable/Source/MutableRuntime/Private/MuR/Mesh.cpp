// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Mesh.h"

#include "Containers/UnrealString.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableTrace.h"

namespace mu
{

MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EBoneUsageFlags);
MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EMeshBufferType);
MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EShapeBindingMethod);
MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EVertexColorUsage);


void Mesh::Serialise( const Mesh* p, OutputArchive& arch )
{
    //p->m_pD->CheckIntegrity();
    arch << *p;
}


MeshPtr Mesh::StaticUnserialise( InputArchive& arch )
{
    MUTABLE_CPUPROFILER_SCOPE(MeshUnserialise)
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();
    arch >> *pResult;

    //pResult->m_pD->CheckIntegrity();

    return pResult;
}


MeshPtr Mesh::Clone() const
{
    //MUTABLE_CPUPROFILER_SCOPE(MeshClone);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();

    pResult->InternalId = InternalId;
	pResult->StaticFormatFlags = StaticFormatFlags;
	pResult->Surfaces = Surfaces;
	pResult->Skeleton = Skeleton;
	pResult->PhysicsBody = PhysicsBody;
	pResult->Tags = Tags;
	pResult->StreamedResources = StreamedResources;
	pResult->MeshIDPrefix = MeshIDPrefix;

    // Clone the main buffers
    pResult->VertexBuffers = VertexBuffers;
    pResult->IndexBuffers = IndexBuffers;

	// Clone additional buffers
	pResult->AdditionalBuffers = AdditionalBuffers;

    // Clone the layouts
	pResult->Layouts = Layouts;

    // The skeleton is not cloned because it is not owned by this mesh and it is always assumed
    // to be shared.

	// physics body doen't need to be deep cloned either as they are also assumed to be shared.
	
	// Clone bone poses
	pResult->BonePoses = BonePoses;
	pResult->BoneMap = BoneMap;

	// Clone SkeletonIDs
	pResult->SkeletonIDs = SkeletonIDs;
	pResult->AdditionalPhysicsBodies = AdditionalPhysicsBodies;

    return pResult;
}


MeshPtr Mesh::Clone(EMeshCopyFlags Flags) const
{
    //MUTABLE_CPUPROFILER_SCOPE(MeshClone);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();

    pResult->InternalId = InternalId;
	pResult->StaticFormatFlags = StaticFormatFlags;

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSurfaces))
	{
		pResult->Surfaces = Surfaces;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSkeleton))
	{
		pResult->Skeleton = Skeleton;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithPhysicsBody))
	{
		pResult->PhysicsBody = PhysicsBody;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithTags))
	{
		pResult->Tags = Tags;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithStreamedResources))
	{
		pResult->StreamedResources = StreamedResources;
	}

    // Clone the main buffers
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithVertexBuffers))
    {
		pResult->VertexBuffers = VertexBuffers;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithIndexBuffers))
    {
		pResult->IndexBuffers = IndexBuffers;
	}

	// Clone additional buffers
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithAdditionalBuffers))
	{
		pResult->AdditionalBuffers = AdditionalBuffers;
	}

    // Clone the layout	
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithLayouts))
	{
		pResult->Layouts = Layouts;
	}
    // The skeleton is not cloned because it is not owned by this mesh and it is always assumed
    // to be shared.

	// physics body doen't need to be deep cloned either as they are also assumed to be shared.
	
	// Clone bone poses
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithPoses))
	{
		pResult->BonePoses = BonePoses;
	}

	// Clone BoneMap
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithBoneMap))
	{
		pResult->BoneMap = BoneMap;
	}

	// Clone SkeletonIDs
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSkeletonIDs))
	{
		pResult->SkeletonIDs = SkeletonIDs;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithAdditionalPhysics))
	{
		pResult->AdditionalPhysicsBodies = AdditionalPhysicsBodies;
	}

    return pResult;
}


void Mesh::CopyFrom(const Mesh& From, EMeshCopyFlags Flags)
{
    //MUTABLE_CPUPROFILER_SCOPE(CopyFrom);

    InternalId = From.InternalId;
	StaticFormatFlags = From.StaticFormatFlags;
	MeshIDPrefix = From.MeshIDPrefix;

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSurfaces))
	{
		Surfaces = From.Surfaces;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSkeleton))
	{
		Skeleton = From.Skeleton;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithPhysicsBody))
	{
		PhysicsBody = From.PhysicsBody;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithTags))
	{
		Tags = From.Tags;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithStreamedResources))
	{
		StreamedResources = From.StreamedResources;
	}

    // Copy the main buffers
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithVertexBuffers))
    {
		VertexBuffers = From.VertexBuffers;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithIndexBuffers))
    {
		IndexBuffers = From.IndexBuffers;
	}

	// Copy additional buffers
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithAdditionalBuffers))
	{
		AdditionalBuffers = From.AdditionalBuffers;
	}

    // Copy the layout	
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithLayouts))
	{
		Layouts = From.Layouts;
	}
    // The skeleton is not copied because it is not owned by this mesh and it is always assumed
    // to be shared.

	// physics body doen't need to be deep copied either as they are also assumed to be shared.
	
	// Copy bone poses
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithPoses))
	{
		BonePoses = From.BonePoses;
	}

	// Copy BoneMap
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithBoneMap))
	{
		BoneMap = From.BoneMap;
	}

	// Copy SkeletonIDs
	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithSkeletonIDs))
	{
		SkeletonIDs = From.SkeletonIDs;
	}

	if (EnumHasAnyFlags(Flags, EMeshCopyFlags::WithAdditionalPhysics))
	{
		AdditionalPhysicsBodies = From.AdditionalPhysicsBodies;
	}

}


uint32 Mesh::GetId() const
{
    return InternalId;
}


int Mesh::GetVertexCount() const
{
    return GetVertexBuffers().GetElementCount();
}


FMeshBufferSet& Mesh::GetVertexBuffers()
{
    return VertexBuffers;
}


const FMeshBufferSet& Mesh::GetVertexBuffers() const
{
    return VertexBuffers;
}


bool Mesh::AreVertexIdsImplicit() const
{
	// Is there a buffer for vertex ids?
	int32 BufferIndex = -1;
	int32 ChannelIndex = -1;
	VertexBuffers.FindChannel(MBS_VERTEXINDEX, 0, &BufferIndex, &ChannelIndex);

	return (MeshIDPrefix != 0) && (BufferIndex < 0) && (ChannelIndex < 0);
}


bool Mesh::AreVertexIdsExplicit() const
{
	// Is there a buffer for vertex ids?
	int32 BufferIndex = -1;
	int32 ChannelIndex = -1;
	VertexBuffers.FindChannel(MBS_VERTEXINDEX, 0, &BufferIndex, &ChannelIndex);

	bool bExplicit = (BufferIndex >= 0) && (ChannelIndex >= 0) && (VertexBuffers.m_buffers[BufferIndex].m_channels[ChannelIndex].m_format == MBF_UINT64);
	if (bExplicit)
	{
		check(MeshIDPrefix == 0);
	}
	return bExplicit;
}


void Mesh::MakeVertexIdsRelative()
{
	check(AreVertexIdsImplicit());

	int32 NewBuffer = VertexBuffers.GetBufferCount();
	VertexBuffers.SetBufferCount(NewBuffer + 1);
	EMeshBufferSemantic Semantic = MBS_VERTEXINDEX;
	int32 SemanticIndex = 0;
	EMeshBufferFormat Format = MBF_UINT32;
	int32 Components = 1;
	int32 Offset = 0;
	VertexBuffers.SetBuffer(NewBuffer, sizeof(uint32), 1, &Semantic, &SemanticIndex, &Format, &Components, &Offset );
	uint32* pIdData = reinterpret_cast<uint32*>( VertexBuffers.GetBufferData(NewBuffer) );

	int32 VertexCount = GetVertexCount();
	for (int32 i = 0; i < VertexCount; ++i)
	{
		(*pIdData++) = i;
	}
}


void Mesh::MakeIdsExplicit()
{
	MUTABLE_CPUPROFILER_SCOPE(Mesh_MakeVertexIndicesExplicit);

	if (!MeshIDPrefix)
	{
		// We already have explicit vertex IDs, or we don't have any.
		return;
	}

	int32 VertexCount = GetVertexCount();

	// Vertex IDs
	{
		MUTABLE_CPUPROFILER_SCOPE(VertexIDs);

		bool bHasRelativeVertexIndices = false;

		int32 OldBuf = -1;
		int32 OldChan = -1;
		VertexBuffers.FindChannel(MBS_VERTEXINDEX, 0, &OldBuf, &OldChan);
		bool bHasVertexIndices = (OldBuf >= 0 && OldChan >= 0);
		if (bHasVertexIndices)
		{
			check(OldChan == 0 && VertexBuffers.m_buffers[OldBuf].m_channels.Num() == 1);

			FMeshBufferChannel& Channel = VertexBuffers.m_buffers[OldBuf].m_channels[0];

			bool bHasExplicitVertexIndices = Channel.m_format == MBF_UINT64;
			if (bHasExplicitVertexIndices)
			{
				// nothing to do
				return;
			}

			// The mesh has relative vertex IDs.
			bHasRelativeVertexIndices = true;

			check(Channel.m_format==MBF_UINT32);
			const uint32* OldIdData = reinterpret_cast<const uint32*>(VertexBuffers.GetBufferData(OldBuf));

			TMemoryTrackedArray<uint8> NewIds;
			NewIds.SetNumUninitialized(VertexCount * sizeof(uint64));
			uint64* NewIdData = reinterpret_cast<uint64*>(NewIds.GetData());

			for (int32 i = 0; i < VertexCount; ++i)
			{
				uint32 OldId = *OldIdData++;
				uint64 Id = (uint64(MeshIDPrefix) << 32) | uint64(OldId);
				(*NewIdData++) = Id;
			}

			// 
			FMeshBuffer& Buffer = VertexBuffers.m_buffers[OldBuf];
			Swap(Buffer.m_data, NewIds);
			Buffer.m_channels[0].m_format = MBF_UINT64;
			Buffer.m_elementSize = sizeof(uint64);
		}

		if (!bHasRelativeVertexIndices)
		{
			// The mesh has implicit Ids
			// Create a new buffer with explicit ids
			int32 NewBuffer = VertexBuffers.GetBufferCount();
			VertexBuffers.SetBufferCount(NewBuffer + 1);
			EMeshBufferSemantic Semantic = MBS_VERTEXINDEX;
			int32 SemanticIndex = 0;
			EMeshBufferFormat Format = MBF_UINT64;
			int32 Components = 1;
			int32 Offset = 0;
			VertexBuffers.SetBuffer(NewBuffer, sizeof(uint64), 1, &Semantic, &SemanticIndex, &Format, &Components, &Offset);
			uint64* IdData = reinterpret_cast<uint64*>(VertexBuffers.GetBufferData(NewBuffer));

			for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
			{
				uint64 Id = (uint64(MeshIDPrefix) << 32) | uint64(VertexIndex);
				(*IdData++) = Id;
			}
		}
	}

	// Layout block IDs
	{
		MUTABLE_CPUPROFILER_SCOPE(LayoutBlockIDs);

		for (FMeshBuffer& Buffer: VertexBuffers.m_buffers)
		{
			for (FMeshBufferChannel& Channel : Buffer.m_channels)
			{
				if (Channel.m_semantic != MBS_LAYOUTBLOCK)
				{
					continue;
				}

				if (Channel.m_format == MBF_UINT64)
				{
					continue;
				}

				check(Buffer.m_channels.Num() == 1);
				check(Buffer.m_channels[0].m_offset == 0);
				check(Buffer.m_elementSize == sizeof(uint16));

				check(Channel.m_format == MBF_UINT16);
				const uint16* OldIdData = reinterpret_cast<const uint16*>(Buffer.m_data.GetData());

				TMemoryTrackedArray<uint8> NewIds;
				NewIds.SetNumUninitialized(VertexCount *sizeof(uint64));
				uint64* NewIdData = reinterpret_cast<uint64*>(NewIds.GetData());

				for (int32 i = 0; i < VertexCount; ++i)
				{
					uint16 OldId = *OldIdData++;
					uint64 Id = (uint64(MeshIDPrefix) << 32) | uint64(OldId);
					(*NewIdData++) = Id;
				}

				// 
				Swap(Buffer.m_data, NewIds);
				Buffer.m_channels[0].m_format = MBF_UINT64;
				Buffer.m_elementSize = sizeof(uint64);
			}
		}
	}

	// Final cleanup
	MeshIDPrefix = 0;
}


mu::Ptr<const Skeleton> Mesh::GetSkeleton() const
{
    return Skeleton;
}


void Mesh::SetSkeleton( Ptr<const mu::Skeleton> s )
{
    Skeleton = s;
}


mu::Ptr<const PhysicsBody> Mesh::GetPhysicsBody() const
{
    return PhysicsBody;
}


void Mesh::SetPhysicsBody( Ptr<const mu::PhysicsBody> InPhysicsBody )
{
    PhysicsBody = InPhysicsBody;
}


int32 Mesh::AddAdditionalPhysicsBody(Ptr<const mu::PhysicsBody> Body)
{
	return AdditionalPhysicsBodies.Add(Body);
}


Ptr<const PhysicsBody> Mesh::GetAdditionalPhysicsBody(int32 I) const
{
	check(I >= 0 && I < AdditionalPhysicsBodies.Num());

	return AdditionalPhysicsBodies[I];
}


int32 Mesh::GetFaceCount() const
{
    return GetIndexBuffers().GetElementCount() / 3;
}


int Mesh::GetIndexCount() const
{
    return GetIndexBuffers().GetElementCount();
}


FMeshBufferSet& Mesh::GetIndexBuffers()
{
    return IndexBuffers;
}


const FMeshBufferSet& Mesh::GetIndexBuffers() const
{
    return IndexBuffers;
}


int Mesh::GetSurfaceCount() const
{
    return Surfaces.Num();
}


void Mesh::GetSurface( int32 surfaceIndex,
                       int32* firstVertex, int32* vertexCount,
                       int32* firstIndex, int32* indexCount,
					   int32* BoneIndex, int32* BoneCount,
					   bool* bCastShadow) const
{
    int count = GetSurfaceCount();

    if (surfaceIndex>=0 && surfaceIndex<count)
    {
        if (surfaceIndex<Surfaces.Num())
        {
            const FMeshSurface& surf = Surfaces[surfaceIndex];
            if (firstVertex) *firstVertex = surf.FirstVertex;
            if (vertexCount) *vertexCount = surf.VertexCount;
            if (firstIndex) *firstIndex = surf.FirstIndex;
            if (indexCount) *indexCount = surf.IndexCount;
            if (BoneIndex) *BoneIndex = surf.BoneMapIndex;
            if (BoneCount) *BoneCount = surf.BoneMapCount;
            if (bCastShadow) *bCastShadow = surf.bCastShadow;
        }
        else
        {
            // No surfaces defined, means only one surface using all the mesh
            if (firstVertex) *firstVertex = 0;
            if (vertexCount) *vertexCount = GetVertexCount();
            if (firstIndex) *firstIndex = 0;
            if (indexCount) *indexCount = GetIndexCount();
			if (BoneIndex) *BoneIndex = 0;
			if (BoneCount) *BoneCount = BoneMap.Num();
			if (bCastShadow) *bCastShadow = false;
        }
    }
    else
    {
        check( false );
        if (firstVertex) *firstVertex = 0;
        if (vertexCount) *vertexCount = 0;
        if (firstIndex) *firstIndex = 0;
        if (indexCount) *indexCount = 0;
		if (BoneIndex) *BoneIndex = 0;
		if (BoneCount) *BoneCount = 0;
		if (bCastShadow) *bCastShadow = false;
    }
}


uint32_t Mesh::GetSurfaceId( int surfaceIndex ) const
{
    if (surfaceIndex>=0 && surfaceIndex<Surfaces.Num())
    {
        const FMeshSurface& surf = Surfaces[surfaceIndex];
        return surf.Id;
    }

    return 0;
}


void Mesh::AddLayout(Ptr<const Layout> pLayout )
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
    Layouts.Add( pLayout );
}


int Mesh::GetLayoutCount() const
{
    return Layouts.Num();
}


const Layout* Mesh::GetLayout( int i ) const
{
    check( i>=0 && i<Layouts.Num() );

    return Layouts[i].get();
}


void Mesh::SetLayout( int i, Ptr<const Layout> pLayout )
{
    check( i>=0 && i<Layouts.Num() );

    Layouts[i] = pLayout;
}


int Mesh::GetTagCount() const
{
    return Tags.Num();
}


void Mesh::SetTagCount( int count )
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
    Tags.SetNum( count );
}


const FString& Mesh::GetTag( int tagIndex ) const
{
    check( tagIndex>=0 && tagIndex<GetTagCount() );

    if (tagIndex >= 0 && tagIndex < GetTagCount())
    {
        return Tags[tagIndex];
    }
    else
    {
		static FString NullString;
        return NullString;
    }
}


void Mesh::SetTag( int tagIndex, const FString& Name )
{
    check( tagIndex>=0 && tagIndex<GetTagCount() );
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    if (tagIndex >= 0 && tagIndex < GetTagCount())
    {
        Tags[tagIndex] = Name;
    }
}


void Mesh::AddStreamedResource(uint64 ResourceId)
{
	StreamedResources.AddUnique(ResourceId);
}


const TArray<uint64>& Mesh::GetStreamedResources() const
{
	return StreamedResources;
}


int32 Mesh::FindBonePose(const FBoneName& BoneId) const
{
	return BonePoses.IndexOfByPredicate([BoneId](const FBonePose& Pose) { return Pose.BoneId == BoneId; });
}


void mu::Mesh::SetBonePoseCount(int32 count)
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
	BonePoses.SetNum(count);
}


int32 mu::Mesh::GetBonePoseCount() const
{
	return BonePoses.Num();
}


void mu::Mesh::SetBonePose(int32 Index, const FBoneName& BoneId, FTransform3f Transform, EBoneUsageFlags BoneUsageFlags)
{
	check(BonePoses.IsValidIndex(Index));
	if (BonePoses.IsValidIndex(Index))
	{
		BonePoses[Index] = FBonePose{ BoneId, BoneUsageFlags, Transform };
	}
}


const FBoneName& Mesh::GetBonePoseId(int32 Index) const
{
	check(BonePoses.IsValidIndex(Index));
	return BonePoses[Index].BoneId;
}


void mu::Mesh::GetBonePoseTransform(int32 BoneIndex, FTransform3f& Transform) const
{
	check(BoneIndex >= 0 && BoneIndex < BonePoses.Num());
	Transform = BoneIndex > INDEX_NONE ? BonePoses[BoneIndex].BoneTransform : FTransform3f::Identity;
}


EBoneUsageFlags Mesh::GetBoneUsageFlags(int32 BoneIndex) const
{
	check(BoneIndex >= 0 && BoneIndex < BonePoses.Num());
	return BoneIndex > INDEX_NONE ? BonePoses[BoneIndex].BoneUsageFlags : EBoneUsageFlags::None;
}


void Mesh::SetBoneMap(const TArray<FBoneName>& InBoneMap)
{
	BoneMap = InBoneMap;
}


const TArray<FBoneName>& Mesh::GetBoneMap() const
{
	return BoneMap;
}


int32 Mesh::GetSkeletonIDsCount() const
{
    return SkeletonIDs.Num();
}


int32 Mesh::GetSkeletonID(int32 SkeletonIndex) const
{
	return SkeletonIDs.IsValidIndex(SkeletonIndex) ? SkeletonIDs[SkeletonIndex] : INDEX_NONE;
}


void Mesh::AddSkeletonID(int32 SkeletonID)
{
	check(SkeletonID != INDEX_NONE);
	SkeletonIDs.AddUnique(SkeletonID);
}


int32 Mesh::GetDataSize() const
{
	// TODO: review if other mesh fields like additional physics assets
	// are relevant and add them to the count.

	// Should be allocation sizes used for this?
	int32 AdditionalBuffersSize = 0;
	for (const TPair<EMeshBufferType, FMeshBufferSet>&  AdditionalBuffer : AdditionalBuffers)
	{
		AdditionalBuffersSize += AdditionalBuffer.Value.GetDataSize();
	}

	return sizeof(Mesh)
		+ IndexBuffers.GetDataSize()
		+ VertexBuffers.GetDataSize()
		+ BonePoses.Num() * sizeof(FBonePose)
		+ AdditionalBuffersSize;
}


bool Mesh::HasCompatibleFormat( const Mesh* pOther ) const
{
    bool compatible = true;

    compatible &= Layouts.Num()==pOther->Layouts.Num();
    compatible &= VertexBuffers.GetBufferCount()
            == pOther->VertexBuffers.GetBufferCount();


    // Indices
    //-----------------
    if ( IndexBuffers.GetElementCount()>0 && pOther->GetIndexCount()>0 )
    {
        check( IndexBuffers.m_buffers.Num() == 1 );
        check( pOther->GetIndexBuffers().m_buffers.Num() == 1 );
        check( IndexBuffers.GetBufferChannelCount(0) == 1 );
        check( pOther->GetIndexBuffers().GetBufferChannelCount(0) == 1 );

        const FMeshBuffer& dest = IndexBuffers.m_buffers[0];
        const FMeshBuffer& source = pOther->GetIndexBuffers().m_buffers[0];

        compatible &= dest.m_channels[0].m_format == source.m_channels[0].m_format;
    }


    // Layouts
    //-----------------
    // TODO?


    // Vertices
    //-----------------
    for ( int vb = 0; vb<VertexBuffers.GetBufferCount(); ++vb )
    {
        const FMeshBuffer& dest = VertexBuffers.m_buffers[vb];
        const FMeshBuffer& source = pOther->GetVertexBuffers().m_buffers[vb];

        // TODO: More checks about channels formats and semantics
        //compatible &= GetVertexBufferElementSize(vb) == pOther->GetVertexBufferElementSize(vb);
        compatible &= dest.m_channels.Num()==source.m_channels.Num();
    }

    return compatible;
}


UE::Math::TIntVector3<uint32_t>  Mesh::GetFaceVertexIndices( int f ) const
{
	UE::Math::TIntVector3<uint32> res;

    MeshBufferIteratorConst<MBF_UINT32,uint32_t,1> it( IndexBuffers, MBS_VERTEXINDEX );
    it += f*3;

    res[0] = (*it)[0];
    ++it;

    res[1] = (*it)[0];
    ++it;

    res[2] = (*it)[0];
    ++it;

    return res;
}


bool Mesh::FVertexMatchMap::DoMatch(int32 v, int32 ov) const
{
	if (v >= 0 && v < FirstMatch.Num())
	{
		int32 start = FirstMatch[v];
		int32 end = v + 1 < FirstMatch.Num() ? FirstMatch[v + 1] : Matches.Num();
		bool res = false;

		while (!res && start < end)
		{
			if (Matches[start] == ov)
			{
				res = true;
			}
			++start;
		}

		return res;
	}

	return false;
}


void Mesh::GetVertexMap( const Mesh& other, FVertexMatchMap& vertexMap, float tolerance ) const
{
    int32 vertexCount = VertexBuffers.GetElementCount();
    vertexMap.FirstMatch.SetNum( vertexCount );
    vertexMap.Matches.SetNum( vertexCount+(vertexCount>>2) );

    int otherVertexCount = other.VertexBuffers.GetElementCount();

    if ( !vertexCount || !otherVertexCount )
    {
        return;
    }


    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 > itp( VertexBuffers, MBS_POSITION);
    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 > itopBegin( other.VertexBuffers, MBS_POSITION);


    // Bucket the other mesh
#define MUTABLE_NUM_BUCKETS 256
#define MUTABLE_BUCKET_CHANNEL 0

    float rangeMin = TNumericLimits<float>::Max();
    float rangeMax = -TNumericLimits<float>::Max();
    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 >  itop = itopBegin;
    for ( int32 ov=0; ov<otherVertexCount; ++ov )
    {
        float v = (*itop)[MUTABLE_BUCKET_CHANNEL];
        rangeMin = FMath::Min( rangeMin, v );
        rangeMax = FMath::Max( rangeMax, v );
        ++itop;
    }
    rangeMin -= tolerance;
    rangeMax += tolerance;

    TArray<int32> buckets[MUTABLE_NUM_BUCKETS];
    for ( int32 b=0; b<MUTABLE_NUM_BUCKETS; ++b )
    {
        buckets[b].Reserve( otherVertexCount/MUTABLE_NUM_BUCKETS*2 );
    }

    float bucketSize = (rangeMax-rangeMin)/float(MUTABLE_NUM_BUCKETS);
    itop = itopBegin;
    for ( int32 ov=0; ov<otherVertexCount; ++ov )
    {
        float v = (*itop)[MUTABLE_BUCKET_CHANNEL];

        int32 bucket0 = int32( floor( (v-tolerance-rangeMin)/bucketSize ) );
        bucket0 = FMath::Min( MUTABLE_NUM_BUCKETS-1, FMath::Max( 0, bucket0 ) );
        buckets[bucket0].Add(ov);

        int32 bucket1 = int32( floor( (v+tolerance-rangeMin)/bucketSize ) );
        bucket1 = FMath::Min( MUTABLE_NUM_BUCKETS-1, FMath::Max( 0, bucket1 ) );

        if (bucket1!=bucket0)
        {
            buckets[bucket1].Add(ov);
        }

        ++itop;
    }

    // TODO Compare only positions?

    // Use buckets
    for ( int32 v=0; v<vertexCount; ++v )
    {
        vertexMap.FirstMatch[v] = vertexMap.Matches.Num();

        float vbucket = (*itp)[MUTABLE_BUCKET_CHANNEL];
        int32 bucket = int32( floor( (vbucket-rangeMin)/bucketSize ) );

        if (bucket>=0 && bucket<MUTABLE_NUM_BUCKETS)
        {
            int32 bucketVertexCount = buckets[bucket].Num();
            for ( int32 ov=0; ov<bucketVertexCount; ++ov )
            {
                int32 otherVertexIndex = buckets[bucket][ov];
                FVector3f p = (itopBegin+otherVertexIndex).GetAsVec3f();

                bool same = true;
                for ( int32 d=0; same && d<3; ++d )
                {
                    float diff = fabs( (*itp)[d] - p[d] );
                    same = diff <= tolerance;
                }

                if ( same )
                {
                    vertexMap.Matches.Add( otherVertexIndex );
                }
            }
        }

        ++itp;
    }

}


void Mesh::EnsureSurfaceData()
{
	if (!Surfaces.Num() && VertexBuffers.GetElementCount())
	{
		FMeshSurface s;
		s.VertexCount = VertexBuffers.GetElementCount();
		s.IndexCount = IndexBuffers.GetElementCount();
		s.BoneMapCount = BoneMap.Num();
		Surfaces.Add(s);
	}
}


void Mesh::ResetBufferIndices()
{
	VertexBuffers.ResetBufferIndices();
	IndexBuffers.ResetBufferIndices();
}


void UnserialiseLegacySurfaces(InputArchive& arch, TArray<FMeshSurface>& OutMeshSurfaces)
{
	struct FMeshSurfaceLegacy
	{
		FMeshSurfaceLegacy()
		{}

		int32 FirstVertex = 0;
		int32 VertexCount = 0;
		int32 FirstIndex = 0;
		int32 IndexCount = 0;
		uint32 Id = 0;

		void Unserialise(InputArchive& arch)
		{
			arch >> FirstVertex;
			arch >> VertexCount;
			arch >> FirstIndex;
			arch >> IndexCount;
			arch >> Id;
		}
	}; 

	TArray<FMeshSurfaceLegacy> LegacyMeshSurfaces;
	arch >> LegacyMeshSurfaces;
	
	const int32 NumSurfaces = LegacyMeshSurfaces.Num();
	OutMeshSurfaces.SetNumZeroed(NumSurfaces);

	for (int32 SurfaceIndex = 0; SurfaceIndex < NumSurfaces; ++SurfaceIndex)
	{
		FMeshSurfaceLegacy& LegacySurface = LegacyMeshSurfaces[SurfaceIndex];
		FMeshSurface& Surface = OutMeshSurfaces[SurfaceIndex];
		Surface.FirstVertex = LegacySurface.FirstVertex;
		Surface.VertexCount = LegacySurface.VertexCount;
		Surface.FirstIndex = LegacySurface.FirstIndex;
		Surface.IndexCount = LegacySurface.IndexCount;
		Surface.Id = LegacySurface.Id;
	}
}


void FMeshSurface::Serialise(OutputArchive& arch) const
{
	const int32 ver = 1;
	arch << ver;

	arch << FirstVertex;
	arch << VertexCount;
	arch << FirstIndex;
	arch << IndexCount;
	arch << BoneMapIndex;
	arch << BoneMapCount;
	arch << bCastShadow;

	arch << Id;
}


void FMeshSurface::Unserialise(InputArchive& arch)
{
	int32 ver = 0;
	arch >> ver;
	check(ver <= 1);

	arch >> FirstVertex;
	arch >> VertexCount;
	arch >> FirstIndex;
	arch >> IndexCount;
	arch >> BoneMapIndex;
	arch >> BoneMapCount;

	if (ver >= 1)
	{
		arch >> bCastShadow;
	}

	arch >> Id;
}


void Mesh::FBonePose::Serialise(OutputArchive& arch) const
{
	const int32 ver = 2;
	arch << ver;

	arch << BoneId;
	arch << BoneUsageFlags;
	arch << BoneTransform;
}


void Mesh::FBonePose::Unserialise(InputArchive& arch)
{
	int32 ver = 0;
	arch >> ver;
	check(ver <= 2);

	if (ver <= 1)
	{
		std::string DeprecatedBoneName;
		arch >> DeprecatedBoneName;

		BoneId = FBoneName(0);
	}
	else
	{
		arch >> BoneId;
	}

	if (ver == 0)
	{
		uint8 Skinned = 0;
		arch >> Skinned;
		BoneUsageFlags = Skinned ? EBoneUsageFlags::Skinning : EBoneUsageFlags::None;
	}
	else
	{
		arch >> BoneUsageFlags;
	}

	arch >> BoneTransform;
}


void Mesh::Serialise(OutputArchive& arch) const
{
	uint32 ver = 19;
	arch << ver;

	arch << IndexBuffers;
	arch << VertexBuffers;
	arch << AdditionalBuffers;
	arch << Layouts;

	arch << SkeletonIDs;

	arch << Skeleton;
	arch << PhysicsBody;

	arch << StaticFormatFlags;
	arch << Surfaces;

	arch << Tags;
	arch << StreamedResources;

	arch << BonePoses;
	arch << BoneMap;

	arch << AdditionalPhysicsBodies;

	arch << MeshIDPrefix;
}


void Mesh::Unserialise(InputArchive& arch)
{
	uint32 ver;
	arch >> ver;
	check(ver <= 19);

	arch >> IndexBuffers;
	arch >> VertexBuffers;
	
	if (ver < 19)
	{
		FMeshBufferSet Dummy;
		arch >> Dummy;
	}

	arch >> AdditionalBuffers;
	arch >> Layouts;

	if (ver >= 14)
	{
		arch >> SkeletonIDs;
	}

	arch >> Skeleton;
	if (ver >= 12)
	{ 
		arch >> PhysicsBody;
	}
	else
	{
		PhysicsBody = nullptr;
	}

	arch >> StaticFormatFlags;

	if (ver >= 16)
	{
		arch >> Surfaces;
	}
	else
	{
		// Deserialize LegacySurfaces
		UnserialiseLegacySurfaces(arch, Surfaces);
	}

	if (ver <= 16)
	{
		struct FACE_GROUP_DEPRECATED
		{
			std::string m_name;
			TArray<int32> m_faces;
			inline void Unserialise(InputArchive& arch) 
			{
				int32 ver = 0;
				arch >> ver;
				arch >> m_name;
				arch >> m_faces;
			}

		};
		TArray<FACE_GROUP_DEPRECATED> FaceGroups;
		arch >> FaceGroups;
	}

	if (ver <= 16)
	{
		TArray < std::string > Temp;
		arch >> Temp;
		Tags.SetNum(Temp.Num());
		for (int32 c = 0; c < Temp.Num(); ++c)
		{
			Tags[c] = Temp[c].c_str();
		}
	}
	else
	{
		arch >> Tags;
	}

	if (ver >= 18)
	{
		arch >> StreamedResources;
	}

	if (ver >= 13)
	{
		arch >> BonePoses;
	}
	else if (Skeleton)
	{
		const int32 NumBones = Skeleton->GetBoneCount();
		BonePoses.SetNum(NumBones);
		check(Skeleton->m_boneTransforms_DEPRECATED.Num() == NumBones);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			BonePoses[BoneIndex].BoneId = FBoneName(static_cast<uint32>(BoneIndex));
			BonePoses[BoneIndex].BoneUsageFlags = EBoneUsageFlags::Skinning;
			BonePoses[BoneIndex].BoneTransform = Skeleton->m_boneTransforms_DEPRECATED[BoneIndex];
		}
	}

	if (ver >= 16)
	{
		arch >> BoneMap;
	}
	else
	{
		const int32 NumBonePoses = BonePoses.Num();
		BoneMap.SetNum(NumBonePoses);
		for (int32 BoneIndex = 0; BoneIndex < NumBonePoses; ++BoneIndex)
		{
			BoneMap[BoneIndex] = FBoneName(static_cast<uint32>(BoneIndex));
		}

		for (FMeshSurface& Surface : Surfaces)
		{
			Surface.BoneMapCount = NumBonePoses;
		}
	}

	if (ver >= 15)
	{
		arch >> AdditionalPhysicsBodies;
	}

	if (ver >= 19)
	{
		arch >> MeshIDPrefix;
	}
}


bool Mesh::IsSimilar(const Mesh& o, bool bCompareLayouts) const
{
	// Some meshes are just vertex indices (masks) we don't consider them for similarity,
	// because the kind of vertex channel data they store is the kind that is ignored.
	if (IndexBuffers.GetElementCount() == 0)
	{
		return false;
	}

	bool equal = IndexBuffers == o.IndexBuffers;
	if (equal && bCompareLayouts) equal = (Layouts.Num() == o.Layouts.Num());
	if (equal && Skeleton != o.Skeleton)
	{
		if (Skeleton && o.Skeleton)
		{
			equal = (*Skeleton == *o.Skeleton);
		}
		else
		{
			equal = false;
		}
	}
	
	if (equal && PhysicsBody != o.PhysicsBody)
	{
		if (PhysicsBody && o.PhysicsBody)
		{
			equal = (*PhysicsBody == *o.PhysicsBody);
		}
		else
		{
			equal = false;
		}
	}

	if (equal) equal = (Surfaces == o.Surfaces);
	if (equal) equal = (Tags == o.Tags);

	// Special comparison for layouts
	if (bCompareLayouts)
	{
		for (int32 i = 0; equal && i < Layouts.Num(); ++i)
		{
			equal &= Layouts[i]->IsSimilar(*o.Layouts[i]);
		}
	}

	// Special comparison for vertex buffers
	if (equal)
	{
		equal = VertexBuffers.IsSimilarRobust(o.VertexBuffers, bCompareLayouts);
	}

	return equal;

}


void Mesh::ResetStaticFormatFlags() const
{
    StaticFormatFlags = 0;

    for ( int f=0; f<SMF_COUNT; ++f )
    {
        if ( s_staticMeshFormatIdentify[f]
             &&
             s_staticMeshFormatIdentify[f]( this ) )
        {
            StaticFormatFlags |= (1<<f);
        }
    }
}


void Mesh::CheckIntegrity() const
{
#ifdef MUTABLE_DEBUG

	{
		int32 BufferIndex = -1;
		int32 ChannelIndex = -1;
		VertexBuffers.FindChannel(MBS_VERTEXINDEX, 0, &BufferIndex, &ChannelIndex);
		if (BufferIndex >= 0 && ChannelIndex >= 0)
		{
			EMeshBufferFormat IdFormat = VertexBuffers.m_buffers[BufferIndex].m_channels[ChannelIndex].m_format;
			if (IdFormat == MBF_UINT64)
			{
				check(MeshIDPrefix==0);
			}
			else if (IdFormat == MBF_UINT32)
			{
				check(MeshIDPrefix != 0);
			}
			else
			{
				check(false);
			}
		}
	}

    // Check vertex indices
    {
        for ( int32 b=0; b<IndexBuffers.GetBufferCount(); ++b )
        {
            int32 elemSize = IndexBuffers.GetElementSize( b );

            for ( int32 c=0; c<IndexBuffers.GetBufferChannelCount(b); ++c )
            {
                EMeshBufferSemantic semantic;
                int32 semanticIndex = 0;
                EMeshBufferFormat format;
                int32 components;
                int32 offset = 0;
                IndexBuffers.GetChannel( b, c, &semantic, &semanticIndex, &format, &components, &offset );

                if ( semantic==MBS_VERTEXINDEX )
                {
                    int32 icount = IndexBuffers.GetElementCount();
                    int32 elemCount = VertexBuffers.GetElementCount();
                    for ( int32 indexIndex = 0; indexIndex < icount; ++indexIndex )
                    {
                        const uint8* pData = IndexBuffers.GetBufferData( b ) + elemSize*indexIndex + offset;

                        switch (format)
                        {
                        case MBF_UINT32:
                        {
                            uint32 index = *(const uint32*)pData;
                            check( index < uint32( elemCount ) );
                            break;
                        }
                        case MBF_UINT16:
                        {
                            uint16 index = *(const uint16*)pData;
                            check( index < uint16( elemCount ) );
                            break;
                        }
                        case MBF_UINT8:
                        {
                            uint8 index = *(const uint8*)pData;
                            check( index < uint8( elemCount ) );
                            break;
                        }
                        default:
                            check(false);
                            break;
                        }
                    }
                }
            }
        }
    }


    // Check bone indices, if there are bones. Bones could have been removed for later addition as an optimisation.
    // For all the attributes in this mesh
    int32 boneCount = Skeleton ? Skeleton->GetBoneCount() : 0;
    if ( Skeleton && boneCount )
    {
        for ( int32 b = 0; b < VertexBuffers.GetBufferCount(); ++b )
        {
            int32 channelCount = VertexBuffers.GetBufferChannelCount( b );
            for ( int32 c = 0; c < channelCount; ++c )
            {
                EMeshBufferSemantic semantic;
                int32 semanticIndex = 0;
                EMeshBufferFormat format;
                int32 components;
                int32 offset = 0;
                VertexBuffers.GetChannel( b, c, &semantic, &semanticIndex, &format, &components, &offset );

                // If it is not one of the relevant semantics
                if (
                        //semantic!=MBS_POSITION &&
                        //semantic!=MBS_TEXCOORDS &&
                        //semantic!=MBS_NORMAL &&
                        //semantic!=MBS_TANGENT &&
                        //semantic!=MBS_BINORMAL &&
                        semantic!=MBS_BONEINDICES
                        // && semantic!=MBS_BONEWEIGHTS
                        )
                {
                    continue;
                }

                int32 elemCount = VertexBuffers.GetElementCount();
                int32 elemSize = VertexBuffers.GetElementSize( b );

                for (int32 vertexIndex = 0; vertexIndex < elemCount; ++vertexIndex )
                {
                    const uint8* pData = VertexBuffers.GetBufferData( b ) + elemSize*vertexIndex + offset;

                    switch (format)
                    {
                    case MBF_UINT8:
                    {
                        for ( int32 d = 0; d < components; ++d )
                        {
                            uint8 index = *pData;
                            check( index < uint64(boneCount) );
                            ++pData;
                        }
                        break;
                    }

                    case MBF_UINT16:
                    {
                        for ( int32 d = 0; d < components; ++d )
                        {
                            uint16 index = *(uint16*)pData;
                            check( index < uint64( boneCount ) );
                            pData+=2;
                        }
                        break;
                    }

                    case MBF_UINT32:
                    {
                        for ( int32 d = 0; d < components; ++d )
                        {
                            uint32 index = *(uint32*)pData;
                            check( index < uint64( boneCount ) );
                            pData+=4;
                        }
                        break;
                    }

                    default:
                        check( false );
                    }
                }
            }
        }
    }
#endif
}


static bool StaticMeshFormatIdentify_None( const Mesh* )
{
    return false;
}


static bool StaticMeshFormatIdentify_Project( const Mesh* pM )
{
    // This format is used internally for the mesh project
    bool res = true;

    // The first vertex buffer must be texcoords(2f), position(3f), normal(3f)
    // all tightly packed
    res &= pM->VertexBuffers.GetBufferCount()>=1;

    if ( res )
    {
        res &= pM->VertexBuffers.m_buffers[0].m_channels.Num()==3;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_TEXCOORDS;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 2;
        //we don't really care about the semantic index
        //res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[1];

        res &= chan.m_semantic == MBS_POSITION;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 8;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[2];

        res &= chan.m_semantic == MBS_NORMAL;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 20;
    }

    // The first index buffer must be just index buffers u32
    if ( res )
    {
        res &= pM->IndexBuffers.m_buffers[0].m_channels.Num()>=1;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->IndexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_VERTEXINDEX;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    return res;
}


static bool StaticMeshFormatIdentify_ProjectWrapping( const Mesh* pM )
{
    // This format is used internally for the mesh project
    bool res = true;

    // The first vertex buffer must be texcoords(2f), position(3f), normal(3f), layoutBlock(uint32_t)
    // all tightly packed
    res &= pM->VertexBuffers.GetBufferCount()>=1;

    if ( res )
    {
        res &= pM->VertexBuffers.m_buffers[0].m_channels.Num()==4;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_TEXCOORDS;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 2;
        // we don't really care about the semantic index as long as there is only one
        // res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[1];

        res &= chan.m_semantic == MBS_POSITION;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 8;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[2];

        res &= chan.m_semantic == MBS_NORMAL;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 20;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->VertexBuffers.m_buffers[0].m_channels[3];

        res &= chan.m_semantic == MBS_LAYOUTBLOCK;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        // we don't really care about the semantic index as long as there is only one
        // res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 32;
    }

    // The first index buffer must be just index buffers u32
    if ( res )
    {
        res &= pM->IndexBuffers.m_buffers[0].m_channels.Num()>=1;
    }

    if ( res )
    {
        const FMeshBufferChannel& chan = pM->IndexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_VERTEXINDEX;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    return res;
}


STATIC_MESH_FORMAT_ID_FUNC s_staticMeshFormatIdentify[] =
{
    StaticMeshFormatIdentify_None,
    StaticMeshFormatIdentify_Project,
    StaticMeshFormatIdentify_ProjectWrapping
};


namespace
{
    void LogBuffer( FString& out, const FMeshBufferSet& bufset, int32 BufferElementLimit)
    {
        (void)out;
        (void)bufset;

		uint32 elemCount = bufset.m_elementCount;
        out += "  Set with "
                + FString::Printf(TEXT("%d"), bufset.m_buffers.Num())
                + " buffers and "
                + FString::Printf(TEXT("%d"), elemCount)
                + " elements.\n";

        for( const FMeshBuffer& buf : bufset.m_buffers )
        {
            const uint8* pData = buf.m_data.GetData();

            out += "    Buffer with "+ FString::Printf(TEXT("%d"), buf.m_channels.Num())
                    + " channels and "+ FString::Printf(TEXT("%d"), buf.m_elementSize)+" elementsize\n";
            for( const FMeshBufferChannel& chan : buf.m_channels )
            {
                out += "      Channel with format: "+ FString::Printf(TEXT("%d"), chan.m_format)
                        + " semantic: "+ FString::Printf(TEXT("%d"), chan.m_semantic)
                        + " " + FString::Printf(TEXT("%d"), chan.m_semanticIndex)
                        + " components: " + FString::Printf(TEXT("%d"), chan.m_componentCount)
                        + " offset: " + FString::Printf(TEXT("%d"), chan.m_offset)+"\n";
                for( size_t e=0; e<elemCount && e<BufferElementLimit; ++e )
                {
                    const uint8* pElementData = pData+buf.m_elementSize*e;
                    const uint8* pChanData = pElementData+chan.m_offset;
                    out += "        ";
                    for (int c=0; c<chan.m_componentCount; ++c)
                    {
                        out += "\t";
                        switch (chan.m_format)
                        {
                        case MBF_UINT32:
                        case MBF_NUINT32: out += FString::Printf(TEXT("%d"), *(const uint32_t*)pChanData); pChanData +=4; break;
                        case MBF_UINT16:
                        case MBF_NUINT16: out += FString::Printf(TEXT("%d"), *(const uint16*)pChanData); pChanData +=2; break;
                        case MBF_UINT8:
                        case MBF_NUINT8: out += FString::Printf(TEXT("%d"), *(const uint8_t*)pChanData); pChanData +=1; break;
						case MBF_FLOAT32: out += FString::Printf(TEXT("%.3f"), *(const float*)pChanData); pChanData += 4; break;
                        case MBF_FLOAT16: out += FString::Printf(TEXT("%d"), *(const uint16*)pChanData); pChanData +=2; break;
                        default: break;
                        }
                        out += ",";
                    }
                    out += "\n";
                }
            }
        }
    }
}


void Mesh::Log( FString& out, int32 BufferElementLimit)
{
    out += "Mesh:\n";

    out += "Indices:\n";
    LogBuffer( out, IndexBuffers, BufferElementLimit);

    out += "Vertices:\n";
    LogBuffer( out, VertexBuffers, BufferElementLimit);
}

	
}
