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

	/**
	* Copy Constructor
	*/
	FInternetAddrXim(const FInternetAddrXim& Src) :
		PlayerId(Src.PlayerId)
	{
	}

public:
	/**
	* Constructor. Sets address to default state
	*/
	FInternetAddrXim() :
		PlayerId(nullptr)
	{
	}

	/**
	* Constructor
	*/
	explicit FInternetAddrXim(const FUniqueNetIdLive& InPlayerId) :
		PlayerId(InPlayerId)
	{
	}

	virtual TArray<uint8> GetRawIp() const override
	{
		check("Not supported for XIM addresses" && 0);
	}

	virtual void SetRawIp(const TArray<uint8>& RawAddr) override
	{
		check("Not supported for XIM addresses" && 0);
	}

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

	virtual void SetLoopbackAddress() override
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

	virtual uint32 GetTypeHash() override
	{
		return ::GetTypeHash(*(uint64*)PlayerId.GetBytes());
	}

	virtual bool IsValid() const override
	{
		return PlayerId.IsValid();
	}

	virtual TSharedRef<FInternetAddr> Clone() const override
	{
		TSharedRef<FInternetAddrXim> NewAddress = MakeShareable(new FInternetAddrXim);
		NewAddress->PlayerId = PlayerId;
		return NewAddress;
	}
};
