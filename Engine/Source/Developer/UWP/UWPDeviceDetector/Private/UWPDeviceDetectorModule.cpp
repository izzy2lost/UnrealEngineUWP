#include "IUWPDeviceDetectorModule.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#if USE_WINRT_DEVICE_WATCHER
#include <vccorlib.h>
#include <collection.h>
#endif

namespace UWPDeviceTypes
{
	const FName Desktop = "Windows.Desktop";
	const FName Xbox = "Windows.Xbox";
}

class FUWPDeviceDetectorModule : public IUWPDeviceDetectorModule
{
public:
	FUWPDeviceDetectorModule();
	~FUWPDeviceDetectorModule();

	virtual void StartDeviceDetection();
	virtual void StopDeviceDetection();

	virtual FOnDeviceDetected& OnDeviceDetected()			{ return DeviceDetected; }
	virtual const TArray<FUWPDeviceInfo> GetKnownDevices()	{ return KnownDevices; }

private:
#if USE_WINRT_DEVICE_WATCHER
	static void DeviceWatcherDeviceAdded(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformation^ Info);
	static void DeviceWatcherDeviceRemoved(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ Info);
	static void DeviceWatcherDeviceUpdated(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ Info);
	static void DeviceWatcherDeviceEnumerationCompleted(Platform::Object^ Sender, Platform::Object^);
#endif

	void AddDevice(const FUWPDeviceInfo& Info);

	//static void OnDevicePortalProbeRequestCompleted(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

private:
	static FUWPDeviceDetectorModule* ThisModule;

#if USE_WINRT_DEVICE_WATCHER
	Windows::Devices::Enumeration::DeviceWatcher^ UwpDeviceWatcher;
#endif

	FOnDeviceDetected DeviceDetected;
	FCriticalSection DevicesLock;
	TArray<FUWPDeviceInfo> KnownDevices;
};

FUWPDeviceDetectorModule* FUWPDeviceDetectorModule::ThisModule = nullptr;

IMPLEMENT_MODULE(FUWPDeviceDetectorModule, UWPDeviceDetector);

FUWPDeviceDetectorModule::FUWPDeviceDetectorModule()
{
	// Hang on to the singleton instance of the loaded module
	// so that we can use it from an arbitrary thread context
	// (in response to Device Watcher events)
	check(!ThisModule);
	ThisModule = this;
}

FUWPDeviceDetectorModule::~FUWPDeviceDetectorModule()
{
	check(ThisModule == this);
	ThisModule = nullptr;
}

void FUWPDeviceDetectorModule::StartDeviceDetection()
{
	// Ensure local device is available even when device portal is off
	// or full device watcher is not enabled.
	if (KnownDevices.Num() == 0)
	{
		FUWPDeviceInfo LocalDevice;
		LocalDevice.HostName = FPlatformProcess::ComputerName();
		LocalDevice.Is64Bit = PLATFORM_64BITS;
		LocalDevice.RequiresCredentials = 0;
		LocalDevice.DeviceTypeName = UWPDeviceTypes::Desktop;
		ThisModule->AddDevice(LocalDevice);
	}

#if USE_WINRT_DEVICE_WATCHER
	using namespace Platform;
	using namespace Platform::Collections;
	using namespace Windows::Foundation;
	using namespace Windows::Devices::Enumeration;

	if (UwpDeviceWatcher != nullptr)
	{
		return;
	}

	// Ensure the HTTP module is initialized so it can be used from device watcher events.
	FHttpModule::Get();

	RoInitialize(RO_INIT_MULTITHREADED);

	// Start up a watcher with a filter designed to pick up devices that have Windows Device Portal enabled
	String^ AqsFilter = "System.Devices.AepService.ProtocolId:={4526e8c1-8aac-4153-9b16-55e86ada0e54} AND System.Devices.Dnssd.Domain:=\"local\" AND System.Devices.Dnssd.ServiceName:=\"_wdp._tcp\"";
	Vector<String^>^ AdditionalProperties = ref new Vector<String^>();
	AdditionalProperties->Append("System.Devices.Dnssd.HostName");
	AdditionalProperties->Append("System.Devices.Dnssd.ServiceName");
	AdditionalProperties->Append("System.Devices.Dnssd.PortNumber");
	AdditionalProperties->Append("System.Devices.Dnssd.TextAttributes");
	AdditionalProperties->Append("System.Devices.IpAddress");

	UwpDeviceWatcher = DeviceInformation::CreateWatcher(AqsFilter, AdditionalProperties, DeviceInformationKind::AssociationEndpointService);
	UwpDeviceWatcher->Added += ref new TypedEventHandler<DeviceWatcher^, DeviceInformation^>(&FUWPDeviceDetectorModule::DeviceWatcherDeviceAdded);
	UwpDeviceWatcher->Removed += ref new TypedEventHandler<DeviceWatcher^, DeviceInformationUpdate^>(&FUWPDeviceDetectorModule::DeviceWatcherDeviceRemoved);
	UwpDeviceWatcher->Updated += ref new TypedEventHandler<DeviceWatcher^, DeviceInformationUpdate^>(&FUWPDeviceDetectorModule::DeviceWatcherDeviceUpdated);
	UwpDeviceWatcher->EnumerationCompleted += ref new TypedEventHandler<DeviceWatcher^, Object^>(&FUWPDeviceDetectorModule::DeviceWatcherDeviceEnumerationCompleted);

	UwpDeviceWatcher->Start();
#endif
}

void FUWPDeviceDetectorModule::StopDeviceDetection()
{
#if USE_WINRT_DEVICE_WATCHER
	if (UwpDeviceWatcher != nullptr)
	{
		UwpDeviceWatcher->Stop();
		UwpDeviceWatcher = nullptr;
	}
#endif
}


#if USE_WINRT_DEVICE_WATCHER
void FUWPDeviceDetectorModule::DeviceWatcherDeviceAdded(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformation^ Info)
{
	using namespace Platform;
	using namespace Platform::Collections;
	using namespace Windows::Foundation;
	using namespace Windows::Devices::Enumeration;

	FUWPDeviceInfo NewDevice;
	NewDevice.WindowsDeviceId = Info->Id->Data();

	NewDevice.HostName = safe_cast<IPropertyValue^>(Info->Properties->Lookup("System.Devices.Dnssd.HostName"))->GetString()->Data();
	NewDevice.HostName.RemoveFromEnd(TEXT(".local"));

	uint32 WdpPort = safe_cast<IPropertyValue^>(Info->Properties->Lookup("System.Devices.Dnssd.PortNumber"))->GetUInt32();
	Array<String^>^ TextAttributes;
	safe_cast<IPropertyValue^>(Info->Properties->Lookup("System.Devices.Dnssd.TextAttributes"))->GetStringArray(&TextAttributes);
	FString Protocol = TEXT("http");
	FName DeviceType = NAME_None;
	for (uint32 i = 0; i < TextAttributes->Length; ++i)
	{
		FString Architecture;
		uint32 SecurePort;
		if (FParse::Value(TextAttributes[i]->Data(), TEXT("S="), SecurePort))
		{
			Protocol = TEXT("https");
			WdpPort = SecurePort;
		}
		else if (FParse::Value(TextAttributes[i]->Data(), TEXT("D="), NewDevice.DeviceTypeName))
		{

		}
		else if (FParse::Value(TextAttributes[i]->Data(), TEXT("A="), Architecture))
		{
			NewDevice.Is64Bit = (Architecture == TEXT("AMD64"));
		}
	}

	Array<String^>^ AllIps;
	safe_cast<IPropertyValue^>(Info->Properties->Lookup("System.Devices.IpAddress"))->GetStringArray(&AllIps);
	if (AllIps->Length <= 0)
	{
		return;
	}
	FString DeviceIp = AllIps[0]->Data();

	NewDevice.WdpUrl = FString::Printf(TEXT("%s://%s:%d"), *Protocol, *DeviceIp, WdpPort);

	// Now make a test request against the device to determine whether or not the Device Portal requires authentication.
	// If it does, the user will have to add it manually so we can collect the username and password.
	TSharedRef<IHttpRequest> TestRequest = FHttpModule::Get().CreateRequest();
	TestRequest->SetVerb(TEXT("GET"));
	TestRequest->SetURL(NewDevice.WdpUrl);
	TestRequest->OnProcessRequestComplete().BindLambda(
		[NewDevice](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
	{
		bool RequiresCredentials = false;
		bool ReachedDevicePortal = false;
		if (bSucceeded && HttpResponse.IsValid())
		{
			int32 ResponseCode = HttpResponse->GetResponseCode();
			RequiresCredentials = ResponseCode == EHttpResponseCodes::Denied;
			ReachedDevicePortal = RequiresCredentials || EHttpResponseCodes::IsOk(ResponseCode);
		}

		if (ReachedDevicePortal || NewDevice.IsLocal())
		{
			FUWPDeviceInfo ValidatedDevice = NewDevice;
			ValidatedDevice.RequiresCredentials = RequiresCredentials;
			ThisModule->AddDevice(ValidatedDevice);
		}
	});
	TestRequest->ProcessRequest();
}

void FUWPDeviceDetectorModule::DeviceWatcherDeviceRemoved(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ Info)
{
	// This doesn't seem to hit even when the remote device goes offline
}

void FUWPDeviceDetectorModule::DeviceWatcherDeviceUpdated(Platform::Object^ Sender, Windows::Devices::Enumeration::DeviceInformationUpdate^ Info)
{

}

void FUWPDeviceDetectorModule::DeviceWatcherDeviceEnumerationCompleted(Platform::Object^ Sender, Platform::Object^)
{

}
#endif

void FUWPDeviceDetectorModule::AddDevice(const FUWPDeviceInfo& Info)
{
	FScopeLock Lock(&DevicesLock);
	for (const FUWPDeviceInfo& Existing : KnownDevices)
	{
		if (Existing.HostName == Info.HostName)
		{
			return;
		}
	}

	KnownDevices.Add(Info);
	DeviceDetected.Broadcast(Info);
}

#include "Windows/HideWindowsPlatformTypes.h"