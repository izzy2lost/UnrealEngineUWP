#pragma once

#include "IPAddress.h"
#include "../OnlineSubsystemLiveTypes.h"

class FInternetAddrXim : public FInternetAddr
{
private:
	// Port is meaningless, but we must pick something
	// that's meaningless but valid (0 is invalid)
	static const int32 FakeXimPort = 1;

PACKAGE_SCOPE:
	FUniqueNetIdLive PlayerId;



public:
	virtual void SetIp(uint32 InAddr) override
	{
		check("Not supported for XIM addresses" && 0);
	}

	virtual void SetIp(const TCHAR* InAddr, bool& IsValid) override;

	virtual void GetIp(uint32& OutAddr) const override
	{
		check("Not supported for XIM addresses" && 0);
	}

	virtual void SetPort(int32 InPort) override
	{
	}

	virtual void GetPort(int32& OutPort) const override
	{
		OutPort = FakeXimPort;
	}

	virtual int32 GetPort() const override
	{
		return FakeXimPort;
	}

	virtual void SetAnyAddress() override
	{
		check("Not supported for XIM addresses" && 0);
	}

	virtual void SetBroadcastAddress() override
	{
		check("Not supported for XIM addresses" && 0);
	}

	virtual FString ToString(bool bAppendPort) const override
	{
		return PlayerId.ToString();
	}

	virtual bool operator==(const FInternetAddr& Other) const override
	{
		const FInternetAddrXim& XimOther = static_cast<const FInternetAddrXim&>(Other);
		return PlayerId == XimOther.PlayerId;
	}

	virtual bool IsValid() const override
	{
		return PlayerId.IsValid();
	}
};
