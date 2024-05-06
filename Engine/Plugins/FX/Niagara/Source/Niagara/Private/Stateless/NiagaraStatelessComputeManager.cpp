// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessComputeManager.h"
#include "Stateless/NiagaraStatelessEmitterData.h"
#include "Stateless/NiagaraStatelessEmitterInstance.h"
#include "Stateless/NiagaraStatelessSimulationShader.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraGPUInstanceCountManager.h"
#include "NiagaraRenderer.h"

#include "GPUSortManager.h"
#include "PrimitiveSceneProxy.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"

bool GNiagaraStatelessComputeManager_UseCache = true;
FAutoConsoleVariableRef CVarNiagaraStatelessComputeManager_UseCache(
	TEXT("fx.NiagaraStateless.ComputeManager.UseCache"),
	GNiagaraStatelessComputeManager_UseCache,
	TEXT("When enabled we will attempt to reuse allocated buffers between frames."),
	ECVF_Default
);

int32 GNiagaraStatelessComputeManager_CPUThreshold = 0;
FAutoConsoleVariableRef CVarNiagaraStatelessComputeManager_CPUThreshold(
	TEXT("fx.NiagaraStateless.ComputeManager.CPUThreshold"),
	GNiagaraStatelessComputeManager_CPUThreshold,
	TEXT("When lower than this particle count prefer to use the CPU over dispatching a compute shader."),
	ECVF_Default
);

FNiagaraStatelessComputeManager::FNiagaraStatelessComputeManager(FNiagaraGpuComputeDispatchInterface* InOwnerInterface)
	: FNiagaraGpuComputeDataManager(InOwnerInterface)
{
	InOwnerInterface->GetOnPreRenderEvent().AddRaw(this, &FNiagaraStatelessComputeManager::OnPostPreRender);
	InOwnerInterface->GetOnPostRenderEvent().AddRaw(this, &FNiagaraStatelessComputeManager::OnPostPostRender);
}

FNiagaraStatelessComputeManager::~FNiagaraStatelessComputeManager()
{
}

FNiagaraDataBuffer* FNiagaraStatelessComputeManager::GetDataBuffer(uintptr_t EmitterKey, const NiagaraStateless::FEmitterInstance_RT* EmitterInstance)
{
	using namespace NiagaraStateless;

	if (TUniquePtr<FStatelessDataCache>* ExistingData = UsedData.Find(EmitterKey))
	{
		return (*ExistingData)->DataBuffer;
	}

	if (EmitterInstance->ExecutionState == ENiagaraExecutionState::Complete || EmitterInstance->ExecutionState == ENiagaraExecutionState::Disabled)
	{
		return nullptr;
	}

	const FNiagaraStatelessEmitterData* EmitterData = EmitterInstance->EmitterData.Get();
	TShaderRef<NiagaraStateless::FSimulationShader> ComputeShader = EmitterData->GetShader();
	if (!ComputeShader.IsValid() || !ComputeShader.GetComputeShader())
	{
		return nullptr;
	}

	uint32 ActiveParticles = 0;
	{
		NiagaraStateless::FCommonShaderParameters* ShaderParameters = EmitterInstance->ShaderParameters.Get();
		ActiveParticles = EmitterData->CalculateActiveParticles(
			EmitterInstance->RandomSeed,
			EmitterInstance->SpawnInfos,
			EmitterInstance->Age,
			&ShaderParameters->SpawnParameters
		);
	}
	if (ActiveParticles == 0)
	{
		return nullptr;
	}

	FNiagaraGpuComputeDispatchInterface* ComputeInterface = GetOwnerInterface();

	const uint32 DataSetLayoutHash = EmitterInstance->EmitterData->ParticleDataSetCompiledData.GetLayoutHash();

	FStatelessDataCache* CacheData = nullptr;
	if (GNiagaraStatelessComputeManager_UseCache)
	{
		for (int32 i=0; i < FreeData.Num(); ++i)
		{
			if (FreeData[i]->DataSetLayoutHash == DataSetLayoutHash)
			{
				CacheData = FreeData[i].Release();
				FreeData.RemoveAtSwap(i, EAllowShrinking::No);
				break;
			}
		}
	}

	if ( CacheData == nullptr )
	{
		CacheData = new FStatelessDataCache();
		CacheData->DataSetLayoutHash = DataSetLayoutHash;
		CacheData->DataSet.Init(&EmitterInstance->EmitterData->ParticleDataSetCompiledData);
		CacheData->DataBuffer = new FNiagaraDataBuffer(&CacheData->DataSet);
	}

	CacheData->EmitterInstance = EmitterInstance;
	CacheData->ActiveParticles = ActiveParticles;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	CacheData->DataBuffer->AllocateGPU(RHICmdList, CacheData->ActiveParticles, ComputeInterface->GetFeatureLevel(), TEXT("StatelessSimBuffer"));

	const bool bAllowGPUExec = EnumHasAnyFlags(EmitterData->FeatureMask, ENiagaraStatelessFeatureMask::ExecuteGPU);
	const bool bUseCPUExec = EnumHasAnyFlags(EmitterData->FeatureMask, ENiagaraStatelessFeatureMask::ExecuteCPU) && (!bAllowGPUExec || (ActiveParticles <= uint32(GNiagaraStatelessComputeManager_CPUThreshold)));
	if (bUseCPUExec)
	{
		FParticleSimulationContext ParticleSimulation(EmitterData, EmitterInstance->BindingBufferData.Get(TArray<uint8>()));
		ParticleSimulation.SimulateGPU(RHICmdList, EmitterInstance->RandomSeed, EmitterInstance->Age, EmitterInstance->DeltaTime, EmitterInstance->SpawnInfos, CacheData->DataBuffer);
		if (ParticleSimulation.GetNumInstances() == 0)
		{
			FreeData.Emplace(CacheData);
			return nullptr;
		}

		//CacheData->DataBuffer->Dump(0, CacheData->DataBuffer->GetNumInstances(), TEXT("Stateless"));
	}
	else
	{
		if (!ensure(bAllowGPUExec))
		{
			FreeData.Emplace(CacheData);
			return nullptr;
		}

		CacheData->DataBuffer->SetNumInstances(CacheData->ActiveParticles);

		FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
		const uint32 CountOffset = CountManager.AcquireOrAllocateEntry(RHICmdList);
		CacheData->DataBuffer->SetGPUInstanceCountBufferOffset(CountOffset);
		CountsToRelease.Add(CountOffset);

		GPUDataToGenerate.Add(CacheData);
	}

	UsedData.Emplace(EmitterKey, CacheData);
	return CacheData->DataBuffer;
}

void FNiagaraStatelessComputeManager::OnPostPreRender(FRDGBuilder& GraphBuilder)
{
	// Anything to process?
	if (GPUDataToGenerate.Num() == 0)
	{
		return;
	}

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, NiagaraStateless);
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FNiagaraStatelessComputeManager::OnPostPreRender"),
		[DataToGenerate=MoveTemp(GPUDataToGenerate), ComputeInterface=GetOwnerInterface()](FRHICommandListImmediate& RHICmdList)
		{
			const int32 NumJobs = DataToGenerate.Num();

			// Get Count Buffer
			FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
			FRHIUnorderedAccessView* CountBufferUAV = CountManager.GetInstanceCountBuffer().UAV;

			// Build Transitions
			TArray<FRHITransitionInfo> TransitionsBefore;
			TArray<FRHITransitionInfo> TransitionsAfter;
			{
				TransitionsBefore.Reserve(1 + (NumJobs * 2));
				TransitionsAfter.Reserve(1 + (NumJobs * 2));

				TransitionsBefore.Emplace(CountManager.GetInstanceCountBuffer().Buffer, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState, ERHIAccess::UAVCompute);
				TransitionsAfter.Emplace(CountManager.GetInstanceCountBuffer().Buffer, ERHIAccess::UAVCompute, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState);

				for (FStatelessDataCache* CacheData : DataToGenerate)
				{
					const FRWBuffer& FloatBuffer = CacheData->DataBuffer->GetGPUBufferFloat();
					if (FloatBuffer.NumBytes > 0)
					{
						TransitionsBefore.Emplace(FloatBuffer.Buffer, ERHIAccess::SRVMask, ERHIAccess::UAVCompute);
						TransitionsAfter.Emplace(FloatBuffer.Buffer, ERHIAccess::UAVCompute, ERHIAccess::SRVMask);
					}
					const FRWBuffer& IntBuffer = CacheData->DataBuffer->GetGPUBufferInt();
					if (IntBuffer.NumBytes > 0)
					{
						TransitionsBefore.Emplace(IntBuffer.Buffer, ERHIAccess::SRVMask, ERHIAccess::UAVCompute);
						TransitionsAfter.Emplace(IntBuffer.Buffer, ERHIAccess::UAVCompute, ERHIAccess::SRVMask);
					}
				}
			}

			FNiagaraEmptyUAVPoolScopedAccess UAVPoolAccessScope(ComputeInterface->GetEmptyUAVPool());
			FRHIUnorderedAccessView* EmptyIntBufferUAV = ComputeInterface->GetEmptyUAVFromPool(RHICmdList, PF_R32_SINT, ENiagaraEmptyUAVType::Buffer);

			// Execute Simulations
			RHICmdList.Transition(TransitionsBefore);

			RHICmdList.BeginUAVOverlap(CountBufferUAV);
			for (FStatelessDataCache* CacheData : DataToGenerate)
			{
				const NiagaraStateless::FEmitterInstance_RT* EmitterInstance = CacheData->EmitterInstance;
				const FNiagaraStatelessEmitterData* EmitterData = CacheData->EmitterInstance->EmitterData.Get();

				// Do we need to update the parameter buffer?
				if (EmitterInstance->BindingBufferData.IsSet())
				{
					EmitterInstance->BindingBuffer.Release();
					EmitterInstance->BindingBuffer.Initialize(RHICmdList, TEXT("FNiagaraStatelessEmitterInstance::BindingBuffer"), sizeof(uint32), EmitterInstance->BindingBufferData->Num() / sizeof(uint32), EPixelFormat::PF_R32_UINT, EBufferUsageFlags::Static);
					void* LockedBuffer = RHICmdList.LockBuffer(EmitterInstance->BindingBuffer.Buffer, 0, EmitterInstance->BindingBuffer.NumBytes, RLM_WriteOnly);
					FMemory::Memcpy(LockedBuffer, EmitterInstance->BindingBufferData->GetData(), EmitterInstance->BindingBuffer.NumBytes);
					RHICmdList.UnlockBuffer(EmitterInstance->BindingBuffer.Buffer);
					EmitterInstance->BindingBufferData.Reset();
				}

				// Update parameters for this compute invocation
				NiagaraStateless::FCommonShaderParameters* ShaderParameters = EmitterInstance->ShaderParameters.Get();
				ShaderParameters->Common_SimulationTime			= EmitterInstance->Age;
				ShaderParameters->Common_SimulationDeltaTime	= EmitterInstance->DeltaTime;
				ShaderParameters->Common_SimulationInvDeltaTime	= EmitterInstance->DeltaTime > 0.0f ? (1.0f / EmitterInstance->DeltaTime) : 0.0f;
				ShaderParameters->Common_OutputBufferStride		= CacheData->DataBuffer->GetFloatStride() / sizeof(float);
				ShaderParameters->Common_GPUCountBufferOffset	= CacheData->DataBuffer->GetGPUInstanceCountBufferOffset();
				ShaderParameters->Common_FloatOutputBuffer		= CacheData->DataBuffer->GetGPUBufferFloat().UAV;
				//ShaderParameters->Common_HalfOutputBuffer		= CacheData->DataBuffer->GetGPUBufferHalf().UAV;
				ShaderParameters->Common_IntOutputBuffer		= CacheData->DataBuffer->GetGPUBufferInt().UAV.IsValid() ? CacheData->DataBuffer->GetGPUBufferInt().UAV.GetReference() : EmptyIntBufferUAV;
				ShaderParameters->Common_GPUCountBuffer			= CountBufferUAV;
				ShaderParameters->Common_StaticFloatBuffer		= EmitterData->StaticFloatBuffer.SRV;
				ShaderParameters->Common_ParameterBuffer		= FNiagaraRenderer::GetSrvOrDefaultUInt(EmitterInstance->BindingBuffer.SRV);

				// Execute the simulation
				TShaderRef<NiagaraStateless::FSimulationShader> ComputeShader = EmitterData->GetShader();
				FRHIComputeShader* ComputeShaderRHI = ComputeShader.GetComputeShader();
				const uint32 NumThreadGroups = FMath::DivideAndRoundUp<uint32>(CacheData->ActiveParticles, NiagaraStateless::FSimulationShader::ThreadGroupSize);

				SetComputePipelineState(RHICmdList, ComputeShaderRHI);
				SetShaderParameters(RHICmdList, ComputeShader, ComputeShaderRHI, EmitterData->GetShaderParametersMetadata(), *ShaderParameters);
				RHICmdList.DispatchComputeShader(NumThreadGroups, 1, 1);
				UnsetShaderUAVs(RHICmdList, ComputeShader, ComputeShaderRHI);
			}
			RHICmdList.EndUAVOverlap(CountBufferUAV);

			RHICmdList.Transition(TransitionsAfter);
		}
	);
	GPUDataToGenerate.Empty();
}

void FNiagaraStatelessComputeManager::OnPostPostRender(FRDGBuilder& GraphBuilder)
{
	// Anything to process?
	if (UsedData.Num() + FreeData.Num() + CountsToRelease.Num() == 0)
	{
		return;
	}

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, NiagaraStateless);
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FNiagaraStatelessComputeManager::OnPostPostRender"),
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FreeData.Empty(UsedData.Num());
			for (auto it=UsedData.CreateIterator(); it; ++it)
			{
				FreeData.Emplace(it.Value().Release());
			}
			UsedData.Empty();

			if (CountsToRelease.Num() > 0)
			{
				FNiagaraGpuComputeDispatchInterface* ComputeInterface = GetOwnerInterface();
				FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
				CountManager.FreeEntryArray(CountsToRelease);
				CountsToRelease.Reset();
			}
		}
	);
}
