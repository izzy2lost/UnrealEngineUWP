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
const static FLazyName NAME_VNone("VNone");
const static TCHAR* TypeElementName = TEXT("_type");
} // namespace

template <typename TLambda>
void FStructuredArchiveVisitor::Field(const TCHAR* Name, TLambda lambda)
{
	Formatter.EnterField(Name);
	lambda();
	Formatter.LeaveField();
}

template <typename TLambda>
void FStructuredArchiveVisitor::Element(const TCHAR* ElementName, ENestingType Type, TLambda Lambda)
{
	BeginElement(ElementName, Type);
	if (Type == ENestingType::Object)
	{
		Formatter.EnterRecord();
	}
	Lambda();
	if (Type == ENestingType::Object)
	{
		Formatter.LeaveRecord();
	}
	EndElement(Type);
}

void FStructuredArchiveVisitor::WriteElementType(FEncodedType EncodedType)
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
			case EEncodedType::Cell:
				ensure(EncodedType.CppClassInfo != nullptr);
				TypeName = FName(EncodedType.CppClassInfo->Name);
				break;
			default:
				V_DIE("Unexpected EncodedType");
		}
		Field(TypeElementName, [this, TypeName]() {
			FName ScratchType = TypeName;
			Formatter.Serialize(ScratchType);
		});
	}
	else
	{
		Field(TypeElementName, [this, EncodedType]() {
			uint8 ScratchType = (uint8)EncodedType.EncodedType;
			Formatter.Serialize(ScratchType);
			if (EncodedType.EncodedType == EEncodedType::Cell)
			{
				ensure(EncodedType.CppClassInfo != nullptr);
				FName TypeName(EncodedType.CppClassInfo->Name);
				Formatter.Serialize(TypeName);
			}
		});
	}
}

FStructuredArchiveVisitor::FEncodedType FStructuredArchiveVisitor::ReadElementType()
{
	FEncodedType EncodedType(EEncodedType::None);
	if (IsTextFormat())
	{
		FName TypeName;
		Field(TypeElementName, [this, &TypeName]() {
			Formatter.Serialize(TypeName);
		});

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
		Field(TypeElementName, [this, &EncodedType]() {
			uint8 ScratchType;
			Formatter.Serialize(ScratchType);
			EncodedType.EncodedType = (EEncodedType)ScratchType;
			if (EncodedType.EncodedType == EEncodedType::Cell)
			{
				FName TypeName;
				Formatter.Serialize(TypeName);
				EncodedType.CppClassInfo = VCppClassInfoRegistry::GetCppClassInfo(*TypeName.ToString());
				if (EncodedType.CppClassInfo == nullptr)
				{
					V_DIE("Unable to find class information for %s", *TypeName.ToString());
				}
			}
		});
	}
	return EncodedType;
}

void FStructuredArchiveVisitor::WriteCellBody(VCell* InCell)
{
	if (InCell == nullptr)
	{
		WriteElementType(FEncodedType(EEncodedType::Null));
	}
	else if (VValue Logic(*InCell); Logic.IsLogic())
	{
		WriteElementType(FEncodedType(Logic.AsBool() ? EEncodedType::True : EEncodedType::False));
	}
	else
	{
		const VCppClassInfo* CppClassInfo = InCell->GetCppClassInfo();
		if (CppClassInfo->Serialize)
		{
			WriteElementType(FEncodedType(EEncodedType::Cell, CppClassInfo));
			CppClassInfo->Serialize(InCell, Context, *this);
		}
		else
		{
			V_DIE("The class \"%s\" does not have a serialization method defined", *CppClassInfo->DebugName());
		}
	}
}

VCell* FStructuredArchiveVisitor::ReadCellBody(FEncodedType EncodedType)
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
		default:
			V_DIE("Unexpected encoded type");
	}
}

void FStructuredArchiveVisitor::VisitCellBody(VCell*& InOutCell)
{
	if (IsLoading())
	{
		FEncodedType EncodedType = ReadElementType();
		InOutCell = ReadCellBody(EncodedType);
	}
	else
	{
		WriteCellBody(InOutCell);
	}
}

void FStructuredArchiveVisitor::Serialize(VCell*& InOutCell)
{
	if (!IsLoading() && InOutCell == nullptr)
	{
		return; // warning???
	}
	BeginObject();
	VisitCellBody(InOutCell);
	EndObject();
}

void FStructuredArchiveVisitor::BeginArray(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	BeginElement(ElementName, ENestingType::Array);
	int32 ScratchNumElements = int32(NumElements);
	Formatter.EnterArray(ScratchNumElements);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndArray()
{
	Formatter.LeaveArray();
	EndElement(ENestingType::Array);
}

void FStructuredArchiveVisitor::BeginSet(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	BeginElement(ElementName, ENestingType::Set);
	int32 ScratchNumElements = int32(NumElements);
	Formatter.EnterArray(ScratchNumElements);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::FStructuredArchiveVisitor::EndSet()
{
	Formatter.LeaveArray();
	EndElement(ENestingType::Set);
}

void FStructuredArchiveVisitor::BeginMap(const TCHAR* ElementName, uint64& NumElements)
{
	// UE is currently limited to array sizes of MAX_int32.  This needs to be resolved in the
	// future, but for now just generate a runtime error.
	if (NumElements > MAX_int32)
	{
		V_DIE("More that int32 number of array elements isn't currently supported");
	}
	BeginElement(ElementName, ENestingType::Map);
	int32 ScratchNumElements = int32(NumElements);
	Formatter.EnterArray(ScratchNumElements);
	NumElements = ScratchNumElements;
}

void FStructuredArchiveVisitor::EndMap()
{
	Formatter.LeaveArray();
	EndElement(ENestingType::Map);
}

void FStructuredArchiveVisitor::BeginObject()
{
	PushNesting(ENestingType::Object);
	Formatter.EnterRecord();
}

void FStructuredArchiveVisitor::EndObject()
{
	Formatter.LeaveRecord();
	PopNesting(ENestingType::Object);
}

void FStructuredArchiveVisitor::VisitNonNull(VCell*& InCell, const TCHAR* ElementName)
{
	Element(ElementName, ENestingType::Object, [this, &InCell]() {
		VisitCellBody(InCell);
	});
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
	Element(ElementName, ENestingType::Object, [this, &InCell]() {
		VisitCellBody(InCell);
	});
}

void FStructuredArchiveVisitor::Visit(UObject* InObject, const TCHAR* ElementName)
{
}

void FStructuredArchiveVisitor::Visit(VValue& Value, const TCHAR* ElementName)
{
	if (IsLoading())
	{
		Element(ElementName, ENestingType::Object, [this, &Value]() {
			FEncodedType EncodedType = ReadElementType();
			switch (EncodedType.EncodedType)
			{
				case EEncodedType::None:
					Value = VValue();
					break;

				case EEncodedType::Int:
				{
					Element(TEXT("Value"), ENestingType::None, [this, &Value]() {
						int64 Int64;
						Formatter.Serialize(Int64);
						Value = VValue(VInt(Context, Int64));
					});
					break;
				}

				case EEncodedType::Float:
				{
					Element(TEXT("Value"), ENestingType::None, [this, &Value]() {
						double DoubleValue;
						Formatter.Serialize(DoubleValue);
						Value = VValue(VFloat(DoubleValue));
					});
					break;
				}

				case EEncodedType::False:
				case EEncodedType::True:
				case EEncodedType::Cell:
					Value = VValue(*ReadCellBody(EncodedType));
					break;

				case EEncodedType::Null:
				default:
					V_DIE("Unexpected encoded type");
			}
		});
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

		Element(ElementName, ENestingType::Object, [this, Value]() {
			// NOTE: This IsCell should handle Logic and HeapInt values.
			if (Value.IsCell())
			{
				WriteCellBody(&Value.AsCell());
			}
			else if (Value.IsInt())
			{
				WriteElementType(FEncodedType(EEncodedType::Int));
				Element(TEXT("Value"), ENestingType::None, [this, Value]() {
					VInt Int = Value.AsInt();
					if (Int.IsInt64())
					{
						int64 Int64 = Int.AsInt64();
						Formatter.Serialize(Int64);
					}
					else
					{
						V_DIE("Arbitrary-precision integers are not yet supported.");
					}
				});
			}
			else if (Value.IsFloat())
			{
				WriteElementType(FEncodedType(EEncodedType::Float));
				Element(TEXT("Value"), ENestingType::None, [this, Value]() {
					double DoubleValue = Value.AsFloat().AsDouble();
					Formatter.Serialize(DoubleValue);
				});
			}
			else if (Value.IsUninitialized())
			{
				WriteElementType(FEncodedType(EEncodedType::None));
			}
			else
			{
				V_DIE("Unhandled Verse value encoding: 0x%" PRIxPTR, Value.GetEncodedBits());
			}
		});
	}
}

void FStructuredArchiveVisitor::Visit(VRestValue& Value, const TCHAR* ElementName)
{
	// Restrict calling VRestValue visitor to using FAbstractVisitor.
	Value.Visit(static_cast<FAbstractVisitor&>(*this), ElementName);
}

void FStructuredArchiveVisitor::Visit(bool& bValue, const TCHAR* ElementName)
{
	Element(ElementName, ENestingType::None, [this, &bValue]() {
		Formatter.Serialize(bValue);
	});
}

void FStructuredArchiveVisitor::Visit(FString& Value, const TCHAR* ElementName)
{
	Element(ElementName, ENestingType::None, [this, &Value]() {
		Formatter.Serialize(Value);
	});
}

void FStructuredArchiveVisitor::Visit(uint64& Value, const TCHAR* ElementName)
{
	Element(ElementName, ENestingType::None, [this, &Value]() {
		Formatter.Serialize(Value);
	});
}

void FStructuredArchiveVisitor::Visit(int64& Value, const TCHAR* ElementName)
{
	Element(ElementName, ENestingType::None, [this, &Value]() {
		Formatter.Serialize(Value);
	});
}

void FStructuredArchiveVisitor::PushNesting(ENestingType InType)
{
	if (NestingInfo.Num() > 0)
	{
		ENestingType NestingType = NestingInfo.Last();
		switch (NestingType)
		{
			case ENestingType::Array:
			case ENestingType::Set:
			case ENestingType::Map:
				Formatter.EnterArrayElement();
				break;
		}
	}
	if (InType != ENestingType::None)
	{
		NestingInfo.Add(InType);
	}
}

void FStructuredArchiveVisitor::PopNesting(ENestingType InExpectedType)
{
	if (InExpectedType != ENestingType::None)
	{
		CheckNesting(InExpectedType);
		check(NestingInfo.Num() > 0);
		NestingInfo.Pop();
	}

	if (NestingInfo.Num() > 0)
	{
		ENestingType NestingType = NestingInfo.Last();
		switch (NestingType)
		{
			case ENestingType::Array:
			case ENestingType::Set:
			case ENestingType::Map:
				Formatter.LeaveArrayElement();
				break;
		}
	}
}

void FStructuredArchiveVisitor::CheckNesting(ENestingType InExpectedType)
{
	check(NestingInfo.Num() > 0 && NestingInfo.Last() == InExpectedType);
}

void FStructuredArchiveVisitor::BeginElement(const TCHAR* ElementName, ENestingType InType)
{
	check(NestingInfo.Num() > 0);
	ENestingType NestingType = NestingInfo.Last();
	switch (NestingType)
	{
		case ENestingType::Object:
			Formatter.EnterField(ElementName);
			break;
	}
	PushNesting(InType);
}

void FStructuredArchiveVisitor::EndElement(ENestingType InType)
{
	if (InType != ENestingType::None)
	{
		PopNesting(InType);
	}
	check(NestingInfo.Num() > 0);
	ENestingType NestingType = NestingInfo.Last();
	switch (NestingType)
	{
		case ENestingType::Object:
			Formatter.LeaveField();
			break;
	}
}

FArchive* FStructuredArchiveVisitor::GetUnderlyingArchive()
{
	return &Formatter.GetUnderlyingArchive();
}

bool FStructuredArchiveVisitor::IsLoading()
{
	return Formatter.GetUnderlyingArchive().IsLoading();
}

bool FStructuredArchiveVisitor::IsTextFormat()
{
	return Formatter.GetUnderlyingArchive().IsTextFormat();
}

FAccessContext FStructuredArchiveVisitor::GetLoadingContext()
{
	return Context;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
