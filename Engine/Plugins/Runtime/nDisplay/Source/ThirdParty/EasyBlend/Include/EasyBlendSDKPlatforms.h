#ifndef _EasyBlendSDKPlatforms_H_
#define _EasyBlendSDKPlatforms_H_

// The following ifdef block is the standard way of creating macros
// which make exporting from a DLL simpler. All files within this
// DLL are compiled with the EasyBlendSDK_EXPORTS symbol defined on
// the command line. this symbol should not be defined on any
// project that uses this DLL. This way any other project whose
// source files include this file see EasyBlendSDK_API functions as
// being imported from a DLL, whereas this DLL sees symbols defined
// with this macro as being exported.

#if  !defined(_EASYBLENDSDK_LINUX) && !defined(_EASYBLENDSDK_STATIC) 
  #ifdef MESHSDK_EXPORTS
  #  define EasyBlendSDK_API extern "C" __declspec(dllexport)
  #else
  #  define EasyBlendSDK_API extern "C" __declspec(dllimport)
  #endif /* ifdef MESHSDK_EXPORTS */
#else
  #  define EasyBlendSDK_API extern "C"
#endif /* ifndef _EASYBLENDSDK_LINUX */


// methods for deprecated functions
#if  !defined(_EASYBLENDSDK_LINUX)
#  define EASYBLENDSDK_DEPRECATED_CALL(txt,func)       \
          __declspec(deprecated(txt)) func
#else
#  define EASYBLENDSDK_DEPRECATED_CALL(txt,func)        \
          func __attribute__ ((deprecated))
#endif /* ifndef _EASYBLENDSDK_LINUX */

#if   defined EASYBLENDSDK_GRAPHICS_API_OGL   //OpenGL entry points
#pragma message ("Using EasyBlendSDK in OpenGL Mode.")
#elif defined EASYBLENDSDK_GRAPHICS_API_DX12  //DX12 entry points
#pragma message ("Using EasyBlendSDK in DX12 Mode.")
#elif defined EASYBLENDSDK_GRAPHICS_API_VK
#pragma message ("Using EasyBlendSDK in Vulkan Mode.")
#elif defined EASYBLENDSDK_GRAPHICS_API_NONE  //no graphics API entry points (data only mode)
#pragma message ("Using EasyBlendSDK in Data Only Mode.")
#else
  #if defined(_WIN32) || defined(WIN32)
    #pragma message ("Using EasyBlendSDK with all Graphics APIs.")
    #define EASYBLENDSDK_GRAPHICS_API_OGL  //entry points for all graphics APIs
    #define EASYBLENDSDK_GRAPHICS_API_DX12
    //#define EASYBLENDSDK_GRAPHICS_API_VK
  #else
    #pragma message ("Using EasyBlendSDK in OpenGL Mode.")
    #define EASYBLENDSDK_GRAPHICS_API_OGL  //entry points for all graphics APIs
  #endif
#endif

#endif // _EasyBlendSDKPlatforms_H_
