// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREECommon.h"

#ifdef WITH_NNE_RUNTIME_IREE

#include "iree/hal/drivers/local_sync/sync_device.h"
#include "iree/hal/local/loaders/static_library_loader.h"
#include "iree/modules/hal/types.h"
#include "iree/vm/bytecode/module.h"

namespace UE::NNERuntimeIREE
{
	namespace Private
	{
		ENNETensorDataType GetTypeFromString(const FString& TypeString)
		{
			if (TypeString.StartsWith("char"))
			{
				return ENNETensorDataType::Char;
			}
			if (TypeString.StartsWith("bool") || TypeString.StartsWith("i1"))
			{
				return ENNETensorDataType::Boolean;
			}
			else if (TypeString.StartsWith("half"))
			{
				return ENNETensorDataType::Half;
			}
			else if (TypeString.StartsWith("f"))
			{
				if (TypeString.StartsWith("f16"))
				{
					return ENNETensorDataType::Half;
				}
				else if (TypeString.StartsWith("float") || TypeString.StartsWith("f32"))
				{
					return ENNETensorDataType::Float;
				}
				else if (TypeString.StartsWith("f64"))
				{
					return ENNETensorDataType::Double;
				}
			}
			else if (TypeString.StartsWith("double"))
			{
				return ENNETensorDataType::Double;
			}
			else if (TypeString.StartsWith("i") || TypeString.StartsWith("si"))
			{
				if (TypeString.EndsWith("i8"))
				{
					return ENNETensorDataType::Int8;
				}
				else if (TypeString.EndsWith("i16"))
				{
					return ENNETensorDataType::Int16;
				}
				else if (TypeString.EndsWith("i32") || TypeString.EndsWith("int"))
				{
					return ENNETensorDataType::Int32;
				}
				else if (TypeString.EndsWith("i64"))
				{
					return ENNETensorDataType::Int64;
				}
			}
			else if (TypeString.StartsWith("ui"))
			{
				if (TypeString.EndsWith("i8"))
				{
					return ENNETensorDataType::UInt8;
				}
				else if (TypeString.EndsWith("i16"))
				{
					return ENNETensorDataType::UInt16;
				}
				else if (TypeString.EndsWith("i32"))
				{
					return ENNETensorDataType::UInt32;
				}
				else if (TypeString.EndsWith("i64"))
				{
					return ENNETensorDataType::UInt64;
				}
			}
			return ENNETensorDataType::None;
		}

		iree_hal_element_types_t NNEToIREEType(ENNETensorDataType Type)
		{
			switch (Type)
			{
				case ENNETensorDataType::None:
					return IREE_HAL_ELEMENT_TYPE_NONE;
					break;
				case ENNETensorDataType::Char:
					return IREE_HAL_ELEMENT_TYPE_UINT_8;
					break;
				case ENNETensorDataType::Boolean:
					return IREE_HAL_ELEMENT_TYPE_BOOL_8;
					break;
				case ENNETensorDataType::Half:
					return IREE_HAL_ELEMENT_TYPE_FLOAT_16;
					break;
				case ENNETensorDataType::Float:
					return IREE_HAL_ELEMENT_TYPE_FLOAT_32;
					break;
				case ENNETensorDataType::Double:
					return IREE_HAL_ELEMENT_TYPE_FLOAT_64;
					break;
				case ENNETensorDataType::Int8:
					return IREE_HAL_ELEMENT_TYPE_INT_8;
					break;
				case ENNETensorDataType::Int16:
					return IREE_HAL_ELEMENT_TYPE_INT_16;
					break;
				case ENNETensorDataType::Int32:
					return IREE_HAL_ELEMENT_TYPE_INT_32;
					break;
				case ENNETensorDataType::Int64:
					return IREE_HAL_ELEMENT_TYPE_INT_64;
					break;
				case ENNETensorDataType::UInt8:
					return IREE_HAL_ELEMENT_TYPE_UINT_8;
					break;
				case ENNETensorDataType::UInt16:
					return IREE_HAL_ELEMENT_TYPE_UINT_16;
					break;
				case ENNETensorDataType::UInt32:
					return IREE_HAL_ELEMENT_TYPE_UINT_32;
					break;
				case ENNETensorDataType::UInt64:
					return IREE_HAL_ELEMENT_TYPE_UINT_64;
					break;
				case ENNETensorDataType::Complex64:
					return IREE_HAL_ELEMENT_TYPE_COMPLEX_FLOAT_64;
					break;
				case ENNETensorDataType::Complex128:
					return IREE_HAL_ELEMENT_TYPE_COMPLEX_FLOAT_128;
					break;
				case ENNETensorDataType::BFloat16:
					return IREE_HAL_ELEMENT_TYPE_BFLOAT_16;
					break;
				default:
					return IREE_HAL_ELEMENT_TYPE_NONE;
					break;
			}
		}
	} // Private

	class FIREEInstance
	{
	public:
		FIREEInstance(iree_runtime_instance_t* InInstance);
		~FIREEInstance();

	public:
		bool CreateModule(TConstArrayView<uint8> VmfbDataView, iree_vm_module_t** Module);
		bool CreateSyncDevice(void* LibraryQueryFuntionPointer, iree_hal_device_t** Device);
		bool CreateSession(iree_hal_device_t* Device, iree_runtime_session_t** Session);

		iree_allocator_t GetHostAllocator();

		TSharedPtr<FIREELibrary> GetLibrary(const FString& Key);
		void SetLibrary(const FString& Key, TSharedPtr<FIREELibrary> Library);

		TSharedPtr<UE::NNE::FSharedModelData> GetModelData(const FString& DirPath, const FString& VmfbFileName);

	public:
		static TSharedPtr<FIREEInstance> GetInstance();

	private:
		static TWeakPtr<FIREEInstance> WeakInstancePtr;
		static FCriticalSection CriticalSection;
		iree_runtime_instance_t* Instance;
		TMap<FString, TWeakPtr<FIREELibrary>> Libraries;
		TMap<FString, TWeakPtr<UE::NNE::FSharedModelData>> ModelData;
	};

	TWeakPtr<FIREEInstance> FIREEInstance::WeakInstancePtr;
	FCriticalSection FIREEInstance::CriticalSection;

	FIREEInstance::FIREEInstance(iree_runtime_instance_t* InInstance)
	{
		check(InInstance);
		Instance = InInstance;
	}

	FIREEInstance::~FIREEInstance()
	{
		iree_runtime_instance_release(Instance);
	}

	void PrintIREEError(FString Message, iree_status_t Status)
	{
		iree_host_size_t TrueLength = 0;
		iree_status_format(Status, 0, (char*)nullptr, &TrueLength);
		void* ErrorString = FMemory::Malloc(TrueLength + 1);
		((char*)ErrorString)[TrueLength] = (char)0;
		iree_status_format(Status, TrueLength, (char*)ErrorString, &TrueLength);
		UE_LOG(LogTemp, Error, TEXT("%s: %s"), *Message, *FString(ANSI_TO_TCHAR(ErrorString)));
		FMemory::Free(ErrorString);
	}

	bool FIREEInstance::CreateModule(TConstArrayView<uint8> VmfbDataView, iree_vm_module_t** Module)
	{
		check(!VmfbDataView.IsEmpty());
		check(Module);

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		const iree_const_byte_span_t ModuleData = iree_make_const_byte_span(VmfbDataView.GetData(), VmfbDataView.Num());

		iree_vm_module_t* TempModule = nullptr;
		Status = iree_vm_bytecode_module_create(iree_runtime_instance_vm_instance(Instance), ModuleData, iree_allocator_null(), GetHostAllocator(), &TempModule);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the module", Status);

			if (TempModule)
			{
				iree_vm_module_release(TempModule);
			}

			iree_status_free(Status);
			return false;
		}

		*Module = TempModule;

		iree_status_free(Status);
		return true;
	}

	bool FIREEInstance::CreateSyncDevice(void* LibraryQueryFuntionPointer, iree_hal_device_t** Device)
	{
		check(LibraryQueryFuntionPointer);
		check(Device);

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		iree_allocator_t HostAllocator = GetHostAllocator();

		iree_hal_executable_loader_t* LibraryLoader = nullptr;
		const iree_hal_executable_library_query_fn_t LibraryList[] = { (iree_hal_executable_library_query_fn_t)LibraryQueryFuntionPointer };
		Status = iree_hal_static_library_loader_create(IREE_ARRAYSIZE(LibraryList), LibraryList, iree_hal_executable_import_provider_null(), HostAllocator, &LibraryLoader);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the library loader", Status);

			if (LibraryLoader)
			{
				iree_hal_executable_loader_release(LibraryLoader);
			}

			iree_status_free(Status);
			return false;
		}

		iree_hal_allocator_t* DeviceAllocator = nullptr;
		iree_string_view_t Identifier = iree_make_cstring_view("sync");
		Status = iree_hal_allocator_create_heap(Identifier, HostAllocator, HostAllocator, &DeviceAllocator);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the device allocator", Status);
			
			if (DeviceAllocator)
			{
				iree_hal_allocator_release(DeviceAllocator);
			}

			iree_hal_executable_loader_release(LibraryLoader);
			iree_status_free(Status);
			return false;
		}

		iree_hal_device_t* TempDevice = nullptr;
		iree_hal_sync_device_params_t DeviceParams;
		iree_hal_sync_device_params_initialize(&DeviceParams);
		Status = iree_hal_sync_device_create(Identifier, &DeviceParams, 1, &LibraryLoader, DeviceAllocator, HostAllocator, &TempDevice);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the device", Status);

			if (TempDevice)
			{
				iree_hal_device_release(TempDevice);
			}

			iree_hal_allocator_release(DeviceAllocator);
			iree_hal_executable_loader_release(LibraryLoader);
			iree_status_free(Status);
			return false;
		}

		iree_hal_allocator_release(DeviceAllocator);
		iree_hal_executable_loader_release(LibraryLoader);

		*Device = TempDevice;

		iree_status_free(Status);
		return true;
	}

	bool FIREEInstance::CreateSession(iree_hal_device_t* Device, iree_runtime_session_t** Session)
	{
		check(Device);
		check(Session);

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		iree_runtime_session_options_t SessionOptions;
		iree_runtime_session_options_initialize(&SessionOptions);

		iree_runtime_session_t* TempSession = nullptr;
		Status = iree_runtime_session_create_with_device(Instance, &SessionOptions, Device, GetHostAllocator(), &TempSession);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the session", Status);

			if (TempSession)
			{
				iree_runtime_session_release(TempSession);
			}

			iree_status_free(Status);
			return false;
		}

		*Session = TempSession;

		iree_status_free(Status);
		return true;
	}

	iree_allocator_t FIREEInstance::GetHostAllocator()
	{
		return iree_runtime_instance_host_allocator(Instance);
	}

	TSharedPtr<FIREELibrary> FIREEInstance::GetLibrary(const FString& Key)
	{
		check(!Key.IsEmpty());
		if (Libraries.Contains(Key))
		{
			return Libraries[Key].Pin();
		}
		return TSharedPtr<FIREELibrary>();
	}

	void FIREEInstance::SetLibrary(const FString& Key, TSharedPtr<FIREELibrary> Library)
	{
		check(!Key.IsEmpty());
		check(Library.IsValid());
		check(!Libraries.Contains(Key) || !Libraries[Key].IsValid());
		Libraries.Emplace(Key, Library);
	}

	TSharedPtr<UE::NNE::FSharedModelData> FIREEInstance::GetModelData(const FString& DirPath, const FString& VmfbFileName)
	{
		check(!VmfbFileName.IsEmpty());

		FString FilePath = FPaths::Combine(DirPath, VmfbFileName);
		if (ModelData.Contains(FilePath) && ModelData[FilePath].IsValid())
		{
			return ModelData[FilePath].Pin();
		}

		TUniquePtr<FArchive> Reader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*FilePath, 0));
		if (!Reader)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to open the vmfb data file '%s'"), *FilePath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		int64 DataSize = Reader->TotalSize();
		if (DataSize < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Vmfb data file '%s' is empty"), *FilePath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		void* Data = FMemory::Malloc(DataSize, IREE_HAL_HEAP_BUFFER_ALIGNMENT);
		Reader->Serialize(Data, DataSize);

		TSharedPtr<UE::NNE::FSharedModelData> Result = MakeShared<UE::NNE::FSharedModelData>(FSharedBuffer::TakeOwnership(Data, DataSize, FMemory::Free), IREE_HAL_HEAP_BUFFER_ALIGNMENT);
		ModelData.Emplace(FilePath, Result);

		return Result;
	}

	TSharedPtr<FIREEInstance> FIREEInstance::GetInstance()
	{
		FScopeLock ScopeLock(&FIREEInstance::CriticalSection);

		if (WeakInstancePtr.IsValid())
		{
			return WeakInstancePtr.Pin();
		}

		iree_status_t Status = iree_ok_status();
		if (!iree_status_is_ok(Status))
		{
			iree_status_free(Status);
			return TSharedPtr<FIREEInstance>();
		}

		iree_runtime_instance_options_t InstanceOptions;
		iree_runtime_instance_options_initialize(&InstanceOptions);
		iree_runtime_instance_options_use_all_available_drivers(&InstanceOptions);

		iree_runtime_instance_t* TempInstance = nullptr;
		Status = iree_runtime_instance_create(&InstanceOptions, iree_allocator_system(), &TempInstance);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to create the instance", Status);

			if (TempInstance)
			{
				iree_runtime_instance_release(TempInstance);
			}

			iree_status_free(Status);
			return TSharedPtr<FIREEInstance>();
		}
		iree_status_free(Status);

		TSharedPtr<FIREEInstance> SharedInstance = MakeShared<FIREEInstance>(TempInstance);
		WeakInstancePtr = SharedInstance;
		return SharedInstance;
	}

	FIREEModule::FIREEModule()
	{
		Module = nullptr;
		MainFunction = {};
	}

	FIREEModule::~FIREEModule()
	{
		if (Module)
		{
			iree_vm_module_release(Module);
		}
		ModelData.Reset();
		Instance.Reset();
	}

	TSharedPtr<FIREEModule> FIREEModule::MakeModule(const FString& DirPath, const FString& VmfbFileName, const UE::NNERuntimeIREE::FModuleMetaData& ModuleMetaData)
	{
		check(!VmfbFileName.IsEmpty());

		TSharedPtr<FIREEInstance> Instance = FIREEInstance::GetInstance();
		if (!Instance.IsValid())
		{
			return TSharedPtr<FIREEModule>();
		}

		TSharedPtr<UE::NNE::FSharedModelData> ModelData = Instance->GetModelData(DirPath, VmfbFileName);
		if (!ModelData.IsValid())
		{
			return TSharedPtr<FIREEModule>();
		}

		iree_vm_module_t* TempModule = nullptr;
		if (!Instance->CreateModule(ModelData->GetView(), &TempModule))
		{
			return TSharedPtr<FIREEModule>();
		}

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		iree_vm_function_t LocalMainFunction = {0};
		TArray<UE::NNE::FTensorDesc> InputTensorDescs;
		TArray<UE::NNE::FTensorDesc> OutputTensorDescs;
		int32 NumMainFunctionCandidates = 0;
		int32 Ordinal = 0;
		while (true)
		{
			iree_vm_function_t MainFunctionCandidate;
			Status = iree_vm_module_lookup_function_by_ordinal(TempModule, IREE_VM_FUNCTION_LINKAGE_EXPORT, Ordinal, &MainFunctionCandidate);
			if (!iree_status_is_ok(Status) || iree_vm_function_is_null(MainFunctionCandidate))
			{
				Ordinal = -1;
				break;
			}
			Ordinal++;

			iree_host_size_t NumInputs = 0;
			iree_host_size_t NumOutputs = 0;
			iree_vm_function_signature_t Signature = iree_vm_function_signature(&MainFunctionCandidate);
			Status = iree_vm_function_call_count_arguments_and_results(&Signature, &NumInputs, &NumOutputs);
			if (iree_status_is_ok(Status) && NumInputs > 0 && NumOutputs > 0)
			{
				if (NumMainFunctionCandidates == 0)
				{
					iree_string_view_t FunctionName = iree_vm_function_name(&MainFunctionCandidate);
					FString FunctionNameString = "";
					FunctionNameString.Append(FunctionName.data, FunctionName.size);
					if (ModuleMetaData.FunctionMetaData.Contains(FunctionNameString))
					{
						FFunctionMetaData MetaData = ModuleMetaData.FunctionMetaData[FunctionNameString];
						if (MetaData.ArgumentMetaData.Num() == NumInputs && MetaData.ResultMetaData.Num() == NumOutputs)
						{
							LocalMainFunction = MainFunctionCandidate;

							for (int32 i = 0; i < NumInputs; i++)
							{
								ENNETensorDataType DataType = UE::NNERuntimeIREE::Private::GetTypeFromString(MetaData.ArgumentMetaData[i].Type);
								if (DataType == ENNETensorDataType::None)
								{
									UE_LOG(LogTemp, Error, TEXT("Unknown argument type in function %s: %s"), *FunctionNameString, *MetaData.ArgumentMetaData[i].Type);
									iree_status_free(Status);
									return TSharedPtr<FIREEModule>();
								}
								InputTensorDescs.Add(UE::NNE::FTensorDesc::Make(MetaData.ArgumentMetaData[i].Name, UE::NNE::FSymbolicTensorShape::Make(MetaData.ArgumentMetaData[i].Shape), DataType));
							}

							for (int32 i = 0; i < NumOutputs; i++)
							{
								ENNETensorDataType DataType = UE::NNERuntimeIREE::Private::GetTypeFromString(MetaData.ResultMetaData[i].Type);
								if (DataType == ENNETensorDataType::None)
								{
									UE_LOG(LogTemp, Error, TEXT("Unknown result type in function %s: %s"), *FunctionNameString, *MetaData.ResultMetaData[i].Type);
									iree_status_free(Status);
									return TSharedPtr<FIREEModule>();
								}
								OutputTensorDescs.Add(UE::NNE::FTensorDesc::Make(MetaData.ResultMetaData[i].Name, UE::NNE::FSymbolicTensorShape::Make(MetaData.ResultMetaData[i].Shape), DataType));
							}
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("Input and output count mismatch in function %s"), *FunctionNameString);
							iree_status_free(Status);
							return TSharedPtr<FIREEModule>();
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to find meta data for function %s"), *FunctionNameString);
						iree_status_free(Status);
						return TSharedPtr<FIREEModule>();
					}
				}
				NumMainFunctionCandidates++;
			}
		}
		if (NumMainFunctionCandidates < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to find a suitable module main function"));
			iree_status_free(Status);
			return TSharedPtr<FIREEModule>();
		}
		if (NumMainFunctionCandidates > 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Found multiple suitable module main functions"));
		}

		TSharedPtr<FIREEModule> Result = MakeShared<FIREEModule>();
		Result->Instance = Instance;
		Result->ModelData = ModelData;
		Result->Module = TempModule;
		Result->MainFunction = LocalMainFunction;
		Result->InputTensorDescs = InputTensorDescs;
		Result->OutputTensorDescs = OutputTensorDescs;

		iree_status_free(Status);
		return Result;
	}

	bool FIREEModule::AppendToSession(iree_runtime_session_t* Session)
	{
		check(Module);
		check(Session);

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		Status = iree_runtime_session_append_module(Session, Module);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to append the module to the session", Status);
			iree_status_free(Status);
			return false;
		}

		iree_status_free(Status);
		return true;
	}

	iree_vm_function_t FIREEModule::GetMainFunction()
	{
		check(Module);
		check(!iree_vm_function_is_null(MainFunction));
		return MainFunction;
	}

	TConstArrayView<UE::NNE::FTensorDesc> FIREEModule::GetInputTensorDescs() const
	{
		check(Module);
		check(!iree_vm_function_is_null(MainFunction));
		return InputTensorDescs;
	}

	TConstArrayView<UE::NNE::FTensorDesc> FIREEModule::GetOutputTensorDescs() const
	{
		check(Module);
		check(!iree_vm_function_is_null(MainFunction));
		return OutputTensorDescs;
	}

	FIREESession::FIREESession(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREEDevice> IREEDevice, iree_runtime_session_t* IREESession)
	{
		check(IREEInstance.IsValid());
		check(IREEDevice.IsValid());
		check(IREESession);

		Instance = IREEInstance;
		Device = IREEDevice;
		Session = IREESession;
	}

	FIREESession::~FIREESession()
	{
		if (Module.IsValid())
		{
			iree_runtime_call_deinitialize(&Call);
		}
		if (Session)
		{
			iree_runtime_session_release(Session);
		}
		Module.Reset();
		Device.Reset();
		Instance.Reset();
	}

	bool FIREESession::AppendModule(TSharedPtr<FIREEModule> IREEModule)
	{
		check(!Module.IsValid());
		check(IREEModule.IsValid());
		check(Session);

		if (!IREEModule->AppendToSession(Session))
		{
			return false;
		}

		iree_vm_function_t MainFunction = IREEModule->GetMainFunction();
		check(!iree_vm_function_is_null(MainFunction));

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));
		
		Status = iree_runtime_call_initialize(Session, MainFunction, &Call);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to initialize the session call", Status);
			iree_status_free(Status);
			return false;
		}

		TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = IREEModule->GetInputTensorDescs();
		check(!InputTensorDescs.IsEmpty());
		bool bAllConcrete = true;
		for (int32 i = 0; i < InputTensorDescs.Num(); i++)
		{
			bAllConcrete &= InputTensorDescs[i].GetShape().IsConcrete();
		}
		if (bAllConcrete)
		{
			InputTensorShapes.Reset();
			for (int32 i = 0; i < InputTensorDescs.Num(); i++)
			{
				InputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(InputTensorDescs[i].GetShape()));
			}
		}

		TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs = IREEModule->GetOutputTensorDescs();
		check(!OutputTensorDescs.IsEmpty());
		bAllConcrete = true;
		for (int32 i = 0; i < OutputTensorDescs.Num(); i++)
		{
			bAllConcrete &= OutputTensorDescs[i].GetShape().IsConcrete();
		}
		if (bAllConcrete)
		{
			OutputTensorShapes.Reset();
			for (int32 i = 0; i < OutputTensorDescs.Num(); i++)
			{
				OutputTensorShapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(OutputTensorDescs[i].GetShape()));
			}
		}

		Module = IREEModule;
		iree_status_free(Status);
		return true;
	}

	int32 FIREESession::SetInputTensorShapes(TConstArrayView<UE::NNE::FTensorShape> InInputShapes)
	{
		check(Device.IsValid());
		check(!InInputShapes.IsEmpty());

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = Module->GetInputTensorDescs();
		if (InputTensorDescs.Num() != InInputShapes.Num())
		{
			iree_status_free(Status);
			return -1;
		}
		for (int32 i = 0; i < InputTensorDescs.Num(); i++)
		{
			if (!InInputShapes[i].IsCompatibleWith(InputTensorDescs[i].GetShape()))
			{
				iree_status_free(Status);
				return -1;
			}
		}

		InputTensorShapes = InInputShapes;

		iree_status_free(Status);
		return 0;
	}

	TConstArrayView<UE::NNE::FTensorShape> FIREESession::GetInputTensorShapes() const
	{
		check(Instance.IsValid());
		check(Device.IsValid());
		check(Module.IsValid());
		check(Session);
		return InputTensorShapes;
	}

	TConstArrayView<UE::NNE::FTensorShape> FIREESession::GetOutputTensorShapes() const
	{
		check(Instance.IsValid());
		check(Device.IsValid());
		check(Module.IsValid());
		check(Session);
		return OutputTensorShapes;
	}

	FIREESessionCPU::FIREESessionCPU(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREEDevice> IREEDevice, iree_runtime_session_t* IREESession) : FIREESession(IREEInstance, IREEDevice, IREESession)
	{

	}

	int32 FIREESessionCPU::RunSyncCPU(TConstArrayView<UE::NNE::FTensorBindingCPU> InInputBindings, TConstArrayView<UE::NNE::FTensorBindingCPU> InOutputBindings)
	{
		check(Instance.IsValid());
		check(Device.IsValid());
		check(Module.IsValid());
		check(Session);

		check(InInputBindings.Num() == InputTensorShapes.Num());

		iree_status_t Status = iree_ok_status();
		check(iree_status_is_ok(Status));

		iree_runtime_call_reset(&Call);

		for (int32 i = 0; i < InInputBindings.Num(); i++)
		{
			check(InInputBindings[i].SizeInBytes == InputTensorShapes[i].Volume() * Module->GetInputTensorDescs()[i].GetElementByteSize());
			
			if (FMath::Modulo<uint64>((uint64)InInputBindings[i].Data, IREE_HAL_HEAP_BUFFER_ALIGNMENT) != 0)
			{
				UE_LOG(LogTemp, Error, TEXT("NNERuntimeIREECpu requires input- and output-buffer memory to be aligned with %d bytes"), IREE_HAL_HEAP_BUFFER_ALIGNMENT);
				return -1;
			}

			iree_hal_buffer_view_t* TempBufferView;
			iree_hal_buffer_params_t Params = { 0 };
			Params.type = IREE_HAL_MEMORY_TYPE_DEVICE_LOCAL;
			Params.usage = IREE_HAL_BUFFER_USAGE_DEFAULT;
			iree_hal_dim_t Shape[UE::NNE::FTensorShape::MaxRank];
			for (int32 j = 0; j < InputTensorShapes[i].Rank(); j++)
			{
				Shape[j] = InputTensorShapes[i].GetData()[j];
			}
			ENNETensorDataType NNEType = Module->GetInputTensorDescs()[i].GetDataType();
			iree_hal_element_types_t IREEType = UE::NNERuntimeIREE::Private::NNEToIREEType(NNEType);
			Status = iree_hal_buffer_view_allocate_buffer(
				Device->GetDeviceAllocator(), 
				InputTensorShapes[i].Rank(), Shape,
				IREEType, IREE_HAL_ENCODING_TYPE_DENSE_ROW_MAJOR,
				Params, 
				iree_make_const_byte_span((void*)InInputBindings[i].Data, InInputBindings[i].SizeInBytes),				
				&TempBufferView);
			if (!iree_status_is_ok(Status))
			{
				PrintIREEError("Failed to allocate the buffer view", Status);
				if (TempBufferView)
				{
					iree_hal_buffer_view_release(TempBufferView);
				}
				iree_status_free(Status);
				return -1;
			}

			Status = iree_runtime_call_inputs_push_back_buffer_view(&Call, TempBufferView);
			iree_hal_buffer_view_release(TempBufferView);
			if (!iree_status_is_ok(Status))
			{
				PrintIREEError("Failed to push the buffer view to the input list", Status);
				iree_status_free(Status);
				return -1;
			}
		}

		Status = iree_runtime_call_invoke(&Call, 0);
		if (!iree_status_is_ok(Status))
		{
			PrintIREEError("Failed to call the model function", Status);
			iree_status_free(Status);
			return -1;
		}

		OutputTensorShapes.Reset();
		TArray<iree_hal_buffer_view_t*> BufferViews;
		iree_hal_buffer_view_t* BufferView = nullptr;
		Status = iree_runtime_call_outputs_pop_front_buffer_view(&Call, &BufferView);
		while (iree_status_is_ok(Status))
		{
			iree_host_size_t Rank = iree_hal_buffer_view_shape_rank(BufferView);
			const iree_hal_dim_t* Dims = iree_hal_buffer_view_shape_dims(BufferView);
			uint32 Shape[UE::NNE::FTensorShape::MaxRank];
			for (int32 i = 0; i < FMath::Min((int32)Rank, UE::NNE::FTensorShape::MaxRank); i++)
			{
				Shape[i] = (uint32)Dims[i];
			}
			OutputTensorShapes.Add(UE::NNE::FTensorShape::Make(TConstArrayView<uint32>(Shape, FMath::Min((int32)Rank, UE::NNE::FTensorShape::MaxRank))));

			BufferViews.Add(BufferView);
			Status = iree_runtime_call_outputs_pop_front_buffer_view(&Call, &BufferView);
		}

		bool bCopyResults = true;
		if (InOutputBindings.Num() != OutputTensorShapes.Num())
		{
			bCopyResults = false;
		}
		for (int32 i = 0; i < InOutputBindings.Num() && bCopyResults; i++)
		{
			if (InOutputBindings[i].SizeInBytes < iree_hal_buffer_view_element_size(BufferViews[i]) * iree_hal_buffer_view_element_count(BufferViews[i]))
			{
				bCopyResults = false;
			}
		}
		int32 Result = 0;
		if (bCopyResults)
		{
			for (int32 i = 0; i < InOutputBindings.Num(); i++)
			{
				iree_hal_buffer_t* Buffer = iree_hal_buffer_view_buffer(BufferViews[i]);
				if (!Buffer)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to get the result buffer"));
					Result = -1;
					break;
				}

				int32 DataSizeInBytes = iree_hal_buffer_view_element_size(BufferViews[i]) * iree_hal_buffer_view_element_count(BufferViews[i]);

				iree_hal_buffer_mapping_t BufferMapping;
				Status = iree_hal_buffer_map_range(Buffer, IREE_HAL_MAPPING_MODE_PERSISTENT, IREE_HAL_MEMORY_ACCESS_READ, 0, DataSizeInBytes, &BufferMapping);
				if (!iree_status_is_ok(Status))
				{
					PrintIREEError("Failed to map the result buffer", Status);
					Result = -1;
					break;
				}
				FMemory::Memcpy(InOutputBindings[i].Data, BufferMapping.contents.data, DataSizeInBytes);
				iree_hal_buffer_unmap_range(&BufferMapping);
			}
		}
		
		for (int32 i = 0; i < InOutputBindings.Num(); i++)
		{
			iree_hal_buffer_view_release(BufferViews[i]);
		}
		iree_status_free(Status);
		return Result;
	}

	FIREEDevice::FIREEDevice(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREELibrary> IREELibrary, iree_hal_device_t* IREEDevice)
	{
		check(IREEInstance.IsValid());
		check(IREELibrary.IsValid());
		check(IREEDevice);

		Instance = IREEInstance;
		Library = IREELibrary;
		Device = IREEDevice;
	}

	FIREEDevice::~FIREEDevice()
	{
		if (Device)
		{
			iree_hal_device_release(Device);
		}
		Library.Reset();
		Instance.Reset();
	}

	iree_hal_allocator_t* FIREEDevice::GetDeviceAllocator()
	{
		check(Device);
		return iree_hal_device_allocator(Device);
	}

	FIREEDeviceCPU::FIREEDeviceCPU(TSharedPtr<FIREEInstance> IREEInstance, TSharedPtr<FIREELibrary> IREELibrary, iree_hal_device_t* IREEDevice) : FIREEDevice(IREEInstance, IREELibrary, IREEDevice)
	{

	}

	TSharedPtr<FIREESessionCPU> FIREEDeviceCPU::MakeSessionCPU(TSharedPtr<FIREEDeviceCPU> IREEDevice)
	{
		check(IREEDevice.IsValid());
		check(IREEDevice->Instance.IsValid());
		check(IREEDevice->Library.IsValid());
		check(IREEDevice->Device);

		iree_runtime_session_t* TempSession = nullptr;
		if (!IREEDevice->Instance->CreateSession(IREEDevice->Device, &TempSession))
		{
			return TSharedPtr<FIREESessionCPU>();
		}

		TSharedPtr<FIREESessionCPU> Result = MakeShared<FIREESessionCPU>(IREEDevice->Instance, IREEDevice, TempSession);
		return Result;
	}

	FIREELibrary::FIREELibrary()
	{
		Library = nullptr;
	}

	FIREELibrary::~FIREELibrary()
	{
		if (Library)
		{
			FPlatformProcess::FreeDllHandle(Library);
		}
		Instance.Reset();
	}

	TSharedPtr<FIREELibrary> FIREELibrary::MakeLibrary(const FString& LibraryPath, const FString& LibraryName)
	{
		check(!LibraryName.IsEmpty());

		TSharedPtr<FIREEInstance> IREEInstance = FIREEInstance::GetInstance();
		if (!IREEInstance.IsValid())
		{
			return TSharedPtr<FIREELibrary>();
		}

		FString LibraryKey = LibraryPath + LibraryName;
		TSharedPtr<FIREELibrary> Result = IREEInstance->GetLibrary(LibraryKey);
		if (Result.IsValid())
		{
			return Result;
		}

		Result = MakeShared<FIREELibrary>();
		Result->Instance = IREEInstance;
		Result->LibraryName = LibraryName;

#ifdef NNE_RUNTIME_IREE_USE_COMBINED_LIB_PATH
		FString CombinedPath = FPaths::Combine(LibraryPath, LibraryName);
		Result->Library = FPlatformProcess::GetDllHandle(*CombinedPath);

		if (!Result->Library)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load the shared library '%s'"), *CombinedPath);
			return TSharedPtr<FIREELibrary>();
		}
#else
		FPlatformProcess::PushDllDirectory(*LibraryPath);
		Result->Library = FPlatformProcess::GetDllHandle(*LibraryName);
		FPlatformProcess::PopDllDirectory(*LibraryPath);

		if (!Result->Library)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load the shared library '%s' from '%s'"), *LibraryName, *LibraryPath);
			return TSharedPtr<FIREELibrary>();
		}
#endif

		IREEInstance->SetLibrary(LibraryKey, Result);
		return Result;
	}

	TSharedPtr<FIREEDeviceCPU> FIREELibrary::MakeDeviceCPU(TSharedPtr<FIREELibrary> Library, const FString& LibraryQueryFunctionName)
	{
		check(Library.IsValid());
		check(Library->Instance.IsValid());
		check(Library->Library);
		check(!LibraryQueryFunctionName.IsEmpty());

		if (Library->Devices.Contains(LibraryQueryFunctionName))
		{
			TSharedPtr<FIREEDeviceCPU> Result = Library->Devices[LibraryQueryFunctionName].Pin();
			if (Result.IsValid())
			{
				return Result;
			}
		}

		void* LibraryQueryFunctionPointer = FPlatformProcess::GetDllExport(Library->Library, *LibraryQueryFunctionName);
		if (!LibraryQueryFunctionPointer)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to find the entry point '%s' in the shared library '%s'"), *LibraryQueryFunctionName, *(Library->GetLibraryName()));
			return TSharedPtr<FIREEDeviceCPU>();
		}

		iree_hal_device_t* Device = nullptr;
		if (!Library->Instance->CreateSyncDevice(LibraryQueryFunctionPointer, &Device))
		{
			return TSharedPtr<FIREEDeviceCPU>();
		}

		TSharedPtr<FIREEDeviceCPU> Result = MakeShared<FIREEDeviceCPU>(Library->Instance, Library, Device);
		Library->Devices.Emplace(LibraryQueryFunctionName, Result);
		return Result;
	}

	FString FIREELibrary::GetLibraryName()
	{
		return LibraryName;
	}
} // UE::NNERuntimeIREE

#endif // WITH_NNE_RUNTIME_IREE