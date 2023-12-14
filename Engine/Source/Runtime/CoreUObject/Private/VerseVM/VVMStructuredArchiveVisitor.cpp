// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMStructuredArchiveVisitor.h"
#include "Serialization/StructuredArchiveFormatter.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMHeapInt.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

namespace
{
const static FLazyName NAME_VNull("VNull");
const static FLazyName NAME_VTrue("VTrue");
const static FLazyName NAME_VFalse("VFalse");
const static FLazyName NAME_VInt("VInt");
const static FLazyName NAME_VFloat("VFloat");
const static FLazyName NAME_VChar("VChar");
const static FLazyName NAME_VChar32("VChar32");
const static FLazyName NAME_VNone("VNone");
const static TCHAR* TypeElementName = TEXT("_type");
const static TCHAR* CellTypeElementName = TEXT("_cellType");
} // namespace

FStructuredArchiveVisitor::ScopedRecord::ScopedRecord(FStructuredArchiveVisitor& InVisitor, const TCHAR* InName)
	: Visitor(InVisitor)
	, Name(InName)
	, Record(Visitor.EnterObject(Name))
{
}

FStructuredArchiveVisitor::ScopedRecord::~ScopedRecord()
{
	Visitor.LeaveObject();
}

void FStructuredArchiveVisitor::WriteElementType(FStructuredArchiveRecord Record, FEncodedType EncodedType)
{
	if (IsTextFormat())
	{
		FName TypeName;
		switch (EncodedType.EncodedType)
		{
			case EEncodedType::None:
				TypeName = NAME_VNone;
				break;
			case EEncodedType::Null:
				TypeName = NAME_VNull;
				break;
			case EEncodedType::True:
				TypeName = NAME_VTrue;
				break;
			case EEncodedType::False:
				TypeName = NAME_VFalse;
				break;
			case EEncodedType::Int:
				TypeName = NAME_VInt;
				break;
			case EEncodedType::Float:
				TypeName = NAME_VFloat;
				break;
			case EEncodedType::Char:
				TypeName = NAME_VChar;
				break;
			case EEncodedType::Char32:
				TypeName = NAME_VChar32;
				break;
			case EEncodedType::Cell:
				ensure(EncodedType.CppClassInfo != nullptr);
				TypeName = FName(EncodedType.CppClassInfo->Name);
				break;
			default:
				V_DIE("Unexpected EncodedType");
		}
		Record.EnterField(TypeElementName) << TypeName;
	}
	else
	{
		{
			FStructuredArchiveSlot Field = Record.EnterField(TypeElementName);
			uint8 ScratchType = (uint8)EncodedType.EncodedType;
			Field << ScratchType;
		}
		if (EncodedType.EncodedType == EEncodedType::Cell)
		{
			ensure(EncodedType.CppClassInfo != nullptr);
			FStructuredArchiveSlot Field = Record.EnterField(CellTypeElementName);
			FName TypeName(EncodedType.CppClassInfo->Name);
			Field << TypeName;
		}
	}
}

FStructuredArchiveVisitor::FEncodedType FStructuredArchiveVisitor::ReadElementType(FStructuredArchiveRecord Record)
{
	FEncodedType EncodedType(EEncodedType::None);
	if (IsTextFormat())
	{
		FName TypeName;
		Record.EnterField(TypeElementName) << TypeName;
		if (TypeName == NAME_VNone)
		{
			EncodedType = FEncodedType(EEncodedType::None);
		}
		else if (TypeName == NAME_VNull)
		{
			EncodedType = FEncodedType(EEncodedType::Null);
		}
		else if (TypeName == NAME_VTrue)
		{
			EncodedType = FEncodedType(EEncodedType::True);
		}
		else if (TypeName == NAME_VFalse)
		{
			EncodedType = FEncodedType(EEncodedType::False);
		}
		else if (TypeName == NAME_VInt)
		{
			EncodedType = FEncodedType(EEncodedType::Int);
		}
		else if (TypeName == NAME_VFloat)
		{
			EncodedType = FEncodedType(EEncodedType::Float);
		}
		else if (TypeName == NAME_VChar)
		{
			EncodedType = FEncodedType(EEncodedType::Char);
		}
		else if (TypeName == NAME_VChar32)
		{
			EncodedType = FEncodedType(EEncodedType::Char32);
		}
		else
		{
			const VCppClassInfo* CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
			if (CppClassInfo != nullptr)
			{
				EncodedType = FEncodedType(EEncodedType::Cell, CppClassInfo);
			}
			else
			{
				V_DIE("Unable to find class information for %s", *TypeName.ToString());
			}
		}
	}
	else
	{
		{
			FStructuredArchiveSlot Field = Record.EnterField(TypeElementName);
			uint8 ScratchType;
			Field << ScratchType;
			EncodedType.EncodedType = (EEncodedType)ScratchType;
		}

		if (EncodedType.EncodedType == EEncodedType::Cell)
		{
			FStructuredArchiveSlot Field = Record.EnterField(CellTypeElementName);
			FName TypeName;
			Field << TypeName;
			EncodedType.CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
			if (EncodedType.CppClassInfo == nullptr)
			{
				V_DIE("Unable to find class information for %s", *TypeName.ToString());
			}
		}
	}
	return EncodedType;
}

void FStructuredArchiveVisitor::WriteCellBody(FStructuredArchiveRecord Record, VCell* InCell)
{
	if (InCell == nullptr)
	{
		WriteElementType(Record, FEncodedType(EEncodedType::Null));
	}
	else if (VValue Logic(*InCell); Logic.IsLogic())
	{
		WriteElementType(Record, FEncodedType(Logic.AsBool() ? EEncodedType::True : EEncodedType::False));
	}
	else
	{
		const VCppClassInfo* CppClassInfo = InCell->GetCppClassInfo();
		if (CppClassInfo->Serialize)
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Cell, CppClassInfo));
			CppClassInfo->Serialize(InCell, Context, *this);
		}
		else
		{
			V_DIE("The class \"%s\" does not have a serialization method defined", *CppClassInfo->DebugName());
		}
	}
}

VCell* FStructuredArchiveVisitor::ReadCellBody(FStructuredArchiveRecord Record, FEncodedType EncodedType)
{
	switch (EncodedType.EncodedType)
	{
		case EEncodedType::Null:
			return nullptr;

		case EEncodedType::False:
			return GlobalFalsePtr.Get();

		case EEncodedType::True:
			return GlobalTruePtr.Get();

		case EEncodedType::Cell:
		{
			if (EncodedType.CppClassInfo->Serialize)
			{
				VCell* NewCell = nullptr;
				EncodedType.CppClassInfo->Serialize(NewCell, Context, *this);
				return NewCell;
			}
			else
			{
				V_DIE("The class \"%s\" does not have a serialization method defined", *EncodedType.CppClassInfo->DebugName());
			}
		}

		case EEncodedType::None:
		case EEncodedType::Int:
		case EEncodedType::Float:
		case EEncodedType::Char:
		case EEncodedType::Char32:
		default:
			V_DIE("Unexpected encoded type");
	}
}

void FStructuredArchiveVisitor::VisitCellBody(FStructuredArchiveRecord Record, VCell*& InOutCell)
{
	if (IsLoading())
	{
		FEncodedType EncodedType = ReadElementType(Record);
		InOutCell = ReadCellBody(Record, EncodedType);
	}
	else
	{
		WriteCellBody(Record, InOutCell);
	}
}

void FStructuredArchiveVisitor::Serialize(VCell*& InOutCell)
{
	if (!IsLoading() && InOutCell == nullptr)
	{
		return; // warning???
	}
	VisitCellBody(ScopedRecord(*this, TEXT("")).Record, InOutCell);
}

void FStructuredArchiveVisitor::Serialize(VValue& InOutValue)
{
	Visit(InOutValue, TEXT(""));
}

void FStructuredArchiveVisitor::BeginArray(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Array);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndArray()
{
	LeaveArray(ENestingType::Array);
}

void FStructuredArchiveVisitor::BeginSet(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Set);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::FStructuredArchiveVisitor::EndSet()
{
	LeaveArray(ENestingType::Set);
}

void FStructuredArchiveVisitor::BeginMap(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	int32 ScratchNumElements = int32(NumElements);
	EnterArray(ElementName, ScratchNumElements, ENestingType::Map);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndMap()
{
	LeaveArray(ENestingType::Map);
}

void FStructuredArchiveVisitor::BeginObject(const TCHAR* ElementName)
{
	EnterObject(ElementName);
}

void FStructuredArchiveVisitor::EndObject()
{
	LeaveObject();
}

void FStructuredArchiveVisitor::VisitNonNull(VCell*& InCell, const TCHAR* ElementName)
{
	VisitCellBody(ScopedRecord(*this, ElementName).Record, InCell);
}

void FStructuredArchiveVisitor::VisitEmergentType(const VCell* InEmergentType)
{
	// Any emergent type formatting has already been done
}

void FStructuredArchiveVisitor::VisitNonNull(UObject* InObject, const TCHAR* ElementName)
{
}

void FStructuredArchiveVisitor::Visit(VCell*& InCell, const TCHAR* ElementName)
{
	VisitCellBody(ScopedRecord(*this, ElementName).Record, InCell);
}

void FStructuredArchiveVisitor::Visit(UObject* InObject, const TCHAR* ElementName)
{
}

void FStructuredArchiveVisitor::Visit(VValue& Value, const TCHAR* ElementName)
{
	ScopedRecord ScopedRecord(*this, ElementName);
	FStructuredArchiveRecord Record = ScopedRecord.Record;
	if (IsLoading())
	{
		FEncodedType EncodedType = ReadElementType(Record);
		switch (EncodedType.EncodedType)
		{
			case EEncodedType::None:
				Value = VValue();
				break;

			case EEncodedType::Int:
			{
				int64 Int64;
				Record.EnterField(TEXT("Value")) << Int64;
				Value = VValue(VInt(Context, Int64));
				break;
			}

			case EEncodedType::Float:
			{
				double DoubleValue;
				Record.EnterField(TEXT("Value")) << DoubleValue;
				Value = VValue(VFloat(DoubleValue));
				break;
			}

			case EEncodedType::Char:
			{
				uint8 Char;
				Record.EnterField(TEXT("Value")) << Char;
				Value = VValue::Char(Char);
				break;
			}

			case EEncodedType::Char32:
			{
				uint32 Char32;
				Record.EnterField(TEXT("Value")) << Char32;
				Value = VValue::Char32(Char32);
				break;
			}

			case EEncodedType::Cell:
				Value = VValue(*ReadCellBody(Record, EncodedType));
				break;

			case EEncodedType::Null:
			default:
				V_DIE("Unexpected encoded type %u", static_cast<uint8>(EncodedType.EncodedType));
		}
	}
	else
	{
		// If possible, resolve any placeholders
		VValue ScratchValue = Value;
		if (ScratchValue.IsPlaceholder())
		{
			VPlaceholder& Placeholder = ScratchValue.AsPlaceholder();
			ScratchValue = Placeholder.Follow();
			if (ScratchValue.IsPlaceholder())
			{
				V_DIE("Unfollowable placeholder: 0x%" PRIxPTR, Value.GetEncodedBits());
			}
		}

		// NOTE: This IsCell should handle Logic and HeapInt values.
		if (Value.IsCell())
		{
			WriteCellBody(Record, &Value.AsCell());
		}
		else if (Value.IsInt())
		{
			VInt Int = Value.AsInt();
			if (Int.IsInt64())
			{
				WriteElementType(Record, FEncodedType(EEncodedType::Int));
				int64 Int64 = Int.AsInt64();
				Record.EnterField(TEXT("Value")) << Int64;
			}
			else
			{
				V_DIE("Arbitrary-precision integers are handled above in IsCell.");
			}
		}
		else if (Value.IsFloat())
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Float));
			double DoubleValue = Value.AsFloat().AsDouble();
			Record.EnterField(TEXT("Value")) << DoubleValue;
		}
		else if (Value.IsChar())
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Char));
			uint8 Char = Value.AsChar();
			Record.EnterField(TEXT("Value")) << Char;
		}
		else if (Value.IsChar32())
		{
			WriteElementType(Record, FEncodedType(EEncodedType::Char32));
			uint32 Char32 = Value.AsChar32();
			Record.EnterField(TEXT("Value")) << Char32;
		}
		else if (Value.IsUninitialized())
		{
			WriteElementType(Record, FEncodedType(EEncodedType::None));
		}
		else
		{
			V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, Value.GetEncodedBits());
		}
	}
}

void FStructuredArchiveVisitor::Visit(VRestValue& Value, const TCHAR* ElementName)
{
	// Restrict calling VRestValue visitor to using FAbstractVisitor.
	Value.Visit(static_cast<FAbstractVisitor&>(*this), ElementName);
}

void FStructuredArchiveVisitor::Visit(bool& bValue, const TCHAR* ElementName)
{
	Slot(ElementName) << bValue;
}

void FStructuredArchiveVisitor::Visit(FString& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(uint64& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

void FStructuredArchiveVisitor::Visit(int64& Value, const TCHAR* ElementName)
{
	Slot(ElementName) << Value;
}

FStructuredArchiveArray FStructuredArchiveVisitor::EnterArray(const TCHAR* ElementName, int32& Num, ENestingType Type)
{
	if (NestingInfo.Num() == 0)
	{
		FStructuredArchiveArray Child = StructuredArchive.Open().EnterArray(Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
	else if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		FStructuredArchiveArray Child = Record.EnterArray(ElementName, Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		FStructuredArchiveArray Child = Array.EnterElement().EnterArray(Num);
		NestingInfo.Push(NestingEntry(Child, Type));
		return Child;
	}
}

void FStructuredArchiveVisitor::LeaveArray(ENestingType Type)
{
	check(NestingInfo.Num() > 0 && NestingInfo.Last().Type == Type);
	NestingInfo.Pop();
}

FStructuredArchiveRecord FStructuredArchiveVisitor::EnterObject(const TCHAR* ElementName)
{
	if (NestingInfo.Num() == 0)
	{
		FStructuredArchiveRecord Child = StructuredArchive.Open().EnterRecord();
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
	else if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		FStructuredArchiveRecord Child = Record.EnterRecord(ElementName);
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		FStructuredArchiveRecord Child = Array.EnterElement().EnterRecord();
		NestingInfo.Push(NestingEntry(Child, ENestingType::Object));
		return Child;
	}
}

void FStructuredArchiveVisitor::LeaveObject()
{
	check(NestingInfo.Num() > 0 && NestingInfo.Last().Type == ENestingType::Object);
	NestingInfo.Pop();
}

FStructuredArchiveSlot FStructuredArchiveVisitor::Slot(const TCHAR* ElementName)
{
	check(NestingInfo.Num() > 0);
	if (NestingInfo.Last().Type == ENestingType::Object)
	{
		FStructuredArchiveRecord& Record = static_cast<FStructuredArchiveRecord&>(NestingInfo.Last().Slot);
		return Record.EnterField(ElementName);
	}
	else
	{
		FStructuredArchiveArray& Array = static_cast<FStructuredArchiveArray&>(NestingInfo.Last().Slot);
		return Array.EnterElement();
	}
}

FArchive* FStructuredArchiveVisitor::GetUnderlyingArchive()
{
	return &StructuredArchive.GetUnderlyingArchive();
}

bool FStructuredArchiveVisitor::IsLoading()
{
	return StructuredArchive.GetUnderlyingArchive().IsLoading();
}

bool FStructuredArchiveVisitor::IsTextFormat()
{
	return StructuredArchive.GetUnderlyingArchive().IsTextFormat();
}

FAccessContext FStructuredArchiveVisitor::GetLoadingContext()
{
	return Context;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
