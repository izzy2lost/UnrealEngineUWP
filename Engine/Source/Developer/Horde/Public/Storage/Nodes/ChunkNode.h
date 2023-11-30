// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"
#include "../BlobType.h"
#include "../BlobHandle.h"
#include "../BlobWriter.h"
#include "../../SharedBufferView.h"
#include "Hash/Blake3.h"

/**
 * Base class for chunked data nodes
 */
class FChunkNode
{
public:
	virtual ~FChunkNode();
};

/**
 * Hashed reference to a chunked data node
 */
struct FChunkNodeRef
{
	FBlobHandle Handle;
	FIoHash Hash;

	FChunkNodeRef(FBlobHandle InHandle, const FIoHash& InHash);
	~FChunkNodeRef();
};

/**
 * Chunked data node containing a leaf of chunked data
 */
class FLeafChunkNode final : public FChunkNode
{
public:
	static const FBlobType BlobType;

	const FSharedBufferView Buffer;

	FLeafChunkNode(FSharedBufferView InBuffer);
	virtual ~FLeafChunkNode() override;

	static FLeafChunkNode Read(FBlob Blob);
	void Write(FBlobWriter& Writer);
};

/**
 * An interior file node
 */
class FInteriorChunkNode final : public FChunkNode
{
public:
	static const FBlobType BlobType;

	TArray<FChunkNodeRef> Children;

	FInteriorChunkNode();
	virtual ~FInteriorChunkNode() override;

	static FInteriorChunkNode Read(FBlob Blob);
	void Write(FBlobWriter& Writer) const;
};

/**
 * Utility class for reading data a data stream from a tree of chunk nodes
 */
class FChunkNodeReader
{
public:
	FChunkNodeReader(FBlob Blob);
	FChunkNodeReader(const FBlobHandle& Handle);
	~FChunkNodeReader();

	bool IsEof() const;
	FMemoryView GetBuffer() const;
	void Advance(int32 Length);

private:
	struct FStackEntry;
	TArray<FStackEntry> Stack;
};
