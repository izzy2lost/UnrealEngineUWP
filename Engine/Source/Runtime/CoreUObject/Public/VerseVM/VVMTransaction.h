// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "AutoRTFM/AutoRTFM.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMLog.h"
#include "VVMWriteBarrier.h"
#include <Containers/Array.h>
#include <Containers/Set.h>

namespace Verse
{

struct FMarkStack;

// T and U are types which are ultimately pointers.
// We tag if the variant holds T or U by tagging the lower bit, so the pointers
// must be at least 2 byte aligned.
template <typename T, typename U>
struct TPtrVariant
{
	static_assert(sizeof(T) == sizeof(U));
	static_assert(sizeof(T) == sizeof(uintptr_t));
	static_assert(!std::is_same_v<T, U>);

	static constexpr uintptr_t UTag = 1;

	TPtrVariant(U InU)
	{
		uintptr_t IncomingPtr = BitCast<uintptr_t>(InU);
		checkSlow(!(IncomingPtr & UTag));
		Ptr = IncomingPtr | UTag;
	}

	TPtrVariant(T InT)
	{
		uintptr_t IncomingPtr = BitCast<uintptr_t>(InT);
		checkSlow(!(IncomingPtr & UTag));
		Ptr = IncomingPtr;
	}

	template <typename V>
	bool Is()
	{
		static_assert(std::is_same_v<T, V> || std::is_same_v<U, V>);
		if constexpr (std::is_same_v<T, V>)
		{
			return !(Ptr & UTag);
		}
		if constexpr (std::is_same_v<U, V>)
		{
			return !!(Ptr & UTag);
		}
		VERSE_UNREACHABLE();
	}

	template <typename V>
	V As()
	{
		static_assert(std::is_same_v<T, V> || std::is_same_v<U, V>);
		if constexpr (std::is_same_v<T, V>)
		{
			checkSlow(Is<T>());
			return BitCast<T>(Ptr);
		}
		if constexpr (std::is_same_v<U, V>)
		{
			checkSlow(Is<U>());
			return BitCast<U>(Ptr & ~UTag);
		}
		VERSE_UNREACHABLE();
	}

	bool operator==(TPtrVariant Other) const
	{
		return Ptr == Other.Ptr;
	}

	uintptr_t RawPtr() const { return Ptr; }

private:
	uintptr_t Ptr;
};

using FAuxOrCell = TPtrVariant<TAux<void>, VCell*>;

template <typename T, typename U>
inline uint32 GetTypeHash(TPtrVariant<T, U> Ptr)
{
	return PointerHash(BitCast<void*>(Ptr.RawPtr()));
}

struct FTransactionLog
{
public:
	struct FEntry
	{
		uintptr_t Key() { return Slot.RawPtr(); }

		using FSlot = TPtrVariant<TWriteBarrier<VValue>*, TWriteBarrier<TAux<void>>*>;

		FAuxOrCell Owner; // The object that needs to remain alive so that we can write OldValue into Slot on abort.
		FSlot Slot;       // The memory location we write OldValue to into on abort.
		uint64 OldValue;  // VValue or TAux<void> depending on how Slot is encoded.
		static_assert(sizeof(OldValue) == sizeof(VValue));
		static_assert(sizeof(OldValue) == sizeof(TAux<void>));

		FEntry(FAuxOrCell Owner, TWriteBarrier<VValue>& InSlot, VValue OldValue)
			: Owner(Owner)
			, Slot(&InSlot)
			, OldValue(OldValue.GetEncodedBits())
		{
		}

		FEntry(FAuxOrCell Owner, TWriteBarrier<TAux<void>>& InSlot, TAux<void> OldValue)
			: Owner(Owner)
			, Slot(&InSlot)
			, OldValue(BitCast<uint64>(OldValue.GetPtr()))
		{
		}

		void Abort(FAccessContext Context)
		{
			if (Slot.Is<TWriteBarrier<TAux<void>>*>())
			{
				TWriteBarrier<TAux<void>>* AuxSlot = Slot.As<TWriteBarrier<TAux<void>>*>();
				AuxSlot->Set(Context, TAux<void>(BitCast<void*>(OldValue)));
			}
			else
			{
				TWriteBarrier<VValue>* ValueSlot = Slot.As<TWriteBarrier<VValue>*>();
				ValueSlot->Set(Context, VValue::Decode(OldValue));
			}
		}

		void MarkReferencedCells(FMarkStack&);
	};

	TSet<uintptr_t> IsInLog; // TODO: We should probably use something like AutoRTFM's HitSet
	TSet<FAuxOrCell> Roots;
	TArray<FEntry> Log;

public:
	void Add(FEntry Entry)
	{
		bool AlreadyHasEntry;
		IsInLog.FindOrAdd(Entry.Key(), &AlreadyHasEntry);
		if (!AlreadyHasEntry)
		{
			Log.Add(MoveTemp(Entry));
		}
	}

	// This version avoids loading from Slot until we need it.
	template <typename T>
	void AddImpl(FAuxOrCell Owner, TWriteBarrier<T>& Slot)
	{
		bool AlreadyHasEntry;
		IsInLog.FindOrAdd(FEntry::FSlot(&Slot).RawPtr(), &AlreadyHasEntry);
		if (!AlreadyHasEntry)
		{
			Log.Add(FEntry{Owner, Slot, Slot.Get()});
		}
	}

	void Add(VCell& Owner, TWriteBarrier<VValue>& Slot)
	{
		AddImpl(FAuxOrCell(&Owner), Slot);
	}

	void Add(VCell& Owner, TWriteBarrier<TAux<void>>& Slot)
	{
		AddImpl(FAuxOrCell(&Owner), Slot);
	}

	template <typename T>
	void Add(TAux<T> Owner, TWriteBarrier<VValue>& Slot)
	{
		AddImpl(FAuxOrCell(BitCast<TAux<void>>(Owner)), Slot);
	}

	void AddRoot(FAuxOrCell Root)
	{
		Roots.Add(Root);
	}

	void Join(FTransactionLog& Child)
	{
		for (FEntry Entry : Child.Log)
		{
			Add(MoveTemp(Entry));
		}
	}

	void Abort(FAccessContext Context)
	{
		for (FEntry Entry : Log)
		{
			Entry.Abort(Context);
		}
	}

	void MarkReferencedCells(FMarkStack&);
};

struct FTransaction
{
	FTransactionLog Log;
	FTransaction* Parent{nullptr};
	bool bHasStarted{false};
	bool bHasCommitted{false};
	bool bHasAborted{false};

	// Note: We can Abort before we Start because of how leniency works. For example, we can't
	// Start the transaction until the effect token is concrete, but the effect token may become
	// concrete after failure occurs.
	void Start(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasStarted);
		V_DIE_IF(Parent);
		bHasStarted = true;

		if (!bHasAborted)
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			Parent = Context.CurrentTransaction();
			Context.SetCurrentTransaction(this);
		}
	}

	// We can't call Commit before we Start because we serialize Start then Commit via the effect token.
	void Commit(FRunningContext Context)
	{
		V_DIE_UNLESS(bHasStarted);
		V_DIE_IF(bHasAborted);
		V_DIE_IF(bHasCommitted);
		bHasCommitted = true;
		AutoRTFM::ForTheRuntime::CommitTransaction();
		if (Parent)
		{
			Parent->Log.Join(Log);
		}
		Context.SetCurrentTransaction(Parent);
	}

	// See above comment as to why we might Abort before we start.
	void Abort(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasAborted);
		bHasAborted = true;
		if (bHasStarted)
		{
			V_DIE_UNLESS(Context.CurrentTransaction() == this);
			AutoRTFM::AbortTransaction();
			AutoRTFM::ForTheRuntime::ClearTransactionStatus();
			Log.Abort(Context);
			Context.SetCurrentTransaction(Parent);
		}
		else
		{
			V_DIE_IF(Parent);
		}
	}

	void LogBeforeWrite(FAccessContext Context, VCell& Owner, TWriteBarrier<VValue>& Slot)
	{
		Log.Add(Owner, Slot);
	}

	void LogBeforeWrite(FAccessContext Context, VCell& Owner, TWriteBarrier<TAux<void>>& Slot)
	{
		Log.Add(Owner, Slot);
	}

	template <typename T>
	void LogBeforeWrite(FAccessContext Context, TAux<T> Owner, TWriteBarrier<VValue>& Slot)
	{
		Log.Add(Owner, Slot);
	}

	template <typename T>
	void AddAuxRoot(FAccessContext Context, TAux<T> Root)
	{
		Log.AddRoot(FAuxOrCell(BitCast<TAux<void>>(Root)));
	}

	void AddRoot(FAccessContext Context, VCell* Root)
	{
		Log.AddRoot(FAuxOrCell(Root));
	}

	static void MarkReferencedCells(FTransaction&, FMarkStack&);
};

} // namespace Verse
#endif // WITH_VERSE_VM
