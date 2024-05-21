// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookDependency.h"

#if WITH_EDITOR
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMath.h"
#include "Hash/Blake3.h"
#include "Misc/AssertionMacros.h"
#include "Misc/StringBuilder.h"
#include "Serialization/Archive.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/UniquePtr.h"
#endif

#if WITH_EDITOR

namespace UE::Cook::Dependency::Private
{

/**
 * Return the map from Name to function created by UE_COOK_DEPENDENCY_FUNCTION macros.
 * Initialized when first called and afterwards immutable
 */
const TMap<FName, FCookDependencyFunction>& GetCookDependencyFunctions();

} // namespace UE::Cook::Dependency::Private

namespace UE::Cook
{

FCookDependency FCookDependency::File(FStringView InFileName)
{
	FCookDependency Result(ECookDependency::File);
	Result.StringData = InFileName;
	return Result;
}

FCookDependency FCookDependency::Function(FName InFunctionName, FCbFieldIterator&& InArgs)
{
	FCookDependency Result(ECookDependency::Function);
	Result.FunctionData.Name = InFunctionName;
	Result.FunctionData.Args = MoveTemp(InArgs);
	Result.FunctionData.Args.MakeOwned();
	return Result;
}

FCookDependency FCookDependency::TransitiveBuildAndRuntime(FName PackageName)
{
	FCookDependency Result(ECookDependency::TransitiveBuild);
	Result.TransitiveBuildData.PackageName = PackageName;
	Result.TransitiveBuildData.bAlsoAddRuntimeDependency = true;
	return Result;
}

FCookDependency FCookDependency::Package(FName PackageName)
{
	FCookDependency Result(ECookDependency::Package);
	Result.NameData = PackageName;
	return Result;
}

FCookDependency FCookDependency::ConsoleVariable(FStringView VariableName)
{
	FCookDependency Result(ECookDependency::ConsoleVariable);
	Result.StringData = VariableName;
	return Result;
}

FCookDependency::FCookDependency()
	: Type(ECookDependency::None)
{
}

FCookDependency::FCookDependency(ECookDependency InType)
	: Type(InType)
{
	Construct();
}

FCookDependency::~FCookDependency()
{
	Destruct();
}

FCookDependency::FCookDependency(const FCookDependency& Other)
	: Type(ECookDependency::None)
{
	*this = Other;
}

FCookDependency::FCookDependency(FCookDependency&& Other)
	: Type(ECookDependency::None)
{
	*this = MoveTemp(Other);
}

FCookDependency& FCookDependency::operator=(const FCookDependency& Other)
{
	Destruct();
	Type = Other.Type;
	Construct();
	switch (Type)
	{
	case ECookDependency::None:
		break;
	case ECookDependency::File:
	case ECookDependency::ConsoleVariable:
		StringData = Other.StringData;
		break;
	case ECookDependency::Function:
		FunctionData = Other.FunctionData;
		break;
	case ECookDependency::TransitiveBuild:
		TransitiveBuildData = Other.TransitiveBuildData;
		break;
	case ECookDependency::Package:
		NameData = Other.NameData;
		break;
	default:
		checkNoEntry();
		break;
	}
	return *this;
}

FCookDependency& FCookDependency::operator=(FCookDependency&& Other)
{
	Destruct();
	Type = Other.Type;
	Construct();
	switch (Type)
	{
	case ECookDependency::None:
		break;
	case ECookDependency::File:
	case ECookDependency::ConsoleVariable:
		StringData = MoveTemp(Other.StringData);
		break;
	case ECookDependency::Function:
		FunctionData = MoveTemp(Other.FunctionData);
		break;
	case ECookDependency::TransitiveBuild:
		TransitiveBuildData = MoveTemp(Other.TransitiveBuildData);
		break;
	case ECookDependency::Package:
		NameData = MoveTemp(Other.NameData);
		break;
	default:
		checkNoEntry();
		break;
	}
	return *this;
}

void FCookDependency::UpdateHash(FCookDependencyContext& Context) const
{
	switch (GetType())
	{
	case ECookDependency::None:
		// Nothing to add, Nones are never invalidated
		return;
	case ECookDependency::File:
	{
		const TCHAR* LocalFilename = GetFileName().GetData(); // contract for GetFileName is a null-terminated TCHAR*.
		LocalFilename = LocalFilename ? LocalFilename : TEXT("");
		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(LocalFilename, FILEREAD_Silent));
		if (!Ar)
		{
			Context.LogError(FString::Printf(
				TEXT("FCookDependency::File('%s') failed to UpdateHash: could not read file."),
				LocalFilename));
			return;
		}
		TArray64<uint8> Buffer;
		Buffer.SetNumUninitialized(64 * 1024);
		const int64 Size = Ar->TotalSize();
		int64 Position = 0;
		// Read in BufferSize chunks
		while (Position < Size)
		{
			const int64 ReadNum = FMath::Min(Size - Position, (int64)Buffer.Num());
			Ar->Serialize(Buffer.GetData(), static_cast<uint64>(ReadNum));
			Context.Update(Buffer.GetData(), ReadNum);
			Position += ReadNum;
		}
		break;
	}
	case ECookDependency::Function:
	{
		const TMap<FName, FCookDependencyFunction>& CookDependencyFunctions =
			UE::Cook::Dependency::Private::GetCookDependencyFunctions();

		const FCookDependencyFunction* Function = CookDependencyFunctions.Find(GetFunctionName());
		if (!Function)
		{
			Context.LogError(FString::Printf(
				TEXT("FCookDependency::Function('%s') failed to UpdateHash: Function not found."),
				*GetFunctionName().ToString()));
			return;
		}
		FCookDependencyContext::FErrorHandlerScope Scope = Context.ErrorHandlerScope([this](FString&& Message)
			{
				return FString::Printf(TEXT("FCookDependency::Function('%s') failed to UpdateHash: %s"),
				*GetFunctionName().ToString(), *Message);
			});
		(*Function)(GetFunctionArgs(), Context);
		return;
	}
	case ECookDependency::TransitiveBuild:
		// Build dependencies do not impact the hash; they instead operate by marking the package
		// as invalidated based on the invalidation of other packages, in a separate pass after its hash is compared
		return;
	case ECookDependency::Package:
		Context.LogError(FString::Printf(
			TEXT("FCookDependency::Package('%s') failed to UpdateHash: Package dependencies do not implement UpdateHash and it should not be called on them."),
			*NameData.ToString()));
		return;
	case ECookDependency::ConsoleVariable:
	{
		IConsoleVariable* VariableInstance = IConsoleManager::Get().FindConsoleVariable(*StringData);
		if (VariableInstance == nullptr)
		{
			Context.LogError(FString::Printf(
				TEXT("FCookDependency::ConsoleVariable('%s') failed to UpdateHash: could not find console variable."),
				*StringData));
			return;
		}
		FString VariableAsString = VariableInstance->GetString();
		Context.Update(VariableAsString.GetCharArray().GetData(), VariableAsString.GetCharArray().Num() * sizeof(FString::ElementType));
		return;
	}
	default:
		checkNoEntry();
		return;
	}
}

void FCookDependency::Construct()
{
	switch (Type)
	{
	case ECookDependency::None:
		break;
	case ECookDependency::File:
	case ECookDependency::ConsoleVariable:
		new(&StringData) FString();
		break;
	case ECookDependency::Function:
		new(&FunctionData) FFunctionData();
		break;
	case ECookDependency::TransitiveBuild:
		new(&TransitiveBuildData) FTransitiveBuildData();
		break;
	case ECookDependency::Package:
		new(&NameData) FName();
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FCookDependency::Destruct()
{
	switch (Type)
	{
	case ECookDependency::None:
		break;
	case ECookDependency::File:
	case ECookDependency::ConsoleVariable:
		StringData.~FString();
		break;
	case ECookDependency::Function:
		FunctionData.~FFunctionData();
		break;
	case ECookDependency::TransitiveBuild:
		TransitiveBuildData.~FTransitiveBuildData();
		break;
	case ECookDependency::Package:
		NameData.~FName();
		break;
	default:
		checkNoEntry();
		break;
	}
}

void FCookDependency::Save(FCbWriter& Writer) const
{
	Writer.BeginArray();
	Writer << static_cast<uint8>(Type);
	switch (Type)
	{
	case ECookDependency::None:
		break;
	case ECookDependency::File:
	case ECookDependency::ConsoleVariable:
		Writer << StringData;
		break;
	case ECookDependency::Function:
		Writer << FunctionData.Name;
		for (FCbFieldViewIterator Iter(FunctionData.Args); Iter; ++Iter)
		{
			Writer << Iter;
		}
		break;
	case ECookDependency::TransitiveBuild:
		Writer << TransitiveBuildData.PackageName;
		Writer << TransitiveBuildData.bAlsoAddRuntimeDependency;
		break;
	case ECookDependency::Package:
		Writer << NameData;
		break;
	default:
		checkNoEntry();
		break;
	}
	Writer.EndArray();
}

bool FCookDependency::Load(FCbFieldView Value)
{
	*this = FCookDependency(ECookDependency::None);
	FCbArrayView ArrayView = Value.AsArrayView();
	if (ArrayView.Num() < 1)
	{
		return false;
	}
	FCbFieldViewIterator Field(Value.CreateViewIterator());
	int32 LocalTypeAsInt = Field.AsUInt8();
	if ((Field++).HasError() || LocalTypeAsInt >= static_cast<uint8>(ECookDependency::Count))
	{
		return false;
	}
	ECookDependency LocalType = static_cast<ECookDependency>(LocalTypeAsInt);

	switch (LocalType)
	{
	case ECookDependency::None:
		return true;
	case ECookDependency::File:
	{
		FUtf8StringView LocalFilename = Field.AsString();
		if ((Field++).HasError())
		{
			return false;
		}
		*this = FCookDependency::File(WriteToString<256>(LocalFilename));
		return true;
	}
	case ECookDependency::Function:
	{
		FName LocalFuncName;
		if (!LoadFromCompactBinary(Field++, LocalFuncName))
		{
			return false;
		}
		*this = FCookDependency::Function(LocalFuncName, FCbFieldIterator::CloneRange(Field));
		FunctionData.Args.MakeOwned();
		return true;
	}
	case ECookDependency::TransitiveBuild:
	{
		FName LocalPackageName;
		if (!LoadFromCompactBinary(Field++, LocalPackageName))
		{
			return false;
		}
		bool bLocalAlsoAddRuntimeDependency = Field.AsBool();
		if ((Field++).HasError())
		{
			return false;
		}
		*this = FCookDependency::TransitiveBuildAndRuntime(LocalPackageName);
		TransitiveBuildData.bAlsoAddRuntimeDependency = bLocalAlsoAddRuntimeDependency;
		return true;
	}
	case ECookDependency::Package:
	{
		FName LocalPackageName;
		if (!LoadFromCompactBinary(Field++, LocalPackageName))
		{
			return false;
		}
		*this = FCookDependency::Package(LocalPackageName);
		return true;
	}
	case ECookDependency::ConsoleVariable:
	{
		FString LocalConsoleVariableName;
		if (!LoadFromCompactBinary(Field++, LocalConsoleVariableName))
		{
			return false;
		}
		*this = FCookDependency::ConsoleVariable(LocalConsoleVariableName);
		return true;
	}
	default:
		break;
	}
	checkNoEntry();
	return false;
}

void FCookDependencyContext::Update(const void* Data, uint64 Size)
{
	FBlake3& Blake3Hasher = *(reinterpret_cast<FBlake3*>(Hasher));
	Blake3Hasher.Update(Data, Size);
}

void FCookDependencyContext::LogError(FString Message)
{
	for (TUniqueFunction<FString(FString&&)>& Handler : ErrorHandlers)
	{
		Message = Handler(MoveTemp(Message));
	}
	OnLogError(MoveTemp(Message));
}

FCookDependencyContext::FErrorHandlerScope::FErrorHandlerScope(FCookDependencyContext& InContext)
	: Context(InContext)
{
}

FCookDependencyContext::FErrorHandlerScope::~FErrorHandlerScope()
{
	check(!Context.ErrorHandlers.IsEmpty());
	Context.ErrorHandlers.Pop(EAllowShrinking::No);
}

FCookDependencyContext::FErrorHandlerScope FCookDependencyContext::ErrorHandlerScope(
	TUniqueFunction<FString(FString&&)>&& ErrorHandler)
{
	ErrorHandlers.Add(MoveTemp(ErrorHandler));
	// We rely on Copy elision to not move-construct this scope, so that it only calls the destructor once.
	return FErrorHandlerScope(*this);
}

} // namespace UE::Cook

namespace UE::Cook::Dependency::Private
{

FCookDependencyFunctionRegistration* List = nullptr;
bool bGCookDependencyFunctionsInitialized = false;
TMap<FName, FCookDependencyFunction> GCookDependencyFunctions;

const TMap<FName, FCookDependencyFunction>& GetCookDependencyFunctions()
{
	if (!bGCookDependencyFunctionsInitialized)
	{
		GCookDependencyFunctions.Empty();
		FCookDependencyFunctionRegistration* Reg = UE::Cook::Dependency::Private::List;
		while (Reg)
		{
			FCookDependencyFunction& ExistingFunction = GCookDependencyFunctions.FindOrAdd(Reg->GetFName(), nullptr);
			checkf(ExistingFunction == nullptr || ExistingFunction == Reg->Function,
				TEXT("UE_COOK_DEPENDENCY_FUNCTION name '%s' is duplicated. UE_COOK_DEPENDENCY_FUNCTION names must be unique."),
				*Reg->GetFName().ToString());
			ExistingFunction = Reg->Function;
			Reg = Reg->Next;
		}
		bGCookDependencyFunctionsInitialized = true;
	}
	return GCookDependencyFunctions;
}

void FCookDependencyFunctionRegistration::Construct()
{
	using namespace UE::Cook::Dependency::Private;

	Next = UE::Cook::Dependency::Private::List;
	UE::Cook::Dependency::Private::List = this;
	bGCookDependencyFunctionsInitialized = false;
}

FCookDependencyFunctionRegistration::~FCookDependencyFunctionRegistration()
{
	using namespace UE::Cook::Dependency::Private;

	// Remove this from the list when destructed, but for better shutdown performance skip this cost during engine
	// exit, and just leave the list with dangling pointers, since the list can only be read by this destructor or by
	// GetCookDependencyFunctions, which is not called during shutdown.
	if (!IsEngineExitRequested())
	{
		if (UE::Cook::Dependency::Private::List == this)
		{
			UE::Cook::Dependency::Private::List = this->Next;
		}
		else
		{
			FCookDependencyFunctionRegistration* Current = UE::Cook::Dependency::Private::List;
			while (Current)
			{
				if (Current->Next == this)
				{
					Current->Next = this->Next;
					break;
				}
				Current = Current->Next;
			}
		}
		bGCookDependencyFunctionsInitialized = false;
	}
}

} // namespace UE::Cook::Dependency::Private

#endif // WITH_EDITOR