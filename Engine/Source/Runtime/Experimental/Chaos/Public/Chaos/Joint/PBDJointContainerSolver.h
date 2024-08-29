// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"
#include "Chaos/Joint/PBDJointSolverGaussSeidel.h"
#include "Chaos/Joint/PBDJointCachedSolverGaussSeidel.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/PBDJointConstraintTypes.h"

namespace Chaos
{
	namespace Private
	{
		/**
		 * Runs the solvers for a set of constraints belonging to a JointConstraints container.
		 * 
		 * For the main scene, each IslandGroup owns a FPBDJointContainerSolver and the list of constraints to be solved
		 * and the order in which they are solved is determined by the constraint graph.
		 * 
		 * For RBAN, there is one FPBDJointContainerSolver that solves all joints in the simulation in the order that
		 * they occur in the container.
		*/
		class FPBDJointContainerSolver : public FConstraintContainerSolver
		{
		public:
			FPBDJointContainerSolver(FPBDJointConstraints& InConstraintContainer, const int32 InPriority);
			~FPBDJointContainerSolver();

			// FConstraintContainerSolver impl
			virtual int32 GetNumConstraints() const override final { return ContainerLinearConstraintGlobalIndices.Num() + ContainerNonLinearConstraintGlobalIndices.Num();}
			virtual void Reset(const int32 InMaxCollisions) override final;
			virtual void AddConstraints() override final;
			virtual void AddConstraints(const TArrayView<Private::FPBDIslandConstraint*>& IslandConstraints) override final;
			virtual void AddBodies(FSolverBodyContainer& SolverBodyContainer) override final;
			virtual void GatherInput(const FReal Dt) override final;
			virtual void GatherInput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
			virtual void ScatterOutput(const FReal Dt) override final;
			virtual void ScatterOutput(const FReal Dt, const int32 BeginIndex, const int32 EndIndex) override final;
			virtual void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
			virtual void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;
			virtual void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) override final;

			FPBDJointConstraints& GetContainer() const { return ConstraintContainer; }
			const FPBDJointSolverSettings& GetSettings() const { return ConstraintContainer.GetSettings(); }
			const FPBDJointSettings& GetLinearConstraintSettings(const int32 InConstraintIndex) const { return ConstraintContainer.GetConstraintSettings(ContainerLinearConstraintGlobalIndices[InConstraintIndex]);}
			const FPBDJointSettings& GetNonLinearConstraintSettings(const int32 InConstraintIndex) const { return ConstraintContainer.GetConstraintSettings(ContainerNonLinearConstraintGlobalIndices[InConstraintIndex]); }
			const FPBDJointSettings& GetConstraintSettings(const int32 InConstraintIndex, const bool bUseLinearSolver = true) const 
			{
				if (bUseLinearSolver)
				{
					return ConstraintContainer.GetConstraintSettings(ContainerLinearConstraintGlobalIndices[InConstraintIndex]);
				}
				else
				{
					return ConstraintContainer.GetConstraintSettings(ContainerNonLinearConstraintGlobalIndices[InConstraintIndex]);
				}
			}
			int32 GetContainerConstraintIndex(const int32 InConstraintIndex, const bool bUseLinearSolver) const 
			{
				if (bUseLinearSolver)
				{
					return ContainerLinearConstraintGlobalIndices[InConstraintIndex];
				}
				else
				{
					return ContainerNonLinearConstraintGlobalIndices[InConstraintIndex];
				}
			}
			int32 GetContainerLinearConstraintGlobalIndex(const int32 InLocalConstraintIndex) const { return ContainerLinearConstraintGlobalIndices[InLocalConstraintIndex];}
			int32 GetContainerNonLinearConstraintGlobalIndex(const int32 InLocalConstraintIndex) const { return ContainerNonLinearConstraintGlobalIndices[InLocalConstraintIndex]; }
			int32 GetContainerLinearGlobalContraintIndex(const int32 InSolverIndex) const { return ContainerLinearConstraintGlobalIndices[InSolverIndex]; }
			int32 GetContainerNonLinearGlobalContraintIndex(const int32 InSolverIndex) const { return ContainerNonLinearConstraintGlobalIndices[InSolverIndex]; }


		private:
			bool UseLinearSolver() const;
			void AddConstraint(const int32 InContainerConstraintIndex, const bool bUseLinearSolver = true);
			void ResizeSolverArrays();
			void ApplyLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts);
			void ApplyNonLinearProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts);

			FPBDJointConstraints& ConstraintContainer;

			// The linear and non-linear joint solvers. One for each joint we wish to solve in the order that they are solved
			TArray<FPBDJointCachedSolver> LinearConstraintSolvers;
			TArray<FPBDJointSolver> NonLinearConstraintSolvers;

			// Index remapping from respective internal index of each array to index in the joint container [0,NumJointsInWorld)
			TArray<int32> ContainerLinearConstraintGlobalIndices;
			TArray<int32> ContainerNonLinearConstraintGlobalIndices;
		};

	}
}