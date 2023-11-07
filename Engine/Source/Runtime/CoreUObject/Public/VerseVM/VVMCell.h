// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMContext.h"
#include "VVMCppClassInfo.h"
#include "VVMHeap.h"
#include "VVMUnreachable.h"

#include <atomic>
#include <type_traits>

namespace Verse
{
struct FAccessContext;
struct VCppClassInfo;
struct FMarkStack;
struct VEmergentType;

struct VCell
{
	DECLARE_BASE_VCPPCLASSINFO(COREUOBJECT_API);

	/// The header word of a VCell is the offset of an emergent type and 4 extra bytes
	// (one reserved for GC)
	uint32 EmergentTypeOffset;
	uint8 GCData{0};
	// The first two bits of this are used by FExternalMutexes in VCell subclasses.
	std::atomic<uint8> Mutex{0};
	union
	{
		struct
		{
			uint8_t Misc2;
			uint8_t Misc3;
		};
		uint16 Misc2And3{0};
	};

	VCell(const VCell&) = delete;
	VCell& operator=(const VCell&) = delete;

	COREUOBJECT_API VCell(FAccessContext, const VEmergentType* EmergentType);

	const VEmergentType* GetEmergentType() const;
	const VCppClassInfo* GetCppClassInfo() const;

	// FIXME: In the future maybe these will take a FRunningContext or FAccessContext rather than
	// the MarkStack or nothing. That's because:
	// 1) Nothing wrong with saying that the GC threads allocate or run arbitrary code if the GC is
	//    concurrent by design. Like, maybe we'll want GC-time hash-consing.
	// 2) In a parallel GC, we'll probably want to just reuse the fact that each context has a
	//    MarkStack.
	template <typename TVisitor>
	void VisitReferences(TVisitor& Visitor);
	COREUOBJECT_API void ConductCensus();
	COREUOBJECT_API void RunDestructor();
	COREUOBJECT_API bool Equal(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

private:
	// Use this if your cell subtype has any outgoing strong references.  It is used by both the
	// GC system and the abstract visitor to collect strong references.
	//
	// Note that this function will run concurrently to the mutator (this function is being called in
	// some collector thread while the VM's other threads are calling other methods on your object)
	// and in parallel (the collector will run multiple threads calling this function). However,
	// you're guaranteed that this function will only be called once per object per collection cycle;
	// i.e. the GC will never call this function simultaneously for the same object.
	//
	// It is defined implicitly by DECLARE_DERIVED_VCPPCLASSINFO, and so you must either implement it
	// or use the DEFINE_TRIVIAL_VISIT_REFERENCES macro to explicitly define a trivial implementation.
	//
	// Visit outgoing references for self. Don't call visit on your super class, or visit references
	// defined by your super class.
	template <typename TVisitor>
	void VisitReferencesImpl(TVisitor&);

protected:
	// Override this if your cell subtype has any outgoing weak references. Call ClearWeakDuringCensus
	// on those pointers in this function.
	//
	// Note that this function may run concurrently to the mutator, in parallel, or from within the
	// mutator's allocation slow paths. This means that this function cannot allocate, since it may be
	// called while internal allocator locks are held. It may also be called while mutator locks are
	// held, if those locks are held across allocations. So, this function cannot acquire locks,
	// unless those locks are never held while either handshaking or allocating.
	//
	// This function is called exactly once per collection cycle for any marked objects that belong to
	// the CensusSpace or the DestructorAndCensusSpace. This function is guaranteed to be called only
	// after all marking is finished, so you can use FHeap::IsMarked and that will tell you if the
	// object survived the collection or not. Because census runs before destruction and sweep, you're
	// also guaranteed that any pointed-to dead objects are still fully intact and usable.
	//
	// It's not meaningful to override this function unless the cell is allocated from the CensusSpace
	// or the DestructorAndCensusSpace.
	COREUOBJECT_API void ConductCensusImpl();

	// Override this if your cell requries deep comparison (simple comparisons should be inlined in VValue::Equal).
	//
	// Note: Using this override may invoke a TLS lookup to acquire the FRunningContext which is expensive.
	// Deep comparisons typically will require a FRunningContext anyways but it is worth checking this is
	// the case each time this is implemented.
	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	// Override this if your cell subtype requires a deep hash.
	COREUOBJECT_API uint32 GetTypeHashImpl();

	// VCell() and SetEmergentType(..) are used during setup when creating some cyclic dependencies.
	VCell()
		: EmergentTypeOffset(0)
	{
		checkSlow(FHeap::OwnsAddress(this));
	}

	void SetEmergentType(FAccessContext, VEmergentType* EmergentType);

public:
	// Override this if your cell subtype has a destructor.
	//
	// Note that this function may run concurrently to the mutator, in parallel, or from within the
	// mutator's allocation slow paths. This means that this function cannot allocate, since it may be
	// called while internal allocator locks are held. It may also be called while mutator locks are
	// held, if those locks are held across allocations. So, this function cannot acquire locks,
	// unless those locks are never held while either handshaking or allocating.
	//
	// This function is called exactly once per collection cycle for any unmarked objects that belong
	// to the DestructorSpace or the DestructorAndCensusSpace. This function is guaranteed to be called
	// only after all marking and census are finished. Because census doesn't cover unmarked objects,
	// you will see nonnull weak references to dead objects. Because destruction runs before sweep,
	// you're also guaranteed that any pointed-to dead objects are still fully intact and usable.
	//
	// It's not meaningful to override this function unless the cell is allocated from the
	// DestructorSpace or the DestructorAndCensusSpace.
	//
	// Note: You must override this if your cell has external memory to report swept external
	// bytes during destruction.
	~VCell() = default;

	template <typename CastType>
	bool IsA() const;

	template <typename CastType>
	const CastType& StaticCast() const;

	template <typename CastType>
	CastType& StaticCast();

	template <typename CastType>
	CastType* DynamicCast();

	template <typename CastType>
	CastType* DynamicCast() const;

	COREUOBJECT_API FString DebugName() const;
};

static_assert(sizeof(VCell) <= 8);

/// `VHeapValue` represents Verse-facing values, while `VCell` represents VM-internal structures.
// To be or not to be ...
// Keep it here for now.
struct VHeapValue : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);

	VHeapValue(FAccessContext Context, const VEmergentType* EmergentType)
		: VCell(Context, EmergentType)
	{
	}
};

} // namespace Verse
