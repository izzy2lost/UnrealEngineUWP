// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/rbf/RBFBehaviorEvaluator.h"
#include "riglogic/rbf/cpu/CPURBFBehaviorEvaluator.h"
#include "riglogic/rbf/cpu/CPURBFBehaviorOutputInstance.h"
#include "riglogic/rbf/cpu/RBFSolver.h"
#include "riglogic/types/Aliases.h"
#include "riglogic/types/bpcm/Optimizer.h"
#include "riglogic/types/LODSpec.h"
#include "riglogic/utils/Extd.h"

#include <tdm/Ang.h>

namespace rl4 {

namespace rbf {

namespace cpu {

template<typename T, typename TF256, typename TF128>
class Factory {
    public:
        static RBFBehaviorEvaluator::Pointer create(const dna::Reader* reader, MemoryResource* memRes) {
            Vector<RBFSolver::Pointer> solvers{memRes};
            std::uint16_t maximumInputCount{};
            std::uint16_t maximumTargetCount{};
            Matrix<std::uint16_t> solverRawControlInputIndices{memRes};
            Matrix<std::uint16_t> solverRawControlOutputIndices{memRes};

            auto instanceFactory = [](std::uint16_t maxInputCount, std::uint16_t maxTargetCount, MemoryResource* instanceMemRes) {
                    using OutputInstancePointer = UniqueInstance<OutputInstance, RBFBehaviorOutputInstance>;
                    return OutputInstancePointer::with(instanceMemRes).create(maxInputCount, maxTargetCount, instanceMemRes);
                };
            auto factory = UniqueInstance<Evaluator<T, TF256, TF128>, RBFBehaviorEvaluator>::with(memRes);

            if (reader == nullptr) {
                return factory.create(LODSpec<std::uint16_t>{memRes},
                                      std::move(solvers),
                                      std::move(solverRawControlInputIndices),
                                      std::move(solverRawControlOutputIndices),
                                      maximumInputCount,
                                      maximumTargetCount,
                                      std::move(instanceFactory));
            }

            auto lods = computeLODs(reader, memRes);
            solvers.reserve(lods.count);
            solverRawControlInputIndices.resize(lods.count);
            solverRawControlOutputIndices.resize(lods.count);

            const std::uint16_t rbfControlsOffset = static_cast<std::uint16_t>(
                reader->getRawControlCount() +
                reader->getPSDCount() +
                reader->getMLControlCount());

            RBFSolverRecipe recipe{};
            Vector<float> targetScales{memRes};
            Vector<float> targetValues{memRes};
            const auto rotationUnit = reader->getRotationUnit();
            std::function<float(float)> angConv;
            if (rotationUnit == dna::RotationUnit::degrees) {
                angConv = [](float angle) {
                        return tdm::frad{tdm::fdeg{angle}}.value;
                    };
            } else {
                angConv = [](float angle) {
                        return tdm::frad{angle}.value;
                    };
            }
            for (std::uint16_t solverIndex = {}; solverIndex < lods.count; ++solverIndex) {
                recipe.solverType = reader->getRBFSolverType(solverIndex);
                recipe.distanceMethod = reader->getRBFSolverDistanceMethod(solverIndex);
                recipe.weightFunction = reader->getRBFSolverFunctionType(solverIndex);
                recipe.normalizeMethod = reader->getRBFSolverNormalizeMethod(solverIndex);
                recipe.twistAxis = reader->getRBFSolverTwistAxis(solverIndex);
                recipe.isAutomaticRadius = reader->getRBFSolverAutomaticRadius(solverIndex) == dna::AutomaticRadius::On;
                recipe.radius = angConv(reader->getRBFSolverRadius(solverIndex));
                recipe.weightThreshold = reader->getRBFSolverWeightThreshold(solverIndex);

                auto rawControlIndices = reader->getRBFSolverRawControlIndices(solverIndex);
                solverRawControlInputIndices[solverIndex].assign(rawControlIndices.begin(), rawControlIndices.end());
                if (rawControlIndices.size() > maximumInputCount) {
                    maximumInputCount = static_cast<std::uint16_t>(rawControlIndices.size());
                }
                recipe.rawControlCount = static_cast<std::uint16_t>(rawControlIndices.size());

                auto poseIndices = reader->getRBFSolverPoseIndices(solverIndex);
                solverRawControlOutputIndices[solverIndex].reserve(poseIndices.size());
                for (const std::uint16_t pi : poseIndices) {
                    solverRawControlOutputIndices[solverIndex].push_back(static_cast<std::uint16_t>(rbfControlsOffset + pi));
                }

                const auto targetCount = static_cast<std::uint16_t>(poseIndices.size());
                if (targetCount > maximumTargetCount) {
                    maximumTargetCount = targetCount;
                    targetScales.resize(maximumTargetCount);
                }

                for (std::uint16_t i = 0u; i < targetCount; ++i) {
                    targetScales[i] = reader->getRBFPoseScale(poseIndices[i]);
                }
                recipe.targetValues = reader->getRBFSolverRawControlValues(solverIndex);
                recipe.targetScales = ConstArrayView<float>{targetScales.data(), targetCount};

                auto solver = RBFSolver::create(recipe, memRes);
                solvers.emplace_back(std::move(solver));
            }

            return factory.create(std::move(lods),
                                  std::move(solvers),
                                  std::move(solverRawControlInputIndices),
                                  std::move(solverRawControlOutputIndices),
                                  maximumInputCount,
                                  maximumTargetCount,
                                  std::move(instanceFactory));
        }

    private:
        static LODSpec<std::uint16_t> computeLODs(const dna::RBFBehaviorReader* reader, MemoryResource* memRes) {
            LODSpec<std::uint16_t> lods{memRes};
            const auto lodCount = reader->getLODCount();
            lods.indicesPerLOD.resize(lodCount);
            for (std::uint16_t lod = {}; lod < lodCount; ++lod) {
                const auto solverIndices = reader->getRBFSolverIndicesForLOD(lod);
                lods.indicesPerLOD[lod].assign(solverIndices.begin(), solverIndices.end());
            }
            lods.count = reader->getRBFSolverCount();
            return lods;
        }

};

}  // namespace cpu

}  // namespace rbf

}  // namespace rl4
