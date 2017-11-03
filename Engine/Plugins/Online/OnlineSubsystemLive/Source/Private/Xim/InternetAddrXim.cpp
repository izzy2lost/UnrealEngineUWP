#include "../OnlineSubsystemLivePrivatePCH.h"
#include "InternetAddrXim.h"

void FInternetAddrXim::SetIp(const TCHAR* InAddr, bool& IsValid)
{
	PlayerId = FUniqueNetIdLive(InAddr);
	IsValid = true;
}