// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Horde.Agents;

/// <summary>
/// Well-known property names for agents
/// </summary>
public static class KnownPropertyNames
{
	/// <summary>
	/// The agent id
	/// </summary>
	public const string Id = "Id";
	
	/// <summary>
	/// Handle of the primary device (usually the agent VM itself)
	/// </summary>
	public const string PrimaryDeviceHandle = "Primary";

	/// <summary>
	/// The operating system (Linux, MacOS, Windows)
	/// </summary>
	public const string OsFamily = "OSFamily";

	/// <summary>
	/// Whether the agent is a .NET self-contained app
	/// </summary>
	public const string SelfContained = "SelfContained";

	/// <summary>
	/// Pools that this agent belongs to
	/// </summary>
	public const string Pool = "Pool";

	/// <summary>
	/// Pools requested by the agent to join when registering with server
	/// </summary>
	public const string RequestedPools = "RequestedPools";

	/// <summary>
	/// Number of logical cores
	/// </summary>
	public const string LogicalCores = "LogicalCores";

	/// <summary>
	/// Amount of RAM, in GB
	/// </summary>
	public const string Ram = "RAM";
	
	/// <summary>
	/// The total size of storage space on drive, in bytes
	/// </summary>
	public const string DiskTotalSize = "DiskTotalSize";

	/// <summary>
	/// Amount of available free space on drive, in bytes
	/// </summary>
	public const string DiskFreeSpace = "DiskFreeSpace";

	/// <summary>
	/// IP address used for sending compute task payloads
	/// </summary>
	public const string ComputeIp = "ComputeIp";
		
	/// <summary>
	/// Port used for sending compute task payloads
	/// </summary>
	public const string ComputePort = "ComputePort";

	/// <summary>
	/// AWS: Instance ID
	/// </summary>
	public const string AwsInstanceId = "aws-instance-id";

	/// <summary>
	/// AWS: Instance type
	/// </summary>
	public const string AwsInstanceType = "aws-instance-type";
	
	/// <summary>
	/// Whether the Wine compatibility layer is enabled (for running Windows applications on Linux)
	/// </summary>
	public const string WineEnabled = "WineEnabled";
}