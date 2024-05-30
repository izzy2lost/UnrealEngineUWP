// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace JobDriver.Execution;

/// <summary>
/// Exception for workspace materializer
/// </summary>
public class WorkspaceMaterializationException : Exception
{
	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="message"></param>
	public WorkspaceMaterializationException(string? message) : base(message)
	{
	}

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="message"></param>
	/// <param name="innerException"></param>
	public WorkspaceMaterializationException(string? message, Exception? innerException) : base(message, innerException)
	{
	}
}

/// <summary>
/// Options passed to SyncAsync
/// </summary>
public class SyncOptions
{
	/// <summary>
	/// Remove any files not referenced by changelist
	/// </summary>
	public bool RemoveUntracked { get; set; }

	/// <summary>
	/// If true, skip syncing actual file data and instead create empty placeholder files.
	/// Used for testing.
	/// </summary>
	public bool FakeSync { get; set; }
}

/// <summary>
/// Interface for materializing a file tree to the local file system.
/// One instance roughly equals one Perforce stream.
/// </summary>
public interface IWorkspaceMaterializer : IDisposable
{
	/// <summary>
	/// Placeholder for resolving the latest available change number of stream during sync
	/// </summary>
	public const int LatestChangeNumber = -2;

	/// <summary>
	/// Path to local file system directory where files from changelist are materialized
	/// </summary>
	DirectoryReference DirectoryPath { get; }

	/// <summary>
	/// Identifier for this workspace
	/// </summary>
	string Identifier { get; }

	/// <summary>
	/// Environment variables expected to be set for applications executing inside the workspace
	/// Mostly intended for Perforce-specific variables when <see cref="IsPerforceWorkspace" /> is set to true
	/// </summary>
	IReadOnlyDictionary<string, string> EnvironmentVariables { get; }

	/// <summary>
	/// Whether the materialized workspace is a true Perforce workspace
	/// This flag is provided as a stop-gap solution to allow replacing ManagedWorkspace with WorkspaceMaterializer.
	/// It's *highly* recommended to set this to false for any new implementations of IWorkspaceMaterializer.
	/// </summary>
	bool IsPerforceWorkspace { get; }

	/// <summary>
	/// Materialize (or sync) a Perforce stream at a given change number
	/// Once method has completed, file tree is available on disk.
	/// </summary>
	/// <param name="changeNum">Change number to materialize</param>
	/// <param name="preflightChangeNum">Preflight change number to add</param>
	/// <param name="options">Additional options</param>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <exception cref="WorkspaceMaterializationException">Thrown if syncing fails</exception>
	/// <returns>Async task</returns>
	Task SyncAsync(int changeNum, int preflightChangeNum, SyncOptions options, CancellationToken cancellationToken);

	/// <summary>
	/// Finalize and clean file system
	/// </summary>
	/// <param name="cancellationToken">Cancellation token for the call</param>
	/// <returns>Async task</returns>
	Task FinalizeAsync(CancellationToken cancellationToken);
}

/// <summary>
/// Factory for creating new workspace materializers
/// </summary>
public interface IWorkspaceMaterializerFactory
{
	/// <summary>
	/// Creates a new workspace materializer instance
	/// </summary>
	/// <param name="name">Name of the materializer to create</param>
	/// <param name="workspaceInfo">Agent workspace</param>
	/// <param name="workspaceDir">Directory for the workspace</param>
	/// <param name="forAutoSdk">Whether intended for AutoSDK materialization</param>
	/// <param name="cancellationToken">Cancellation token for the operation</param>
	/// <returns>A new workspace materializer instance</returns>
	Task<IWorkspaceMaterializer?> CreateMaterializerAsync(string name, RpcAgentWorkspace workspaceInfo, DirectoryReference workspaceDir, bool forAutoSdk = false, CancellationToken cancellationToken = default);
}

class WorkspaceMaterializerFactory : IWorkspaceMaterializerFactory
{
	readonly IServiceProvider _serviceProvider;

	public WorkspaceMaterializerFactory(IServiceProvider serviceProvider)
		=> _serviceProvider = serviceProvider;

	/// <inheritdoc/>
	public async Task<IWorkspaceMaterializer?> CreateMaterializerAsync(string name, RpcAgentWorkspace workspaceInfo, DirectoryReference workspaceDir, bool forAutoSdk, CancellationToken cancellationToken)
	{
		if (name.Equals(ManagedWorkspaceMaterializer.Name, StringComparison.OrdinalIgnoreCase))
		{
			if (forAutoSdk)
			{
				return await ManagedWorkspaceMaterializer.CreateAsync(workspaceInfo, workspaceDir, true, false, _serviceProvider.GetRequiredService<ILogger<ManagedWorkspaceMaterializer>>(), cancellationToken);
			}
			else
			{
				return await ManagedWorkspaceMaterializer.CreateAsync(workspaceInfo, workspaceDir, false, true, _serviceProvider.GetRequiredService<ILogger<ManagedWorkspaceMaterializer>>(), cancellationToken);
			}
		}
		return null;
	}
}
