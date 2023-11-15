// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/ArrayCollection.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/SoftsSolverParticlesRange.h"
#include "Chaos/SoftsSolverCollisionParticles.h"
#include "Chaos/SoftsSolverCollisionParticlesRange.h"
#include "Chaos/VelocityField.h"

namespace Chaos::Softs
{

/**
 * Solver can contain multiple "Groups". Groups do not interact with each other. 
 * They may be in different spaces. They may be solved in parallel, completely independently of each other. 
 * The only reason why they're in the same evolution is because they share the same solver
 * settings and step together in time. 
 * 
 * A Group can contain multiple "SoftBodies". SoftBodies can interact but have different 
 * constraint rules/forces.
 */
class FEvolution 
{
public:

	CHAOS_API FEvolution(const FCollectionPropertyConstFacade& Properties);
	~FEvolution() = default;

	/** Reset/empty everything.*/
	CHAOS_API void Reset();

	/** Move forward in time */
	CHAOS_API void AdvanceOneTimeStep(const FSolverReal Dt, const FSolverReal TimeDependentIterationMultiplier);

	/** Add custom collection arrays */
	void AddGroupArray(TArrayCollectionArrayBase* Array) { Groups.AddArray(Array); }
	void AddParticleArray(TArrayCollectionArrayBase* Array) { Particles.AddArray(Array); }
	void AddCollisionParticleArray(TArrayCollectionArrayBase* Array) { CollisionParticles.AddArray(Array); }

	const FSolverParticles& GetParticles() const { return Particles; }
	// Giving non-const access so data can be set freely, but do not add or remove particles here. Use AddSoftBody
	FSolverParticles& GetParticles() { return Particles; }

	const TSet<uint32>& GetActiveGroups() const { return ActiveGroups; }
	CHAOS_API int32 NumActiveParticles() const;

	/** 
	 * Add a SoftBody to a Group.
	 * 
	 * @return SoftBodyId
	 */
	CHAOS_API int32 AddSoftBody(uint32 GroupId, int32 NumParticles, bool bEnable);
	int32 GetSoftBodyParticleNum(int32 SoftBodyId) const { return SoftBodies.ParticleRanges[SoftBodyId].GetRangeSize(); }
	int32 GetSoftBodyGroupId(int32 SoftBodyId) const { return SoftBodies.GroupId[SoftBodyId]; }
	CHAOS_API void SetSoftBodyProperties(int32 SoftBodyId, const FCollectionPropertyConstFacade& PropertyCollection);
	CHAOS_API void ActivateSoftBody(int32 SoftBodyId, bool bActivate);
	bool IsSoftBodyActive(int32 SoftBodyId) const { return SoftBodies.Active[SoftBodyId]; }
	FSolverParticlesRange& GetSoftBodyParticles(int32 SoftBodyId) { return SoftBodies.ParticleRanges[SoftBodyId]; }
	const FSolverParticlesRange& GetSoftBodyParticles(int32 SoftBodyId) const { return SoftBodies.ParticleRanges[SoftBodyId]; }
	const TArray<int32>& GetGroupSoftBodies(uint32 GroupId) const { return Groups.SoftBodies[GroupId]; }
	const TSet<int32>& GetGroupActiveSoftBodies(uint32 GroupId) const { return Groups.ActiveSoftBodies[GroupId]; }

	/**
	 * Add Collision particle range to a group.
	 * 
	 * @return Particle range offset (unique id for this range)
	 */
	CHAOS_API int32 AddCollisionParticleRange(uint32 GroupId, int32 NumParticles, bool bEnable);
	CHAOS_API void RemoveCollisionParticleRange(int32 CollisionRangeId);
	CHAOS_API void ActivateCollisionParticleRange(int32 CollisionRangeId, bool bEnable);
	const TSet<int32>& GetGroupActiveCollisionParticleRanges(uint32 GroupId) const { return Groups.ActiveCollisionParticleRanges[GroupId]; }
	CHAOS_API TArray<FSolverCollisionParticlesRange> GetActiveCollisionParticles(uint32 GroupId) const;

	FSolverCollisionParticlesRange& GetCollisionParticleRange(int32 CollisionRangeId) { return CollisionRanges.ParticleRanges[CollisionRangeId]; }
	const FSolverCollisionParticlesRange& GetCollisionParticleRange(int32 CollisionRangeId) const { return CollisionRanges.ParticleRanges[CollisionRangeId]; }
	
	/** Global Rules*/
	typedef TFunction<void(FSolverParticlesRange&, const FSolverReal Dt, const FSolverReal Time)> KinematicUpdateFunc;
	typedef TFunction<void(FSolverCollisionParticlesRange&, const FSolverReal Dt, const FSolverReal Time)> CollisionKinematicUpdateFunc;
	void SetKinematicUpdateFunction(KinematicUpdateFunc Func) { KinematicUpdate = Func; }
	void SetCollisionKinematicUpdateFunction(CollisionKinematicUpdateFunc Func) { CollisionKinematicUpdate = Func; }

	/** Soft Body Rules. Add Ranges to allocate space for your rules. You will get back an ArrayView where you can then set the rules. */
	typedef TFunction<void(const FSolverParticlesRange&, const FSolverReal)> PreSubstepParallelInitFunc;
	typedef TFunction<void(FSolverParticlesRange&, const FSolverReal)> ExternalForceRuleFunc;
	typedef TFunction<void(const FSolverParticlesRange&, const FSolverReal)> ConstraintParallelInitFunc;
	typedef TFunction<void(FSolverParticlesRange&, const FSolverReal)> ConstraintRuleFunc;
	typedef TFunction<void(FSolverParticlesRange&, const FSolverReal, const TArray<FSolverCollisionParticlesRange>&)> CollisionConstraintRuleFunc;
	// Warning: Rules are allocated into shared buffers. The ArrayViews may go stale if you allocate ANY new rules.
	// Allocate your batch of rules, then get your array views.	
	void AllocatePreSubstepParallelInitRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ConstParticleRules, SoftBodies.PreSubstepParallelInits);
	}
	void AllocateExternalForceRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ParticleRules, SoftBodies.ExternalForceRules);
	}
	void AllocateConstraintParallelInitsRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ConstParticleRules, SoftBodies.ConstraintParallelInits);
	}
	void AllocatePreSubstepConstraintRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ParticleRules, SoftBodies.PreSubstepConstraintRules);
	}
	void AllocatePerIterationConstraintRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ParticleRules, SoftBodies.PerIterationConstraintRules);
	}
	void AllocatePerIterationCollisionConstraintRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, CollisionRules, SoftBodies.PerIterationCollisionConstraintRules);
	}
	void AllocatePerIterationPostCollisionsConstraintRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ParticleRules, SoftBodies.PerIterationPostCollisionsConstraintRules);
	}
	void AllocatePostSubstepConstraintRulesRange(int32 SoftBodyId, int32 NumRules)
	{
		return AllocateRulesRange(SoftBodyId, NumRules, ParticleRules, SoftBodies.PostSubstepConstraintRules);
	}

	TArrayView<PreSubstepParallelInitFunc> GetPreSubstepParallelInitRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PreSubstepParallelInits);
	}
	TArrayView<ExternalForceRuleFunc> GetExternalForceRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.ExternalForceRules);
	}
	TArrayView<ConstraintParallelInitFunc> GetConstraintParallelInitsRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.ConstraintParallelInits);
	}
	TArrayView<ConstraintRuleFunc> GetPreSubstepConstraintRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PreSubstepConstraintRules);
	}
	TArrayView<ConstraintRuleFunc> GetPerIterationConstraintRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PerIterationConstraintRules);
	}
	TArrayView<CollisionConstraintRuleFunc> GetPerIterationCollisionConstraintRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PerIterationCollisionConstraintRules);
	}
	TArrayView<ConstraintRuleFunc> GetPerIterationPostCollisionsConstraintRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PerIterationPostCollisionsConstraintRules);
	}
	TArrayView<ConstraintRuleFunc> GetPostSubstepConstraintRulesRange(int32 SoftBodyId)
	{
		return GetRulesRange(SoftBodyId, SoftBodies.PostSubstepConstraintRules);
	}

	/** Solver settings */
	FSolverReal GetTime() const { return Time; }
	int32 GetIterations() const { return NumIterations; }
	int32 GetMaxIterations() const { return MaxNumIterations; }
	bool GetDisableTimeDependentNumIterations() const { return bDisableTimeDependentNumIterations; }
	bool GetDoQuasistatics() const { return bDoQuasistatics; }
	void SetDisableTimeDependentNumIterations(bool bDisable) { bDisableTimeDependentNumIterations = bDisable; }
	CHAOS_API void SetSolverProperties(const FCollectionPropertyConstFacade& PropertyCollection);

private:

	template<typename ElementType>
	struct TArrayRange
	{
		static TArrayRange AddRange(TArray<ElementType>& InArray, int32 InRangeSize)
		{
			TArrayRange Range;
			Range.Offset = InArray.Num();
			Range.Array = &InArray;
			Range.Array->AddDefaulted(InRangeSize);
			Range.RangeSize = InRangeSize;
			return Range;
		}

		bool IsValid() const
		{
			return Array && Offset >= 0 && Offset + RangeSize <= Array->Num();
		}

		TConstArrayView<ElementType> GetConstArrayView() const
		{
			check(IsValid());
			return TConstArrayView<ElementType>(Array->GetData() + Offset, RangeSize);
		}

		TArrayView<ElementType> GetArrayView()
		{
			check(IsValid());
			return TArrayView<ElementType>(Array->GetData() + Offset, RangeSize);
		}

		bool IsEmpty() const { return RangeSize == 0; }
		int32 GetRangeSize() const { return RangeSize; }
	private:
		TArray<ElementType>* Array = nullptr;
		int32 Offset = INDEX_NONE;
		int32 RangeSize = 0;
	};

	// SoftBody SOA
	struct FSoftBodies : public TArrayCollection
	{
		FSoftBodies()
		{
			TArrayCollection::AddArray(&Active);
			TArrayCollection::AddArray(&GroupId);
			TArrayCollection::AddArray(&ParticleRanges);
			TArrayCollection::AddArray(&GlobalDampings);
			TArrayCollection::AddArray(&LocalDampings);
			TArrayCollection::AddArray(&UsePerParticleDamping);
			TArrayCollection::AddArray(&PreSubstepParallelInits);
			TArrayCollection::AddArray(&ExternalForceRules);
			TArrayCollection::AddArray(&ConstraintParallelInits);
			TArrayCollection::AddArray(&PreSubstepConstraintRules);
			TArrayCollection::AddArray(&PerIterationConstraintRules);
			TArrayCollection::AddArray(&PerIterationCollisionConstraintRules);
			TArrayCollection::AddArray(&PerIterationPostCollisionsConstraintRules);
			TArrayCollection::AddArray(&PostSubstepConstraintRules);
		}

		void Reset()
		{
			ResizeHelper(0);
		}

		int32 AddSoftBody()
		{
			const int32 Offset = Size();
			AddElementsHelper(1);
			return Offset;
		}

		TArrayCollectionArray<bool> Active;
		TArrayCollectionArray<uint32> GroupId;
		TArrayCollectionArray<FSolverParticlesRange> ParticleRanges;
		TArrayCollectionArray<FSolverReal> GlobalDampings;
		TArrayCollectionArray<FSolverReal> LocalDampings;
		TArrayCollectionArray<bool> UsePerParticleDamping;
		TArrayCollectionArray<TArrayRange<PreSubstepParallelInitFunc>> PreSubstepParallelInits;
		TArrayCollectionArray<TArrayRange<ExternalForceRuleFunc>> ExternalForceRules;
		TArrayCollectionArray<TArrayRange<ConstraintParallelInitFunc>> ConstraintParallelInits;
		TArrayCollectionArray<TArrayRange<ConstraintRuleFunc>> PreSubstepConstraintRules;
		TArrayCollectionArray<TArrayRange<ConstraintRuleFunc>> PerIterationConstraintRules;
		TArrayCollectionArray<TArrayRange<CollisionConstraintRuleFunc>> PerIterationCollisionConstraintRules;
		TArrayCollectionArray<TArrayRange<ConstraintRuleFunc>> PerIterationPostCollisionsConstraintRules;
		TArrayCollectionArray<TArrayRange<ConstraintRuleFunc>> PostSubstepConstraintRules;
	};

	// CollisionBodyRange SOA
	struct FCollisionBodyRanges : public TArrayCollection
	{
		FCollisionBodyRanges()
		{
			TArrayCollection::AddArray(&Status);
			TArrayCollection::AddArray(&GroupId);
			TArrayCollection::AddArray(&ParticleRanges);
		}

		void Reset()
		{
			ResizeHelper(0);
		}

		int32 AddRange()
		{
			const int32 Offset = Size();
			AddElementsHelper(1);
			return Offset;
		}

		enum struct EStatus : uint8
		{
			Invalid = 0,
			Active = 1,
			Inactive = 2,
			Free = 3 // Available for recycling
		};
		TArrayCollectionArray<EStatus> Status;
		TArrayCollectionArray<uint32> GroupId;
		TArrayCollectionArray<FSolverCollisionParticlesRange> ParticleRanges;
	};

	struct FGroups : public TArrayCollection
	{
		FGroups()
		{
			TArrayCollection::AddArray(&SoftBodies);
			TArrayCollection::AddArray(&ActiveSoftBodies);
			TArrayCollection::AddArray(&ActiveCollisionParticleRanges);
		}
		
		void Reset()
		{
			ResizeHelper(0);
		}

		void AddGroupsToSize(uint32 DesiredSize)
		{
			if (ensure(DesiredSize >= Size()))
			{
				ResizeHelper((int32)DesiredSize);
			}
		}

		TArrayCollectionArray<TArray<int32>> SoftBodies;
		TArrayCollectionArray<TSet<int32>> ActiveSoftBodies;
		TArrayCollectionArray<TSet<int32>> ActiveCollisionParticleRanges;
	};

	template<typename RuleFunc>
	void AllocateRulesRange(int32 SoftBodyId, int32 NumRules, TArray<RuleFunc>& RuleArray, TArrayCollectionArray<TArrayRange<RuleFunc>>& RangeArray)
	{
		check(RangeArray[SoftBodyId].IsEmpty());
		RangeArray[SoftBodyId] = TArrayRange<RuleFunc>::AddRange(RuleArray, NumRules);
	}

	template<typename RuleFunc>
	TArrayView<RuleFunc> GetRulesRange(int32 SoftBodyId, TArrayCollectionArray<TArrayRange<RuleFunc>>& RangeArray)
	{
		return RangeArray[SoftBodyId].GetArrayView();
	}

	void AdvanceOneTimeStepInternal(const FSolverReal Dt, const int32 TimeDependentNumIterations, uint32 GroupId);

	// Solver data
	int32 NumIterations;
	int32 MaxNumIterations; // Used for time-dependent iteration counts
	bool bDisableTimeDependentNumIterations;
	bool bDoQuasistatics;
	FSolverReal SolverFrequency; 
	FSolverReal Time;

	// Per-Particle data
	FSolverParticles Particles;
	TArrayCollectionArray<FSolverReal> ParticleDampings;

	// Per-Collision particle data
	FSolverCollisionParticles CollisionParticles;

	// Per-SoftBody data
	FSoftBodies SoftBodies;

	// Per-CollisionBodyRange data
	FCollisionBodyRanges CollisionRanges;

	// Collision Range free-list
	TMap<int32, TArray<int32>> CollisionRangeFreeList; // Key = NumParticles, Value = CollisionRangeId(s)

	// Per-Group data
	FGroups Groups;
	TSet<uint32> ActiveGroups; // Groups with at least one active softbody

	KinematicUpdateFunc KinematicUpdate;
	CollisionKinematicUpdateFunc CollisionKinematicUpdate;

	// Rules that run on ParticleRanges (SoftBodies will have views into these)
	TArray<TFunction<void(const FSolverParticlesRange&, const FSolverReal)>> ConstParticleRules;
	TArray<TFunction<void(FSolverParticlesRange&, const FSolverReal)>> ParticleRules;

	// Collision Rules
	TArray<TFunction<void(FSolverParticlesRange&, const FSolverReal, const TArray<FSolverCollisionParticlesRange>&)>> CollisionRules;
};
}