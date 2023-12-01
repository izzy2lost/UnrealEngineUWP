// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blob.h"
#include "BlobHandle.h"
#include "BlobLocator.h"
#include "RefName.h"

struct FIoHash;

/**
 * Reads data from a blob object
 */
class FBlobReader
{
public:
	FBlobReader(const FBlob& InBlob);
	FBlobReader(const FMemoryView& InBuffer, const TArray<FBlobHandle>& InImports);

	/** Gets a pointer to the remaining memory. */
	const unsigned char* GetBuffer() const;

	/** Gets the remaining memory to read from. */
	FMemoryView GetView() const;

	/** Advance the current read position. */
	void Advance(size_t Size);

	/** Reads the next import. */
	FBlobHandle ReadImport();

private:
	FMemoryView Buffer;
	const TArray<FBlobHandle>& Imports;
	int32 NextImportIdx;
};

// ------------------------------------------------------------------------

FBlobHandle ReadBlobHandle(FBlobReader& Reader);
FBlobHandleWithHash ReadBlobHandleWithHash(FBlobReader& Reader);
int ReadInt32(FBlobReader& Reader);
FIoHash ReadIoHash(FBlobReader& Reader);
FMemoryView ReadFixedLengthBytes(FBlobReader& Reader, size_t Length);
size_t ReadUnsignedVarInt(FBlobReader& Reader);
FUtf8String ReadString(FBlobReader& Reader);
FMemoryView ReadStringSpan(FBlobReader& Reader);
