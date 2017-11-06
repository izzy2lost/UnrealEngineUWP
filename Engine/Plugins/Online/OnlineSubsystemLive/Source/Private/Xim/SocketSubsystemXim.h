#pragma once

#include "SocketSubsystem.h"
#include "SocketSubsystemModule.h"
#include "UObject/WeakObjectPtr.h"
#include "IPAddress.h"
#include "OnlineSubsystemLivePackage.h"

class FSocketXim;

namespace xbox
{
	namespace services
	{
		namespace xbox_integrated_multiplayer
		{
			class xim_player;
		}
	}
}

class FSocketSubsystemXim : public ISocketSubsystem
{
PACKAGE_SCOPE:
	static FSocketSubsystemXim* Create();
	static void Destroy();

private:
	static FSocketSubsystemXim* SocketSingleton;

public:
	/**
	* Does initialization of the sockets library
	*
	* @param Error a string that is filled with error information
	*
	* @return true if initialized ok, false otherwise
	*/
	virtual bool Init(FString& Error) override;

	/**
	* Performs platform specific socket clean up
	*/
	virtual void Shutdown() override;

	/**
	* Creates a socket
	*
	* @Param SocketType type of socket to create (DGram, Stream, etc)
	* @param SocketDescription debug description
	* @param bForceUDP overrides any platform specific protocol with UDP instead
	*
	* @return the new socket or NULL if failed
	*/
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false) override;

	/**
	* Cleans up a socket class
	*
	* @param Socket the socket object to destroy
	*/
	virtual void DestroySocket(class FSocket* Socket) override;

	/**
	* Does a DNS look up of a host name
	*
	* @param HostName the name of the host to look up
	* @param OutAddr the address to copy the IP address to
	*/
	virtual ESocketErrors GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr) override;

	/**
	* Some platforms require chat data (voice, text, etc.) to be placed into
	* packets in a special way. This function tells the net connection
	* whether this is required for this platform
	*/
	virtual bool RequiresChatDataBeSeparate() override
	{
		return true;
	}

	/**
	* Some platforms require packets be encrypted. This function tells the
	* net connection whether this is required for this platform
	*/
	virtual bool RequiresEncryptedPackets() override
	{
		// Encryption dealt with internally
		return false;
	}

	/**
	* Determines the name of the local machine
	*
	* @param HostName the string that receives the data
	*
	* @return true if successful, false otherwise
	*/
	virtual bool GetHostName(FString& HostName) override;

	/**
	*	Create a proper FInternetAddr representation
	* @param Address host address
	* @param Port host port
	*/
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address = 0, uint32 Port = 0) override;

	/**
	* @return Whether the machine has a properly configured network device or not
	*/
	virtual bool HasNetworkDevice() override;

	/**
	*	Get the name of the socket subsystem
	* @return a string naming this subsystem
	*/
	virtual const TCHAR* GetSocketAPIName() const override;

	/**
	* Returns the last error that has happened
	*/
	virtual ESocketErrors GetLastErrorCode() override;

	/**
	* Translates the platform error code to a ESocketErrors enum
	*/
	virtual ESocketErrors TranslateErrorCode(int32 Code) override;

	/**
	* Gets the list of addresses associated with the adapters on the local computer.
	*
	* @param OutAdresses - Will hold the address list.
	*
	* @return true on success, false otherwise.
	*/
	virtual bool GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr> >& OutAdresses) override
	{
		bool bCanBindAll;

		OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));

		return true;
	}

	/**
	*	Get local IP to bind to
	*/
	virtual TSharedRef<FInternetAddr> GetLocalBindAddr(FOutputDevice& Out) override;
};

FName CreateXimSocketSubsystem();
void DestroyXimSocketSubsystem();