#include "../OnlineSubsystemLivePrivatePCH.h"

#if USE_XIM

#include "SocketSubsystemXim.h"
#include "SocketSubsystemModule.h"
#include "InternetAddrXim.h"
#include "SocketsXim.h"
#include "OnlineSessionInterfaceXim.h"
#include "../OnlineIdentityInterfaceLive.h"
#include <xboxintegratedmultiplayer.h>

FSocketSubsystemXim* FSocketSubsystemXim::SocketSingleton = nullptr;

/**
* Create the socket subsystem for the given platform service
*/
FName CreateXimSocketSubsystem()
{
	// Create and register our singleton factory with the main online subsystem for easy access
	FSocketSubsystemXim* SocketSubsystem = FSocketSubsystemXim::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		FSocketSubsystemModule& SSS = FModuleManager::LoadModuleChecked<FSocketSubsystemModule>("Sockets");
		SSS.RegisterSocketSubsystem(LIVE_SUBSYSTEM, SocketSubsystem, true);
		return LIVE_SUBSYSTEM;
	}
	else
	{
		FSocketSubsystemXim::Destroy();
		return NAME_None;
	}
}

/**
* Tear down the socket subsystem for the given platform service
*/
void DestroyXimSocketSubsystem()
{
	FModuleManager& ModuleManager = FModuleManager::Get();

	if (ModuleManager.IsModuleLoaded("Sockets"))
	{
		FSocketSubsystemModule& SSS = FModuleManager::GetModuleChecked<FSocketSubsystemModule>("Sockets");
		SSS.UnregisterSocketSubsystem(LIVE_SUBSYSTEM);
	}
	FSocketSubsystemXim::Destroy();
}

/**
* Singleton interface for this subsystem
* @return the only instance of this subsystem
*/
FSocketSubsystemXim* FSocketSubsystemXim::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemXim();
	}

	return SocketSingleton;
}

/**
* Performs Xim specific socket clean up
*/
void FSocketSubsystemXim::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}

/**
* Does Xim platform initialization of the sockets library
*
* @param Error a string that is filled with error information
*
* @return true if initialized ok, false otherwise
*/
bool FSocketSubsystemXim::Init(FString& Error)
{
	return true;
}

/**
* Performs platform specific socket clean up
*/
void FSocketSubsystemXim::Shutdown()
{
}

/**
* Creates a socket
*
* @Param SocketType type of socket to create (DGram, Stream, etc)
* @param SocketDescription debug description
* @param bForceUDP overrides any platform specific protocol with UDP instead
*
* @return the new socket or NULL if failed
*/
FSocket* FSocketSubsystemXim::CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP)
{
	FSocket* NewSocket = nullptr;
	if (SocketType == NAME_DGram)
	{
		FOnlineSubsystemLive* LiveSubsystem = static_cast<FOnlineSubsystemLive*>(IOnlineSubsystem::Get(LIVE_SUBSYSTEM));
		FOnlineSessionXimPtr SessionInt = LiveSubsystem->GetSessionInterfaceXim();
		if (SessionInt.IsValid())
		{
			if (SessionInt->CurrentSession.IsValid())
			{
				NewSocket = new FSocketXim(LiveSubsystem, SocketDescription);
			}
		}

		return NewSocket;
	}
	else
	{
		ISocketSubsystem* PlatformSocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (PlatformSocketSub)
		{
			NewSocket = PlatformSocketSub->CreateSocket(SocketType, SocketDescription, bForceUDP);
		}
	}

	if (!NewSocket)
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}

void FSocketSubsystemXim::DestroySocket(FSocket* Socket)
{
}

ESocketErrors FSocketSubsystemXim::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
	return SE_EADDRNOTAVAIL;
}

bool FSocketSubsystemXim::GetHostName(FString& HostName)
{
	return false;
}

TSharedRef<FInternetAddr> FSocketSubsystemXim::CreateInternetAddr(uint32 Address, uint32 Port)
{
	check(Address == 0);
	check(Port == 0);
	FInternetAddrXim* XimAddr = new FInternetAddrXim();
	return MakeShareable(XimAddr);
}

bool FSocketSubsystemXim::HasNetworkDevice()
{
	return true;
}

/**
*	Get the name of the socket subsystem
* @return a string naming this subsystem
*/
const TCHAR* FSocketSubsystemXim::GetSocketAPIName() const
{
	return TEXT("XimSockets");
}

/**
* Returns the last error that has happened
*/
ESocketErrors FSocketSubsystemXim::GetLastErrorCode()
{
	return TranslateErrorCode(0);
}

/**
* Translates the platform error code to a ESocketErrors enum
*/
ESocketErrors FSocketSubsystemXim::TranslateErrorCode(int32 Code)
{
	return (ESocketErrors)0;
}

TSharedRef<FInternetAddr> FSocketSubsystemXim::GetLocalBindAddr(FOutputDevice& Out)
{
	TSharedRef<FInternetAddrXim> XimAddr = StaticCastSharedRef<FInternetAddrXim>(CreateInternetAddr());

	IOnlineSubsystem* LiveSubsystem = IOnlineSubsystem::Get(LIVE_SUBSYSTEM);
	FOnlineSessionXimPtr SessionInt = StaticCastSharedPtr<FOnlineSessionXim>(LiveSubsystem->GetSessionInterface());
	if (SessionInt.IsValid())
	{
		if (SessionInt->CurrentSession.IsValid())
		{
			XimAddr->PlayerId = FUniqueNetIdLive(SessionInt->CurrentSession->LocalOwnerId->ToString());
		}
	}
	return XimAddr;
}

#endif