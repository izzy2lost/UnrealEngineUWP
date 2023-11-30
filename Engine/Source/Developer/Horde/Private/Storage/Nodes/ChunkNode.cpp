// Copyright Epic Games, Inc. All Rights Reserved.

#include "Storage/Nodes/ChunkNode.h"
#include "Storage/Blob.h"

FChunkNode::~FChunkNode()
{
}

// ----------------------------------------------------------------------

FChunkNodeRef::FChunkNodeRef(FBlobHandle InHandle, const FIoHash& InHash)
	: Handle(MoveTemp(InHandle))
	, Hash(InHash)
{ }

FChunkNodeRef::~FChunkNodeRef()
{ }

// ----------------------------------------------------------------------

const FBlobType FLeafChunkNode::BlobType(FGuid(0xB27AFB68, 0x4A4B9E20, 0x8A78D8A4, 0x39D49840), 1);

FLeafChunkNode::FLeafChunkNode(FSharedBufferView InBuffer)
	: Buffer(MoveTemp(InBuffer))
{ }

FLeafChunkNode::~FLeafChunkNode()
{ }

FLeafChunkNode FLeafChunkNode::Read(FBlob Blob)
{
	return FLeafChunkNode(MoveTemp(Blob.Data));
}

void FLeafChunkNode::Write(FBlobWriter& Writer)
{
	void* Target = Writer.GetOutputBuffer(Buffer.GetLength());
	memcpy(Target, Buffer.GetPointer(), Buffer.GetLength());
	Writer.Advance(Buffer.GetLength());
}

// ----------------------------------------------------------------------

const FBlobType FInteriorChunkNode::BlobType(FGuid(0xF4DEDDBC, 0x4C7A70CB, 0x11F04783, 0xB9CDCCAF), 2);

FInteriorChunkNode::FInteriorChunkNode()
{
}

FInteriorChunkNode::~FInteriorChunkNode()
{
}

FInteriorChunkNode FInteriorChunkNode::Read(FBlob Blob)
{
	FInteriorChunkNode Node;

	int32 NumNodes = (int32)(Blob.Data.GetLength() / sizeof(FIoHash));
	Node.Children.Reserve(NumNodes);

	const FIoHash* Hashes = (const FIoHash*)Blob.Data.GetPointer();
	for (int32 Idx = 0; Idx < NumNodes; Idx++)
	{
		Node.Children.Add(FChunkNodeRef(MoveTemp(Blob.References[Idx]), Hashes[Idx]));
	}

	return MoveTemp(Node);
}

void FInteriorChunkNode::Write(FBlobWriter& Writer) const
{
	FIoHash* Hashes = (FIoHash*)Writer.GetOutputBuffer(sizeof(FIoHash) * Children.Num());
	for (int32 Idx = 0; Idx < Children.Num(); Idx++)
	{
		Hashes[Idx] = Children[Idx].Hash;
		Writer.AddImport(Children[Idx].Handle);
	}
	Writer.Advance(sizeof(FIoHash) * Children.Num());
}

// ----------------------------------------------------------------------

struct FChunkNodeReader::FStackEntry
{
	FBlob Blob;
	size_t Position;

	FStackEntry(FBlob InBlob)
		: Blob(MoveTemp(InBlob))
		, Position(0)
	{ }
};

FChunkNodeReader::FChunkNodeReader(FBlob Blob)
{
	Stack.Add(FStackEntry(MoveTemp(Blob)));
	Advance(0);
}

FChunkNodeReader::FChunkNodeReader(const FBlobHandle& Handle)
	: FChunkNodeReader(Handle->Read())
{
}

FChunkNodeReader::~FChunkNodeReader()
{
}

bool FChunkNodeReader::IsEof() const
{
	return Stack.Num() == 0;
}

FMemoryView FChunkNodeReader::GetBuffer() const
{
	if (Stack.Num() == 0)
	{
		return FMemoryView();
	}

	const FStackEntry& StackTop = Stack.Top();
	check(StackTop.Blob.Type.Guid == FLeafChunkNode::BlobType.Guid);

	FMemoryView View = StackTop.Blob.Data.GetView();
	return View.Mid(StackTop.Position);
}

void FChunkNodeReader::Advance(int32 Size)
{
	while (Stack.Num() > 0)
	{
		FStackEntry& StackTop = Stack.Top();
		if (StackTop.Blob.Type.Guid == FLeafChunkNode::BlobType.Guid)
		{
			FMemoryView BlobData = StackTop.Blob.Data.GetView();

			size_t ChunkSize = FMath::Min<size_t>(Size, BlobData.GetSize() - StackTop.Position);
			StackTop.Position += ChunkSize;
			Size -= ChunkSize;

			if (StackTop.Position < BlobData.GetSize())
			{
				break;
			}

			Stack.Pop();
		}
		else if (StackTop.Blob.Type.Guid == FInteriorChunkNode::BlobType.Guid)
		{
			FBlobHandle ChildHandle = StackTop.Blob.References[StackTop.Position];

			StackTop.Position++;
			if (StackTop.Position == StackTop.Blob.References.Num())
			{
				Stack.Pop();
			}

			Stack.Add(FStackEntry(ChildHandle->Read()));
		}
		else
		{
			// Invalid blob type
			check(false);
		}
	}
}
