// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "HAL/PreprocessorHelpers.h"
#include "Serialization/CompactBinary.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"

class FCbFieldView;
class FCbWriter;
#endif

#if WITH_EDITOR

namespace UE::Cook
{

/** Context passed into FCookDependencyFunction to provide calling flags and receive their hash output. */
struct FCookDependencyContext
{
public:
	/** InHasher is void* to mask the implementation details of the hashbuilder. See Update function. */
	explicit FCookDependencyContext(void* InHasher, TUniqueFunction<void(FString&&)>&& InOnLogError);

	/**
	 * Update the hashbuilder for the key being constructed (e.g. TargetDomainKey for cooked packages)
	 * with the given Data of Size bytes.
	 */
	COREUOBJECT_API void Update(const void* Data, uint64 Size);

	/**
	 * Reports failure to compute the hash (e.g. because a file cannot be read).
	 * When calculating the initial hash during package save, this error will be logged as an error
	 * and the package will be recooked on the next cook. When calculating the hash during an incremental cook
	 * the message will be logged at Log level and will cause the package to be recooked.
	 */
	COREUOBJECT_API void LogError(FString Message);

	/**
	 * Private implementation struct used for AddErrorHandlerScope. Should only be used via
	 * FCookDependencyContext::FErrorHandlerScope& Scope = Context.ErrorHandlerScope(<Function>);
	 */
	struct FErrorHandlerScope
	{
	public:
		COREUOBJECT_API ~FErrorHandlerScope();
	private:
		COREUOBJECT_API FErrorHandlerScope(FCookDependencyContext& InContext);
		friend FCookDependencyContext;
		FCookDependencyContext& Context;
	};
	/**
	 * Add a function that will be removed when the return value goes out of scope, to modify error strings reported
	 * inside the scope before passing them on to higher scopes or the error consumer.
	 * e.g. 
	 * FCookDependencyContext::FErrorHandlerScope Scope = Context.ErrorHandlerScope([](FString&& Inner)
	 * { return FString::Printf(TEXT("OuterClass for %s: %s"), *Name, *Inner);});
	 */
	[[nodiscard]] COREUOBJECT_API FErrorHandlerScope ErrorHandlerScope(
		TUniqueFunction<FString(FString&&)>&& ErrorHandler);

private:
	TUniqueFunction<void(FString&&)> OnLogError;
	TArray<TUniqueFunction<FString(FString&&)>, TInlineAllocator<1>> ErrorHandlers;
	void* Hasher; // Type is void* to mask the implementation detail
};

/**
 * TypeSelector enum for the FCookDependency variable type. Values are serialized into the oplog as integers,
 * so do not change them without changing oplog version.
 */
enum class ECookDependency : uint8
{
	None = 0x00,
	File = 0x01,
	Function = 0x02,
	TransitiveBuild = 0x03,
};

/**
 * TargetDomain dependencies that can be reported from the class instances in a package. These dependencies are
 * stored in the cook oplog and are evaluated during incremental cook. If any of them changes, the package is
 * invalidated and must be recooked (loaded/saved). These dependencies do not impact whether DDC keys built
 * from the package need to be recalculated.
 */
class FCookDependency
{
public:
	/**
	 * Create a dependency on the contents of the file. Filename will be normalized. Contents are loaded via
	 * IFileManager::Get().CreateFileReader and contents are hashed for comparison.
	 */
	COREUOBJECT_API static FCookDependency File(FStringView InFileName);
	/**
	 * Create a dependency on a call to the specified function with the given arguments. Arguments should be
	 * created using FCbWriter Writer; ... <append arbitrary number of fields to Writer> ...; Writer.Save().
	 * The function should read the arguments using the corresponding FCbFieldIteratorView methods and
	 * LoadFromCompactBinary calls.
	 * 
	 * The function must be registered during editor startup for use with FCookDependency via
	 * UE_COOK_DEPENDENCY_FUNCTION(CppTokenUsedAsName, CppNameOfFunctionToCall).
	 * The name to pass to FCookDependency::Function can be retrieved via
	 * UE_COOK_DEPENDENCY_FUNCTION_CALL(CppTokenUsedAsName).
	 */
	COREUOBJECT_API static FCookDependency Function(FName InFunctionName, FCbFieldIterator&& InArgs);

	/**
	 * Create a transitive build dependency on another package. In an incremental cook if the other package was not
	 * cooked in a previous cook session, or its previous cook result was invalidated, the current package will also
	 * have its cook result invalidated.
	 *
	 * This version of the function also adds a runtime dependency - the requested package will be staged for the
	 * current platform. Adding a transitive build dependency without adding a runtime dependency is not yet supported
	 * due to limitations in the cooker.
	 */
	COREUOBJECT_API static FCookDependency TransitiveBuildAndRuntime(FName PackageName);

	/** Construct an empty dependency; it will never be invalidated. */
	COREUOBJECT_API FCookDependency();

	COREUOBJECT_API ~FCookDependency();
	COREUOBJECT_API FCookDependency(const FCookDependency& Other);
	COREUOBJECT_API FCookDependency(FCookDependency&& Other);
	COREUOBJECT_API FCookDependency& operator=(const FCookDependency& Other);
	COREUOBJECT_API FCookDependency& operator=(FCookDependency&& Other);
	
	/** FCookDependency is a vararg type. Return the type of this instance. */
	ECookDependency GetType() const;

	/** FileName if GetType() == File, else empty. StringView points to null or a null-terminated string. */
	FStringView GetFileName() const;

	/** FunctionName if GetType() == Function, else NAME_None. */
	FName GetFunctionName() const;
	/** FunctionArgs if GetType() == Function, else FCbFieldViewIterator(). */
	FCbFieldViewIterator GetFunctionArgs() const;

	/** PackageName if GetType() == TransitiveBuild, else NAME_None. */
	FName GetPackageName() const;
	/** If GetType() == TransitiveBuild, whether AlsoAddRuntimeDependency was selected, otherwise false. */
	bool IsAlsoAddRuntimeDependency() const;

	/**
	 * Comparison operator for e.g. deterministic ordering of dependencies.
	 * Uses persistent comparison data and is somewhat expensive.
	 */
	bool operator<(const FCookDependency& Other) const;

	/** Calculate the current hash of this CookDependency, and add it into Context. */
	COREUOBJECT_API void UpdateHash(FCookDependencyContext& Context) const;

private:
	explicit FCookDependency(ECookDependency InType);
	void Construct();
	void Destruct();
	COREUOBJECT_API void Save(FCbWriter& Writer) const;
	COREUOBJECT_API bool Load(FCbFieldView Value);

	/** Public hidden friend for operator<< into an FCbWriter. */
	friend FCbWriter& operator<<(FCbWriter& Writer, const FCookDependency& CookDependencies)
	{
		CookDependencies.Save(Writer);
		return Writer;
	}
	/** Public hidden friend for LoadFromCompactBinary. */
	friend bool LoadFromCompactBinary(FCbFieldView Value, FCookDependency& CookDependencies)
	{
		return CookDependencies.Load(Value);
	}

private:
	ECookDependency Type;
	struct FFunctionData
	{
		FName Name;
		FCbFieldIterator Args;
	};
	struct FTransitiveBuildData
	{
		FName PackageName;
		bool bAlsoAddRuntimeDependency = true;
	};
	union
	{
		FString FileName;
		FFunctionData FunctionData;
		FTransitiveBuildData TransitiveBuildData;
	};
};

/**
 * Type of functions used in FCookDependency to append the hash values of arbitrary data.
 * 
 * @param Args Variable-length, variable-typed input data (e.g. names of files, configuration flags)
 *             that specify which hash data. The function should read this data using FCbFieldViewIterator
 *             methods and LoadFromCompactBinary calls that correspond to the FCbWriter methods used
 *             at the callsite of FCookDependency::Function.
 * @param Context that provides calling flags and receives the hashdata. The function should call
 *        Context.Update with the data to be added to the target key (e.g. the hash of the contents
 *        of a filename that was specified in Args).
 */
using FCookDependencyFunction = void (*)(FCbFieldViewIterator Args, FCookDependencyContext& Context);

} // namespace UE::Cook

namespace UE::Cook::Dependency::Private
{

/**
 * Implementation struct of UE_COOK_DEPENDENCY_FUNCTION. Instances of this class are stored in global or
 * namespace scope and add themselves to a list during c++ pre-main static initialization. This list
 * is read later to create a map from FName to c++ function.
 */
struct FCookDependencyFunctionRegistration
{
	template<int N>
	FCookDependencyFunctionRegistration(const TCHAR(&InName)[N], FCookDependencyFunction InFunction)
		: Name(InName), Function(InFunction), Next(nullptr)
	{
		static_assert(N > 0, "Name must be provided");
		check(InName[0] != '\0');
		Construct();
	}
	COREUOBJECT_API ~FCookDependencyFunctionRegistration();
	COREUOBJECT_API void Construct();
	FName GetFName();

	FLazyName Name;
	FCookDependencyFunction Function;
	FCookDependencyFunctionRegistration* Next;
};

} // namespace UE::Cook::Dependency::Private

/**
 * UE_COOK_DEPENDENCY_FUNCTION(<CppToken> Name, void (*)(FCbFieldView Args, FCookDependencyContext& Context))
 * 
 * Registers the given function pointer to handle FCookDependency::Function(Name, Args) calls.
 * @see FCookDependencyFunction. @see FCookDependency::Function.
 * 
 * Name should be a bare cpptoken, e.g.
 * UE_COOK_DEPENDENCY_FUNCTION(MyTypeDependencies, UE::MyTypeDependencies::ImplementationFunction).
*/
#define UE_COOK_DEPENDENCY_FUNCTION(Name, Function) \
	UE::Cook::Dependency::Private::FCookDependencyFunctionRegistration \
	PREPROCESSOR_JOIN(FCookDependencyFunctionRegistration_,Name)(TEXT(#Name), Function)
/**
 * Return the FName to use to call a function that was registered via UE_COOK_DEPENDENCY_FUNCTION(Name, Function).
 * Name should be the same bare cpptoken that was passed into UE_COOK_DEPENDENCY_FUNCTION.
 */
#define UE_COOK_DEPENDENCY_FUNCTION_CALL(Name) \
	PREPROCESSOR_JOIN(FCookDependencyFunctionRegistration_,Name).GetFName()

#else // WITH_EDITOR

#define UE_COOK_DEPENDENCY_FUNCTION(Name, Function)
#define UE_COOK_DEPENDENCY_FUNCTION_CALL(Name) NAME_None

#endif // !WITH_EDITOR

#if WITH_EDITOR
namespace UE::Cook
{

inline ECookDependency FCookDependency::GetType() const
{
	return Type;
}

inline FStringView FCookDependency::GetFileName() const
{
	return Type == ECookDependency::File ? FileName : FStringView();
}

inline FName FCookDependency::GetFunctionName() const
{
	return Type == ECookDependency::Function ? FunctionData.Name : NAME_None;
}

inline FCbFieldViewIterator FCookDependency::GetFunctionArgs() const
{
	return Type == ECookDependency::Function ? FunctionData.Args : FCbFieldViewIterator();
}

inline FName FCookDependency::GetPackageName() const
{
	return Type == ECookDependency::TransitiveBuild ? TransitiveBuildData.PackageName : NAME_None;
}

inline bool FCookDependency::IsAlsoAddRuntimeDependency() const
{
	return Type == ECookDependency::TransitiveBuild ? TransitiveBuildData.bAlsoAddRuntimeDependency : false;
}

inline bool FCookDependency::operator<(const FCookDependency& Other) const
{
	if (static_cast<uint8>(Type) != static_cast<uint8>(Other.Type))
	{
		return static_cast<uint8>(Type) < static_cast<uint8>(Other.Type);
	}

	switch (Type)
	{
	case ECookDependency::None:
		return false;
	case ECookDependency::File:
		return FileName.Compare(Other.FileName, ESearchCase::IgnoreCase) < 0;
	case ECookDependency::Function:
	{
		int32 Compare = FunctionData.Name.Compare(Other.FunctionData.Name);
		if (Compare != 0)
		{
			return Compare < 0;
		}
		FMemoryView ViewA;
		FMemoryView ViewB;
		bool bHasViewA = FunctionData.Args.TryGetRangeView(ViewA);
		bool bHasViewB = FunctionData.Args.TryGetRangeView(ViewB);
		if ((!bHasViewA) | (!bHasViewB))
		{
			return bHasViewB; // If both false, return false. If only one, return true only if A is the false.
		}
		return ViewA.CompareBytes(ViewB) < 0;
	}
	case ECookDependency::TransitiveBuild:
	{
		// FName.Compare is lexical and case-insensitive, which is what we want
		int32 Compare = TransitiveBuildData.PackageName.Compare(Other.TransitiveBuildData.PackageName) < 0;
		if (Compare != 0)
		{
			return Compare < 0;
		}
		if (TransitiveBuildData.bAlsoAddRuntimeDependency != Other.TransitiveBuildData.bAlsoAddRuntimeDependency)
		{
			return TransitiveBuildData.bAlsoAddRuntimeDependency == false;
		}
		return false;
	}
	default:
		checkNoEntry();
		return false;
	}
}

inline FCookDependencyContext::FCookDependencyContext(void* InHasher, TUniqueFunction<void(FString&&)>&& InOnLogError)
	: OnLogError(MoveTemp(InOnLogError))
	, Hasher(InHasher)
{
}

} // namespace UE::Cook

namespace UE::Cook::Dependency::Private
{

inline FName FCookDependencyFunctionRegistration::GetFName()
{
	return Name.Resolve();
}

} // namespace UE::Cook::Dependency::Private

#endif