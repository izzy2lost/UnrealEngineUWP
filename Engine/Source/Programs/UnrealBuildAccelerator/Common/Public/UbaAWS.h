// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS

#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaLogger.h"

#include <winhttp.h> 
#pragma comment (lib, "Winhttp.lib")

#define UBA_USE_AWS

namespace uba
{
	struct HttpRequest
	{
		bool Init(Logger& logger, HINTERNET connection, const wchar_t* verb, const wchar_t* objectName)
		{
			m_connection = connection;
			m_verb = verb;
			m_objectName = objectName;
			m_request = WinHttpOpenRequest(m_connection, verb, objectName, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_REFRESH);

			return m_request != NULL;
		}

		~HttpRequest()
		{
			if (m_request)
				WinHttpCloseHandle(m_request);
		}

		bool Send(Logger& logger, StringBufferBase& outContent, const wchar_t* headers = nullptr, u32* outStatusCode = nullptr)
		{
			if (!m_request)
				return false;

			if (!WinHttpSendRequest(m_request, headers ? headers : WINHTTP_NO_ADDITIONAL_HEADERS, headers ? u32(wcslen(headers)) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
			{
				DWORD err = GetLastError();

				WinHttpCloseHandle(m_request);
				m_request = NULL;

				if (err == ERROR_WINHTTP_CANNOT_CONNECT || err == ERROR_WINHTTP_TIMEOUT)
					return false;

				if (!Init(logger, m_connection, m_verb, m_objectName))
					return logger.Error(L"HttpOpenRequestW reinit error: %ls", LastErrorToText().data);

				if (!WinHttpSendRequest(m_request, headers ? headers : WINHTTP_NO_ADDITIONAL_HEADERS, headers ? u32(wcslen(headers)) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
				{
					err = GetLastError();
					if (err != ERROR_WINHTTP_CANNOT_CONNECT && err != ERROR_WINHTTP_TIMEOUT)
						if (headers) // hack to prevent log for non-aws machines
							logger.Info(L"WinHttpSendRequest error: %ls", LastErrorToText().data);

					WinHttpCloseHandle(m_request);
					m_request = NULL;
					return false;
				}
			}

			if (!WinHttpReceiveResponse(m_request, NULL))
			{
				WinHttpCloseHandle(m_request);
				m_request = NULL;
				return false;
			}

			if (outStatusCode)
			{
				DWORD bufferLen = sizeof(u32);
				if (!WinHttpQueryHeaders(m_request, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, outStatusCode, &bufferLen, WINHTTP_NO_HEADER_INDEX))
					return logger.Error(L"Error %u in WinHttpQueryHeaders", GetLastError());
			}

			for (u32 i=0; i!=100; ++i)
			{
				// Check for available data.
				DWORD dwSize = 0;
				if (!WinHttpQueryDataAvailable(m_request, &dwSize))
					return logger.Error(L"Error %u in WinHttpQueryDataAvailable", GetLastError());

				if (dwSize == 0)
					return true;

				CHAR szBuffer[4097];
				DWORD dwDownloaded = 0;
				if (!WinHttpReadData(m_request, (LPVOID)szBuffer, dwSize, &dwDownloaded))
					return logger.Error(L"Error %u in WinHttpReadData", GetLastError());
				szBuffer[dwDownloaded] = 0;
				outContent.Appendf(L"%hs", szBuffer);
			}
			return logger.Error(L"Unknown error reading http query data", GetLastError());
		}

		HINTERNET m_connection = NULL;
		HINTERNET m_request = NULL;
		const wchar_t* m_verb = nullptr;
		const wchar_t* m_objectName = nullptr;
	};

	class AWS
	{
	public:
		~AWS()
		{
			if (m_connection)
				WinHttpCloseHandle(m_connection);
			if (m_session)
				WinHttpCloseHandle(m_session);
		}

		bool InitCore(Logger& logger, const wchar_t* application)
		{
			m_session = WinHttpOpen(L"AWS", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!m_session)
				return logger.Error(L"WinHttpOpen failed: %ls", LastErrorToText().data);
			auto internetGuard = MakeGuard([&]() { WinHttpCloseHandle(m_session); m_session = NULL; });

			if (!WinHttpSetTimeouts(m_session, 100, 100, 100, 100))
				return logger.Error(L"WinHttpSetTimeouts failed: %ls", LastErrorToText().data);

			m_connection = WinHttpConnect(m_session, L"169.254.169.254", INTERNET_DEFAULT_HTTP_PORT, 0);
			if (!m_connection)
				return logger.Error(L"WinHttpConnect failed: %ls", LastErrorToText().data);
			auto connectionGuard = MakeGuard([&]() { WinHttpCloseHandle(m_connection); m_connection = NULL; });

			HttpRequest tokenPut;
			if (!tokenPut.Init(logger, m_connection, L"PUT", L"latest/api/token"))
				return false;
			StringBuffer<512> awsToken;
			awsToken.Append(L"X-aws-ec2-metadata-token: ");
			if (!tokenPut.Send(logger, awsToken, L"X-aws-ec2-metadata-token-ttl-seconds: 21600")) // Expires after 6 hours
				return false;
			if (awsToken.IsEmpty())
				return false;

			connectionGuard.Cancel();
			internetGuard.Cancel();

			WinHttpSetTimeouts(m_session, 1000, 1000, 1000, 1000);

			m_awsToken = awsToken.data;
			return true;
		}

		bool Init(Logger& logger, StringBufferBase& outExtraInfo, const wchar_t* application)
		{
			if (!InitCore(logger, application))
				return false;

			HttpRequest instanceIdReq;
			if (instanceIdReq.Init(logger, m_connection, L"GET", L"latest/meta-data/instance-id"))
			{
				outExtraInfo.Append(L", AWS: ");
				if (!instanceIdReq.Send(logger, outExtraInfo, m_awsToken.c_str()))
					outExtraInfo.Clear();
			}

			HttpRequest lifeCycleReq;
			if (lifeCycleReq.Init(logger, m_connection, L"GET", L"latest/meta-data/instance-life-cycle"))
			{
				StringBuffer<32> instanceLifeCycle;
				if (lifeCycleReq.Send(logger, instanceLifeCycle, m_awsToken.c_str()))
				{
					outExtraInfo.Append(' ').Append(instanceLifeCycle);
					m_isSpot = instanceLifeCycle.Contains(L"spot");
				}
			}

			if (m_autoscalingRequest.Init(logger, m_connection, L"GET", L"latest/meta-data/autoscaling/target-lifecycle-state"))
			{
				StringBuffer<32> instanceLifeCycle;
				u32 statusCode = 0;
				if (m_autoscalingRequest.Send(logger, instanceLifeCycle, m_awsToken.c_str(), &statusCode) && statusCode == 200)
				{
					outExtraInfo.Append(m_isSpot ? '/' : ' ').Append(L"autoscale");
					m_isAutoscaling = true;
				}
			}

			if (!InitAvailabilityZone(logger))
				return false;

			return true;
		}

		bool InitAvailabilityZone(Logger& logger)
		{
			HttpRequest availabilityZoneReq;
			if (!availabilityZoneReq.Init(logger, m_connection, L"GET", L"latest/meta-data/placement/availability-zone"))
				return false;
			StringBuffer<256> availabilityZone;
			if (!availabilityZoneReq.Send(logger, availabilityZone, m_awsToken.c_str()))
				return false;
			m_availabilityZone = availabilityZone.data;
			return true;
		}

		bool InitPolling(Logger& logger)
		{
			if (m_isSpot)
				m_instanceActionRequest.Init(logger, m_connection, L"GET", L"latest/meta-data/spot/instance-action");
			return true;
		}

		bool IsTerminating(Logger& logger, StringBufferBase& outReason, u64& outTerminationTimeMs)
		{
			outTerminationTimeMs = 0;
			if (m_isSpot)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (m_instanceActionRequest.Send(logger, content, m_awsToken.c_str(), &statusCode) && statusCode == 200)
				{
					outReason.Append(L"AWS spot instance interruption");
					return true;
				}
			}

			if (m_isAutoscaling)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (m_autoscalingRequest.Send(logger, content, m_awsToken.c_str(), &statusCode) && statusCode == 200)
				{
					//if (!content.Equals(L"InService"))
					//{
					//	wprintf(L"AWSACTION: AUTOSCALE: %ls\n", content.data);
					//}

					if (!content.Contains(L"InService")) // AWS can return "InServiceI" as well?
					{
						//wprintf(L"AWSACTION: AUTOSCALE REBALANCING!!!! (%ls)\n", content.data);
						outReason.Append(L"AWS autoscale rebalancing");
						return true;
					}
				}
			}
			return false;
		}

		const wchar_t* GetAvailabilityZone()
		{
			return m_availabilityZone.c_str();
		}

	private:
		HINTERNET m_session = NULL;
		HINTERNET m_connection = NULL;
		TString m_awsToken;
		TString m_availabilityZone;
		HttpRequest m_autoscalingRequest;
		HttpRequest m_instanceActionRequest;

		bool m_isSpot = false;
		bool m_isAutoscaling = false;
	};
}

#endif // PLATFORM_WINDOWS
