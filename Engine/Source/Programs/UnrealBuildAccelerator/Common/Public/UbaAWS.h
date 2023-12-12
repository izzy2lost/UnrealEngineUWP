// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileAccessor.h"
#include "UbaNetworkBackend.h"
#include "UbaStringBuffer.h"

#define UBA_USE_AWS !PLATFORM_MAC

#if UBA_USE_AWS

namespace uba
{
	class AWS
	{
	public:

		static constexpr char g_imdsHost[]						= "169.254.169.254";
		static constexpr char g_imdsInstanceId[]				= "latest/meta-data/instance-id";
		static constexpr char g_imdsInstanceLifeCycle[]			= "latest/meta-data/instance-life-cycle";
		static constexpr char g_imdsAutoScalingLifeCycleState[] = "latest/meta-data/autoscaling/target-lifecycle-state";
		static constexpr char g_imdsInstanceAvailabilityZone[]	= "latest/meta-data/placement/availability-zone";
		static constexpr char g_imdsSpotInstanceAction[]		= "latest/meta-data/spot/instance-action";


		bool QueryInformation(Logger& logger, StringBufferBase& outExtraInfo, const tchar* rootDir)
		{
			if (IsNotAWS(logger, rootDir))
				return false;

			HttpConnection http;

			u32 statusCode = 0;

			StringBuffer<128> instanceId;
			if (!http.Get(logger, instanceId, statusCode, g_imdsHost, g_imdsInstanceId))
				return WriteIsNotAws(logger, rootDir);

			outExtraInfo.Append(TC(", AWS: ")).Append(instanceId);

			StringBuffer<32> instanceLifeCycle;
			if (http.Get(logger, instanceLifeCycle, statusCode, g_imdsHost, g_imdsInstanceLifeCycle))
			{
				outExtraInfo.Append(' ').Append(instanceLifeCycle);
				m_isSpot = instanceLifeCycle.Contains(TC("spot"));
			}

			StringBuffer<32> autoscaling;
			if (http.Get(logger, autoscaling, statusCode, g_imdsHost, g_imdsAutoScalingLifeCycleState) && statusCode == 200)
			{
				outExtraInfo.Append(m_isSpot ? '/' : ' ').Append(TC("autoscale"));
				m_isAutoscaling = true;
			}

			if (!QueryAvailabilityZone(logger, nullptr))
				return false;

			return true;
		}

		bool QueryAvailabilityZone(Logger& logger, const tchar* rootDir)
		{
			if (rootDir && IsNotAWS(logger, rootDir))
				return false;

			HttpConnection http;

			StringBuffer<128> availabilityZone;
			u32 statusCode = 0;
			if (!http.Get(logger, availabilityZone, statusCode, g_imdsHost, g_imdsInstanceAvailabilityZone))
			{
				if (rootDir)
					WriteIsNotAws(logger, rootDir);
				return false;
			}
			m_availabilityZone = availabilityZone.data;
			return true;
		}

		// Returns true if we _know_ we are not in AWS
		bool IsNotAWS(Logger& logger, const tchar* rootDir)
		{
			StringBuffer<512> file;
			file.Append(rootDir).EnsureEndsWithSlash().Append(".isNotAWS");
			if (FileExists(logger, file.data))
				return true;
			return false;
		}

		bool WriteIsNotAws(Logger& logger, const tchar* rootDir)
		{
			StringBuffer<512> file;
			file.Append(rootDir).EnsureEndsWithSlash().Append(".isNotAWS");
			FileAccessor f(logger, file.data);
			if (!f.CreateWrite())
				return false;
			if (!f.Close())
				return false;
			return false;
		}

		bool IsTerminating(Logger& logger, StringBufferBase& outReason, u64& outTerminationTimeMs)
		{
			HttpConnection http;

			outTerminationTimeMs = 0;
			if (m_isSpot)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (http.Get(logger, content, statusCode, g_imdsHost, g_imdsSpotInstanceAction) && statusCode == 200)
				{
					outReason.Append(TC("AWS spot instance interruption"));
					return true;
				}
			}

			if (m_isAutoscaling)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (http.Get(logger, content, statusCode, g_imdsHost, g_imdsAutoScalingLifeCycleState) && statusCode == 200)
				{
					//if (!content.Equals(L"InService"))
					//{
					//	wprintf(L"AWSACTION: AUTOSCALE: %ls\n", content.data);
					//}

					if (!content.Contains(TC("InService"))) // AWS can return "InServiceI" as well?
					{
						//wprintf(L"AWSACTION: AUTOSCALE REBALANCING!!!! (%ls)\n", content.data);
						outReason.Append(TC("AWS autoscale rebalancing"));
						return true;
					}
				}
			}
			return false;
		}

		const tchar* GetAvailabilityZone()
		{
			return m_availabilityZone.c_str();
		}

		TString m_availabilityZone;
		bool m_isSpot = false;
		bool m_isAutoscaling = false;
	};
}

#endif
