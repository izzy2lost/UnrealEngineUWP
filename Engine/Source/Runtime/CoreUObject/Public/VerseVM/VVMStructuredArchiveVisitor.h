// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "VVMAbstractVisitor.h"

class FStructuredArchiveFormatter;

namespace Verse
{
struct VCppClassInfo;

struct FStructuredArchiveVisitor : FAbstractVisitor
{
	FStructuredArchiveVisitor(FAllocationContext InContext, FStructuredArchiveFormatter& InFormatter)
		: Context(InContext)
		, Formatter(InFormatter)
	{
	}

	void Serialize(VCell*& InOutCell);

	virtual void BeginArray(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndArray() override;
	virtual void BeginSet(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndSet() override;
	virtual void BeginMap(const TCHAR* ElementName, uint64& NumElements) override;
	virtual void EndMap() override;
	virtual void BeginObject() override;
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

	void PushNesting(ENestingType InType);
	void PopNesting(ENestingType InExpectedType);
	void CheckNesting(ENestingType InExpectedType);
	void BeginElement(const TCHAR* ElementName, ENestingType InType);
	void EndElement(ENestingType InType);

	// Helper methods to simplify the pairing of push/pop, enter/leave methods
	template <typename TLambda>
	void Field(const TCHAR* ElementName, TLambda Lambda);
	template <typename TLambda>
	void Element(const TCHAR* ElementName, ENestingType Type, TLambda Lambda);

	// Read/Write the element type description
	void WriteElementType(FEncodedType EncodedType);
	FEncodedType ReadElementType();

	// Read/Write a cell while handling null, true, and false types
	void WriteCellBody(VCell* InCell);
	VCell* ReadCellBody(FEncodedType EncodedType);
	void VisitCellBody(VCell*& InOutCell);

	TArray<ENestingType> NestingInfo;
	FAllocationContext Context;
	FStructuredArchiveFormatter& Formatter;
};

} // namespace Verse

#endif // WITH_VERSE_VM