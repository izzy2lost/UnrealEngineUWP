// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDefinitions.h"

#if RHI_RAYTRACING

#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RayTracing/RayTracingScene.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "RenderGraphUtils.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "ShaderCompilerCore.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "Nanite/NaniteRayTracing.h"
#include "Lumen/LumenReflections.h"

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipBackFaceHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipBackFaceHitDistance"),
	5.0f,
	TEXT("Distance to trace with backface culling enabled, useful when the Ray Tracing geometry doesn't match the GBuffer (Nanite Proxy geometry)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenHardwareRayTracingSkipTwoSidedHitDistance(
	TEXT("r.Lumen.HardwareRayTracing.SkipTwoSidedHitDistance"),
	1.0f,
	TEXT("When the SkipBackFaceHitDistance is enabled, the first two-sided material hit within this distance will be skipped. This is useful for avoiding self-intersections with the Nanite fallback mesh on foliage, as SkipBackFaceHitDistance doesn't work on two sided materials."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenHardwareRayTracing
{
	// 0 - hit group with AVOID_SELF_INTERSECTIONS=0
	// 1 - hit group with AVOID_SELF_INTERSECTIONS=1
	constexpr uint32 NumHitGroups = 2;
};

IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::LumenMinimal, 16);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLumenHardwareRayTracingUniformBufferParameters, "LumenHardwareRayTracingUniformBuffer");

class FLumenHardwareRayTracingMaterialHitGroup : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialHitGroup, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenHardwareRayTracingUniformBufferParameters, LumenHardwareRayTracingUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_REF(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		SHADER_PARAMETER_STRUCT_REF(FSceneUniformParameters, Scene)
	END_SHADER_PARAMETER_STRUCT()

	class FAvoidSelfIntersectionsMode : SHADER_PERMUTATION_ENUM_CLASS("AVOID_SELF_INTERSECTIONS_MODE", LumenHardwareRayTracing::EAvoidSelfIntersectionsMode);
	class FNaniteRayTracing : SHADER_PERMUTATION_BOOL("NANITE_RAY_TRACING");
	using FPermutationDomain = TShaderPermutationDomain<FAvoidSelfIntersectionsMode, FNaniteRayTracing>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialHitGroup, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "closesthit=LumenHardwareRayTracingMaterialCHS anyhit=LumenHardwareRayTracingMaterialAHS", SF_RayHitGroup);

class FLumenHardwareRayTracingMaterialMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FLumenHardwareRayTracingMaterialMS, FGlobalShader)

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform) && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::LumenMinimal;
	}

	using FParameters = FEmptyShaderParameters;
};

IMPLEMENT_GLOBAL_SHADER(FLumenHardwareRayTracingMaterialMS, "/Engine/Private/Lumen/LumenHardwareRayTracingMaterials.usf", "LumenHardwareRayTracingMaterialMS", SF_RayMiss);

void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingUniformBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	FLumenHardwareRayTracingUniformBufferParameters* LumenHardwareRayTracingUniformBufferParameters = GraphBuilder.AllocParameters<FLumenHardwareRayTracingUniformBufferParameters>();
	LumenHardwareRayTracingUniformBufferParameters->SkipBackFaceHitDistance = CVarLumenHardwareRayTracingSkipBackFaceHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters->SkipTwoSidedHitDistance = CVarLumenHardwareRayTracingSkipTwoSidedHitDistance.GetValueOnRenderThread();
	LumenHardwareRayTracingUniformBufferParameters->SkipTranslucent         = LumenReflections::UseTranslucentRayTracing(View) ? 0.0f : 1.0f;
	View.LumenHardwareRayTracingUniformBuffer = GraphBuilder.CreateUniformBuffer(LumenHardwareRayTracingUniformBufferParameters);
}

uint32 CalculateLumenHardwareRayTracingUserData(const FRayTracingMeshCommand& MeshCommand)
{
	return (MeshCommand.MaterialShaderIndex & LUMEN_MATERIAL_SHADER_INDEX_MASK)
		| (((MeshCommand.bAlphaMasked != 0) & 0x01) << 28)
		| (((MeshCommand.bCastRayTracedShadows != 0) & 0x01) << 29)
		| (((MeshCommand.bTwoSided != 0) & 0x01) << 30)
		| (((MeshCommand.bIsTranslucent != 0) & 0x01) << 31);
}

// TODO: This should be moved into FRayTracingScene and used as a base for other effects. There is not need for it to be Lumen specific.
void FDeferredShadingSceneRenderer::SetupLumenHardwareRayTracingHitGroupBuffer(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::BuildLumenHardwareRayTracingHitGroupData);

	const FRayTracingSceneInitializer2& SceneInitializer = Scene->RayTracingScene.GetRHIRayTracingSceneChecked()->GetInitializer();
	const uint32 NumTotalSegments = FMath::Max(SceneInitializer.NumTotalSegments, 1u);

	FRDGUploadData<Lumen::FHitGroupRootConstants> HitGroupData(GraphBuilder, NumTotalSegments);

	const uint32 NumTotalMeshCommands = View.VisibleRayTracingMeshCommands.Num();

	if(NumTotalMeshCommands > 0)
	{
		const uint32 TargetCommandsPerTask = 512;

		// Distribute work evenly to the available task graph workers based on NumTotalMeshCommands.
		const uint32 NumThreads = FMath::Min(FTaskGraphInterface::Get().GetNumWorkerThreads(), CVarRHICmdWidth.GetValueOnRenderThread());
		const uint32 NumTasks = FMath::Min(NumThreads, FMath::DivideAndRoundUp(NumTotalMeshCommands, TargetCommandsPerTask));
		const uint32 NumCommandsPerTask = FMath::DivideAndRoundUp(NumTotalMeshCommands, NumTasks);

		for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			const uint32 FirstTaskCommandIndex = TaskIndex * NumCommandsPerTask;
			const FVisibleRayTracingMeshCommand* MeshCommands = View.VisibleRayTracingMeshCommands.GetData() + FirstTaskCommandIndex;
			const uint32 NumCommands = FMath::Min(NumCommandsPerTask, NumTotalMeshCommands - FirstTaskCommandIndex);

			GraphBuilder.AddSetupTask([MeshCommands, NumCommands, HitGroupData, &SceneInitializer]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(BuildLumenHardwareRayTracingHitGroupDataTask);

					for (uint32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
					{
						const FVisibleRayTracingMeshCommand VisibleMeshCommand = MeshCommands[CommandIndex];
						const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

						const uint32 InstanceIndex = VisibleMeshCommand.InstanceIndex;
						const uint32 SegmentIndex = MeshCommand.GeometrySegmentIndex;

						const uint32 HitGroupIndex = SceneInitializer.SegmentPrefixSum[InstanceIndex] + SegmentIndex;

						HitGroupData[HitGroupIndex].UserData = CalculateLumenHardwareRayTracingUserData(MeshCommand);
					}
				});
		}
	}
	
	View.LumenHardwareRayTracingHitDataBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("LumenHardwareRayTracingHitDataBuffer"), HitGroupData);
}

void FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline(FRDGBuilder& GraphBuilder, FViewInfo& View, const TArrayView<FRHIRayTracingShader*>& RayGenShaderTable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::CreateLumenHardwareRayTracingMaterialPipeline);
	SCOPE_CYCLE_COUNTER(STAT_CreateLumenRayTracingPipeline);

	// create RTPSO
	{
		FRHICommandList& RHICmdList = GraphBuilder.RHICmdList;

		FRayTracingPipelineStateInitializer Initializer;

		Initializer.SetRayGenShaderTable(RayGenShaderTable);

		Initializer.MaxPayloadSizeInBytes = GetRayTracingPayloadTypeMaxSize(ERayTracingPayloadType::LumenMinimal);

		// Get the ray tracing materials
		FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
		auto HitGroupShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
		auto HitGroupShaderWithAvoidSelfIntersections = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
		auto HitGroupShaderNaniteRT = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
		PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
		auto HitGroupShaderNaniteRTWithAvoidSelfIntersections = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);

		FRHIRayTracingShader* HitShaderTable[] = {
			HitGroupShader.GetRayTracingShader(),
			HitGroupShaderWithAvoidSelfIntersections.GetRayTracingShader(),
			HitGroupShaderNaniteRT.GetRayTracingShader(),
			HitGroupShaderNaniteRTWithAvoidSelfIntersections.GetRayTracingShader()
		};
		Initializer.SetHitGroupTable(HitShaderTable);
		Initializer.bAllowHitGroupIndexing = true;

		auto MissShader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialMS>();
		FRHIRayTracingShader* MissShaderTable[] = { MissShader.GetRayTracingShader() };
		Initializer.SetMissShaderTable(MissShaderTable);

		FRayTracingPipelineState* PipelineState = PipelineStateCache::GetAndOrCreateRayTracingPipelineState(RHICmdList, Initializer);

		View.LumenHardwareRayTracingMaterialPipeline = PipelineState;
	}

	// launch tasks to setup bindings
	{
		FRHIUniformBuffer* SceneUniformBuffer = GetSceneUniforms().GetBufferRHI(GraphBuilder);
		FRHIUniformBuffer* LumenHardwareRayTracingUniformBuffer = GraphBuilder.ConvertToExternalUniformBuffer(View.LumenHardwareRayTracingUniformBuffer);

		struct FBinding
		{
			int32 ShaderIndexInPipeline;
			uint32 NumUniformBuffers;
			FRHIUniformBuffer** UniformBufferArray;
		};

		auto SetupBinding = [&](FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector)
			{
				auto Shader = View.ShaderMap->GetShader<FLumenHardwareRayTracingMaterialHitGroup>(PermutationVector);
				auto HitGroupShader = Shader.GetRayTracingShader();

				FBinding Binding;
				Binding.ShaderIndexInPipeline = FindRayTracingHitGroupIndex(View.LumenHardwareRayTracingMaterialPipeline, HitGroupShader, true);
				Binding.NumUniformBuffers = Shader->ParameterMapInfo.UniformBuffers.Num();
				Binding.UniformBufferArray = (FRHIUniformBuffer**)View.LumenRayTracingMaterialBindingsMemory.Alloc(sizeof(FRHIUniformBuffer*) * Binding.NumUniformBuffers, alignof(FRHIUniformBuffer*));

				const auto& LumenHardwareRayTracingUniformBufferParameter = Shader->GetUniformBufferParameter<FLumenHardwareRayTracingUniformBufferParameters>();
				const auto& ViewUniformBufferParameter = Shader->GetUniformBufferParameter<FViewUniformShaderParameters>();
				const auto& SceneUniformBufferParameter = Shader->GetUniformBufferParameter<FSceneUniformParameters>();
				const auto& NaniteUniformBufferParameter = Shader->GetUniformBufferParameter<FNaniteRayTracingUniformParameters>();

				if (LumenHardwareRayTracingUniformBufferParameter.IsBound())
				{
					check(LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
					Binding.UniformBufferArray[LumenHardwareRayTracingUniformBufferParameter.GetBaseIndex()] = LumenHardwareRayTracingUniformBuffer;
				}

				if (ViewUniformBufferParameter.IsBound())
				{
					check(ViewUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
					Binding.UniformBufferArray[ViewUniformBufferParameter.GetBaseIndex()] = View.ViewUniformBuffer.GetReference();
				}

				if (SceneUniformBufferParameter.IsBound())
				{
					check(SceneUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
					Binding.UniformBufferArray[SceneUniformBufferParameter.GetBaseIndex()] = SceneUniformBuffer;
				}

				if (NaniteUniformBufferParameter.IsBound())
				{
					check(NaniteUniformBufferParameter.GetBaseIndex() < Binding.NumUniformBuffers);
					Binding.UniformBufferArray[NaniteUniformBufferParameter.GetBaseIndex()] = Nanite::GRayTracingManager.GetUniformBuffer().GetReference();
				}

				return Binding;
			};

		FBinding* ShaderBindings = (FBinding*)View.LumenRayTracingMaterialBindingsMemory.Alloc(sizeof(FBinding) * LumenHardwareRayTracing::NumHitGroups, alignof(FBinding));
		FBinding* ShaderBindingsNaniteRT = (FBinding*)View.LumenRayTracingMaterialBindingsMemory.Alloc(sizeof(FBinding) * LumenHardwareRayTracing::NumHitGroups, alignof(FBinding));

		{
			FLumenHardwareRayTracingMaterialHitGroup::FPermutationDomain PermutationVector;

			{
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
				ShaderBindings[0] = SetupBinding(PermutationVector);

				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(false);
				ShaderBindings[1] = SetupBinding(PermutationVector);
			}

			{
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::Disabled);
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
				ShaderBindingsNaniteRT[0] = SetupBinding(PermutationVector);

				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FAvoidSelfIntersectionsMode>(LumenHardwareRayTracing::EAvoidSelfIntersectionsMode::AHS);
				PermutationVector.Set<FLumenHardwareRayTracingMaterialHitGroup::FNaniteRayTracing>(true);
				ShaderBindingsNaniteRT[1] = SetupBinding(PermutationVector);
			}
		}

		{
			const uint32 NumTotalMeshCommands = View.VisibleRayTracingMeshCommands.Num();
			const uint32 TargetCommandsPerTask = 4096; // Granularity chosen based on profiling Infiltrator scene to balance wall time speedup and total CPU thread time.
			const uint32 NumTasks = FMath::Max(1u, FMath::DivideAndRoundUp(NumTotalMeshCommands, TargetCommandsPerTask));
			const uint32 CommandsPerTask = FMath::DivideAndRoundUp(NumTotalMeshCommands, NumTasks); // Evenly divide commands between tasks (avoiding potential short last task)

			FGraphEventArray TaskList;
			TaskList.Reserve(NumTasks);
			View.LumenRayTracingMaterialBindings.SetNum(NumTasks);

			for (uint32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
			{
				const uint32 FirstTaskCommandIndex = TaskIndex * CommandsPerTask;
				const FVisibleRayTracingMeshCommand* MeshCommands = View.VisibleRayTracingMeshCommands.GetData() + FirstTaskCommandIndex;
				const uint32 NumCommands = FMath::Min(CommandsPerTask, NumTotalMeshCommands - FirstTaskCommandIndex);

				FRayTracingLocalShaderBindingWriter* BindingWriter = new FRayTracingLocalShaderBindingWriter();
				View.LumenRayTracingMaterialBindings[TaskIndex] = BindingWriter;

				TaskList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
					[ShaderBindings, ShaderBindingsNaniteRT, BindingWriter, MeshCommands, NumCommands, TaskIndex]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(BuildLumenHardwareRayTracingMaterialBindingsTask);

						for (uint32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
						{
							const FVisibleRayTracingMeshCommand VisibleMeshCommand = MeshCommands[CommandIndex];
							const FRayTracingMeshCommand& MeshCommand = *VisibleMeshCommand.RayTracingMeshCommand;

							for (uint32 HitGroupIndex = 0; HitGroupIndex < LumenHardwareRayTracing::NumHitGroups; ++HitGroupIndex)
							{
								const FBinding& LumenBinding = MeshCommand.IsUsingNaniteRayTracing() ? ShaderBindingsNaniteRT[HitGroupIndex] : ShaderBindings[HitGroupIndex];

								FRayTracingLocalShaderBindings& Binding = BindingWriter->AddWithExternalParameters();
								Binding.ShaderSlot = HitGroupIndex;
								Binding.ShaderIndexInPipeline = LumenBinding.ShaderIndexInPipeline;
								Binding.InstanceIndex = VisibleMeshCommand.InstanceIndex;
								Binding.SegmentIndex = MeshCommand.GeometrySegmentIndex;
								Binding.UserData = CalculateLumenHardwareRayTracingUserData(MeshCommand);
								Binding.UniformBuffers = LumenBinding.UniformBufferArray;
								Binding.NumUniformBuffers = LumenBinding.NumUniformBuffers;
							}
						}
					},
					TStatId(), nullptr, ENamedThreads::AnyThread));
			}

			View.LumenRayTracingMaterialBindingsTask = FFunctionGraphTask::CreateAndDispatchWhenReady([]() {}, TStatId(), &TaskList, ENamedThreads::AnyHiPriThreadHiPriTask);
		}
	}
}

void FDeferredShadingSceneRenderer::BindLumenHardwareRayTracingMaterialPipeline(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BindLumenHardwareRayTracingMaterialPipeline);

	FTaskGraphInterface::Get().WaitUntilTaskCompletes(View.LumenRayTracingMaterialBindingsTask, ENamedThreads::GetRenderThread_Local()); // TODO: move this sync point to the end of RDG setup, before execution
	MergeAndSetRayTracingBindings(RHICmdList, Allocator, View.GetRayTracingSceneChecked(), View.LumenHardwareRayTracingMaterialPipeline, View.LumenRayTracingMaterialBindings, ERayTracingBindingType::HitGroup);

	// Move the ray tracing binding container ownership to the command list, so that memory will be
	// released on the RHI thread timeline, after the commands that reference it are processed.
	RHICmdList.EnqueueLambda([Ptrs = MoveTemp(View.LumenRayTracingMaterialBindings), Mem = MoveTemp(View.LumenRayTracingMaterialBindingsMemory)](FRHICommandListImmediate&)
	{
		for (auto Ptr : Ptrs)
		{
			delete Ptr;
		}
	});
}

#endif // RHI_RAYTRACING