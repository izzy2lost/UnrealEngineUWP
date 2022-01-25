// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	UWPPlatform.h: Setup for the windows platform
==================================================================================*/

#pragma once

#include "MSVC/MSVCPlatform.h"

/** Define the UWP platform to be the active one **/
#define PLATFORM_UWP					1

/**
* Windows specific types
**/
struct FUWPTypes : public FGenericPlatformTypes
{
	// defined in windefs.h, even though this is equivalent, the compiler doesn't think so
	typedef unsigned long       DWORD;
#ifdef _WIN64
	typedef unsigned __int64	SIZE_T;
	typedef __int64				SSIZE_T;
#else
	typedef unsigned long		SIZE_T;
	typedef long				SSIZE_T;
#endif

	typedef decltype(__nullptr) TYPE_OF_NULLPTR;
};

typedef FUWPTypes FPlatformTypes;

// Base defines, must define these for the platform, there are no defaults
#define PLATFORM_DESKTOP				1
#if defined( _WIN64 )
#define PLATFORM_64BITS					1
#else
#define PLATFORM_64BITS					0
#endif

#define PLATFORM_CAN_SUPPORT_EDITORONLY_DATA	0

// Base defines, defaults are commented out

#define PLATFORM_LITTLE_ENDIAN								1
#define PLATFORM_SUPPORTS_PRAGMA_PACK						1
#define PLATFORM_ENABLE_VECTORINTRINSICS					1
#define PLATFORM_TASKGRAPH_GO_WIDE                          1
//#define PLATFORM_USE_LS_SPEC_FOR_WIDECHAR					0
//#define PLATFORM_COMPILER_DISTINGUISHES_DWORD_AND_UINT		1
//#define PLATFORM_COMPILER_HAS_GENERIC_KEYWORD				1
#define PLATFORM_HAS_BSD_TIME								0
#define PLATFORM_HAS_BSD_SOCKETS							1
#define PLATFORM_HAS_BSD_IPV6_SOCKETS						1
#define PLATFORM_USE_PTHREADS								0
#define PLATFORM_MAX_FILEPATH_LENGTH						MAX_PATH
#define PLATFORM_USES_DYNAMIC_RHI							1
#define PLATFORM_REQUIRES_FILESERVER						1
#define PLATFORM_SUPPORTS_MULTITHREADED_GC					0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS			1
#define PLATFORM_USES_MICROSOFT_LIBC_FUNCTIONS				1
//#define PLATFORM_SUPPORTS_NAMED_PIPES						1
#define PLATFORM_COMPILER_HAS_VARIADIC_TEMPLATES			1
#define PLATFORM_COMPILER_HAS_EXPLICIT_OPERATORS			1
//#define EXCEPTION_EXECUTE_HANDLER                           1
#define WINDOWS_PF_COMPARE_EXCHANGE128                      PF_COMPARE_EXCHANGE128
//#define PLATFORM_COMPILER_HAS_DEFAULT_FUNCTION_TEMPLATE_ARGUMENTS	1

#if UE_BUILD_SHIPPING
#define PLATFORM_BREAK() ((void)0)
#else
#define PLATFORM_BREAK() ((void)(FUWPMisc::IsDebuggerPresent() && (__debugbreak(), 1)))
#endif

//@todo.UWP: Fixup once sockets are supported
#define PLATFORM_SUPPORTS_MESSAGEBUS						1

// @SPLASH_DAMAGE_CHANGE: v-jacsmi@microsoft.com - BEGIN
#define PLATFORM_SUPPORTS_XBOX_LIVE 0
// @SPLASH_DAMAGE_CHANGE: v-jacsmi@microsoft.com - END

#define PLATFORM_HAS_128BIT_ATOMICS							PLATFORM_64BITS

// Function type macros.
#define VARARGS			__cdecl					/* Functions with variable arguments */
#define CDECL			__cdecl					/* Standard C function */
#define STDCALL			__stdcall				/* Standard calling convention */
#define FORCEINLINE		__forceinline			/* Force code to be inline */
#define FORCENOINLINE	__declspec(noinline)	/* Force code to NOT be inline */

// Hints compiler that expression is true; generally restricted to comparisons against constants
#define ASSUME(expr)	__assume(expr)

#define DECLARE_UINT64(x)	x

// Optimization macros (uses __pragma to enable inside a #define).
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL __pragma(optimize("",off))
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL  __pragma(optimize("",on))

// Backwater of the spec. All compilers support this except microsoft, and they will soon
#define TYPENAME_OUTSIDE_TEMPLATE

#pragma warning(disable : 4481) // nonstandard extension used: override specifier 'override'
#define ABSTRACT abstract
#define CONSTEXPR

// Strings.
#define LINE_TERMINATOR		TEXT("\r\n")
#define LINE_TERMINATOR_ANSI "\r\n"

// Alignment.
#define MS_ALIGN(n) __declspec(align(n))

// Pragmas
#define MSVC_PRAGMA(Pragma) __pragma(Pragma)

// DLL export and import definitions
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

#if PLATFORM_64BITS
#define UNSUPPORTED_API_FOR_WACK "D3D12GetDebugInterface"
#else
#define UNSUPPORTED_API_FOR_WACK "_D3D12GetDebugInterface@8"
#endif

#define FORCE_WACK_FAILURE(LogCategory, LogMessage) \
		__pragma(message("WARNING: " LogMessage)) \
		__pragma(message("WARNING: Deliberately inserting reference to " UNSUPPORTED_API_FOR_WACK " in order to force WACK failure.")) \
		__pragma(comment(lib, "d3d12.lib")) \
		__pragma(comment(linker, "/include:" UNSUPPORTED_API_FOR_WACK)) \
		UE_LOG(LogCategory, Warning, TEXT(LogMessage))

// disable this now as it is annoying for generic platform implementations
#pragma warning(disable : 4100) // unreferenced formal parameter