// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_NNE_RUNTIME_IREE

#include "NNERuntimeCPU.h"
#include "NNERuntimeIREECommon.h"
#include "NNERuntimeIREEMetaData.h"

namespace UE::NNERuntimeIREECpu
{
	class FModelInstance : public UE::NNE::IModelInstanceCPU
	{
	public:
		FModelInstance(TSharedPtr<UE::NNERuntimeIREE::FIREEModule> IREEModule, TSharedPtr<UE::NNERuntimeIREE::FIREESessionCPU> IREESession);
		virtual ~FModelInstance() = default;

		//~ Begin IModelInstanceCPU Interface
		virtual TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const override;
		virtual TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const override;
		virtual TConstArrayView<UE::NNE::FTensorShape> GetInputTensorShapes() const override;
		virtual TConstArrayView<UE::NNE::FTensorShape> GetOutputTensorShapes() const override;
		virtual int32 SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes) override;
		virtual int32 RunSync(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings) override;
		//~ End IModelInstanceCPU Interface

	private:
		TSharedPtr<UE::NNERuntimeIREE::FIREEModule> Module;
		TSharedPtr<UE::NNERuntimeIREE::FIREESessionCPU> Session;
	};

	class FModel : public UE::NNE::IModelCPU
	{
	public:
		FModel() {};
		virtual ~FModel() = default;

		bool Init(const FString& DirPath, const FString& SharedLibraryFileName, const FString& VmfbFileName, const FString& LibraryQueryFunctionName, const UE::NNERuntimeIREE::FModuleMetaData& ModuleMetaData);

		//~ Begin IModelCPU Interface
		virtual TSharedPtr<UE::NNE::IModelInstanceCPU> CreateModelInstanceCPU() override;
		//~ End IModelCPU Interface

	private:
		TSharedPtr<UE::NNERuntimeIREE::FIREEModule> Module;
		TSharedPtr<UE::NNERuntimeIREE::FIREEDeviceCPU> Device;
	};	
} // UE::NNERuntimeIREECpu

#endif // WITH_NNE_RUNTIME_IREE