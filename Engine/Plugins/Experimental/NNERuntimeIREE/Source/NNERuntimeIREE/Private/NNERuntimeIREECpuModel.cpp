// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREECpuModel.h"

#ifdef WITH_NNE_RUNTIME_IREE

namespace UE::NNERuntimeIREECpu
{
	FModelInstance::FModelInstance(TSharedPtr<UE::NNERuntimeIREE::FIREEModule> IREEModule, TSharedPtr<UE::NNERuntimeIREE::FIREESessionCPU> IREESession)
	{
		check(IREEModule.IsValid());
		check(IREESession.IsValid());
		
		Module = IREEModule;
		Session = IREESession;
	}

	TConstArrayView<UE::NNE::FTensorDesc> FModelInstance::GetInputTensorDescs() const
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Module->GetInputTensorDescs();
	}

	TConstArrayView<UE::NNE::FTensorDesc> FModelInstance::GetOutputTensorDescs() const
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Module->GetOutputTensorDescs();
	}

	TConstArrayView<UE::NNE::FTensorShape> FModelInstance::GetInputTensorShapes() const
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Session->GetInputTensorShapes();
	}

	TConstArrayView<UE::NNE::FTensorShape> FModelInstance::GetOutputTensorShapes() const
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Session->GetOutputTensorShapes();
	}

	int32 FModelInstance::SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes)
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Session->SetInputTensorShapes(InInputShapes);
	}

	int32 FModelInstance::RunSync(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<NNE::FTensorBindingCPU> InOutputBindings)
	{
		check(Module.IsValid());
		check(Session.IsValid());
		return Session->RunSyncCPU(InInputBindings, InOutputBindings);
	}

	bool FModel::Init(const FString& DirPath, const FString& SharedLibraryFileName, const FString& VmfbFileName, const FString& LibraryQueryFunctionName, const UE::NNERuntimeIREE::FModuleMetaData& ModuleMetaData)
	{
		check(!Module.IsValid());
		check(!SharedLibraryFileName.IsEmpty());
		check(!VmfbFileName.IsEmpty());
		check(!LibraryQueryFunctionName.IsEmpty());

		Module = UE::NNERuntimeIREE::FIREEModule::MakeModule(DirPath, VmfbFileName, ModuleMetaData);
		if (!Module.IsValid())
		{
			return false;
		}

		TSharedPtr<UE::NNERuntimeIREE::FIREELibrary> Library = UE::NNERuntimeIREE::FIREELibrary::MakeLibrary(DirPath, SharedLibraryFileName);
		if (!Library.IsValid())
		{
			Module.Reset();
			return false;
		}

		Device = UE::NNERuntimeIREE::FIREELibrary::MakeDeviceCPU(Library, LibraryQueryFunctionName);
		if (!Device.IsValid())
		{
			Library.Reset();
			Module.Reset();
			return false;
		}

		return true;
	}

	TSharedPtr<UE::NNE::IModelInstanceCPU> FModel::CreateModelInstanceCPU()
	{
		check(Module.IsValid());
		check(Device.IsValid());

		TSharedPtr<UE::NNERuntimeIREE::FIREESessionCPU> Session = UE::NNERuntimeIREE::FIREEDeviceCPU::MakeSessionCPU(Device);
		if (!Session.IsValid())
		{
			return TSharedPtr<UE::NNE::IModelInstanceCPU>();
		}

		if (!Session->AppendModule(Module))
		{
			return TSharedPtr<UE::NNE::IModelInstanceCPU>();
		}

		return MakeShared<FModelInstance>(Module, Session);
	}
} // UE::NNERuntimeIREECpu

#endif // WITH_NNE_RUNTIME_IREE