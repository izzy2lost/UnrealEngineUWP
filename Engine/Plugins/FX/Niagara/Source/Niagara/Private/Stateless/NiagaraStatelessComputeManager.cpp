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

namespace NiagaraStatelessComputeManagerPrivate
{
	enum class EComputeExecutionPath
	{
		None,
		CPU,
		GPU,
	};

	bool GUseDataBufferCache = true;
	FAutoConsoleVariableRef CVarUseCache(
		TEXT("fx.NiagaraStateless.ComputeManager.UseCache"),
		GUseDataBufferCache,
		TEXT("When enabled we will attempt to reuse allocated buffers between frames."),
		ECVF_Default
	);

	int32 GParticleCountCPUThreshold = 0;
	FAutoConsoleVariableRef CVarCPUThreshold(
		TEXT("fx.NiagaraStateless.ComputeManager.CPUThreshold"),
		GParticleCountCPUThreshold,
		TEXT("When lower than this particle count prefer to use the CPU over dispatching a compute shader."),
		ECVF_Default
	);

	EComputeExecutionPath DetermineComputeExecutionPath(const FNiagaraStatelessEmitterData* EmitterData, uint32 ActiveParticlesEstimate)
	{
		const bool bAllowGPUExec = EnumHasAnyFlags(EmitterData->FeatureMask, ENiagaraStatelessFeatureMask::ExecuteGPU);
		const bool bUseCPUExec = EnumHasAnyFlags(EmitterData->FeatureMask, ENiagaraStatelessFeatureMask::ExecuteCPU) && (!bAllowGPUExec || (ActiveParticlesEstimate <= uint32(GParticleCountCPUThreshold)));
		if (bUseCPUExec)
		{
			return EComputeExecutionPath::CPU;
		}
		if (bAllowGPUExec)
		{
			return EComputeExecutionPath::GPU;
		}
		return EComputeExecutionPath::None;
	}

	void GenerateGPUData(FRHICommandList& RHICmdList, FNiagaraGpuComputeDispatchInterface* ComputeInterface, TConstArrayView<FNiagaraStatelessComputeManager::FStatelessDataCache*> DataToGenerate)
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

			for (FNiagaraStatelessComputeManager::FStatelessDataCache* CacheData : DataToGenerate)
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
		for (FNiagaraStatelessComputeManager::FStatelessDataCache* CacheData : DataToGenerate)
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
			ShaderParameters->Common_SimulationTime = EmitterInstance->Age;
			ShaderParameters->Common_SimulationDeltaTime = EmitterInstance->DeltaTime;
			ShaderParameters->Common_SimulationInvDeltaTime = EmitterInstance->DeltaTime > 0.0f ? (1.0f / EmitterInstance->DeltaTime) : 0.0f;
			ShaderParameters->Common_OutputBufferStride = CacheData->DataBuffer->GetFloatStride() / sizeof(float);
			ShaderParameters->Common_GPUCountBufferOffset = CacheData->DataBuffer->GetGPUInstanceCountBufferOffset();
			ShaderParameters->Common_FloatOutputBuffer = CacheData->DataBuffer->GetGPUBufferFloat().UAV;
			//ShaderParameters->Common_HalfOutputBuffer		= CacheData->DataBuffer->GetGPUBufferHalf().UAV;
			ShaderParameters->Common_IntOutputBuffer = CacheData->DataBuffer->GetGPUBufferInt().UAV.IsValid() ? CacheData->DataBuffer->GetGPUBufferInt().UAV.GetReference() : EmptyIntBufferUAV;
			ShaderParameters->Common_GPUCountBuffer = CountBufferUAV;
			ShaderParameters->Common_StaticFloatBuffer = EmitterData->StaticFloatBuffer.SRV;
			ShaderParameters->Common_ParameterBuffer = FNiagaraRenderer::GetSrvOrDefaultUInt(EmitterInstance->BindingBuffer.SRV);

			// Execute the simulation
			TShaderRef<NiagaraStateless::FSimulationShader> ComputeShader = EmitterData->GetShader();
			FRHIComputeShader* ComputeShaderRHI = ComputeShader.GetComputeShader();
			const uint32 NumThreadGroups = FMath::DivideAndRoundUp<uint32>(CacheData->ActiveParticles, NiagaraStateless::FSimulationShader::ThreadGroupSize);

			const FIntVector NumWrappedThreadGroups = FComputeShaderUtils::GetGroupCountWrapped(NumThreadGroups);
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, EmitterData->GetShaderParametersMetadata(), *ShaderParameters, NumWrappedThreadGroups);
		}
		RHICmdList.EndUAVOverlap(CountBufferUAV);

		RHICmdList.Transition(TransitionsAfter);
	}
}

FNiagaraStatelessComputeManager::FNiagaraStatelessComputeManager(FNiagaraGpuComputeDispatchInterface* InOwnerInterface)
	: FNiagaraGpuComputeDataManager(InOwnerInterface)
{
	InOwnerInterface->GetOnPreRenderEvent().AddRaw(this, &FNiagaraStatelessComputeManager::OnPostPreRender);
	InOwnerInterface->GetOnPostRenderEvent().AddRaw(this, &FNiagaraStatelessComputeManager::OnPostPostRender);
}

FNiagaraStatelessComputeManager::~FNiagaraStatelessComputeManager()
{
}

FNiagaraDataBuffer* FNiagaraStatelessComputeManager::GetDataBuffer(FRHICommandListBase& RHICmdList, uintptr_t EmitterKey, const NiagaraStateless::FEmitterInstance_RT* EmitterInstance)
{
	using namespace NiagaraStateless;
	using namespace NiagaraStatelessComputeManagerPrivate;
	
	//-OPT: This lock is very conservative, ideally we only have it around the relevant parts
	UE::TScopeLock ScopeLock(GetDataBufferGuard);

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

	const uint32 DataSetLayoutHash = EmitterInstance->EmitterData->ParticleDataSetCompiledData->GetLayoutHash();

	FStatelessDataCache* CacheData = nullptr;
	if (GUseDataBufferCache)
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
		CacheData->DataSetCompiledData = EmitterInstance->EmitterData->ParticleDataSetCompiledData;
		CacheData->DataSet.Init(CacheData->DataSetCompiledData.Get());
		CacheData->DataBuffer = new FNiagaraDataBuffer(&CacheData->DataSet);
	}

	CacheData->EmitterInstance = EmitterInstance;
	CacheData->ActiveParticles = ActiveParticles;

	CacheData->DataBuffer->AllocateGPU(RHICmdList, CacheData->ActiveParticles, ComputeInterface->GetFeatureLevel(), TEXT("StatelessSimBuffer"));

	const EComputeExecutionPath ComputeExecutionPath = DetermineComputeExecutionPath(EmitterData, ActiveParticles);
	switch (ComputeExecutionPath)
	{
		case EComputeExecutionPath::CPU:
		{
			FParticleSimulationContext ParticleSimulation(EmitterData, EmitterInstance->BindingBufferData.Get(TArray<uint8>()));
			ParticleSimulation.SimulateGPU(RHICmdList, EmitterInstance->RandomSeed, EmitterInstance->Age, EmitterInstance->DeltaTime, EmitterInstance->SpawnInfos, CacheData->DataBuffer);
			if (ParticleSimulation.GetNumInstances() == 0)
			{
				FreeData.Emplace(CacheData);
				return nullptr;
			}
			break;
		}

		case EComputeExecutionPath::GPU:
		{
			CacheData->DataBuffer->SetNumInstances(CacheData->ActiveParticles);

			FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
			const uint32 CountOffset = CountManager.AllocateDeferredEntry();
			CacheData->DataBuffer->SetGPUInstanceCountBufferOffset(CountOffset);
			CountsToRelease.Add(CountOffset);

			GPUDataToGenerate.Add(CacheData);
			break;
		}

		default:
			ensureMsgf(false, TEXT("No execution path was found for stateless emitter, data will not be generated"));
			FreeData.Emplace(CacheData);
			return nullptr;
	}

	UsedData.Emplace(EmitterKey, CacheData);
	return CacheData->DataBuffer;
}

void FNiagaraStatelessComputeManager::GenerateDataBufferForDebugging(FRHICommandListImmediate& RHICmdList, FNiagaraDataBuffer* DataBuffer, const NiagaraStateless::FEmitterInstance_RT* EmitterInstance) const
{
	using namespace NiagaraStateless;
	using namespace NiagaraStatelessComputeManagerPrivate;

	check(IsInRenderingThread());

	const FNiagaraStatelessEmitterData* EmitterData = EmitterInstance->EmitterData.Get();
	const uint32 ActiveParticlesEstimate = EmitterData->CalculateActiveParticles(
		EmitterInstance->RandomSeed,
		EmitterInstance->SpawnInfos,
		EmitterInstance->Age,
		&EmitterInstance->ShaderParameters->SpawnParameters
	);

	if (ActiveParticlesEstimate == 0)
	{
		DataBuffer->SetNumInstances(0);
		return;
	}

	const EComputeExecutionPath ComputeExecutionPath = DetermineComputeExecutionPath(EmitterData, ActiveParticlesEstimate);
	switch (ComputeExecutionPath)
	{
		case EComputeExecutionPath::CPU:
		{
			FParticleSimulationContext ParticleSimulation(EmitterData, EmitterInstance->BindingBufferData.Get(TArray<uint8>()));
			ParticleSimulation.Simulate(EmitterInstance->RandomSeed, EmitterInstance->Age, EmitterInstance->DeltaTime, EmitterInstance->SpawnInfos, DataBuffer);
			break;
		}

		case EComputeExecutionPath::GPU:
		{
			FNiagaraGpuComputeDispatchInterface* ComputeInterface = GetOwnerInterface();
			FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();

			// Allocate counter and destination data
			FNiagaraDataBufferRef GPUDataBuffer = new FNiagaraDataBuffer(DataBuffer->GetOwner());

			uint32 CountIndex = CountManager.AcquireOrAllocateEntry(RHICmdList);
			GPUDataBuffer->AllocateGPU(RHICmdList, ActiveParticlesEstimate, ComputeInterface->GetFeatureLevel(), TEXT("StatelessSimBuffer"));
			GPUDataBuffer->SetGPUInstanceCountBufferOffset(CountIndex);

			// Generate the data
			FStatelessDataCache DataCache;
			DataCache.DataBuffer		= GPUDataBuffer;
			DataCache.EmitterInstance	= EmitterInstance;
			DataCache.ActiveParticles	= ActiveParticlesEstimate;

			FStatelessDataCache* DataCachePtr = &DataCache;
			GenerateGPUData(RHICmdList, ComputeInterface, MakeConstArrayView(&DataCachePtr, 1));

			// Copy to CPU data
			//TransferGPUToCPU(RHICmdList, ComputeInterface, GPUDataBuffer, DataBuffer);
			GPUDataBuffer->TransferGPUToCPUImmediate(RHICmdList, ComputeInterface, DataBuffer);

			// Release the GPU buffer and count
			GPUDataBuffer->ReleaseGPU();
			GPUDataBuffer->SetGPUInstanceCountBufferOffset(INDEX_NONE);
			CountManager.FreeEntry(CountIndex);
			break;
		}

		default:
			break;
	}
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

	// Ensure we allocate any deferred counts that we need
	FNiagaraGpuComputeDispatchInterface* ComputeInterface = GetOwnerInterface();
	{
		FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
		CountManager.AllocateDeferredCounts(GraphBuilder.RHICmdList);
	}

	// Execute dispatches
	AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("FNiagaraStatelessComputeManager::OnPostPreRender"),
		[DataToGenerate=MoveTemp(GPUDataToGenerate), ComputeInterface](FRHICommandListImmediate& RHICmdList)
		{
			SCOPED_DRAW_EVENT(RHICmdList, FNiagaraStatelessComputeManager_OnPostPreRender);

			FNiagaraGPUInstanceCountManager& CountManager = ComputeInterface->GetGPUInstanceCounterManager();
			NiagaraStatelessComputeManagerPrivate::GenerateGPUData(RHICmdList, ComputeInterface, DataToGenerate);
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
