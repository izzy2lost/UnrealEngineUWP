// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothConstraints.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothingPatternData.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/XPBDStretchBiasElementConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/XPBDAnisotropicBendingConstraints.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/XPBDAxialSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/XPBDLongRangeConstraints.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDShapeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDSelfCollisionSphereConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/PBDTriangleMeshIntersections.h"
#include "Chaos/PBDEvolution.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/Deformable/GaussSeidelMasterConstraint.h"
#include "Chaos/Deformable/GaussSeidelCorotatedCodimensionalConstraints.h"

namespace Chaos {
#if !UE_BUILD_SHIPPING

	bool bEnableGS = false;

    static FAutoConsoleVariableRef CVarClothbEnableGS(TEXT("p.Chaos.Cloth.EnableGaussSeidel"), bEnableGS, TEXT("Use Gauss Seidel constraints instead of XPBD [def: false]"));

	bool bDisplayResidual = false;

	static FAutoConsoleVariableRef CVarClothbDisplayResidual(TEXT("p.Chaos.Cloth.DisplayResidual"), bDisplayResidual, TEXT("Diplay residual norms for the first 10 iters [def: false]"));

	int32 MaxResidualIters = 10;

	static FAutoConsoleVariableRef CVarClothMaxResidualIters(TEXT("p.Chaos.Cloth.MaxResidualIters"), MaxResidualIters, TEXT("Max number of iterations to diaplay residuals [def: 10]"));

	bool bWriteResidual2File = false;

	static FAutoConsoleVariableRef CVarClothbWriteResidual2File(TEXT("p.Chaos.Cloth.WriteResidual2File"), bWriteResidual2File, TEXT("Write residual to file [def: false]"));

	bool bReplaceBiasElementsWithCorotatedCod = false;

	static FAutoConsoleVariableRef CVarClothReplaceBiasElementsWithCorotatedCod(TEXT("p.Chaos.Cloth.ReplaceBiasElementsWithCorotatedCod"), bReplaceBiasElementsWithCorotatedCod, TEXT("Replace existing aniso bias element constraint with gauss seidel corotated codimensional [def: false]"));

	Softs::FSolverReal YoungsModulus = 10000.f;

	static FAutoConsoleVariableRef CVarClothYoungsModulus(TEXT("p.Chaos.Cloth.YoungsModulus"), YoungsModulus, TEXT("Youngs modulus [def: 1e4]"));

	bool bClothDoQuasistatics = false;

	static FAutoConsoleVariableRef CVarClothDoQuasistatics(TEXT("p.Chaos.Cloth.DoQuasistatics"), bClothDoQuasistatics, TEXT("Do cloth quasistatics [def: false]"));

	bool bEnableCG = false;

	static FAutoConsoleVariableRef CVarClothEnableCG(TEXT("p.Chaos.Cloth.EnableCG"), bEnableCG, TEXT("Use conjugate gradient instead of nonlinear gauss seidel [def: false]"));

	bool bMakeSandwich = false;

	static FAutoConsoleVariableRef CVarClothMakeSandwich(TEXT("p.Chaos.Cloth.MakeSandwich"), bMakeSandwich, TEXT("MakeSandwich solver of xpbd - Newton/GS [def: false]"));

	int32 UpperBreadIters = 3;

	static FAutoConsoleVariableRef CVarClothUpperBreadIters(TEXT("p.Chaos.Cloth.UpperBreadIters"), UpperBreadIters, TEXT("Upper Bread Iters for the sandwich solver [def: 3]"));

	int32 MiddleBreadIters = 3;

	static FAutoConsoleVariableRef CVarClothMiddleBreadIters(TEXT("p.Chaos.Cloth.MiddleBreadIters"), MiddleBreadIters, TEXT("Middle Bread Iters for the sandwich solver [def: 3]"));

	bool bWriteFinalResiduals = false;

	static FAutoConsoleVariableRef CVarClothWriteFinalResiduals(TEXT("p.Chaos.Cloth.WriteFinalResiduals"), bWriteFinalResiduals, TEXT("Write final residuals at each timestep to a file [def: false]"));

	bool bUseSOR = true;

	static FAutoConsoleVariableRef CVarClothUseSOR(TEXT("p.Chaos.Cloth.UseSOR"), bUseSOR, TEXT("Use SOR acceleration for Gauss Seidel [def: true]"));

	Softs::FSolverReal SOROmega = 1.7f;

	static FAutoConsoleVariableRef CVarClothSOROmega(TEXT("p.Chaos.Cloth.SOROmega"), SOROmega, TEXT("SOR omega coefficient for acceleration [def: 1.7]"));

#endif

FClothConstraints::FClothConstraints()
	: Evolution(nullptr)
	, AnimationPositions(nullptr)
	, OldAnimationPositions_Deprecated(nullptr)
	, AnimationNormals(nullptr)
	, AnimationVelocities(nullptr)
	, ParticleOffset(0)
	, NumParticles(0)
	, ConstraintInitOffset(INDEX_NONE)
	, ConstraintRuleOffset(INDEX_NONE)
	, PostCollisionConstraintRuleOffset(INDEX_NONE)
	, PostprocessingConstraintRuleOffset(INDEX_NONE)
	, NumConstraintInits(0)
	, NumConstraintRules(0)
	, NumPostCollisionConstraintRules(0)
	, NumPostprocessingConstraintRules(0)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FClothConstraints::~FClothConstraints()
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FClothConstraints::Initialize(
	Softs::FPBDEvolution* InEvolution,
	const TArray<Softs::FSolverVec3>& InAnimationPositions,
	const TArray<Softs::FSolverVec3>& InOldAnimationPositions,
	const TArray<Softs::FSolverVec3>& InAnimationNormals,
	int32 InParticleOffset,
	int32 InNumParticles)
{
	Evolution = InEvolution;
	AnimationPositions = &InAnimationPositions;
	OldAnimationPositions_Deprecated = &InOldAnimationPositions;
	AnimationNormals = &InAnimationNormals;
	AnimationVelocities = nullptr;
	ParticleOffset = InParticleOffset;
	NumParticles = InNumParticles;
}

void FClothConstraints::Initialize(
	Softs::FPBDEvolution* InEvolution,
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
	const TArray<Softs::FSolverVec3>& /*InOldAnimationPositions*/, // deprecated
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
	const TArray<Softs::FSolverVec3>& InAnimationVelocities,
	int32 InParticleOffset,
	int32 InNumParticles)
{
	Evolution = InEvolution;
	AnimationPositions = &InInterpolatedAnimationPositions;
	OldAnimationPositions_Deprecated = nullptr;
	AnimationNormals = &InInterpolatedAnimationNormals;
	AnimationVelocities = &InAnimationVelocities;
	ParticleOffset = InParticleOffset;
	NumParticles = InNumParticles;
}

void FClothConstraints::Enable(bool bEnable)
{
	check(Evolution);
	if (ConstraintInitOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintInitRange(ConstraintInitOffset, bEnable);
	}
	if (ConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintRuleRange(ConstraintRuleOffset, bEnable);
	}
	if (PostCollisionConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivatePostCollisionConstraintRuleRange(PostCollisionConstraintRuleOffset, bEnable);
	}
	if (PostprocessingConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintPostprocessingsRange(PostprocessingConstraintRuleOffset, bEnable);
	}
}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const TArray<TConstArrayView<FRealSingle>>& WeightMapArray,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale,
	bool bEnabled)
{
	// Build new weight map container from the old legacy weight map enum
	TMap<FString, TConstArrayView<FRealSingle>> WeightMaps;

	const UEnum* const ChaosWeightMapTargetEnum = StaticEnum<EChaosWeightMapTarget>();
	const int32 NumWeightMaps = (int32)ChaosWeightMapTargetEnum->GetMaxEnumValue() + 1;

	WeightMaps.Reserve(NumWeightMaps);

	for (int32 EnumIndex = 0; EnumIndex < ChaosWeightMapTargetEnum->NumEnums(); ++EnumIndex)
	{
		const int32 TargetIndex = (int32)ChaosWeightMapTargetEnum->GetValueByIndex(EnumIndex);
		const FString WeightMapName = ChaosWeightMapTargetEnum->GetNameByIndex(EnumIndex).ToString();

		WeightMaps.Add(WeightMapName, WeightMapArray[TargetIndex]);
	}

	// Call new AddRules function
	AddRules(ConfigProperties, TriangleMesh, nullptr, WeightMaps, TMap<FString, const TSet<int32>*>(),
		Tethers, MeshScale, bEnabled);
}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale, bool bEnabled)
{
	AddRules(ConfigProperties, TriangleMesh, PatternData, WeightMaps, TMap<FString, const TSet<int32>*>(),
		Tethers, MeshScale, bEnabled);
}

//only counts the number of rules that GS/CG currently cupports
void FClothConstraints::GetGSNumRules()
{
#if !UE_BUILD_SHIPPING
	NumConstraintInits = 1;
	NumConstraintRules = 0;
	if (XStretchBiasConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XBendingElementConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XEdgeConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XAnisoBendingElementConstraints)
	{
		NumConstraintInits += 1;
	}

	if (bMakeSandwich)
	{
		NumConstraintRules = 2;
	}
	else
	{
		NumConstraintRules = 1;
	}
#endif
}


void FClothConstraints::CreateGSRules()
{
#if !UE_BUILD_SHIPPING
	check(Evolution);
	check(ConstraintInitOffset == INDEX_NONE)
	GetGSNumRules();

	ConstraintInitOffset = Evolution->AddConstraintInitRange(NumConstraintInits, false);

	check(ConstraintRuleOffset == INDEX_NONE)

	ConstraintRuleOffset = Evolution->AddConstraintRuleRange(NumConstraintRules, false);

	check(PostprocessingConstraintRuleOffset == INDEX_NONE)

	int32 PostProcessingConstraintRule = 0;
	if (bWriteFinalResiduals)
	{
		PostProcessingConstraintRule = 1;
	}

	if (PostProcessingConstraintRule)
	{
		PostprocessingConstraintRuleOffset = Evolution->AddConstraintPostprocessingsRange(PostProcessingConstraintRule, false);
	}

	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintInits = Evolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintRules = Evolution->ConstraintRules().GetData() + ConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostprocessingRules = Evolution->ConstraintPostprocessings().GetData() + PostprocessingConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;

	int32 ConstraintPostProcessingIndex = 0;

	GSMasterConstraint = MakeShared<Chaos::Softs::FGaussSeidelMasterConstraint<Softs::FSolverReal, Softs::FSolverParticles>>(Evolution->Particles(), bClothDoQuasistatics, bUseSOR, SOROmega, 100);

	if (bClothDoQuasistatics)
	{
		Evolution->SetQuasistatics(bClothDoQuasistatics);
		const Softs::FSolverVec3& GravityTerm = Evolution->GetGravity(0);
		GSMasterConstraint->ExternalForce[0] = 0.f;
		GSMasterConstraint->ExternalForce[1] = 0.f;
		GSMasterConstraint->ExternalForce[2] = GravityTerm[2];
	}

	ConstraintInits[ConstraintInitIndex++] =
		[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
	{
		this->GSMasterConstraint->Init(Dt, InParticles);
	};

	if (XStretchBiasConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Init();
			XStretchBiasConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XBendingElementConstraints->Init(InParticles);
			this->XBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XAnisoBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XAnisoBendingElementConstraints->Init(InParticles);
			this->XAnisoBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XEdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (bMakeSandwich)
	{
		if (XStretchBiasConstraints)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, PreIterXPBDNums = UpperBreadIters, bbDisplayResidual = bDisplayResidual](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < PreIterXPBDNums; i++)
				{
					XStretchBiasConstraints->Apply(Particles, Dt);
					if (this->XBendingElementConstraints)
					{
						this->XBendingElementConstraints->Apply(Particles, Dt);
					}
					if (bbDisplayResidual)
					{
						TArray<Chaos::Softs::FSolverVec3> Residual = this->GSMasterConstraint->ComputeNewtonResiduals(Particles, Dt);
					}
				}
			};
		}


		//middle layer of sandwich:
		if (bEnableCG)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, GSIterNums = MiddleBreadIters](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < GSIterNums; i++)
				{
					this->GSMasterConstraint->ApplyCG(InParticles, Dt);
				}
			};
		}
		else
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, GSIterNums = MiddleBreadIters](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < GSIterNums; i++)
				{
					this->GSMasterConstraint->Apply(InParticles, Dt, MaxResidualIters, bWriteResidual2File);
				}
			};
		}

	}
	else
	{
		if (bEnableCG)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMasterConstraint->ApplyCG(InParticles, Dt);
			};
		}
		else
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMasterConstraint->Apply(InParticles, Dt, MaxResidualIters, bWriteResidual2File);
			};
		}
	}



	if (XStretchBiasConstraints)
	{
		if (bReplaceBiasElementsWithCorotatedCod)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;

			const TArray<TVec3<int32>> CodimensionalMesh = XStretchBiasConstraints->GetConstraints();

			GSCorotatedCodimensionalConstraint = MakeShared<Chaos::Softs::FGaussSeidelCorotatedCodimensionalConstraints<Softs::FSolverReal, Softs::FSolverParticles>>(Evolution->Particles(), CodimensionalMesh, false, YoungsModulus);

			GSMasterConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			const int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->GSCorotatedCodimensionalConstraint->AddHyperelasticResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}
		else
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMasterConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			const int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XStretchBiasConstraints->AddStretchBiasElementResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};

			if (bEnableCG)
			{
				const int32 ForceDifferentialRange = GSMasterConstraint->AddAddInternalForceDifferentialsRange(1);

				GSMasterConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
				{
					this->XStretchBiasConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
				};
			}

		}

		XStretchBiasConstraints->InitializeDmInvAndMeasures(Evolution->Particles());

	}

	if (XAnisoBendingElementConstraints)
	{
		TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
		GSMasterConstraint->AddStaticConstraints(XAnisoBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
		const int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

		GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
		{
			this->XAnisoBendingElementConstraints->AddAnisotropicBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
		};

		if (bEnableCG)
		{
			const int32 ForceDifferentialRange = GSMasterConstraint->AddAddInternalForceDifferentialsRange(1);

			GSMasterConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
			{
				this->XAnisoBendingElementConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
			};
		}
	}

	if (XBendingElementConstraints)
	{

		TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
		GSMasterConstraint->AddStaticConstraints(XBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
		const int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

		GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
		{
			this->XBendingElementConstraints->AddBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
		};


		if (bEnableCG)
		{
			const int32 ForceDifferentialRange = GSMasterConstraint->AddAddInternalForceDifferentialsRange(1);

			GSMasterConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
			{
				this->XBendingElementConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
			};
		}
	}

	GSMasterConstraint->InitStaticColor(Evolution->Particles());

	if (bWriteFinalResiduals)
	{
		FString file = FPaths::ProjectDir();
		file.Append(TEXT("/DebugOutput/NewtonResidual.txt"));
		FFileHelper::SaveStringToFile(FString(TEXT("Newton Norm\r\n")), *file);
		PostprocessingRules[ConstraintPostProcessingIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->GSMasterConstraint->ComputeNewtonResiduals(InParticles, Dt, true, nullptr);
		};
	}

#endif

}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TMap<FString, const TSet<int32>*>& VertexSets,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale, bool bEnabled)
{
	// Self collisions
	CreateSelfCollisionConstraints(ConfigProperties, VertexSets, TriangleMesh);

	// Edge constraints
	CreateStretchConstraints(ConfigProperties, WeightMaps, TriangleMesh, PatternData);

	// Bending constraints
	CreateBendingConstraints(ConfigProperties, WeightMaps, TriangleMesh, PatternData);

	// Area constraints
	CreateAreaConstraints(ConfigProperties, WeightMaps, TriangleMesh);

	// Long range constraints
	CreateLongRangeConstraints(ConfigProperties, WeightMaps, Tethers, MeshScale);

	// Max distances
	CreateMaxDistanceConstraints(ConfigProperties, WeightMaps, MeshScale);

	// Backstop Constraints
	CreateBackstopConstraints(ConfigProperties, WeightMaps, MeshScale);

	// Animation Drive Constraints
	CreateAnimDriveConstraints(ConfigProperties, WeightMaps);


	// Commit rules to solver
PRAGMA_DISABLE_DEPRECATION_WARNINGS

#if !UE_BUILD_SHIPPING
	if (!bEnableGS)
	{
		CreateRules();  // TODO: Move CreateRules to private
	} 
	else
	{
		CreateGSRules();
	}
#else
	CreateRules();
#endif

PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Enable or disable constraints as requested
	Enable(bEnabled);
}

void FClothConstraints::CreateSelfCollisionConstraints(const Softs::FCollectionPropertyConstFacade& ConfigProperties, 
	const TMap<FString, const TSet<int32>*>& VertexSets, const FTriangleMesh& TriangleMesh)
{
	const bool bUseSelfCollisions = ConfigProperties.GetValue<bool>(TEXT("UseSelfCollisions"));

	if (bUseSelfCollisions)
	{
		static const int32 DisabledCollisionElementsN = 5;  // TODO: Make this a parameter?
		TSet<TVec2<int32>> DisabledCollisionElements;  // TODO: Is this needed? Turn this into a bit array?

		const TVec2<int32> Range = TriangleMesh.GetVertexRange();
		for (int32 Index = Range[0]; Index <= Range[1]; ++Index)
		{
			const TSet<int32> Neighbors = TriangleMesh.GetNRing(Index, DisabledCollisionElementsN);
			for (int32 Element : Neighbors)
			{
				check(Index != Element);
				DisabledCollisionElements.Emplace(TVec2<int32>(Index, Element));
				DisabledCollisionElements.Emplace(TVec2<int32>(Element, Index));
			}
		}

		SelfCollisionInit = MakeShared<Softs::FPBDTriangleMeshCollisions>(
			ParticleOffset,
			NumParticles,
			TriangleMesh,
			ConfigProperties);

		SelfCollisionConstraints = MakeShared<Softs::FPBDCollisionSpringConstraints>(
			ParticleOffset,
			NumParticles,
			TriangleMesh,
			AnimationPositions,
			MoveTemp(DisabledCollisionElements),
			ConfigProperties);

		++NumConstraintInits;
		++NumPostCollisionConstraintRules;

		SelfIntersectionConstraints = MakeShared<Softs::FPBDTriangleMeshIntersections>(
			ParticleOffset,
			NumParticles,
			TriangleMesh);
		++NumConstraintInits;
		++NumPostprocessingConstraintRules;
	}
	else if (Softs::FPBDSelfCollisionSphereConstraints::IsEnabled(ConfigProperties))
	{
		SelfCollisionSphereConstraints = MakeShared< Softs::FPBDSelfCollisionSphereConstraints>(
			ParticleOffset,
			NumParticles,
			VertexSets,
			ConfigProperties);
		++NumConstraintInits;
		++NumPostCollisionConstraintRules;
	}
}

void FClothConstraints::CreateStretchConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData)
{
	if (PatternData && PatternData->PatternPositions.Num() && Softs::FXPBDStretchBiasElementConstraints::IsEnabled(ConfigProperties))
	{
		XStretchBiasConstraints = MakeShared<Softs::FXPBDStretchBiasElementConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			TriangleMesh,
			PatternData->WeldedFaceVertexPatternPositions,
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDEdgeSpringConstraints::IsEnabled(ConfigProperties))
	{
		XEdgeConstraints = MakeShared<Softs::FXPBDEdgeSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			TriangleMesh.GetSurfaceElements(),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FPBDEdgeSpringConstraints::IsEnabled(ConfigProperties))
	{
		EdgeConstraints = MakeShared<Softs::FPBDEdgeSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			TriangleMesh.GetSurfaceElements(),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateBendingConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData)
{
	if (PatternData && PatternData->PatternPositions.Num() && Softs::FXPBDAnisotropicBendingConstraints::IsEnabled(ConfigProperties))
	{
		XAnisoBendingElementConstraints = MakeShared<Softs::FXPBDAnisotropicBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			TriangleMesh,
			PatternData->WeldedFaceVertexPatternPositions,
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDBendingConstraints::IsEnabled(ConfigProperties))
	{
		TArray<Chaos::TVec4<int32>> BendingElements = TriangleMesh.GetUniqueAdjacentElements();

		XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FPBDBendingConstraints::IsEnabled(ConfigProperties))
	{
		TArray<Chaos::TVec4<int32>> BendingElements = TriangleMesh.GetUniqueAdjacentElements();

		BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDBendingSpringConstraints::IsEnabled(ConfigProperties))
	{
		const TArray<Chaos::TVec2<int32>> CrossEdges = TriangleMesh.GetUniqueAdjacentPoints();

		XBendingConstraints = MakeShared<Softs::FXPBDBendingSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			CrossEdges,
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FPBDBendingSpringConstraints::IsEnabled(ConfigProperties))
	{
		const TArray<Chaos::TVec2<int32>> CrossEdges = TriangleMesh.GetUniqueAdjacentPoints();

		BendingConstraints = MakeShared<Softs::FPBDBendingSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			CrossEdges,
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateAreaConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh)
{
	if (Softs::FXPBDAreaSpringConstraints::IsEnabled(ConfigProperties))
	{
		XAreaConstraints = MakeShared<Softs::FXPBDAreaSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			TriangleMesh.GetSurfaceElements(),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FPBDAreaSpringConstraints::IsEnabled(ConfigProperties))
	{
		AreaConstraints = MakeShared<Softs::FPBDAreaSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			TriangleMesh.GetSurfaceElements(),
			WeightMaps,
			ConfigProperties,
			/*bTrimKinematicConstraints =*/ true);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateLongRangeConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale)
{
	if (Softs::FPBDLongRangeConstraints::IsEnabled(ConfigProperties))
	{
		//  Now that we're only doing a single iteration of Long range constraints, and they're more of a fake constraint to jump start our initial guess, it's not clear that using XPBD makes sense here.
		LongRangeConstraints = MakeShared<Softs::FPBDLongRangeConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			Tethers,
			WeightMaps,
			ConfigProperties,
			MeshScale);

		++NumConstraintInits;  // Uses init to both update the property tables and apply the constraint
	}
}

void FClothConstraints::CreateMaxDistanceConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale)
{
	if (ConfigProperties.GetValue("UseLegacyConfig", false))
	{
		FString MaxDistanceString = TEXT("MaxDistance");
		MaxDistanceString = ConfigProperties.GetStringValue(MaxDistanceString, MaxDistanceString);  // Uses the same string for both the default weight map name and the property name
		const TConstArrayView<FRealSingle> MaxDistances = WeightMaps.FindRef(MaxDistanceString);
		if (MaxDistances.Num() != NumParticles)
		{
			return;  // Legacy configs disable the constraint when the weight map is missing
		}
	}

	if (Softs::FPBDSphericalConstraint::IsEnabled(ConfigProperties))
	{
		MaximumDistanceConstraints = MakeShared<Softs::FPBDSphericalConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			WeightMaps,
			ConfigProperties,
			MeshScale);

		++NumConstraintRules;
	}
}

void FClothConstraints::CreateBackstopConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale)
{
	if (ConfigProperties.GetValue("UseLegacyConfig", false))
	{
		FString BackstopRadiusString = TEXT("BackstopRadius");
		FString BackstopDistanceString = TEXT("BackstopDistance");
		BackstopRadiusString = ConfigProperties.GetStringValue(BackstopRadiusString, BackstopRadiusString);        // Uses the same string for both the default weight map name and the property name
		BackstopDistanceString = ConfigProperties.GetStringValue(BackstopDistanceString, BackstopDistanceString);  //
		const TConstArrayView<FRealSingle> BackstopRadiuses = WeightMaps.FindRef(BackstopRadiusString);
		const TConstArrayView<FRealSingle> BackstopDistances = WeightMaps.FindRef(BackstopDistanceString);

		if (BackstopRadiuses.Num() != NumParticles || BackstopDistances.Num() != NumParticles)
		{
			return;  // Legacy configs disable the constraint when the weight maps are missing
		}
	}

	if (Softs::FPBDSphericalBackstopConstraint::IsEnabled(ConfigProperties))
	{
		BackstopConstraints = MakeShared<Softs::FPBDSphericalBackstopConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			*AnimationNormals,
			WeightMaps,
			ConfigProperties,
			MeshScale);

		++NumConstraintRules;
	}
}

void FClothConstraints::CreateAnimDriveConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (Softs::FPBDAnimDriveConstraint::IsEnabled(ConfigProperties))
	{
		check(AnimationVelocities); // Legacy code didn't use to have AnimationVelocities

		AnimDriveConstraints = MakeShared<Softs::FPBDAnimDriveConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			*AnimationVelocities,
			WeightMaps,
			ConfigProperties);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateRules()
{
	check(Evolution);
	check(ConstraintInitOffset == INDEX_NONE)
#if !UE_BUILD_SHIPPING	
	if (bDisplayResidual)
	{
		NumConstraintRules++;
	}
	if (bWriteFinalResiduals || bDisplayResidual)
	{
		NumConstraintInits++;
	}
#endif
	if (NumConstraintInits)
	{
		ConstraintInitOffset = Evolution->AddConstraintInitRange(NumConstraintInits, false);
	}
	check(ConstraintRuleOffset == INDEX_NONE)

	if (NumConstraintRules)
	{
		ConstraintRuleOffset = Evolution->AddConstraintRuleRange(NumConstraintRules, false);
	}
	check(PostCollisionConstraintRuleOffset == INDEX_NONE);
	if (NumPostCollisionConstraintRules)
	{
		PostCollisionConstraintRuleOffset = Evolution->AddPostCollisionConstraintRuleRange(NumPostCollisionConstraintRules, false);
	}
	check(PostprocessingConstraintRuleOffset == INDEX_NONE);
#if !UE_BUILD_SHIPPING	
	if (bWriteFinalResiduals)
	{
		NumPostprocessingConstraintRules += 1;
	}
#endif
	if (NumPostprocessingConstraintRules)
	{
		PostprocessingConstraintRuleOffset = Evolution->AddConstraintPostprocessingsRange(NumPostprocessingConstraintRules, false);
	}

	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintInits = Evolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintRules = Evolution->ConstraintRules().GetData() + ConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostCollisionConstraintRules = Evolution->PostCollisionConstraintRules().GetData() + PostCollisionConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostprocessingConstraintRules = Evolution->ConstraintPostprocessings().GetData() + PostprocessingConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;
	int32 PostCollisionConstraintRuleIndex = 0;
	int32 PostprocessingConstraintRuleIndex = 0;
	#if !UE_BUILD_SHIPPING	
	if (bDisplayResidual || bWriteFinalResiduals)
	{
		GSMasterConstraint = MakeShared<Chaos::Softs::FGaussSeidelMasterConstraint<Softs::FSolverReal, Softs::FSolverParticles>>(Evolution->Particles(), false, false, (Softs::FSolverReal)1.2, 100);

		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->GSMasterConstraint->Init(Dt, InParticles);
		};
		if (bDisplayResidual)
		{

			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				if (this->GSMasterConstraint->DebugResidual && this->GSMasterConstraint->PassedIters < MaxResidualIters)
				{
					UE_LOG(LogTemp, Warning, TEXT("Current Iteration As From Evolution: %d"), this->Evolution->GetIterations());
					this->GSMasterConstraint->ComputeNewtonResiduals(InParticles, Dt, bWriteResidual2File);
					this->GSMasterConstraint->PassedIters += 1;
				}
			};
		}

		if (XStretchBiasConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMasterConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XStretchBiasConstraints->AddStretchBiasElementResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}

		if (XAnisoBendingElementConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMasterConstraint->AddStaticConstraints(XAnisoBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XAnisoBendingElementConstraints->AddAnisotropicBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}


		if (XBendingElementConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMasterConstraint->AddStaticConstraints(XBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMasterConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMasterConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XBendingElementConstraints->AddBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}

		GSMasterConstraint->InitStaticColor(Evolution->Particles());

		if (bWriteFinalResiduals)
		{
			FString file = FPaths::ProjectDir();
			file.Append(TEXT("/DebugOutput/NewtonResidual.txt"));
			FFileHelper::SaveStringToFile(FString(TEXT("Newton Norm\r\n")), *file);
			PostprocessingConstraintRules[PostprocessingConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMasterConstraint->ComputeNewtonResiduals(InParticles, Dt, true, nullptr);
			};
		}
	}
#endif

	if (XStretchBiasConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Init();
			XStretchBiasConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Apply(Particles, Dt);
		};
	}
	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XEdgeConstraints->Init();
				XEdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] = 
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XEdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (XEdgeConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XEdgeConstraints_Deprecated->Init();
			XEdgeConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XEdgeConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (EdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				EdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				EdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (EdgeConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			EdgeConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			EdgeConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (XBendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XBendingConstraints->Init();
				XBendingConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XBendingConstraints->Apply(Particles, Dt);
			};
	}
	if (XBendingConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XBendingConstraints_Deprecated->Init();
			XBendingConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (BendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				BendingConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingConstraints->Apply(Particles, Dt);
			};
	}
	if (BendingConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			BendingConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			BendingConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (BendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingElementConstraints->Init(Particles);
				BendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingElementConstraints->Apply(Particles, Dt);
			};
	}
	if (XBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingElementConstraints->Init(Particles);
			XBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingElementConstraints->Apply(Particles, Dt);
		};
	}
	if (XAnisoBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoBendingElementConstraints->Init(Particles);
			XAnisoBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoBendingElementConstraints->Apply(Particles, Dt);
		};
	}
	if (XAreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XAreaConstraints->Init();
				XAreaConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XAreaConstraints->Apply(Particles, Dt);
			};
	}
	if (XAreaConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XAreaConstraints_Deprecated->Init();
			XAreaConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAreaConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (AreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				AreaConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AreaConstraints->Apply(Particles, Dt);
			};
	}
	if (AreaConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			AreaConstraints_Deprecated->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			AreaConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (ThinShellVolumeConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			ThinShellVolumeConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (VolumeConstraints_Deprecated)  // TODO: Remove for 5.4
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			VolumeConstraints_Deprecated->Apply(Particles, Dt);
		};
	}
	if (MaximumDistanceConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				MaximumDistanceConstraints->Apply(Particles, Dt);
			};
	}
	if (BackstopConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BackstopConstraints->Apply(Particles, Dt);
			};
	}
	if (AnimDriveConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				AnimDriveConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AnimDriveConstraints->Apply(Particles, Dt);
			};
	}
	if (ShapeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				ShapeConstraints->Apply(Particles, Dt);
			};
	}

	if (SelfCollisionInit && SelfCollisionConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal /*Dt*/)
			{
				// Thickness * 2 to account for collision radius for both particles
				SelfCollisionInit->Init(Particles, SelfCollisionConstraints->GetThickness() * (Softs::FSolverReal)2.f);
				SelfCollisionConstraints->Init(Particles, SelfCollisionInit->GetSpatialHash(), SelfCollisionInit->GetVertexGIAColors(), SelfCollisionInit->GetTriangleGIAColors());
			};

		PostCollisionConstraintRules[PostCollisionConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfCollisionConstraints->Apply(Particles, Dt);
			};

	}

	if (SelfCollisionSphereConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal /*Dt*/)
			{
				SelfCollisionSphereConstraints->Init(Particles);
			};

		PostCollisionConstraintRules[PostCollisionConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfCollisionSphereConstraints->Apply(Particles, Dt);
			};
	}

	// The following constraints only run once per subframe, so we do their Apply as part of the Init() which modifies P
	// To avoid possible dependency order issues, add them last
	if (SelfCollisionInit && SelfIntersectionConstraints)
	{
		// The following constraints only run once per subframe, so we do their Apply as part of the Init() which modifies P
		// To avoid possible dependency order issues, add them last
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetContourMinimizationIntersections(), Dt);
			};

		PostprocessingConstraintRules[PostprocessingConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			const int32 NumContourIterations = SelfCollisionInit->GetNumContourMinimizationPostSteps();
			for (int32 Iter = 0; Iter < NumContourIterations; ++Iter)
			{
				SelfCollisionInit->PostStepInit(Particles);
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetPostStepContourMinimizationIntersections(), Dt);
			}
		};
	}

	// Long range constraints modify particle P as part of Init. To avoid possible dependency order issues,
	// add them last
	if (LongRangeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{	
				// Only doing one iteration.
				constexpr int32 NumLRAIterations = 1;
				LongRangeConstraints->ApplyProperties(Dt, NumLRAIterations);
				LongRangeConstraints->Apply(Particles, Dt);  // Run the LRA constraint only once per timestep
			};
	}
	check(ConstraintInitIndex == NumConstraintInits);
	check(ConstraintRuleIndex == NumConstraintRules);
	check(PostCollisionConstraintRuleIndex == NumPostCollisionConstraintRules);
}

void FClothConstraints::SetEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		const TConstArrayView<FRealSingle> DampingMultipliers;

		XEdgeConstraints_Deprecated = MakeShared<Softs::FXPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			DampingMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		EdgeConstraints_Deprecated = MakeShared<Softs::FPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetXPBDEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& DampingRatioMultipliers)
{
	check(Evolution);
	XEdgeConstraints_Deprecated = MakeShared<Softs::FXPBDSpringConstraints>(
		Evolution->Particles(),
		ParticleOffset, NumParticles,
		SurfaceElements,
		StiffnessMultipliers,
		DampingRatioMultipliers,
		/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
		/*InDampingRatio =*/ Softs::FSolverVec2::ZeroVector,
		/*bTrimKinematicConstraints =*/ true);
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(const TArray<TVec2<int32>>& Edges, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		const TConstArrayView<FRealSingle> DampingMultipliers;

		XBendingConstraints_Deprecated = MakeShared<Softs::FXPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			Edges,
			StiffnessMultipliers,
			DampingMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		BendingConstraints_Deprecated = MakeShared<Softs::FPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			Edges,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		const TConstArrayView<FRealSingle> DampingMultipliers;

		XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			DampingMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
			/*InBucklingStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*InDampingRatio =*/ Softs::FSolverVec2::ZeroVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
			/*InBucklingStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);

	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetXPBDBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, const TConstArrayView<FRealSingle>& DampingRatioMultipliers)
{
	check(Evolution);
	XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
		Evolution->Particles(),
		ParticleOffset, NumParticles,
		MoveTemp(BendingElements),
		StiffnessMultipliers,
		BucklingStiffnessMultipliers,
		DampingRatioMultipliers,
		/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
		/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
		/*InBucklingStiffness =*/ Softs::FSolverVec2::UnitVector,
		/*InDampingRatio =*/ Softs::FSolverVec2::ZeroVector,
		/*bTrimKinematicConstraints =*/ true);
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, Softs::FSolverReal BendingStiffness)
{
	// Deprecated 5.1
	check(Evolution);

	BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
		Evolution->Particles(),
		ParticleOffset,
		NumParticles,
		MoveTemp(BendingElements),
		/*StiffnessMultipliers =*/ TConstArrayView<FRealSingle>(),
		/*BucklingStiffnessMultipliers =*/ TConstArrayView<FRealSingle>(),
		/*InStiffness =*/ BendingStiffness,
		/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
		/*InBucklingStiffness =*/ Softs::FSolverVec2::UnitVector,
		/*bTrimKinematicConstraints =*/ true);

	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetAreaConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XAreaConstraints_Deprecated = MakeShared<Softs::FXPBDAxialSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		AreaConstraints_Deprecated = MakeShared<Softs::FPBDAxialSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(const TArray<TVec2<int32>>& DoubleBendingEdges, Softs::FSolverReal VolumeStiffness)
{
	check(Evolution);

	ThinShellVolumeConstraints_Deprecated = MakeShared<Softs::FPBDSpringConstraints>(
		Evolution->Particles(),
		ParticleOffset,
		NumParticles,
		DoubleBendingEdges,
		TConstArrayView<FRealSingle>(),
		VolumeStiffness,
		/*bTrimKinematicConstraints =*/ true);
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(TArray<TVec3<int32>>&& SurfaceElements, Softs::FSolverReal VolumeStiffness)
{
	check(Evolution);
	check(VolumeStiffness > 0.f && VolumeStiffness <= 1.f);

	VolumeConstraints_Deprecated = MakeShared<Softs::FPBDVolumeConstraint>(Evolution->Particles(), MoveTemp(SurfaceElements), VolumeStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetLongRangeConstraints(
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers,
	const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
	const Softs::FSolverVec2& TetherScale,
	Softs::FSolverReal MeshScale)
{
	check(Evolution);
	//  Now that we're only doing a single iteration of Long range constraints, and they're more of a fake constraint to jump start our initial guess, it's not clear that using XPBD makes sense here.
	LongRangeConstraints = MakeShared<Softs::FPBDLongRangeConstraints>(
		Evolution->Particles(),
		ParticleOffset,
		NumParticles,
		Tethers,
		TetherStiffnessMultipliers,
		TetherScaleMultipliers,
		/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
		TetherScale,
		MeshScale);
	++NumConstraintInits;  // Uses init to both update the property tables and apply the constraint
}

// Deprecated in 5.1
void FClothConstraints::SetLongRangeConstraints(
	const TArray<TConstArrayView<TTuple<int32,
	int32, FRealSingle>>>& Tethers,
	const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers,
	const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
	const Softs::FSolverVec2& TetherScale,
	bool bUseXPBDConstraints,
	Softs::FSolverReal MeshScale)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return SetLongRangeConstraints(Tethers, TetherStiffnessMultipliers, TetherScaleMultipliers, TetherScale, MeshScale);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FClothConstraints::SetMaximumDistanceConstraints(const TConstArrayView<FRealSingle>& MaxDistances)
{
	MaximumDistanceConstraints = MakeShared<Softs::FPBDSphericalConstraint>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		MaxDistances);
	++NumConstraintRules;
}

void FClothConstraints::SetBackstopConstraints(const TConstArrayView<FRealSingle>& BackstopDistances, const TConstArrayView<FRealSingle>& BackstopRadiuses, bool bUseLegacyBackstop)
{
	BackstopConstraints = MakeShared<Softs::FPBDSphericalBackstopConstraint>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationNormals,
		BackstopRadiuses,
		BackstopDistances,
		bUseLegacyBackstop);
	++NumConstraintRules;
}

void FClothConstraints::SetAnimDriveConstraints(const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers, const TConstArrayView<FRealSingle>& AnimDriveDampingMultipliers)
{
	if (AnimationVelocities)
	{
		AnimDriveConstraints = MakeShared<Softs::FPBDAnimDriveConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			TArray<Softs::FSolverVec3>(),
			*AnimationVelocities,
			AnimDriveStiffnessMultipliers,
			AnimDriveDampingMultipliers);
	}

	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetShapeTargetConstraints(Softs::FSolverReal ShapeTargetStiffness)
{
	// TODO: Review this constraint. Currently does nothing more than the anim drive with less controls
	check(ShapeTargetStiffness > 0.f && ShapeTargetStiffness <= 1.f);

	ShapeConstraints = MakeShared<Softs::FPBDShapeConstraints>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationPositions,
		ShapeTargetStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetSelfCollisionConstraints(const FTriangleMesh& TriangleMesh, TSet<TVec2<int32>>&& DisabledCollisionElements, Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFrictionCoefficient, bool bGlobalIntersectionAnalysis, bool bContourMinimization)
{
	SelfCollisionInit = MakeShared<Softs::FPBDTriangleMeshCollisions>(
		ParticleOffset,
		NumParticles,
		TriangleMesh,
		bGlobalIntersectionAnalysis,
		bContourMinimization);

	SelfCollisionConstraints = MakeShared<Softs::FPBDCollisionSpringConstraints>(
		ParticleOffset,
		NumParticles,
		TriangleMesh,
		AnimationPositions,
		MoveTemp(DisabledCollisionElements),
		SelfCollisionThickness,
		Softs::FPBDCollisionSpringConstraintsBase::BackCompatStiffness,
		SelfCollisionFrictionCoefficient);

	++NumConstraintInits;
	++NumPostCollisionConstraintRules;

	SelfIntersectionConstraints = MakeShared<Softs::FPBDTriangleMeshIntersections>(
		ParticleOffset,
		NumParticles,
		TriangleMesh);
	++NumConstraintInits;
}

void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TMap<FString, const TSet<int32>*>& VertexSets,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	if (EdgeConstraints)
	{
		EdgeConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XEdgeConstraints)
	{
		XEdgeConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XStretchBiasConstraints)
	{
		XStretchBiasConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (BendingConstraints)
	{
		BendingConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XBendingConstraints)
	{
		XBendingConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (BendingElementConstraints)
	{
		BendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XBendingElementConstraints)
	{
		XBendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XAnisoBendingElementConstraints)
	{
		XAnisoBendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (AreaConstraints)
	{
		AreaConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XAreaConstraints)
	{
		XAreaConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (LongRangeConstraints)
	{
		LongRangeConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale);
	}
	if (MaximumDistanceConstraints)
	{
		MaximumDistanceConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale * MaxDistancesScale);
	}
	if (BackstopConstraints)
	{
		BackstopConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale);
	}
	if (AnimDriveConstraints)
	{
		AnimDriveConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (SelfCollisionConstraints)
	{
		SelfCollisionConstraints->SetProperties(ConfigProperties);
	}
	if (SelfCollisionInit)
	{
		SelfCollisionInit->SetProperties(ConfigProperties);
	}
	if (SelfCollisionSphereConstraints)
	{
		SelfCollisionSphereConstraints->SetProperties(ConfigProperties, VertexSets);
	}
}

// Deprecated
void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	Update(ConfigProperties, WeightMaps, TMap<FString, const TSet<int32>*>(), MeshScale, MaxDistancesScale);
}

// Deprecated
void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	Update(ConfigProperties, TMap<FString, TConstArrayView<FRealSingle>>(), TMap<FString, const TSet<int32>*>(), MeshScale, MaxDistancesScale);
}

void FClothConstraints::SetEdgeProperties(const Softs::FSolverVec2& EdgeStiffness, const Softs::FSolverVec2& DampingRatio)
{
	if (EdgeConstraints)
	{
		static_cast<Softs::FPBDSpringConstraints*>(EdgeConstraints.Get())->SetProperties(EdgeStiffness);
	}
	if (XEdgeConstraints)
	{
		static_cast<Softs::FXPBDSpringConstraints*>(XEdgeConstraints.Get())->SetProperties(EdgeStiffness, DampingRatio);
	}
}

void FClothConstraints::SetBendingProperties(const Softs::FSolverVec2& BendingStiffness, Softs::FSolverReal BucklingRatio, const Softs::FSolverVec2& BucklingStiffness, const Softs::FSolverVec2& BendingDampingRatio)
{
	if (BendingConstraints)
	{
		static_cast<Softs::FPBDSpringConstraints*>(BendingConstraints.Get())->SetProperties(BendingStiffness);
	}
	if (XBendingConstraints)
	{
		static_cast<Softs::FXPBDSpringConstraints*>(XBendingConstraints.Get())->SetProperties(BendingStiffness);
	}
	if (BendingElementConstraints)
	{
		BendingElementConstraints->SetProperties(BendingStiffness, BucklingRatio, BucklingStiffness);
	}
	if (XBendingElementConstraints)
	{
		XBendingElementConstraints->SetProperties(BendingStiffness, BucklingRatio, BucklingStiffness, BendingDampingRatio);
	}
}

void FClothConstraints::SetAreaProperties(const Softs::FSolverVec2& AreaStiffness)
{
	if (AreaConstraints)
	{
		static_cast<Softs::FPBDAxialSpringConstraints*>(AreaConstraints.Get())->SetProperties(AreaStiffness);
	}
	if (XAreaConstraints)
	{
		static_cast<Softs::FXPBDAxialSpringConstraints*>(XAreaConstraints.Get())->SetProperties(AreaStiffness);
	}
}

void FClothConstraints::SetThinShellVolumeProperties(Softs::FSolverReal VolumeStiffness)
{
	if (ThinShellVolumeConstraints_Deprecated)
	{
		ThinShellVolumeConstraints_Deprecated->SetProperties(VolumeStiffness);
	}
}

void FClothConstraints::SetVolumeProperties(Softs::FSolverReal VolumeStiffness)
{
	if (VolumeConstraints_Deprecated)
	{
		VolumeConstraints_Deprecated->SetStiffness(VolumeStiffness);
	}
}

void FClothConstraints::SetLongRangeAttachmentProperties(
	const Softs::FSolverVec2& TetherStiffness,
	const Softs::FSolverVec2& TetherScale,
	Softs::FSolverReal MeshScale)
{
	if (LongRangeConstraints)
	{
		LongRangeConstraints->SetProperties(TetherStiffness, TetherScale, MeshScale);
	}
}

void FClothConstraints::SetMaximumDistanceProperties(Softs::FSolverReal MeshScale)
{
	if (MaximumDistanceConstraints)
	{
		MaximumDistanceConstraints->SetScale(MeshScale);
	}
}

void FClothConstraints::SetAnimDriveProperties(const Softs::FSolverVec2& AnimDriveStiffness, const Softs::FSolverVec2& AnimDriveDamping)
{
	if (AnimDriveConstraints)
	{
		AnimDriveConstraints->SetProperties(AnimDriveStiffness, AnimDriveDamping);
	}
}

void FClothConstraints::SetSelfCollisionProperties(Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFrictionCoefficient, bool bGlobalIntersectionAnalysis, bool bContourMinimization)
{
	if (SelfCollisionInit)
	{
		SelfCollisionInit->SetGlobalIntersectionAnalysis(bGlobalIntersectionAnalysis);
		SelfCollisionInit->SetContourMinimization(bContourMinimization);
	}
	if (SelfCollisionConstraints)
	{
		SelfCollisionConstraints->SetThickness(SelfCollisionThickness);
		SelfCollisionConstraints->SetFrictionCoefficient(SelfCollisionFrictionCoefficient);
	}
}

void FClothConstraints::SetBackstopProperties(bool bEnabled, Softs::FSolverReal MeshScale)
{
	if (BackstopConstraints)
	{
		BackstopConstraints->SetEnabled(bEnabled);
		BackstopConstraints->SetScale(MeshScale);
	}
}

}  // End namespace Chaos
