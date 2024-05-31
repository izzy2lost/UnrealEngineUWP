// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce.Managed;
using JobDriver.Utility;
using HordeCommon.Rpc.Messages;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;

namespace JobDriver.Execution;

/// <summary>
/// Workspace materializer wrapping ManagedWorkspace and WorkspaceInfo
/// </summary>
public sealed class ManagedWorkspaceMaterializer : IWorkspaceMaterializer
{
	/// <summary>
	/// Name of this materializer
	/// </summary>
	public const string Name = "ManagedWorkspace";

	private readonly RpcAgentWorkspace _agentWorkspace;
	private readonly bool _useCacheFile;
	private readonly bool _cleanDuringFinalize;
	private readonly WorkspaceInfo _workspace;
	private readonly ILogger _logger;

	/// <inheritdoc/>
	public DirectoryReference DirectoryPath => _workspace.WorkspaceDir;

	/// <inheritdoc/>
	public string Identifier => _agentWorkspace.Identifier;

	/// <inheritdoc/>
	public IReadOnlyDictionary<string, string> EnvironmentVariables { get; }

	/// <inheritdoc/>
	public bool IsPerforceWorkspace => true;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="agentWorkspace">Workspace configuration</param>
	/// <param name="useCacheFile">Whether to use a cache file during syncs</param>
	/// <param name="cleanDuringFinalize">Whether to clean and revert files during finalize</param>
	/// <param name="workspace"></param>
	/// <param name="logger"></param>
	private ManagedWorkspaceMaterializer(
		RpcAgentWorkspace agentWorkspace,
		bool useCacheFile,
		bool cleanDuringFinalize,
		WorkspaceInfo workspace,
		ILogger logger)
	{
		_agentWorkspace = agentWorkspace;
		_useCacheFile = useCacheFile;
		_cleanDuringFinalize = cleanDuringFinalize;
		_workspace = workspace;
		_logger = logger;

		// Variables expected to be set for UAT/BuildGraph when Perforce is enabled (-P4 flag is set) 
		EnvironmentVariables = new Dictionary<string, string>()
		{
			["uebp_PORT"] = _workspace.ServerAndPort,
			["uebp_USER"] = _workspace.UserName,
			["uebp_CLIENT"] = _workspace.ClientName,
			["uebp_CLIENT_ROOT"] = $"//{_workspace.ClientName}",
			["P4USER"] = _workspace.UserName,
			["P4CLIENT"] = _workspace.ClientName
		};
	}

	/// <inheritdoc/>
	public void Dispose()
	{
	}

	/// <inheritdoc/>
	public static async Task<ManagedWorkspaceMaterializer> CreateAsync(RpcAgentWorkspace agentWorkspace, DirectoryReference workingDir, bool useCacheFile, bool cleanDuringFinalize, ILogger logger, CancellationToken cancellationToken)
	{
		ManagedWorkspaceOptions options = WorkspaceInfo.GetMwOptions(agentWorkspace);
		WorkspaceInfo workspace = await WorkspaceInfo.CreateWorkspaceInfoAsync(agentWorkspace, workingDir, options, logger, cancellationToken);
		return new ManagedWorkspaceMaterializer(agentWorkspace, useCacheFile, cleanDuringFinalize, workspace, logger);
	}

	/// <inheritdoc/>
	public async Task SyncAsync(int changeNum, int preflightChangeNum, SyncOptions options, CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.SyncAsync");
		scope.Span.SetTag("ChangeNum", changeNum);
		scope.Span.SetTag("RemoveUntracked", options.RemoveUntracked);

		using IPerforceConnection perforce = await PerforceConnection.CreateAsync(_workspace.PerforceSettings, _logger);
		await _workspace.SetupWorkspaceAsync(perforce, cancellationToken);

		if (changeNum == IWorkspaceMaterializer.LatestChangeNumber)
		{
			int latestChangeNum = await _workspace.GetLatestChangeAsync(perforce, cancellationToken);
			scope.Span.SetTag("LatestChangeNum", latestChangeNum);
			changeNum = latestChangeNum;
		}

		FileReference cacheFile = FileReference.Combine(_workspace.MetadataDir, "Contents.dat");
		if (_useCacheFile)
		{
			bool isSyncedDataDirty = await _workspace.UpdateLocalCacheMarkerAsync(cacheFile, changeNum, preflightChangeNum);
			scope.Span.SetTag("IsSyncedDataDirty", isSyncedDataDirty);
			if (!isSyncedDataDirty)
			{
				return;
			}
		}
		else
		{
			WorkspaceInfo.RemoveLocalCacheMarker(cacheFile);
		}

		await _workspace.SyncAsync(perforce, changeNum, preflightChangeNum, cacheFile, cancellationToken);
	}

	/// <inheritdoc/>
	public async Task FinalizeAsync(CancellationToken cancellationToken)
	{
		using IScope scope = CreateTraceSpan("ManagedWorkspaceMaterializer.FinalizeAsync");

		if (_workspace != null && _cleanDuringFinalize)
		{
			using IPerforceConnection perforceClient = await PerforceConnection.CreateAsync(_workspace.PerforceSettings, _logger);
			await _workspace.CleanAsync(perforceClient, cancellationToken);
		}
	}

	/// <summary>
	/// Get info for Perforce workspace
	/// </summary>
	/// <returns>Workspace info</returns>
	public WorkspaceInfo? GetWorkspaceInfo()
	{
		return _workspace;
	}

	private IScope CreateTraceSpan(string operationName)
	{
		IScope scope = GlobalTracer.Instance.BuildSpan(operationName).WithTag("resource.name", _agentWorkspace.Identifier).StartActive();
		scope.Span.SetTag("UseHaveTable", WorkspaceInfo.ShouldUseHaveTable(_agentWorkspace.Method));
		scope.Span.SetTag("Cluster", _agentWorkspace.Cluster);
		scope.Span.SetTag("Incremental", _agentWorkspace.Incremental);
		scope.Span.SetTag("Method", _agentWorkspace.Method);
		scope.Span.SetTag("Stream", _agentWorkspace.Stream);
		scope.Span.SetTag("Partitioned", _agentWorkspace.Partitioned);
		scope.Span.SetTag("UseCacheFile", _useCacheFile);
		scope.Span.SetTag("CleanDuringFinalize", _cleanDuringFinalize);
		return scope;
	}
}

class ManagedWorkspaceMaterializerFactory : IWorkspaceMaterializerFactory
{
	readonly IServiceProvider _serviceProvider;

	public ManagedWorkspaceMaterializerFactory(IServiceProvider serviceProvider)
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
