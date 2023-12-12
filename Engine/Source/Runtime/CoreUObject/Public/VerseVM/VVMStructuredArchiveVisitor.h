// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Serialization/StructuredArchiveSlotBase.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "VVMAbstractVisitor.h"

class FStructuredArchive;

namespace Verse
{
struct VCppClassInfo;

struct FStructuredArchiveVisitor : FAbstractVisitor
{
	FStructuredArchiveVisitor(FAllocationContext InContext, FStructuredArchive& InStructuredArchive)
		: Context(InContext)
		, StructuredArchive(InStructuredArchive)
	{
	}

	void Serialize(VCell*& InOutCell);

	virtual void BeginArray(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndArray() override;
	virtual void BeginSet(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndSet() override;
	virtual void BeginMap(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndMap() override;
	virtual void BeginObject(const TCHAR* ElementName = nullptr) override;
	virtual void EndObject() override;
	virtual void VisitNonNull(VCell*& InCell, const TCHAR* ElementName) override;
	virtual void VisitEmergentType(const VCell* InEmergentType) override;
	virtual void VisitNonNull(UObject* InObject, const TCHAR* ElementName) override;
	virtual void Visit(VCell*& InCell, const TCHAR* ElementName) override;
	virtual void Visit(UObject* InObject, const TCHAR* ElementName) override;
	virtual void Visit(VValue& Value, const TCHAR* ElementName) override;
	virtual void Visit(VRestValue& Value, const TCHAR* ElementName) override;
	virtual void Visit(bool& bValue, const TCHAR* ElementName) override;
	virtual void Visit(FString& Value, const TCHAR* ElementName) override;
	virtual void Visit(uint64& Value, const TCHAR* ElementName) override;
	virtual void Visit(int64& Value, const TCHAR* ElementName) override;

	virtual FArchive* GetUnderlyingArchive() override;
	virtual bool IsLoading() override;
	virtual bool IsTextFormat() override;
	virtual FAccessContext GetLoadingContext() override;

private:
	enum class ENestingType : uint8
	{
		None,
		Object,
		Array,
		Set,
		Map,
	};

	enum class EEncodedType : uint8
	{
		None,
		Null,
		True,
		False,
		Int,
		Float,
		Cell, // The name will follow this value
	};

	struct FEncodedType
	{
		explicit FEncodedType(EEncodedType InEncodedType, const VCppClassInfo* InCppClassInfo = nullptr)
			: EncodedType(InEncodedType)
			, CppClassInfo(InCppClassInfo)
		{
		}
		EEncodedType EncodedType;
		const VCppClassInfo* CppClassInfo;
	};

	// Read/Write the element type description
	void WriteElementType(FStructuredArchiveRecord Record, FEncodedType EncodedType);
	FEncodedType ReadElementType(FStructuredArchiveRecord Record);

	// Enter and leaving objects or arrays
	FStructuredArchiveArray EnterArray(const TCHAR* ElementName, int32& Num, ENestingType Type);
	void LeaveArray(ENestingType Type);
	FStructuredArchiveRecord EnterObject(const TCHAR* ElementName);
	void LeaveObject();
	FStructuredArchiveSlot Slot(const TCHAR* ElementName);

	// Read/Write a cell while handling null, true, and false types
	void WriteCellBody(FStructuredArchiveRecord Record, VCell* InCell);
	VCell* ReadCellBody(FStructuredArchiveRecord Record, FEncodedType EncodedType);
	void VisitCellBody(FStructuredArchiveRecord Record, VCell*& InOutCell);

	struct NestingEntry
	{
		UE::StructuredArchive::Private::FSlotBase Slot;
		ENestingType Type;
	};
	TArray<NestingEntry> NestingInfo;
	FAllocationContext Context;
	FStructuredArchive& StructuredArchive;

	struct ScopedRecord
	{
		ScopedRecord(FStructuredArchiveVisitor& InVisitor, const TCHAR* InName);
		~ScopedRecord();

		FStructuredArchiveVisitor& Visitor;
		const TCHAR* Name;
		FStructuredArchiveRecord Record;
	};
};

} // namespace Verse

#endif // WITH_VERSE_VM