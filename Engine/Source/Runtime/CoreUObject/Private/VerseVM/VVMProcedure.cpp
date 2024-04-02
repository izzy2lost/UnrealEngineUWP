// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMPackage.h"

namespace Verse
{

// Specializations for bytecode fields so we can visit them

template <>
void Visit(FAbstractVisitor& Visitor, FRegisterIndex& Value, const TCHAR* ElementName)
{
	Visitor.Visit(Value.Index, ElementName);
}

template <>
void Visit(FMarkStackVisitor& Visitor, const FRegisterIndex& Value, FMarkStackVisitor::ConsumeElementName ElementName)
{
}

template <>
void Visit(FAbstractVisitor& Visitor, FValueOperand& Value, const TCHAR* ElementName)
{
	Visitor.Visit(Value.Index, ElementName);
}

template <>
void Visit(FMarkStackVisitor& Visitor, const FValueOperand& Value, FMarkStackVisitor::ConsumeElementName ElementName)
{
}

template <>
void Visit(FAbstractVisitor& Visitor, FLabelOffset& Value, const TCHAR* ElementName)
{
	Visitor.Visit(Value.Offset, ElementName);
}

template <>
void Visit(FMarkStackVisitor& Visitor, const FLabelOffset& Value, FMarkStackVisitor::ConsumeElementName ElementName)
{
}

template <typename T>
void Visit(FAbstractVisitor& Visitor, TOperandRange<T>& Value, const TCHAR* ElementName)
{
	Visitor.Visit(Value.Index, ElementName);
	Visitor.Visit(Value.Num, ElementName);
}

template <typename T>
void Visit(FMarkStackVisitor& Visitor, const TOperandRange<T>& Value, FMarkStackVisitor::ConsumeElementName ElementName)
{
}

namespace Private
{

// Helper type to detect if we need to serialize the given value
template <typename T>
struct OperandNeedsSerialization
{
};

template <>
struct OperandNeedsSerialization<FRegisterIndex> : public std::false_type
{
};

template <>
struct OperandNeedsSerialization<FValueOperand> : public std::false_type
{
};

template <typename T>
struct OperandNeedsSerialization<TOperandRange<T>> : public std::false_type
{
};

// Disable VPackage serialization (in the NewClass opcode) for now.
template <>
struct OperandNeedsSerialization<TWriteBarrier<VPackage>> : public std::false_type
{
};

template <typename CellType>
struct OperandNeedsSerialization<TWriteBarrier<CellType>> : public std::true_type
{
};
} // namespace Private

DEFINE_DERIVED_VCPPCLASSINFO(VProcedure);
TGlobalTrivialEmergentTypePtr<&VProcedure::StaticCppClassInfo> VProcedure::GlobalTrivialEmergentType;

template <typename FuncType>
void VProcedure::ForEachOpCode(FuncType&& Func)
{
	for (FOp* CurrentOp = GetOpsBegin(); CurrentOp != GetOpsEnd();)
	{
		checkf(CurrentOp != nullptr, TEXT("The current opcode was invalid!"));
		switch (CurrentOp->Opcode)
		{
#define VISIT_OP(Name)                                                    \
	case EOpcode::Name:                                                   \
	{                                                                     \
		FOp##Name* CurrentDerivedOp = static_cast<FOp##Name*>(CurrentOp); \
		Func(*CurrentDerivedOp);                                          \
		CurrentOp = BitCast<FOp*>(CurrentDerivedOp + 1);                  \
		break;                                                            \
	}
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
			default:
				V_DIE("Invalid opcode encountered: %u", static_cast<FOpcodeInt>(CurrentOp->Opcode));
				break;
		}
	}
}

void VProcedure::SaveOpCodes(FAbstractVisitor& Visitor)
{
	int32 ValueCount = 0;
	TArray<uint8> SanitizedOpCodes(BitCast<uint8*>(GetOpsBegin()), NumOpBytes);

	// Scan the opcodes looking for any operands that will need to be written out seperately.
	// If one is found, we blank out that value in the sanitized op codes to make the output
	// more deterministic.
	ForEachOpCode([this, &SanitizedOpCodes, &ValueCount](auto& Op) {
		Op.ForEachOperand([this, &SanitizedOpCodes, &ValueCount](EOperandRole Role, auto& Operand, const TCHAR* Name) {
			using DecayedType = std::decay_t<decltype(Operand)>;
			if constexpr (Private::OperandNeedsSerialization<DecayedType>::value)
			{
				++ValueCount;
				uint32 ByteOffset = BytecodeOffset(&Operand);
				check(ByteOffset >= 0 && ByteOffset + sizeof(Operand) <= NumOpBytes);
				FMemory::Memset(&SanitizedOpCodes[ByteOffset], 0, sizeof(Operand));
			}
		});
	});

	Visitor.VisitBulkData(SanitizedOpCodes.GetData(), SanitizedOpCodes.Num(), TEXT("OpBytes"));

	// Scan again writing the values
	uint64 ScratchNumValues = ValueCount;
	Visitor.BeginArray(TEXT("OpCodeValues"), ScratchNumValues);
	if (ValueCount > 0)
	{
		ForEachOpCode([this, &Visitor](auto& Op) {
			Op.ForEachOperand([this, &Visitor](EOperandRole Role, auto& Operand, const TCHAR* Name) {
				using DecayedType = std::decay_t<decltype(Operand)>;
				if constexpr (Private::OperandNeedsSerialization<DecayedType>::value)
				{
					Visit(Visitor, Operand, TEXT(""));
				}
			});
		});
	}
	Visitor.EndArray();
}

void VProcedure::LoadOpCodes(FAbstractVisitor& Visitor)
{
	Visitor.VisitBulkData(GetOpsBegin(), NumOpBytes, TEXT("OpBytes"));

	uint64 ScratchNumValues = 0;
	Visitor.BeginArray(TEXT("OpCodeValues"), ScratchNumValues);
	if (ScratchNumValues > 0)
	{
		int32 ValueCount = 0;
		ForEachOpCode([this, &Visitor, &ValueCount](auto& Op) {
			Op.ForEachOperand([this, &Visitor, &ValueCount](EOperandRole Role, auto& Operand, const TCHAR* Name) {
				using DecayedType = std::decay_t<decltype(Operand)>;
				if constexpr (Private::OperandNeedsSerialization<DecayedType>::value)
				{
					++ValueCount;
					Visit(Visitor, Operand, TEXT(""));
				}
			});
		});
		checkSlow(ScratchNumValues == ValueCount);
	}
	Visitor.EndArray();
}

template <typename TVisitor>
void VProcedure::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumNamedParams = NumNamedParameters;
		Visitor.BeginArray(TEXT("NamedParams"), ScratchNumNamedParams);
		Visitor.Visit(GetNamedParamsBegin(), GetNamedParamsEnd());
		Visitor.EndArray();

		uint64 ScratchNumConstants = NumConstants;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants);
		Visitor.Visit(GetConstantsBegin(), GetConstantsEnd());
		Visitor.EndArray();

		ForEachOpCode([&Visitor](auto& Op) {
			Op.ForEachOperand([&Visitor](EOperandRole Role, auto& Operand, const TCHAR* Name) {
				// Disable VPackage serialization (in the NewClass opcode) for now.
				using DecayedType = std::decay_t<decltype(Operand)>;
				if constexpr (!std::is_same_v<DecayedType, TWriteBarrier<VPackage>>)
				{
					Visit(Visitor, Operand, Name);
				}
			});
		});

		uint64 ScratchNumOperands = NumOperands;
		Visitor.BeginArray(TEXT("Operands"), ScratchNumOperands);
		Visitor.Visit(GetOperandsBegin(), GetOperandsEnd());
		Visitor.EndArray();

		uint64 ScratchNumLabels = NumLabels;
		Visitor.BeginArray(TEXT("Labels"), ScratchNumLabels);
		Visitor.Visit(GetLabelsBegin(), GetLabelsEnd());
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(GetNamedParamsBegin(), GetNamedParamsEnd());
		Visitor.Visit(GetConstantsBegin(), GetConstantsEnd());

		ForEachOpCode([&Visitor](auto& Op) {
			Op.ForEachOperand([&Visitor](EOperandRole Role, auto& Operand, const TCHAR* Name) {
				Visit(Visitor, Operand, Name);
			});
		});
	}
}

void VProcedure::SerializeImpl(VProcedure*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint32 ScratchNumRegisters = 0;
		uint32 ScratchNumParameters = 0;
		uint32 ScratchNumNamedParameters = 0;
		uint32 ScratchNumConstants = 0;
		uint64 ScratchNumOpBytes = 0;
		uint32 ScratchNumOperands = 0;
		uint32 ScratchNumLabels = 0;
		Visitor.Visit(ScratchNumRegisters, TEXT("NumRegisters"));
		Visitor.Visit(ScratchNumParameters, TEXT("NumParameters"));
		Visitor.Visit(ScratchNumNamedParameters, TEXT("NumNamedParameters"));
		Visitor.Visit(ScratchNumConstants, TEXT("NumConstants"));
		Visitor.Visit(ScratchNumOpBytes, TEXT("NumOpBytes"));
		Visitor.Visit(ScratchNumOperands, TEXT("NumOperands"));
		Visitor.Visit(ScratchNumLabels, TEXT("NumLabels"));

		This = &VProcedure::NewUninitialized(Context, (uint32)ScratchNumParameters, (uint32)ScratchNumNamedParameters, (uint32)ScratchNumRegisters, (uint32)ScratchNumConstants, (uint32)ScratchNumOpBytes, (uint32)ScratchNumOperands, (uint32)ScratchNumLabels);

		uint64 ScratchNumNamedParams64 = 0;
		Visitor.BeginArray(TEXT("NamedParameters"), ScratchNumNamedParams64);
		Visitor.Visit(This->GetNamedParamsBegin(), This->GetNamedParamsEnd());
		Visitor.EndArray();

		uint64 ScratchNumConstants64 = 0;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants64);
		Visitor.Visit(This->GetConstantsBegin(), This->GetConstantsEnd());
		Visitor.EndArray();

		This->LoadOpCodes(Visitor);

		uint64 ScratchNumOperands64 = 0;
		Visitor.BeginArray(TEXT("Operands"), ScratchNumOperands64);
		Visitor.Visit(This->GetOperandsBegin(), This->GetOperandsEnd());
		Visitor.EndArray();

		uint64 ScratchNumLabels64 = 0;
		Visitor.BeginArray(TEXT("Labels"), ScratchNumLabels64);
		Visitor.Visit(This->GetLabelsBegin(), This->GetLabelsEnd());
		Visitor.EndArray();
	}
	else
	{
		uint32 ScratchNumRegisters = This->NumRegisters;
		uint32 ScratchNumParameters = This->NumParameters;
		uint32 ScratchNumNamedParameters = This->NumNamedParameters;
		uint32 ScratchNumConstants = This->NumConstants;
		uint64 ScratchNumOpBytes = (uint64)This->NumOpBytes;
		uint32 ScratchNumOperands = This->NumOperands;
		uint32 ScratchNumLabels = This->NumLabels;
		Visitor.Visit(ScratchNumRegisters, TEXT("NumRegisters"));
		Visitor.Visit(ScratchNumParameters, TEXT("NumParameters"));
		Visitor.Visit(ScratchNumNamedParameters, TEXT("NumNamedParameters"));
		Visitor.Visit(ScratchNumConstants, TEXT("NumConstants"));
		Visitor.Visit(ScratchNumOpBytes, TEXT("NumOpBytes"));
		Visitor.Visit(ScratchNumOperands, TEXT("NumOperands"));
		Visitor.Visit(ScratchNumLabels, TEXT("NumLabels"));

		uint64 ScratchNumNamedParams64 = This->NumNamedParameters;
		Visitor.BeginArray(TEXT("NamedParameters"), ScratchNumNamedParams64);
		Visitor.Visit(This->GetNamedParamsBegin(), This->GetNamedParamsEnd());
		Visitor.EndArray();

		uint64 ScratchNumConstants64 = This->NumConstants;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants64);
		Visitor.Visit(This->GetConstantsBegin(), This->GetConstantsEnd());
		Visitor.EndArray();

		This->SaveOpCodes(Visitor);

		uint64 ScratchNumOperands64 = This->NumOperands;
		Visitor.BeginArray(TEXT("Operands"), ScratchNumOperands64);
		Visitor.Visit(This->GetOperandsBegin(), This->GetOperandsEnd());
		Visitor.EndArray();

		uint64 ScratchNumLabels64 = This->NumLabels;
		Visitor.BeginArray(TEXT("Labels"), ScratchNumLabels64);
		Visitor.Visit(This->GetLabelsBegin(), This->GetLabelsEnd());
		Visitor.EndArray();
	}
}

} // namespace Verse

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
