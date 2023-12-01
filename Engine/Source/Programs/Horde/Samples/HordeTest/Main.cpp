// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Storage/StorageClient.h"
#include "Storage/Clients/BundleStorageClient.h"
#include "Storage/Clients/MemoryStorageClient.h"
#include "Storage/Clients/FileStorageClient.h"
#include "Storage/Nodes/ChunkNode.h"
#include "Storage/Nodes/DirectoryNode.h"
#include "Storage/BlobWriter.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <assert.h>
#include "Memory/SharedBuffer.h"
#include "Storage/ChunkedBufferWriter.h"

extern TCHAR GInternalProjectName[64] = { 0, };
extern const TCHAR* GForeignEngineDir = nullptr;

int CreateBundle(const std::filesystem::path& InputDir, const std::filesystem::path& OutputFile);
int ExtractBundle(const std::filesystem::path& InputFile, const std::filesystem::path& OutputDir);

int main(int ArgC, const char* ArgV[])
{
	if (ArgC == 5 && FCStringAnsi::Stricmp(ArgV[1], "bundle") == 0 && FCStringAnsi::Stricmp(ArgV[2], "create") == 0)
	{
		std::filesystem::path InputDir = ArgV[3];
		std::filesystem::path OutputFile = ArgV[4];
		return CreateBundle(InputDir, OutputFile);
	}

	if (ArgC == 5 && FCStringAnsi::Stricmp(ArgV[1], "bundle") == 0 && FCStringAnsi::Stricmp(ArgV[2], "extract") == 0)
	{
		std::filesystem::path InputFile = ArgV[3];
		std::filesystem::path OutputDir = ArgV[4];
		return ExtractBundle(InputFile, OutputDir);
	}

	printf("Arguments:\n");
	printf("\n");
	printf("  bundle create <InputDir> <OutputFile>\n");
	printf("  bundle extract <InputFile> <OutputDir>\n");
	return 1;
}

// --------------------------------------------------------------------------------------------------------

FBlobHandleWithHash CreateFromStream(std::ifstream& Stream, FBlobWriter& Writer, int64& OutLength, FIoHash& OutStreamHash)
{
	OutLength = 0;

	FChunkNodeWriter ChunkWriter(Writer);

	char ReadBuffer[4096];
	while (!Stream.eof())
	{
		Stream.read(ReadBuffer, sizeof(ReadBuffer));
		
		int64 ReadSize = Stream.gcount();
		if (ReadSize == 0)
		{
			break;
		}
		OutLength += ReadSize;

		ChunkWriter.Write(FMemoryView(ReadBuffer, ReadSize));
	}

	return ChunkWriter.Flush(OutStreamHash);
}

FFileEntry CreateFromFile(const std::filesystem::path& Path, FBlobWriter& Writer)
{
	std::ifstream InputStream(Path, std::ios::binary);
	check(InputStream);
	
	int64 Length = 0;
	FIoHash StreamHash;
	FBlobHandleWithHash Target = CreateFromStream(InputStream, Writer, Length, StreamHash);

	return FFileEntry(Target, FUtf8String(Path.filename().string().c_str()), EFileEntryFlags::None, Length, StreamHash, FSharedBufferView());
}

FDirectoryEntry CreateFromDirectory(const std::filesystem::path& Path, FBlobWriter& Writer)
{
	printf("Found %ws\n", Path.c_str());
	int64 Length = 0;

	FDirectoryNode DirectoryNode;
	for (const std::filesystem::directory_entry Entry : std::filesystem::directory_iterator(Path))
	{
		if (Entry.is_directory())
		{
			FDirectoryEntry NewEntry = CreateFromDirectory(Entry.path(), Writer);
			FUtf8String Name = NewEntry.Name;
			DirectoryNode.NameToDirectory.Add(Name, MoveTemp(NewEntry));
			Length += NewEntry.Length;
		}
		else if (Entry.is_regular_file())
		{
			FFileEntry NewEntry = CreateFromFile(Entry.path(), Writer);
			FUtf8String Name = NewEntry.Name;
			DirectoryNode.NameToFile.Add(Name, MoveTemp(NewEntry));
			Length += NewEntry.Length;
		}
	}

	FBlobHandle DirectoryHandle = DirectoryNode.Write(Writer);

	return FDirectoryEntry(DirectoryHandle, FIoHash(), FUtf8String(Path.filename().string().c_str()), Length);
}

int CreateBundle(const std::filesystem::path& InputDir, const std::filesystem::path& OutputFile)
{
	TSharedRef<FFileStorageClient> FileStorage = MakeShared<FFileStorageClient>(OutputFile.parent_path());
	TSharedRef<FBundleStorageClient> Storage = MakeShared<FBundleStorageClient>(FileStorage);

	TUniquePtr<FBlobWriter> Writer = Storage->CreateWriter("");
	FDirectoryEntry RootEntry = CreateFromDirectory(InputDir, *Writer.Get());
	Writer->Flush();

	FFileStorageClient::WriteRefToFile(OutputFile, RootEntry.Target->GetLocator());
	return 0;
}

// --------------------------------------------------------------------------------------------------------

void ExtractFile(const std::filesystem::path& Path, const FBlobHandle& Handle)
{
	printf("Extracting %ws\n", Path.c_str());

	std::ofstream Stream(Path, std::ios::binary);

	FChunkNodeReader ChunkReader(Handle);
	while (ChunkReader)
	{
		FMemoryView View = ChunkReader.GetBuffer();
		Stream.write((const char*)View.GetData(), View.GetSize());
		ChunkReader.Advance(View.GetSize());
	}
}

void ExtractDirectory(const std::filesystem::path& Path, const FBlobHandle& Handle)
{
	printf("Extracting %ws\n", Path.c_str());
	std::filesystem::create_directory(Path);

	FBlob Blob = Handle->Read();
	FDirectoryNode Directory = FDirectoryNode::Read(Blob);

	for (TMap<FUtf8String, FDirectoryEntry>::TConstIterator Iter(Directory.NameToDirectory); Iter; ++Iter)
	{
		ExtractDirectory(Path / std::string((const char*)*Iter.Key()), Iter.Value().Target);
	}

	for (TMap<FUtf8String, FFileEntry>::TConstIterator Iter(Directory.NameToFile); Iter; ++Iter)
	{
		ExtractFile(Path / std::string((const char*)*Iter.Key()), Iter.Value().Target.Handle);
	}
}

int ExtractBundle(const std::filesystem::path& InputFile, const std::filesystem::path& OutputDir)
{
	TSharedRef<FFileStorageClient> FileStorage = MakeShared<FFileStorageClient>(InputFile.parent_path());
	TSharedRef<FBundleStorageClient> Storage = MakeShared<FBundleStorageClient>(FileStorage);

	FBlobHandle Handle = Storage->CreateHandle(FFileStorageClient::ReadRefFromFile(InputFile));
	ExtractDirectory(OutputDir, Handle);

	return 0;
}
