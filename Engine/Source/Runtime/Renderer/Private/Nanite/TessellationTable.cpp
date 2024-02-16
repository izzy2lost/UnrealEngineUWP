// Copyright Epic Games, Inc. All Rights Reserved.

#include "TessellationTable.h"
#include "NaniteDefinitions.h"

#ifdef _MSC_VER
#pragma warning(disable : 6385)
#endif

namespace Nanite
{

FTessellationTable::FTessellationTable()
{
	/*
		NumPatterns = (MaxTessFactor + 2) choose 3
		NumPatterns = 1/6 * N(N+1)(N+2)
		= 816
	*/

	const uint32 HashSize = NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE;
	const uint32 MaxNumTris = NANITE_TESSELLATION_TABLE_SIZE * NANITE_TESSELLATION_TABLE_SIZE;
	HashTable.Clear( HashSize, MaxNumTris );

	OffsetTable.AddZeroed( 2 * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE );

	// TessFactors in descending order to reduce size of table.
	
	// Regular tessellation table
	for( uint32 TessFactorZ = 1; TessFactorZ <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorZ++ )
	{
		for( uint32 TessFactorY = TessFactorZ; TessFactorY <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorY++ )
		{
			for( uint32 TessFactorX = TessFactorY; TessFactorX <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorX++ )
			{
				FirstVert = Verts.Num();
				FirstTri = Indexes.Num();

				UniformTessellateAndSnap( FIntVector( TessFactorX, TessFactorY, TessFactorZ ) );
				
				AddToVertsAndIndices( false, TessFactorX, TessFactorY, TessFactorZ );
			}
		}
	}

	// Immediate-mode tessellation table
	for( uint32 TessFactorZ = 1; TessFactorZ <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorZ++ )
	{
		for( uint32 TessFactorY = TessFactorZ; TessFactorY <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorY++ )
		{
			for( uint32 TessFactorX = TessFactorY; TessFactorX <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorX++ )
			{
				FirstVert = Verts.Num();
				FirstTri = Indexes.Num();

				// TODO: Reuse already generated data instead of generating it again?
				UniformTessellateAndSnap( FIntVector( TessFactorX, TessFactorY, TessFactorZ ) );

				const uint32 NumTrisBefore	= Indexes.Num() - FirstTri;
				
				ConstrainToCacheWindow();
				ConstrainForImmediateTessellation();

				const uint32 NumTrisAfter = Indexes.Num() - FirstTri;
				check( NumTrisAfter <= NumTrisBefore + 2 );	// Two degenerate triangles needed for first triangle, so this is optimal
				
				AddToVertsAndIndices( true, TessFactorX, TessFactorY, TessFactorZ );
				
			#if TESSELLATION_TABLE_DUMP_SVG
				char Filename[128];
				sprintf(Filename, "f:\\tessellation_pattern\\%d_%d_%d.svg", TessFactors.X, TessFactors.Y, TessFactors.Z);
				DumpSVG(Filename);
			#endif
			}
		}
	}
	
	HashTable.Free();
}

int32 FTessellationTable::GetPattern( FIntVector TessFactors ) const
{
	checkSlow( 0 < TessFactors[0] && TessFactors[0] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );
	checkSlow( 0 < TessFactors[1] && TessFactors[1] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );
	checkSlow( 0 < TessFactors[2] && TessFactors[2] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );

	if( TessFactors[0] < TessFactors[1] ) Swap( TessFactors[0], TessFactors[1] );
	if( TessFactors[0] < TessFactors[2] ) Swap( TessFactors[0], TessFactors[2] );
	if( TessFactors[1] < TessFactors[2] ) Swap( TessFactors[1], TessFactors[2] );

	return
		( TessFactors[0] - 1 ) +
		( TessFactors[1] - 1 ) * NANITE_TESSELLATION_TABLE_PO2_SIZE +
		( TessFactors[2] - 1 ) * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE;
}

FIntVector FTessellationTable::GetBarycentrics( uint32 Vert ) const
{
	FIntVector Barycentrics;
	Barycentrics.X = Vert & 0xffff;
	Barycentrics.Y = Vert >> 16;
	Barycentrics.Z = BarycentricMax - Barycentrics.X - Barycentrics.Y;
	return Barycentrics;
}

// Average barycentric == average cartesian

float FTessellationTable::LengthSquared( const FIntVector& Barycentrics, const FIntVector& TessFactors ) const
{
	// Barycentric displacement vector:
	// 0 = x + y + z

	FVector3f Norm = FVector3f( Barycentrics ) / BarycentricMax;

	// Length of displacement
	// [ Schindler and Chen 2012, "Barycentric Coordinates in Olympiad Geometry" https://web.evanchen.cc/handouts/bary/bary-full.pdf ]
	return	-Norm.X * Norm.Y * FMath::Square( TessFactors[0] )
			-Norm.Y * Norm.Z * FMath::Square( TessFactors[1] )
			-Norm.Z * Norm.X * FMath::Square( TessFactors[2] );
}

// Snap to exact TessFactor at the edges
void FTessellationTable::SnapAtEdges( FIntVector& Barycentrics, const FIntVector& TessFactors ) const
{
	for( uint32 i = 0; i < 3; i++ )
	{
		const uint32 e0 = i;
		const uint32 e1 = (1 << e0) & 3;

		// Am I on this edge?
		if( Barycentrics[ e0 ] + Barycentrics[ e1 ] == BarycentricMax )
		{
			// Snap toward min barycentric means snapping mirrors. Adjacent patches will thus match.
			uint32 MinIndex = Barycentrics[ e0 ] <  Barycentrics[ e1 ] ? e0 : e1;
			uint32 MaxIndex = Barycentrics[ e0 ] >= Barycentrics[ e1 ] ? e0 : e1;

			// Fixed point round
			uint32 Snapped = ( Barycentrics[ MinIndex ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

			Barycentrics[ MinIndex ] = Snapped / TessFactors[i];
			Barycentrics[ MaxIndex ] = BarycentricMax - Barycentrics[ MinIndex ];
		}
	}
}

uint32 FTessellationTable::AddVert( uint32 Vert )
{
	uint32 Hash = MurmurFinalize32( Vert );

	// Find if there already exists one
	uint32 Index;
	for( Index = HashTable.First( Hash ); HashTable.IsValid( Index ); Index = HashTable.Next( Index ) )
	{
		if( Verts[ FirstVert + Index ] == Vert )
		{
			break;
		}
	}
	if( !HashTable.IsValid( Index ) )
	{
		Index = Verts.Add( Vert ) - FirstVert;
		HashTable.Add( Hash, Index );
	}

	return Index;
}

void FTessellationTable::SplitEdge( uint32 TriIndex, uint32 EdgeIndex, uint32 LeftFactor, uint32 RightFactor, const FIntVector& TessFactors )
{
	/*
	===========
		v0
		/\
	e2 /  \ e0
	  /____\
	v2  e1  v1
	===========
	*/

	const uint32 e0 = EdgeIndex;
	const uint32 e1 = (1 << e0) & 3;
	const uint32 e2 = (1 << e1) & 3;

	const uint32 Triangle = Indexes[ TriIndex ];
	const uint32 i0 = ( Triangle >> (e0 * 10) ) & 1023;
	const uint32 i1 = ( Triangle >> (e1 * 10) ) & 1023;
	const uint32 i2 = ( Triangle >> (e2 * 10) ) & 1023;

#if 0
	// Sort verts for deterministic split
	uint32 v[2];
	v[0] = FMath::Min( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] );
	v[1] = FMath::Max( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] );

	uint32 OriginallyZero = 0;
	FIntVector Barycentrics[2];
	for( int j = 0; j < 2; j++ )
	{
		Barycentrics[j] = GetBarycentrics( v[j] );

		// Count how many were zero originally.
		OriginallyZero += Barycentrics[j].X == 0 ?  1 : 0;
		OriginallyZero += Barycentrics[j].Y == 0 ?  4 : 0;
		OriginallyZero += Barycentrics[j].Z == 0 ? 16 : 0;
	}

	FIntVector SplitBarycentrics = Barycentrics[0] * LeftFactor + Barycentrics[1] * RightFactor;

	for( uint32 i = 0; i < 3; i++ )
		SplitBarycentrics[i] = FMath::DivideAndRoundNearest( (uint32)SplitBarycentrics[i], LeftFactor + RightFactor );

	for( uint32 i = 0; i < 3; i++ )
	{
		// If both verts were originally zero then force split to be zero as well.
		if( ( OriginallyZero & 3 ) == 2 )
			SplitBarycentrics[i] = 0;

		OriginallyZero >>= 2;
	}
#else
	// Sort verts for deterministic split
	FIntVector SplitBarycentrics =
		GetBarycentrics( FMath::Min( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] ) ) * LeftFactor +
		GetBarycentrics( FMath::Max( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] ) ) * RightFactor;

	bool bOriginallyZero[3] =
	{
		SplitBarycentrics.X == 0,
		SplitBarycentrics.Y == 0,
		SplitBarycentrics.Z == 0,
	};

	for( uint32 i = 0; i < 3; i++ )
		SplitBarycentrics[i] = FMath::DivideAndRoundNearest( (uint32)SplitBarycentrics[i], LeftFactor + RightFactor );
#endif
	
	uint32 Largest = FMath::Max3Index( SplitBarycentrics[0], SplitBarycentrics[1], SplitBarycentrics[2] );
	uint32 Sum = SplitBarycentrics[0] + SplitBarycentrics[1] + SplitBarycentrics[2];
	SplitBarycentrics[ Largest ] += BarycentricMax - Sum;

	SnapAtEdges( SplitBarycentrics, TessFactors );

	check( SplitBarycentrics[0] + SplitBarycentrics[1] + SplitBarycentrics[2] == BarycentricMax );
	check( !bOriginallyZero[0] || SplitBarycentrics[0] == 0 );
	check( !bOriginallyZero[1] || SplitBarycentrics[1] == 0 );
	check( !bOriginallyZero[2] || SplitBarycentrics[2] == 0 );

	uint32 SplitVert = SplitBarycentrics[0] | ( SplitBarycentrics[1] << 16 );
	uint32 SplitIndex = AddVert( SplitVert );

	checkf( SplitIndex != i0 && SplitIndex != i1 && SplitIndex != i2, TEXT("Degenerate triangle generated") );
	
	// Replace v0
	Indexes.Add( SplitIndex | (i1 << 10) | (i2 << 20) );

	// Replace v1
	Indexes[ TriIndex ] = i0 | ( SplitIndex << 10 ) | (i2 << 20);
}

// Longest edge bisection. Uses Diagsplit rules instead of exact bisection.
void FTessellationTable::RecursiveSplit( const FIntVector& TessFactors )
{
	// Start with patch triangle
	Verts.Add( BarycentricMax + 0 );	// Avoids TArray:Add grabbing reference to constexpr and forcing ODR-use.
	Verts.Add( BarycentricMax << 16 );
	Verts.Add( 0 );

	Indexes.Add( 0 | (1 << 10) | (2 << 20) );

	HashTable.Clear();
	HashTable.Add( Verts[0], 0 );
	HashTable.Add( Verts[1], 1 );
	HashTable.Add( Verts[2], 2 );

	for( int32 TriIndex = FirstTri; TriIndex < Indexes.Num(); )
	{
		float EdgeLength2[3];
		for( uint32 i = 0; i < 3; i++ )
		{
			const uint32 e0 = i;
			const uint32 e1 = (1 << e0) & 3;

			const uint32 Triangle = Indexes[ TriIndex ];
			const uint32 i0 = ( Triangle >> (e0 * 10) ) & 1023;
			const uint32 i1 = ( Triangle >> (e1 * 10) ) & 1023;

			FIntVector b0 = GetBarycentrics( Verts[ FirstVert + i0 ] );
			FIntVector b1 = GetBarycentrics( Verts[ FirstVert + i1 ] );

			EdgeLength2[i] = LengthSquared( b0 - b1, TessFactors );
		}

		uint32 EdgeIndex = FMath::Max3Index( EdgeLength2[0], EdgeLength2[1], EdgeLength2[2] );
		check( EdgeLength2[ EdgeIndex ] >= 0.0f );

		uint32 NumEdgeSplits = FMath::RoundToInt( FMath::Sqrt( EdgeLength2[ EdgeIndex ] ) );
		uint32 HalfSplit = NumEdgeSplits >> 1;

		if( NumEdgeSplits <= 1 )
		{
			// Triangle is small enough
			TriIndex++;
			continue;
		}

		SplitEdge( TriIndex, EdgeIndex, HalfSplit, NumEdgeSplits - HalfSplit, TessFactors );
	}
}

void FTessellationTable::UniformTessellateAndSnap( const FIntVector& TessFactors )
{
	/*
	===========
		v0
		/\
	e2 /  \ e0
	  /____\
	v2  e1  v1
	===========
	*/

	HashTable.Clear();

	const uint32 NumTris = TessFactors[0] * TessFactors[0];

	for( uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++ )
	{
		/*
			Starts from top point. Adds rows of verts and corresponding rows of tri strips.

			|\
		row |\|\
			|\|\|\
			column
		*/

		// Find largest tessellation with NumTris <= TriIndex. These are the preceding tris before this row.
		uint32 TriRow = FMath::FloorToInt( FMath::Sqrt( (float)TriIndex ) );
		uint32 TriCol = TriIndex - TriRow * TriRow;
		/*
			Vert order:
			0    0__1
			|\   \  |
			| \   \ |  <= flip triangle
			|__\   \|
			2   1   2
		*/
		uint32 FlipTri = TriCol & 1;
		uint32 VertCol = TriCol >> 1;

		uint32 VertRowCol[3][2] =
		{
			{ TriRow,		VertCol		},
			{ TriRow + 1,	VertCol + 1	},
			{ TriRow + 1,	VertCol		},
		};
		VertRowCol[1][0] -= FlipTri;
		VertRowCol[2][1] += FlipTri;

		uint32 TriVerts[3];
		for( int Corner = 0; Corner < 3; Corner++ )
		{
			/*
				b0
				|\
			t2  | \  t0
				|__\
			   b2   b1
				 t1
			*/
			FIntVector Barycentrics;
			Barycentrics[0] = TessFactors[0] - VertRowCol[ Corner ][0];
			Barycentrics[1] = VertRowCol[ Corner ][1];
			Barycentrics[2] = VertRowCol[ Corner ][0] - VertRowCol[ Corner ][1];
			Barycentrics *= BarycentricMax;

			// Fixed point round
			Barycentrics[0] = ( Barycentrics[0] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics[1] = ( Barycentrics[1] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics[2] = ( Barycentrics[2] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics /= TessFactors[0];

			{
				const uint32 e0 = FMath::Max3Index( Barycentrics[0], Barycentrics[1], Barycentrics[2] );
				const uint32 e1 = (1 << e0) & 3;
				const uint32 e2 = (1 << e1) & 3;

				Barycentrics[ e0 ] = BarycentricMax - Barycentrics[ e1 ] - Barycentrics[ e2 ];
			}

#if 1
			for( uint32 i = 0; i < 3; i++ )
			{
				const uint32 e0 = i;
				const uint32 e1 = (1 << e0) & 3;
				const uint32 e2 = (1 << e1) & 3;

				if( Barycentrics[ e0 ] == 0 ||
					Barycentrics[ e1 ] == 0 ||
					Barycentrics[ e2 ] == 0 )
					continue;

				uint32 Sum = Barycentrics[ e0 ] + Barycentrics[ e1 ];

#if 0
				// Snap toward min barycentric means snapping mirrors.
				uint32 MinIndex = Barycentrics[ e0 ] <  Barycentrics[ e1 ] ? e0 : e1;
				uint32 MaxIndex = Barycentrics[ e0 ] >= Barycentrics[ e1 ] ? e0 : e1;

				// Fixed point round
				uint32 Snapped = ( Barycentrics[ MinIndex ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

				Barycentrics[ MinIndex ] = FMath::Min( Sum, Snapped / TessFactors[i] );
				Barycentrics[ MaxIndex ] = Sum - Barycentrics[ MinIndex ];

				if( Barycentrics[ MinIndex ] > Barycentrics[ MaxIndex ] )
				{
					Barycentrics[ e0 ] = Sum / 2;
					Barycentrics[ e1 ] = Sum - Barycentrics[ e0 ];
				}
#else
				// Fixed point round
				uint32 Snapped = ( Barycentrics[ e0 ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

				Barycentrics[ e0 ] = FMath::Min( Sum, Snapped / TessFactors[i] );
				Barycentrics[ e1 ] = Sum - Barycentrics[ e0 ];
#endif
			}
#endif

#if 1
			// Snap verts to the edge if they are close.
			if( Barycentrics.X != 0 &&
				Barycentrics.Y != 0 &&
				Barycentrics.Z != 0 )
			{
				// Find closest point on edge
				uint32 b0 = FMath::Min3Index( Barycentrics[0], Barycentrics[1], Barycentrics[2] );
				uint32 b1 = (1 << b0) & 3;
				uint32 b2 = (1 << b1) & 3;

				//if( Barycentrics[ b1 ] < Barycentrics[ b2 ] )
				//	Swap( b1, b2 );

				uint32 Sum = Barycentrics[ b1 ] + Barycentrics[ b2 ];

				FIntVector ClosestEdgePoint;
				ClosestEdgePoint[ b0 ] = 0;
				ClosestEdgePoint[ b1 ] = ( Barycentrics[ b1 ] * BarycentricMax ) / Sum;
				ClosestEdgePoint[ b2 ] = BarycentricMax - ClosestEdgePoint[ b1 ];

				// Want edge point in its final position so we get the correct distance.
				SnapAtEdges( ClosestEdgePoint, TessFactors );

				float DistSqr = LengthSquared( Barycentrics - ClosestEdgePoint, TessFactors );
				if( DistSqr < 0.25f )
				{
					Barycentrics = ClosestEdgePoint;
				}
			}
#endif

			SnapAtEdges( Barycentrics, TessFactors );

			TriVerts[ Corner ] = Barycentrics[0] | ( Barycentrics[1] << 16 );
		}

		// Degenerate
		if( TriVerts[0] == TriVerts[1] ||
			TriVerts[1] == TriVerts[2] ||
			TriVerts[2] == TriVerts[0] )
			continue;

		uint32 VertIndexes[3];
		for( int Corner = 0; Corner < 3; Corner++ )
			VertIndexes[ Corner ] = AddVert( TriVerts[ Corner ] );

		Indexes.Add( VertIndexes[0] | ( VertIndexes[1] << 10 ) | ( VertIndexes[2] << 20 ) );
	}
}


#define CACHE_WINDOW_SIZE	32

// Weights for individual cache entries based on simulated annealing optimization on DemoLevel.
static int16 CacheWeightTable[ CACHE_WINDOW_SIZE ] = {
	 577,	 616,	 641,  512,		 614,  635,  478,  651,
	  65,	 213,	 719,  490,		 213,  726,  863,  745,
	 172,	 939,	 805,  885,		 958, 1208, 1319, 1318,
	1475,	1779,	2342,  159,		2307, 1998, 1211,  932
};

// Constrain index buffer to only use vertex references that are within a fixed sized trailing window from the current highest encountered vertex index.
// Triangles are reordered based on a FIFO-style cache optimization to minimize the number of vertices that need to be duplicated.
void FTessellationTable::ConstrainToCacheWindow()
{
	uint32 NumOldVertices = Verts.Num() - FirstVert;
	uint32 NumOldTriangles = Indexes.Num() - FirstTri;

	check( NANITE_TESSELLATION_TABLE_SIZE <= 16 );
	constexpr uint32 MaxNumTris = 16 * 16;
	constexpr uint32 MaxTrianglesInDwords = ( MaxNumTris + 31 ) / 32;

	uint32 VertexToTriangleMasks[ MaxNumTris * 3 ][ MaxTrianglesInDwords ] = {};

	// Generate vertex to triangle masks
	for( uint32 i = 0; i < NumOldTriangles; i++ )
	{
		const uint32 i0 = ( Indexes[ FirstTri + i ] >>  0 ) & 1023;
		const uint32 i1 = ( Indexes[ FirstTri + i ] >> 10 ) & 1023;
		const uint32 i2 = ( Indexes[ FirstTri + i ] >> 20 ) & 1023;
		check( i0 != i1 && i1 != i2 && i2 != i0 ); // Degenerate input triangle!
		check( i0 < NumOldVertices && i1 < NumOldVertices && i2 < NumOldVertices );

		VertexToTriangleMasks[ i0 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i1 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i2 ][ i >> 5 ] |= 1 << ( i & 31 );
	}

	uint32 TrianglesEnabled[ MaxTrianglesInDwords ] = {};	// Enabled triangles are in the current material range and have not yet been visited.
	uint32 TrianglesTouched[ MaxTrianglesInDwords ] = {};	// Touched triangles have had at least one of their vertices visited.

	uint32 NumNewVertices = 0;
	uint32 NumNewTriangles = 0;
	uint16 OldToNewVertex[ MaxNumTris * 3 ];

	uint32 NewVerts[ MaxNumTris * 3 ] = {};	// Initialize to make static analysis happy
	uint32 NewIndexes[ MaxNumTris ];
	
	FMemory::Memset( OldToNewVertex, -1, sizeof( OldToNewVertex ) );

	uint32 DwordEnd	= NumOldTriangles / 32;
	uint32 BitEnd	= NumOldTriangles & 31;

	FMemory::Memset( TrianglesEnabled, -1, DwordEnd * sizeof( uint32 ) );

	if( BitEnd != 0 )
		TrianglesEnabled[ DwordEnd ] = ( 1u << BitEnd ) - 1u;

	auto ScoreVertex = [ &OldToNewVertex, &NumNewVertices ] ( uint32 OldVertex )
	{
		uint16 NewIndex = OldToNewVertex[ OldVertex ];

		int32 CacheScore = 0;
		if( NewIndex != 0xFFFF )
		{
			uint32 CachePosition = ( NumNewVertices - 1 ) - NewIndex;
			if( CachePosition < CACHE_WINDOW_SIZE )
				CacheScore = CacheWeightTable[ CachePosition ];
		}

		return CacheScore;
	};

	while( true )
	{
		uint32 NextTriangleIndex = 0xFFFF;
		int32  NextTriangleScore = 0;

		// Pick highest scoring available triangle
		for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MaxTrianglesInDwords; TriangleDwordIndex++ )
		{
			uint32 CandidateMask = TrianglesTouched[ TriangleDwordIndex ] & TrianglesEnabled[ TriangleDwordIndex ];
			while( CandidateMask )
			{
				uint32 TriangleDwordOffset = FMath::CountTrailingZeros( CandidateMask );
				CandidateMask &= CandidateMask - 1;

				int32 TriangleIndex = ( TriangleDwordIndex << 5 ) + TriangleDwordOffset;

				int32 TriangleScore = 0;
				TriangleScore += ScoreVertex( ( Indexes[ FirstTri + TriangleIndex ] >>  0 ) & 1023 );
				TriangleScore += ScoreVertex( ( Indexes[ FirstTri + TriangleIndex ] >> 10 ) & 1023 );
				TriangleScore += ScoreVertex( ( Indexes[ FirstTri + TriangleIndex ] >> 20 ) & 1023 );

				if( TriangleScore > NextTriangleScore )
				{
					NextTriangleIndex = TriangleIndex;
					NextTriangleScore = TriangleScore;
				}
			}
		}

		if( NextTriangleIndex == 0xFFFF )
		{
			// If we didn't find a triangle. It might be because it is part of a separate component. Look for an unvisited triangle to restart from.
			for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MaxTrianglesInDwords; TriangleDwordIndex++ )
			{
				uint32 EnableMask = TrianglesEnabled[ TriangleDwordIndex ];
				if( EnableMask )
				{
					NextTriangleIndex = ( TriangleDwordIndex << 5 ) + FMath::CountTrailingZeros( EnableMask );
					break;
				}
			}

			if( NextTriangleIndex == 0xFFFF )
				break;
		}

		uint32 OldIndex[3];
		OldIndex[0] = ( Indexes[ FirstTri + NextTriangleIndex ] >>  0 ) & 1023;
		OldIndex[1] = ( Indexes[ FirstTri + NextTriangleIndex ] >> 10 ) & 1023;
		OldIndex[2] = ( Indexes[ FirstTri + NextTriangleIndex ] >> 20 ) & 1023;

		// Mark incident triangles
		for( uint32 i = 0; i < MaxTrianglesInDwords; i++ )
		{
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[0] ][i];
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[1] ][i];
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[2] ][i];
		}

		uint32 NewIndex[3];
		NewIndex[0] = OldToNewVertex[ OldIndex[0] ];
		NewIndex[1] = OldToNewVertex[ OldIndex[1] ];
		NewIndex[2] = OldToNewVertex[ OldIndex[2] ];

		uint32 NumNew = (NewIndex[0] == 0xFFFF) + (NewIndex[1] == 0xFFFF) + (NewIndex[2] == 0xFFFF);

		// Generate new indices such that they are all within a trailing window of CACHE_WINDOW_SIZE of NumNewVertices.
		// This can require multiple iterations as new/duplicate vertices can push other vertices outside the window.			
		uint32 TestNumNewVertices = NumNewVertices;
		TestNumNewVertices += NumNew;

		while(true)
		{
			if (NewIndex[0] != 0xFFFF && TestNumNewVertices - NewIndex[0] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[0] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}

			if (NewIndex[1] != 0xFFFF && TestNumNewVertices - NewIndex[1] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[1] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}

			if (NewIndex[2] != 0xFFFF && TestNumNewVertices - NewIndex[2] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[2] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}
			break;
		}

		for( int k = 0; k < 3; k++ )
		{
			if( NewIndex[k] == 0xFFFF)
				NewIndex[k] = NumNewVertices++;

			OldToNewVertex[ OldIndex[k] ] = (uint16)NewIndex[k];

			NewVerts[ NewIndex[k] ] = Verts[ FirstVert + OldIndex[k] ];
		}

		// Rotate triangle such that 1st index is smallest
		const uint32 i0 = FMath::Min3Index( NewIndex[0], NewIndex[1], NewIndex[2] );
		const uint32 i1 = (1 << i0) & 3;
		const uint32 i2 = (1 << i1) & 3;

		// Output triangle
		NewIndexes[ NumNewTriangles++ ] = NewIndex[ i0 ] | ( NewIndex[ i1 ] << 10 ) | ( NewIndex[ i2 ] << 20 );

		// Disable selected triangle
		TrianglesEnabled[ NextTriangleIndex >> 5 ] &= ~( 1 << ( NextTriangleIndex & 31 ) );
	}


	check( NumNewTriangles == NumOldTriangles );

	if( NumNewVertices > NumOldVertices )
		Verts.AddUninitialized( NumNewVertices - NumOldVertices );
	check(NumNewVertices == NumOldVertices);

	// Write back new triangle order
	FMemory::Memcpy( &Verts[ FirstVert ],	NewVerts,	NumNewVertices * sizeof( uint32 ) );
	FMemory::Memcpy( &Indexes[ FirstTri ],	NewIndexes,	NumNewTriangles * sizeof( uint32 ) );
}

void FTessellationTable::ConstrainForImmediateTessellation()
{
	// Constrain such that the tessellation pattern has the same number of triangles and vertices.
	// Triangles can only references vertices with an index lower or equal to the current triangle index.
	// Vertex references can reference at most 32 back from the triangle index.
	// Each triangle adds one new vertex and has two vertex references.
	// The new vertex is always the first of the 3 indices.

	const uint32 NumOldVerts = Verts.Num() - FirstVert;
	const uint32 NumOldTris = Indexes.Num() - FirstTri;
	
	const uint32 InvalidVert = 0xFFFFu;

	TArray<uint16> OldToNewVertex;
	OldToNewVertex.Init( InvalidVert, NumOldVerts );
	
	TArray<uint32> NewVerts;
	TArray<uint32> NewTris;
	for (uint32 OldTriIndex = 0; OldTriIndex < NumOldTris; OldTriIndex++)
	{
		const uint32 IndexData = Indexes[FirstTri + OldTriIndex];
		const uint32 Index0 = IndexData & 0x3FFu;
		const uint32 Index1 = (IndexData >> 10) & 0x3FFu;
		const uint32 Index2 = IndexData >> 20;

		uint32 NumAddedVerts = 0;
		while (true)
		{
			if (OldToNewVertex[Index0] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index0] > 32)
			{
				OldToNewVertex[Index0] = NewVerts.Num();
				NewVerts.Add(Verts[FirstVert + Index0]);
				NumAddedVerts++;
				continue;
			}

			if (OldToNewVertex[Index1] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index1] > 32)
			{
				OldToNewVertex[Index1] = NewVerts.Num();
				NewVerts.Add(Verts[FirstVert + Index1]);
				NumAddedVerts++;
				continue;
			}

			if (OldToNewVertex[Index2] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index2] > 32)
			{
				OldToNewVertex[Index2] = NewVerts.Num();
				NewVerts.Add(Verts[FirstVert + Index2]);
				NumAddedVerts++;
				continue;
			}

			if (NumAddedVerts == 0)
			{
				// No new vertices needed.
				// Arbitrarily duplicate the Index0 vertex.
				const uint32 Data = NewVerts[OldToNewVertex[Index0]];
				OldToNewVertex[Index0] = NewVerts.Num();
				NewVerts.Add(Data);
				NumAddedVerts++;
				continue;
			}

			break;
		}

		// Add any degenerate triangles
		for (uint32 i = 0; i + 1 < NumAddedVerts; i++)
		{
			const uint32 Index = NewVerts.Num() - NumAddedVerts;
			NewTris.Add( (Index << 20) | (Index << 10) | Index );
		}
		check(NumAddedVerts != 0);

		// Add triangle
		{
			uint32 I0 = OldToNewVertex[Index0];
			uint32 I1 = OldToNewVertex[Index1];
			uint32 I2 = OldToNewVertex[Index2];

			// Rotate such that first index is the highest one
			while (I1 > I0 || I2 > I0)
			{
				uint32 Tmp = I0;
				I0 = I1; I1 = I2; I2 = Tmp;
			}
			check(I0 == NewTris.Num());
			NewTris.Add( (I2 << 20) | (I1 << 10) | I0 );
		}
		check(NewVerts.Num() == NewTris.Num());
	}

	check((uint32)NewVerts.Num() >= NumOldVerts);
	check((uint32)NewTris.Num() >= NumOldTris);

	if ((uint32)NewVerts.Num() > NumOldVerts)
		Verts.AddUninitialized(NewVerts.Num() - NumOldVerts);

	if ((uint32)NewTris.Num() > NumOldTris)
		Indexes.AddUninitialized(NewTris.Num() - NumOldTris);

	// Write back new triangle order
	FMemory::Memcpy( &Verts[ FirstVert ],	NewVerts.GetData(), NewVerts.Num() * sizeof(uint32));
	FMemory::Memcpy( &Indexes[ FirstTri ],	NewTris.GetData(),	NewTris.Num() * sizeof(uint32));
}

FTessellationTable& GetTessellationTable()
{
	static FTessellationTable TessellationTable;
	return TessellationTable;
}

void FTessellationTable::AddToVertsAndIndices( bool bImmediate, uint32 TessFactorX, uint32 TessFactorY, uint32 TessFactorZ )
{
	uint32 Index =	(TessFactorZ - 1u) * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE +
					(TessFactorY - 1u) * NANITE_TESSELLATION_TABLE_PO2_SIZE +
					(TessFactorX - 1u);
	
	if (bImmediate) Index += NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE;

	const uint32 NumVerts = Verts.Num() - FirstVert;
	const uint32 NumTris = Indexes.Num() - FirstTri;

	check(NumVerts < 512);
	check(NumTris < 1024);

	OffsetTable[Index].X = VertsAndIndexes.Num();
	OffsetTable[Index].Y = /* Pattern | */ (NumVerts << 13) | (NumTris << 22);

	VertsAndIndexes.Append( Verts.GetData() + FirstVert, NumVerts );
	VertsAndIndexes.Append( Indexes.GetData() + FirstTri, NumTris );
}

#if TESSELLATION_TABLE_DUMP_SVG
void FTessellationTable::DumpSVG(const char* Filename)
{
	FILE* File = nullptr;
	fopen_s(&File, Filename, "wb");

	const uint32 NumVerts = Verts.Num() - FirstVert;
	const uint32 NumTris = Indexes.Num() - FirstTri;

	fputs(R"xyz(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg viewBox="0 0 1024 1024" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
<rect fill="#fff" stroke="#000" x="0" y="0" width="1024" height="1024"/>
<g opacity="0.8">
)xyz", File);
	const FVector2f PatchCorner0 = FVector2f(512, 0);
	const FVector2f PatchCorner1 = FVector2f(1023, 1023);
	const FVector2f PatchCorner2 = FVector2f(0, 1023);
	for (uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++)
	{
		const uint32 TriData = Indexes[FirstTri + TriIndex];
		const uint32 Index0 = TriData & 0x3FFu;
		const uint32 Index1 = (TriData >> 10) & 0x3FFu;
		const uint32 Index2 = (TriData >> 20) & 0x3FFu;

		const FIntVector Barycentrics0 = GetBarycentrics(Verts[FirstVert + Index0]);
		const FIntVector Barycentrics1 = GetBarycentrics(Verts[FirstVert + Index1]);
		const FIntVector Barycentrics2 = GetBarycentrics(Verts[FirstVert + Index2]);
		const FVector2f TriCorner0 = (PatchCorner0 * Barycentrics0.X + PatchCorner1 * Barycentrics0.Y + PatchCorner2 * Barycentrics0.Z) * (1.0f / BarycentricMax);
		const FVector2f TriCorner1 = (PatchCorner0 * Barycentrics1.X + PatchCorner1 * Barycentrics1.Y + PatchCorner2 * Barycentrics1.Z) * (1.0f / BarycentricMax);
		const FVector2f TriCorner2 = (PatchCorner0 * Barycentrics2.X + PatchCorner1 * Barycentrics2.Y + PatchCorner2 * Barycentrics2.Z) * (1.0f / BarycentricMax);

		//const uint32 ColorR = NumTris ? (TriIndex * 255) / (NumTris - 1) : 0xFFu;
		const uint32 ColorR = 255;
		const uint32 ColorG = 255;
		const uint32 ColorB = 255;

		fprintf(File, "\t<polyline points = \"%d,%d %d,%d %d,%d %d,%d\" stroke = \"black\" stroke-width = \"4\" fill = \"#%02x%02x%02x\" />\n",
			int(TriCorner0.X), int(TriCorner0.Y),
			int(TriCorner1.X), int(TriCorner1.Y),
			int(TriCorner2.X), int(TriCorner2.Y),
			int(TriCorner0.X), int(TriCorner0.Y),
			ColorR, ColorG, ColorB);
	}

	fputs("(</g></svg>", File);

	fclose(File);
}
#endif

} // namespace Nanite