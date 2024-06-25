// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDeformablePhysicsComponent.h"
#include "Chaos/Deformable/ChaosDeformableSolver.h"
#include "Chaos/Deformable/ChaosDeformableSolverProxy.h"
#include "Chaos/Deformable/ChaosDeformableSolverTypes.h"
#include "ChaosFlesh/ChaosDeformableSolverThreading.h"
#include "ChaosFlesh/ChaosDeformableTypes.h"
#include "DeformableInterface.h"
#include "Components/SceneComponent.h"
#include "Interfaces/DataflowPhysicsSolver.h"
#include "UObject/ObjectMacros.h"

#include "ChaosDeformableSolverComponent.generated.h"

class UDeformablePhysicsComponent;
class UDeformableCollisionsComponent;

USTRUCT(BlueprintType)
struct FConnectedObjectsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConnectedObjects", meta = (EditCondition = "false"))
	TArray< TObjectPtr<UDeformablePhysicsComponent> > DeformableComponents;
};

USTRUCT(BlueprintType)
struct FSolverTimingGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		int32 NumSubSteps = 2;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		int32 NumSolverIterations = 5;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		bool FixTimeStep = false;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		float TimeStepSize = 0.05;

	UPROPERTY(EditAnywhere, Category = "SolverTiming")
		bool bDoThreadedAdvance = true;

	/** ObjectType defines how to initialize the rigid objects state, Kinematic, Sleeping, Dynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SolverTiming")
		EDeformableExecutionModel ExecutionModel = EDeformableExecutionModel::Chaos_Deformable_PostPhysics;
};


USTRUCT(BlueprintType)
struct FSolverDebuggingGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Physics")
	bool CacheToFile = false;
};


USTRUCT(BlueprintType)
struct FSolverQuasistaticsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Quasistatics")
	bool bDoQuasistatics = false;
};


USTRUCT(BlueprintType)
struct FSolverEvolutionGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Evolution")
	FSolverQuasistaticsGroup SolverQuasistatics;

};


USTRUCT(BlueprintType)
struct FSolverGridBasedCollisionsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "GridBasedCollisions")
	bool bUseGridBasedConstraints = false;

	UPROPERTY(EditAnywhere, Category = "GridBasedCollisions")
	float GridDx = 25.;
};

USTRUCT(BlueprintType)
struct FInComponentSpringCollisionGroup
{
	GENERATED_USTRUCT_BODY()
	/**
	* If uses in-component spring self-collision
	*/
	UPROPERTY(EditAnywhere, Category = "InComponentSpringCollision")
	bool bDoInComponentSpringCollision = false;
	/**
	* N ring to exclude for in-component spring self-collision
	*/
	UPROPERTY(EditAnywhere, Category = "InComponentSpringCollision")
	int32 NRingExcluded = 1;
};

USTRUCT(BlueprintType)
struct FSpringCollisionGroup
{
	GENERATED_USTRUCT_BODY()
	/**
	* If uses component-component spring collision
	*/
	UPROPERTY(EditAnywhere, Category = "SpringCollision")
	bool bDoSpringCollision = false;
	/**
	* In-component spring self collision detection parameters
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpringCollision")
	FInComponentSpringCollisionGroup InComponentSpringCollision;
	/**
	* Search radius for point triangle collision pairs
	*/
	UPROPERTY(EditAnywhere, Category = "SpringCollision")
	float CollisionSearchRadius = 0.f;
	/**
	* Collision spring stiffness; larger value will stop penetration better
	*/
	UPROPERTY(EditAnywhere, Category = "SpringCollision")
	float SpringCollisionStiffness = 500.f;
	/**
	* Anisotropic springs will allow sliding on the triangle
	*/
	UPROPERTY(EditAnywhere, Category = "SpringCollision")
	bool bAllowSliding = true;
	/**
	* Do self collision with kinematic triangles as well
	*/
	UPROPERTY(EditAnywhere, Category = "SpringCollision")
	bool bCollideWithFullmesh = true;
};

USTRUCT(BlueprintType)
struct FSphereRepulsionGroup
{
	GENERATED_USTRUCT_BODY()
	/**
	* If uses sphere repulsion for collision
	*/
	UPROPERTY(EditAnywhere, Category = "SphereRepulsion")
	bool bDoSphereRepulsion = false;
	/**
	* Search radius for repulsion pairs
	*/
	UPROPERTY(EditAnywhere, Category = "SphereRepulsion")
	float SphereRepulsionRadius = 0.f;
	/**
	* Stiffness for sphere repulsion
	*/
	UPROPERTY(EditAnywhere, Category = "SphereRepulsion")
	float SphereRepulsionStiffness = 500.f;
};

USTRUCT(BlueprintType)
struct FSolverGaussSeidelConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	/**
	* Enable the Gauss Seidel solver instead of the existing XPBD.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseGaussSeidelConstraints = false;

	/**
	* Enable another model that runs simulation faster.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseGSNeohookean = false;

	/**
	* Enable acceleration technique for Gauss Seidel solver to make simulation look better within a limited budget.
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bUseSOR = true;

	/**
	* Acceleration related parameter. Tune it down if simulation becomes unstable. 
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	float OmegaSOR = 1.6f;

	/**
	* Enable dynamic springs controlled by constraint manager. 
	*/
	UPROPERTY(EditAnywhere, Category = "GaussSeidelConstraints")
	bool bEnableDynamicSprings = true;
	
	/**
	* Component-component collision detection radius and stiffness
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GaussSeidelConstraints")
	FSpringCollisionGroup SpringCollision;

	/**
	* Sphere repulsion parameters
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GaussSeidelConstraints")
	FSphereRepulsionGroup SphereRepulsion;
};

USTRUCT(BlueprintType)
struct FSolverCollisionsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Physics")
	bool bUseFloor = true;

	//UPROPERTY(EditAnywhere, Category = "Collisions")
	//FSolverGridBasedCollisionsGroup SolverGridBasedCollisions;
};

USTRUCT(BlueprintType)
struct FSolverCorotatedConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Corotated")
	bool bEnableCorotatedConstraint = true;

	UPROPERTY(EditAnywhere, Category = "Corotated")
	bool bDoBlended = false;

	UPROPERTY(EditAnywhere, Category = "Corotated")
	float BlendedZeta = 0;
};

USTRUCT(BlueprintType)
struct FSolverConstraintsGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Constraints")
	bool bEnablePositionTargets = true;

	UPROPERTY(EditAnywhere, Category = "Constraints")
	bool bEnableKinematics = true;

	UPROPERTY(EditAnywhere, Category = "Constraints")
	FSolverCorotatedConstraintsGroup CorotatedConstraints;

	/**
	* These are options for another solver. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constraints")
	FSolverGaussSeidelConstraintsGroup GaussSeidelConstraints;
};

USTRUCT(BlueprintType)
struct FSolverForcesGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Forces")
	float YoungModulus = 100000;

	UPROPERTY(EditAnywhere, Category = "Forces")
	float Damping = 0;

	UPROPERTY(EditAnywhere, Category = "Forces")
	bool bEnableGravity = true;
};

USTRUCT(BlueprintType)
struct FSolverMuscleActivationGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "MuscleActivation")
	bool bDoMuscleActivation = false;
};

USTRUCT(BlueprintType)
struct FDataflowFleshSolverProxy : public FDataflowPhysicsSolverProxy
{
	GENERATED_USTRUCT_BODY()

	FDataflowFleshSolverProxy(Chaos::Softs::FDeformableSolverProperties InProp = Chaos::Softs::FDeformableSolverProperties()) :
		FDataflowPhysicsSolverProxy()
	{
		Solver = MakeUnique<Chaos::Softs::FDeformableSolver>(InProp);
	}
	virtual ~FDataflowFleshSolverProxy() override = default;

	// Begin FPhysicsSolverInterface overrides
	virtual void AdvanceSolverDatas(const float DeltaTime) override
	{
		Chaos::Softs::FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver.Get(), Chaos::Softs::FPhysicsThreadAccessor());
		PhysicsThreadAccess.Simulate(DeltaTime);
	}
	virtual float GetTimeStep() override
	{
		const Chaos::Softs::FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver.Get(), Chaos::Softs::FPhysicsThreadAccessor());
		return PhysicsThreadAccess.GetProperties().TimeStepSize;
	}
	virtual bool IsValid() const override { return Solver.IsValid();}
	// End FPhysicsSolverInterface overrides

	/** Chaos deformable solver that will be used in the component */
	TUniquePtr<Chaos::Softs::FDeformableSolver> Solver;
};

template <>
struct TStructOpsTypeTraits<FDataflowFleshSolverProxy> : public TStructOpsTypeTraitsBase2<FDataflowFleshSolverProxy>
{
	enum { WithCopy = false };
};

/**
*	UDeformableSolverComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class CHAOSFLESHENGINE_API UDeformableSolverComponent : public USceneComponent, public IDeformableInterface, public IDataflowPhysicsSolverInterface
{
	GENERATED_UCLASS_BODY()

public:
	typedef Chaos::Softs::FThreadingProxy FThreadingProxy;
	typedef Chaos::Softs::FFleshThreadingProxy FFleshThreadingProxy;
	typedef Chaos::Softs::FDeformablePackage FDeformablePackage;
	typedef Chaos::Softs::FDataMapValue FDataMapValue;
	typedef Chaos::Softs::FDeformableSolver FDeformableSolver;

	~UDeformableSolverComponent();
	void UpdateTickGroup();

	// Begin IDataflowPhysicsSolverInterface overrides
	virtual FString GetSimulationName() const override {return GetName();};
	virtual FDataflowSimulationAsset& GetSimulationAsset() override {return SimulationAsset;};
	virtual const FDataflowSimulationAsset& GetSimulationAsset() const override {return SimulationAsset;};
	virtual FDataflowSimulationProxy* GetSimulationProxy() override {return &FleshSolverProxy;}
	virtual const FDataflowSimulationProxy* GetSimulationProxy() const  override {return &FleshSolverProxy;}
	virtual void BuildSimulationProxy() override;
	virtual void ResetSimulationProxy() override;
	virtual void WriteToSimulation(const float DeltaTime) override;
	virtual void ReadFromSimulation(const float DeltaTime) override;
	// End IDataflowPhysicsSolverInterface overrides

	// Begin UActorComponent overrides
	virtual bool ShouldCreatePhysicsState() const override {return true;}
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent overrides

	/* Game thread access to the solver proxy */
	FDeformableSolver::FGameThreadAccess GameThreadAccess();

	/* Physics thread access to the solver proxy */
	FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess();

	bool IsSimulating(UDeformablePhysicsComponent*) const;
	bool IsSimulatable() const;
	void AddDeformableProxy(UDeformablePhysicsComponent* InComponent);
	void RemoveDeformableProxy(UDeformablePhysicsComponent* InComponent);
	void Simulate(float DeltaTime);
	void SetSimulationTicking(const bool InSimulationTicking) {bSimulationTicking = InSimulationTicking;}

	/* Callback to trigger the deformable update after the simulation */
	void UpdateDeformableEndTickState(bool bRegister);
	
	/* Solver dataflow asset used to advance in time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FDataflowSimulationAsset SimulationAsset;

	/* Properties : Do NOT place ungrouped properties in this class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (EditCondition = "false"))
	FConnectedObjectsGroup ConnectedObjects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverTimingGroup SolverTiming;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverEvolutionGroup SolverEvolution;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverCollisionsGroup SolverCollisions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverConstraintsGroup SolverConstraints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverForcesGroup SolverForces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverDebuggingGroup SolverDebugging;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FSolverMuscleActivationGroup SolverMuscleActivation;

	// Simulation Variables
	FDataflowFleshSolverProxy FleshSolverProxy;

#if WITH_EDITOR
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
#endif

protected:

	/** Ref for the deformable solvers parallel task, so we can detect whether or not a sim is running */
	FGraphEventRef ParallelDeformableTask;
	FDeformableEndTickFunction DeformableEndTickFunction;

	/** Boolean to check if we can tick the simulation */
	bool bSimulationTicking = true;
};

