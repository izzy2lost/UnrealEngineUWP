// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/SoftsSolverCollisionParticles.h"
#include "Chaos/SoftsSolverCollisionParticlesRange.h"
#include "Chaos/SoftsSolverParticlesRange.h"
#include "Chaos/CollectionPropertyFacade.h"

#if !defined(CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT)
#define CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
const bool bChaos_SoftBodyCollision_ISPC_Enabled = INTEL_ISPC && CHAOS_PER_PARTICLE_COLLISION_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_SoftBodyCollision_ISPC_Enabled;
#endif

namespace Chaos::Softs
{

class FPBDSoftBodyCollisionConstraintBase
{
public:
	FPBDSoftBodyCollisionConstraintBase(
		const TArray<FSolverRigidTransform3>& InLastCollisionTransforms,
		FSolverReal InCollisionThickness,
		FSolverReal InFrictionCoefficient,
		bool bInUseCCD,
		TArray<bool>* InCollisionParticleCollided = nullptr,
		TArray<FSolverVec3>* InContacts = nullptr,
		TArray<FSolverVec3>* InNormals = nullptr,
		TArray<FSolverReal>* InPhis = nullptr
		)
		: LastCollisionTransforms(InLastCollisionTransforms)
		, CollisionThickness(InCollisionThickness)
		, FrictionCoefficient(InFrictionCoefficient)
		, bUseCCD(bInUseCCD)
		, CollisionParticleCollided(InCollisionParticleCollided)
		, Contacts(InContacts)
		, Normals(InNormals)
		, Phis(InPhis)
	{}

	void SetWriteDebugContacts(bool bWrite) { bWriteDebugContacts = bWrite; }

	CHAOS_API void Apply(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const;

private:
	template<bool bLockAndWriteContacts, bool bWithFriction>
	void ApplyInternal(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const;
	template<bool bLockAndWriteContacts, bool bWithFriction>
	void ApplyInternalCCD(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const;

	template<bool bWithFriction>
	void ApplyInternalISPC(FSolverParticlesRange& Particles, const FSolverReal Dt, const TArray<FSolverCollisionParticlesRange>& CollisionParticles) const;

protected:
	const TArray<FSolverRigidTransform3>& LastCollisionTransforms; // Used by CCD
	FSolverReal CollisionThickness;
	FSolverReal FrictionCoefficient;
	bool bUseCCD;

	/**  Used for writing debug contacts */
	bool bWriteDebugContacts = false;
	TArray<bool>* const CollisionParticleCollided; // Per collision particle
	// List of contact data
	TArray<FSolverVec3>* const Contacts;
	TArray<FSolverVec3>* const Normals;
	TArray<FSolverReal>* const Phis;
	mutable FCriticalSection DebugMutex;
};


class FPBDSoftBodyCollisionConstraint : public FPBDSoftBodyCollisionConstraintBase
{
	typedef FPBDSoftBodyCollisionConstraintBase Base;

public:
	static constexpr FSolverReal DefaultCollisionThickness = (FSolverReal)1.;
	static constexpr FSolverReal DefaultFrictionCoefficient = (FSolverReal)0.8;

	FPBDSoftBodyCollisionConstraint(
		const TArray<FSolverRigidTransform3>& InLastCollisionTransforms,
		const FCollectionPropertyConstFacade& PropertyCollection,
		FSolverReal InMeshScale,
		TArray<bool>* InCollisionParticleCollided = nullptr,
		TArray<FSolverVec3>* InContacts = nullptr,
		TArray<FSolverVec3>* InNormals = nullptr,
		TArray<FSolverReal>* InPhis = nullptr)
		: Base(InLastCollisionTransforms,
			InMeshScale * GetCollisionThickness(PropertyCollection, DefaultCollisionThickness),
			GetFrictionCoefficient(PropertyCollection, DefaultFrictionCoefficient),
			GetUseCCD(PropertyCollection, false),
			InCollisionParticleCollided, InContacts, InNormals, InPhis)
		, MeshScale(InMeshScale)
		, CollisionThicknessIndex(PropertyCollection)
		, FrictionCoefficientIndex(PropertyCollection)
		, UseCCDIndex(PropertyCollection)
	{}

	CHAOS_API void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection);

private:
	const FSolverReal MeshScale;
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(CollisionThickness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(FrictionCoefficient, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseCCD, bool);
};

}  // End namespace Chaos::Softs
