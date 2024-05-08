// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileUtilities/ZipArchiveWriter.h"

#if WITH_ENGINE

#include "Containers/Utf8String.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "ZipArchivePrivate.h"

DEFINE_LOG_CATEGORY(LogZipArchive);

FZipArchiveWriter::FZipArchiveWriter(IFileHandle* InFile)
	: File(InFile)
{
}

FZipArchiveWriter::~FZipArchiveWriter()
{
	// Zip File Format Specification:
	// https://www.loc.gov/preservation/digital/formats/digformatspecs/APPNOTE%2820120901%29_Version_6.3.3.txt

	UE_LOG(LogZipArchive, Display, TEXT("Closing zip file with %d entries."), Files.Num());

	// Write the file directory
	uint64 DirStartOffset = Tell();
	for (FFileEntry& Entry : Files)
	{
		// Central directory File header: (from specification linked above)
		const static uint8 Footer[] =
		{
			0x50, 0x4b, 0x01, 0x02,  // Central file header signature
			0x3f, 0x00,  // Version made by (MS-DOS - v6.3)
			0x2d, 0x00,  // Version needed to extract (MS-DOS - v4.5)
			0x00, 0x08,  // General purpose bit flag (Language encoding flag = 1)
			0x00, 0x00  // Compression method (none)
		};
		Write((void*)Footer, sizeof(Footer));
		Write(Entry.Time);
		Write(Entry.Crc32);

		// Compressed and Uncompressed size - unused.
		Write((uint64)0xffffffffffffffff);

		FUtf8String UTF8Filename = *Entry.Filename;
		Write((uint16)UTF8Filename.Len());
		const static uint8 Fields[] =
		{
			0x20, 0x00, // Length of extra fields (Zip64 Extended Information)
			0x00, 0x00, // File comment length
			0x00, 0x00, // Disk number start
			0x00, 0x00, // Internal file attributes
			0x20, 0x00, 0x00, 0x00, // External file attributes
			0xff, 0xff, 0xff, 0xff // Relative offset of local header
		};
		Write((void*)Fields, sizeof(Fields));
		Write((void*)GetData(UTF8Filename), UTF8Filename.Len());

		// Zip64 Extended Information block
		Write((uint16)0x01);
		Write((uint16)0x1c);

		Write((uint64)Entry.Length);
		Write((uint64)Entry.Length);
		Write((uint64)Entry.Offset);
		Write((uint32)0);

		Flush();
	}
	uint64 DirEndOffset = Tell();

	uint64 DirectorySizeInBytes = DirEndOffset - DirStartOffset;

	// Write ZIP64 end of central directory record
	const static uint8 Record[] =
	{
		0x50, 0x4b, 0x06, 0x06, 0x2c, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x2d, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	Write((void*)Record, sizeof(Record));
	Write((uint64)Files.Num());
	Write((uint64)Files.Num());
	Write(DirectorySizeInBytes);
	Write(DirStartOffset);

	// Write ZIP64 end of central directory locator
	const static uint8 Locator[] =
	{
		0x50, 0x4b, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00,
	};
	Write((void*)Locator, sizeof(Locator));
	Write(DirEndOffset);
	Write((uint32)0x01);

	// Write normal end of central directory record
	const static uint8 EndRecord[] =
	{
		0x50, 0x4b, 0x05, 0x06, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00
	};
	Write((void*)EndRecord, sizeof(EndRecord));

	Flush();

	if (File)
	{
		// Close the file
		delete File;
		File = nullptr;
	}
}

void FZipArchiveWriter::AddFile(const FString& Filename, TConstArrayView<uint8> Data, const FDateTime& Timestamp)
{
	if (!ensureMsgf(!Filename.IsEmpty(), TEXT("Failed to write data to zip file; filename is empty.")))
	{
		return;
	}
	uint32 Crc = FCrc::MemCrc32(Data.GetData(), Data.Num());

	// Convert the date-time to a zip file timestamp (2-second resolution).
	uint32 ZipTime =
		(Timestamp.GetSecond() / 2) |
		(Timestamp.GetMinute() << 5) |
		(Timestamp.GetHour() << 11) |
		(Timestamp.GetDay() << 16) |
		(Timestamp.GetMonth() << 21) |
		((Timestamp.GetYear() - 1980) << 25);

	uint64 FileOffset = Tell();

	FFileEntry* Entry = new (Files) FFileEntry(Filename, Crc, Data.Num(), FileOffset, ZipTime);

	// Local File Header
	static const uint8 Header[] =
	{
		0x50, 0x4b, 0x03, 0x04, // Local file header signature
		0x2d, 0x00, // Version needed to extract (MS DOS - v4.5)
		0x00, 0x08, // General purpose bit flag (Language encoding flag = 1)
		0x00, 0x00 // Compression method (none)
	};
	Write((void*)Header, sizeof(Header));
	Write(ZipTime);
	Write(Crc);

	// Compressed and Uncompressed size - unused.
	Write((uint64)0xffffffffffffffff);

	FUtf8String UTF8Filename = *Entry->Filename;
	Write((uint16)UTF8Filename.Len());
	Write((uint16)0x20); // Length of extra fields (Zip64 Extended Information)
	Write((void*)GetData(UTF8Filename), UTF8Filename.Len());

	// Zip64 Extended Information block
	Write((uint16)0x01);
	Write((uint16)0x1c);
	Write((uint64)Data.Num());
	Write((uint64)Data.Num());
	Write((uint64)FileOffset);
	Write((uint32)0);

	Write((void*)Data.GetData(), Data.Num());

	Flush();
}

void FZipArchiveWriter::AddFile(const FString& Filename, const TArray<uint8>& Data, const FDateTime& Timestamp)
{
	AddFile(Filename, TConstArrayView<uint8>(Data), Timestamp);
}

void FZipArchiveWriter::Flush()
{
	if (Buffer.Num())
	{
		if (File && !File->Write(Buffer.GetData(), Buffer.Num()))
		{
			UE_LOG(LogZipArchive, Error, TEXT("Failed to write to zip file. Zip file writing aborted."));
			delete File;
			File = nullptr;
		}

		Buffer.Reset(Buffer.Num());
	}
}

#endif