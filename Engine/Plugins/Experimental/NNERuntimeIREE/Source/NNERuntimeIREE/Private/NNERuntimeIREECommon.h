// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_NNE_RUNTIME_IREE

#include "CoreMinimal.h"
#include "NNEModelData.h"
#include "NNERuntimeIREECpu.h"
#include "NNERuntimeIREEMetaData.h"
#include "NNETypes.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include "Microsoft/AllowMicrosoftPlatformAtomics.h"
#endif // PLATFORM_MICROSOFT
THIRD_PARTY_INCLUDES_START
#include "iree/hal/buffer.h"
#include "iree/hal/channel.h"
#include "iree/runtime/call.h"
#include "iree/runtime/session.h"
#include "iree/vm/module.h"
THIRD_PARTY_INCLUDES_END
#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformAtomics.h"
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif // PLATFORM_MICROSOF

namespace UE::NNERuntimeIREE
{
	class FIREEInstance;
	class FIREELibrary;

	class FIREEModule
	{
	public:
		FIREEModule();
		~FIREEModule();

		static TSharedPtr<FIREEModule> MakeModule(const FString& DirPath, const FString& VmfbFileName, const UNNERuntimeIREEModuleMetaData& ModuleMetaData);
		bool AppendToSession(iree_runtime_session_t* Session);

		iree_vm_function_t GetMainFunction();

		TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const;
		TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const;

	private:
		TSharedPtr<FIREEInstance> Instance;
		TSharedPtr<UE::NNE::FSharedModelData> ModelData;
		iree_vm_module_t* Module;
		iree_vm_function_t MainFunction;
		TArray<UE::NNE::FTensorDesc> InputTensorDescs;
		TArray<UE::NNE::FTensorDesc> OutputTensorDescs;
	};

	class FIREEDevice
	{
	public:
		FIREEDevice(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREELibrary> IREELibrary, iree_hal_device_t* IREEDevice);
		virtual ~FIREEDevice();

		iree_hal_allocator_t* GetDeviceAllocator();

	protected:
		TSharedPtr<FIREEInstance> Instance;
		TSharedPtr<FIREELibrary> Library;
		iree_hal_device_t* Device;
	};

	class FIREESession
	{
	public:
		FIREESession(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREEDevice> IREEDevice, iree_runtime_session_t* IREESession);
		virtual ~FIREESession();

		bool AppendModule(TSharedPtr<FIREEModule> IREEModule);

		int32 SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes);

		TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const;
		TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const;

	protected:
		TSharedPtr<FIREEInstance> Instance;
		TSharedPtr<FIREEDevice> Device;
		TSharedPtr<FIREEModule> Module;
		iree_runtime_session_t* Session;
		iree_runtime_call_t Call;
		TArray<UE::NNE::FTensorShape> InputTensorShapes;
		TArray<UE::NNE::FTensorShape> OutputTensorShapes;
	};

	class FIREESessionCPU : public FIREESession
	{
	public:
		FIREESessionCPU(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREEDevice> IREEDevice, iree_runtime_session_t* IREESession);
		virtual ~FIREESessionCPU() = default;

		int32 RunSyncCPU(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings);
	};

	class FIREEDeviceCPU : public FIREEDevice
	{
	public:
		FIREEDeviceCPU(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREELibrary> IREELibrary, iree_hal_device_t* IREEDevice);
		virtual ~FIREEDeviceCPU() = default;

		static TSharedPtr<FIREESessionCPU> MakeSessionCPU(TSharedPtr<FIREEDeviceCPU> IREEDevice);
	};

	class FIREELibrary
	{
	public:
		FIREELibrary();
		~FIREELibrary();

		static TSharedPtr<FIREELibrary> MakeLibrary(const FString& LibraryPath, const FString& LibraryName);
		static TSharedPtr<FIREEDeviceCPU> MakeDeviceCPU(TSharedPtr<FIREELibrary> Library, const FString& LibraryQueryFunctionName);

		FString GetLibraryName();

	private:
		FString LibraryName;
		TSharedPtr<FIREEInstance> Instance;
		void* Library;
		TMap<FString, TWeakPtr<FIREEDeviceCPU>> Devices;
	};
} // UE::NNERuntimeIREE

#endif // WITH_NNE_RUNTIME_IREE