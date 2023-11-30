// Copyright Epic Games, Inc. All Rights Reserved.

#include "Storage/BlobReader.h"
#include "Containers/Utf8String.h"
#include "IO/IoHash.h"
#include "HordePlatform.h"

// ------------------------------------------------------------------------

FBlobReader::FBlobReader(const FBlob& InBlob)
	: FBlobReader(InBlob.Data.GetView(), InBlob.References)
{
}

FBlobReader::FBlobReader(const FMemoryView& InBuffer, const TArray<FBlobHandle>& InImports)
	: Buffer(InBuffer)
	, Imports(InImports)
	, NextImportIdx(0)
{
}

const unsigned char* FBlobReader::GetBuffer() const
{
	return (const unsigned char*)GetView().GetData();
}

FMemoryView FBlobReader::GetView() const
{
	return Buffer;
}

void FBlobReader::Advance(size_t Size)
{
	Buffer = Buffer.Mid(Size);
}

FBlobHandle FBlobReader::ReadImport()
{
	return Imports[NextImportIdx++];
}

// ------------------------------------------------------------------------

int ReadInt32(FBlobReader& Reader)
{
	int Value = *(const int*)Reader.GetBuffer();
	Reader.Advance(sizeof(int));
	return Value;
}

FIoHash ReadIoHash(FBlobReader& Reader)
{
	FIoHash Hash;
	memcpy(&Hash, Reader.GetBuffer(), sizeof(FIoHash));
	Reader.Advance(sizeof(FIoHash));
	return Hash;
}

FMemoryView ReadFixedLengthBytes(FBlobReader& Reader, size_t Length)
{
	FMemoryView View = Reader.GetView();
	Reader.Advance(Length);
	return View.Left(Length);
}

size_t ReadUnsignedVarInt(FBlobReader& Reader)
{
	// Figure out the length of the buffer
	const unsigned char* Data = Reader.GetBuffer();
	size_t NumBytes = FHordePlatform::CountLeadingZeros((unsigned char)(~*Data)) - 23;

	// Decode the value
	size_t value = (size_t)(Data[0] & (0xff >> NumBytes));
	for (int i = 1; i < NumBytes; i++)
	{
		value <<= 8;
		value |= Data[i];
	}

	Reader.Advance(NumBytes);
	return value;
}

FUtf8String ReadString(FBlobReader& Reader)
{
	FMemoryView String = ReadStringSpan(Reader);
	return FUtf8String::ConstructFromPtrSize((const UTF8CHAR*)String.GetData(), String.GetSize());
}

FMemoryView ReadStringSpan(FBlobReader& Reader)
{
	size_t Length = ReadUnsignedVarInt(Reader);
	return ReadFixedLengthBytes(Reader, Length);
}

