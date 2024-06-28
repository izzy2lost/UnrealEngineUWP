// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text.Json.Serialization;
using EpicGames.Core;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Common;
using EpicGames.Horde.Compute;
using HordeServer.Acls;
using HordeServer.Agents;
using HordeServer.Agents.Pools;
using HordeServer.Agents.Sessions;
using HordeServer.Agents.Software;
using HordeServer.Artifacts;
using HordeServer.Configuration;
using HordeServer.Dashboard;
using HordeServer.Devices;
using HordeServer.Jobs;
using HordeServer.Jobs.Bisect;
using HordeServer.Logs;
using HordeServer.Notifications;
using HordeServer.Perforce;
using HordeServer.Plugins;
using HordeServer.Projects;
using HordeServer.Streams;
using HordeServer.Tools;
using HordeServer.Utilities;

namespace HordeServer.Server
{
	using JsonObject = System.Text.Json.Nodes.JsonObject;

	/// <summary>
	/// Global configuration
	/// </summary>
	[JsonSchema("https://unrealengine.com/horde/global")]
	[JsonSchemaCatalog("Horde Globals", "Horde global configuration file", new[] { "globals.json", "*.global.json" })]
	[ConfigIncludeRoot]
	[ConfigMacroScope]
	public class GlobalConfig
	{
		/// <summary>
		/// Global server settings object
		/// </summary>
		[JsonIgnore]
		public ServerSettings ServerSettings { get; private set; } = null!;

		/// <summary>
		/// Unique identifier for this config revision. Useful to detect changes.
		/// </summary>
		[JsonIgnore]
		public string Revision { get; set; } = String.Empty;

		/// <summary>
		/// Version number for the server. Values are indicated by the <see cref="ConfigVersion"/>.
		/// </summary>
		public int Version { get; set; }

		/// <summary>
		/// Version number for the server, as an enum.
		/// </summary>
		[JsonIgnore]
		public ConfigVersion VersionEnum => (ConfigVersion)Version;

		/// <summary>
		/// Other paths to include
		/// </summary>
		public List<ConfigInclude> Include { get; set; } = new List<ConfigInclude>();

		/// <summary>
		/// Macros within the global scope
		/// </summary>
		public List<ConfigMacro> Macros { get; set; } = new List<ConfigMacro>();

		/// <summary>
		/// Settings for the dashboard
		/// </summary>
		public DashboardConfig Dashboard { get; set; } = new DashboardConfig();

		/// <summary>
		/// List of scheduled downtime
		/// </summary>
		public List<ScheduledDowntime> Downtime { get; set; } = new List<ScheduledDowntime>();

		/// <summary>
		/// Plugin config objects
		/// </summary>
		public PluginConfigCollection Plugins { get; set; } = new PluginConfigCollection();

		/// <summary>
		/// General parameters for other tools. Can be queried through the api/v1/parameters endpoint.
		/// </summary>
		public JsonObject Parameters { get; set; } = new JsonObject();

		/// <summary>
		/// Access control list
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Accessor for the ACL scope lookup
		/// </summary>
		[JsonIgnore]
		public IReadOnlyDictionary<AclScopeName, AclConfig> AclScopes => _aclLookup;

		private readonly Dictionary<AclScopeName, AclConfig> _aclLookup = new Dictionary<AclScopeName, AclConfig>();

		/// <inheritdoc cref="AclConfig.Authorize(AclAction, ClaimsPrincipal)"/>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> Acl.Authorize(action, user);

		/// <summary>
		/// Called after the config file has been read
		/// </summary>
		public void PostLoad(ServerSettings serverSettings, IReadOnlyList<ILoadedPlugin> loadedPlugins)
		{
			ServerSettings = serverSettings;

			AclConfig defaultAcl = CreateRootAcl();
			Acl.PostLoad(defaultAcl, defaultAcl.ScopeName);

			// Ensure that all plugins have an entry in the global config so they can register their ACLs
			foreach (ILoadedPlugin loadedPlugin in loadedPlugins)
			{
				if (!Plugins.TryGetValue(loadedPlugin.Name, out _))
				{
					IPluginConfig pluginConfig = (IPluginConfig)Activator.CreateInstance(loadedPlugin.GlobalConfigType)!;
					Plugins.Add(loadedPlugin.Name, pluginConfig);
				}
			}

			PluginConfigOptions pluginConfigOptions = new PluginConfigOptions(VersionEnum, Acl);
			foreach (IPluginConfig pluginConfig in Plugins.Values)
			{
				pluginConfig.PostLoad(pluginConfigOptions);
			}

			UpdateWorkspacesForPools();

			_aclLookup.Clear();
			BuildAclScopeLookup(Acl, _aclLookup);

			BuildConfig? buildConfig;
			if (Plugins.TryGetBuildConfig(out buildConfig))
			{
				foreach (ProjectConfig project in buildConfig.Projects)
				{
					AclScopeName legacyProjectScopeName = Acl.ScopeName.Append($"p:{project.Id}");
					_aclLookup.Add(legacyProjectScopeName, project.Acl);

					foreach (StreamConfig stream in project.Streams)
					{
						AclScopeName legacyStreamScopeName = Acl.ScopeName.Append($"s:{stream.Id}");
						_aclLookup.Add(legacyStreamScopeName, stream.Acl);

						foreach (TemplateRefConfig template in stream.Templates)
						{
							AclScopeName legacyTemplateScopeName = legacyStreamScopeName.Append($"t:{template.Id}");
							_aclLookup.Add(legacyTemplateScopeName, template.Acl);
						}
					}
				}
			}
		}

		/// <summary>
		/// Creates the default root ACL
		/// </summary>
		static AclConfig CreateRootAcl()
		{
			AclConfig defaultAcl = new AclConfig();

			defaultAcl.Entries = new List<AclEntryConfig>();
			defaultAcl.Entries.Add(new AclEntryConfig(new AclClaimConfig(ClaimTypes.Role, "internal:AgentRegistration"), new[] { AgentAclAction.CreateAgent, SessionAclAction.CreateSession }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.AgentRegistrationClaim, new[] { AgentAclAction.CreateAgent, SessionAclAction.CreateSession, AgentAclAction.UpdateAgent, AgentSoftwareAclAction.DownloadSoftware, PoolAclAction.CreatePool, PoolAclAction.UpdatePool, PoolAclAction.ViewPool, PoolAclAction.DeletePool, PoolAclAction.ListPools, StreamAclAction.ViewStream, ProjectAclAction.ViewProject, JobAclAction.ViewJob, ServerAclAction.ViewCosts }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.AgentRoleClaim, new[] { ProjectAclAction.ViewProject, StreamAclAction.ViewStream, LogAclAction.CreateEvent, AgentSoftwareAclAction.DownloadSoftware }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.DownloadSoftwareClaim, new[] { AgentSoftwareAclAction.DownloadSoftware }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.UploadToolsClaim, new[] { AgentSoftwareAclAction.UploadSoftware, ToolAclAction.UploadTool }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.ConfigureProjectsClaim, new[] { ProjectAclAction.CreateProject, ProjectAclAction.UpdateProject, ProjectAclAction.ViewProject, StreamAclAction.CreateStream, StreamAclAction.UpdateStream, StreamAclAction.ViewStream }));
			defaultAcl.Entries.Add(new AclEntryConfig(HordeClaims.StartChainedJobClaim, new[] { JobAclAction.CreateJob, JobAclAction.ExecuteJob, JobAclAction.UpdateJob, JobAclAction.ViewJob, StreamAclAction.ViewTemplate, StreamAclAction.ViewStream }));

			defaultAcl.Profiles = new List<AclProfileConfig>();
			defaultAcl.Profiles.Add(new AclProfileConfig
			{
				Id = new AclProfileId("default-read"),
				Actions = new List<AclAction>
				{
					AgentAclAction.ListAgents,
					AgentAclAction.ViewAgent,
					ArtifactAclAction.DownloadArtifact,
					ArtifactAclAction.ReadArtifact,
					BisectTaskAclAction.ViewBisectTask,
					DeviceAclAction.DeviceRead,
					JobAclAction.ViewJob,
					LogAclAction.ViewEvent,
					LogAclAction.ViewLog,
					NotificationAclAction.CreateSubscription,
					PoolAclAction.ListPools,
					PoolAclAction.ViewPool,
					ProjectAclAction.ViewProject,
					ServerAclAction.IssueBearerToken,
					StreamAclAction.ViewChanges,
					StreamAclAction.ViewStream,
					StreamAclAction.ViewTemplate,
				}
			});
			defaultAcl.Profiles.Add(new AclProfileConfig
			{
				Id = new AclProfileId("default-run"),
				Extends = new List<AclProfileId>
				{
					new AclProfileId("default-read")
				},
				Actions = new List<AclAction>
				{
					JobAclAction.CreateJob,
					JobAclAction.UpdateJob,
					JobAclAction.RetryJobStep,
					DeviceAclAction.DeviceWrite,
					BisectTaskAclAction.CreateBisectTask,
					BisectTaskAclAction.UpdateBisectTask,
				}
			});

			defaultAcl.PostLoad(null, AclScopeName.Root);
			return defaultAcl;
		}

		static void BuildAclScopeLookup(AclConfig acl, Dictionary<AclScopeName, AclConfig> aclLookup)
		{
			aclLookup.Add(acl.ScopeName, acl);
			if (acl.Children != null)
			{
				foreach (AclConfig childAcl in acl.Children)
				{
					BuildAclScopeLookup(childAcl, aclLookup);
				}
			}
		}

		void UpdateWorkspacesForPools()
		{
			// Try to get the compute config, and skip if it isn't configured
			ComputeConfig? computeConfig;
			if (!Plugins.TryGetValue(new PluginName("compute"), out computeConfig))
			{
				return;
			}

			BuildConfig? buildConfig;
			if (!Plugins.TryGetValue(new PluginName("build"), out buildConfig))
			{
				return;
			}

			// Lookup table of pool id to workspaces
			Dictionary<PoolId, AutoSdkConfig> poolToAutoSdkView = new Dictionary<PoolId, AutoSdkConfig>();
			Dictionary<PoolId, List<AgentWorkspaceInfo>> poolToAgentWorkspaces = new Dictionary<PoolId, List<AgentWorkspaceInfo>>();

			// Populate the workspace list from the current stream
			foreach (StreamConfig streamConfig in buildConfig.Streams)
			{
				foreach (KeyValuePair<string, AgentConfig> agentTypePair in streamConfig.AgentTypes)
				{
					// Create the new agent workspace
					if (streamConfig.TryGetAgentWorkspace(agentTypePair.Value, out AgentWorkspaceInfo? agentWorkspace, out AutoSdkConfig? autoSdkConfig))
					{
						AgentConfig agentType = agentTypePair.Value;

						// Find or add a list of workspaces for this pool
						List<AgentWorkspaceInfo>? agentWorkspaces;
						if (!poolToAgentWorkspaces.TryGetValue(agentType.Pool, out agentWorkspaces))
						{
							agentWorkspaces = new List<AgentWorkspaceInfo>();
							poolToAgentWorkspaces.Add(agentType.Pool, agentWorkspaces);
						}

						// Add it to the list
						if (!agentWorkspaces.Contains(agentWorkspace))
						{
							agentWorkspaces.Add(agentWorkspace);
						}
						if (autoSdkConfig != null)
						{
							AutoSdkConfig? existingAutoSdkConfig;
							poolToAutoSdkView.TryGetValue(agentType.Pool, out existingAutoSdkConfig);
							poolToAutoSdkView[agentType.Pool] = AutoSdkConfig.Merge(autoSdkConfig, existingAutoSdkConfig);
						}
					}
				}
			}

			// Update the list of workspaces for each pool
			foreach (PoolConfig pool in computeConfig.Pools)
			{
				// Get the new list of workspaces for this pool
				List<AgentWorkspaceInfo>? newWorkspaces;
				if (!poolToAgentWorkspaces.TryGetValue(pool.Id, out newWorkspaces))
				{
					newWorkspaces = new List<AgentWorkspaceInfo>();
				}

				// Get the autosdk view
				AutoSdkConfig? newAutoSdkConfig;
				if (!poolToAutoSdkView.TryGetValue(pool.Id, out newAutoSdkConfig))
				{
					newAutoSdkConfig = AutoSdkConfig.None;
				}

				pool.Workspaces = newWorkspaces;
				pool.AutoSdkConfig = newAutoSdkConfig;
			}
		}

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="scopeName">Name of the scope to auth against</param>
		/// <param name="scopeConfig">Configuration for the scope</param>
		public bool TryGetAclScope(AclScopeName scopeName, [NotNullWhen(true)] out AclConfig? scopeConfig)
			=> _aclLookup.TryGetValue(scopeName, out scopeConfig);

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="scopeName">Name of the scope to auth against</param>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclScopeName scopeName, AclAction action, ClaimsPrincipal user)
			=> _aclLookup.TryGetValue(scopeName, out AclConfig? scopeConfig) && scopeConfig.Authorize(action, user);

		IReadOnlyList<string>? _cachedGroupClaims;

		/// <summary>
		/// Gets all the valid <see cref="HordeClaimTypes.Group"/> claims referenced by ACL entries within the config object.
		/// </summary>
		public IReadOnlyList<string> GetValidAccountGroupClaims()
		{
			if (_cachedGroupClaims == null)
			{
				HashSet<string> groups = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				FindGroupClaimsFromObject(Acl, groups);
				_cachedGroupClaims = groups.ToArray();
			}
			return _cachedGroupClaims;
		}

		static void FindGroupClaimsFromObject(AclConfig config, HashSet<string> groups)
		{
			if (config.Entries != null)
			{
				foreach (AclEntryConfig entry in config.Entries)
				{
					AclClaimConfig claim = entry.Claim;
					if (claim.Type.Equals(HordeClaimTypes.Group, StringComparison.OrdinalIgnoreCase))
					{
						groups.Add(claim.Value);
					}
				}
			}
			if (config.Children != null)
			{
				foreach (AclConfig childConfig in config.Children)
				{
					FindGroupClaimsFromObject(childConfig, groups);
				}
			}
		}
	}

	/// <summary>
	/// Profile for executing compute requests
	/// </summary>
	[DebuggerDisplay("{Id}")]
	public class ComputeClusterConfig
	{
		/// <summary>
		/// The owning global config instance
		/// </summary>
		[JsonIgnore]
		public GlobalConfig GlobalConfig { get; private set; } = null!;

		/// <summary>
		/// Name of the partition
		/// </summary>
		public ClusterId Id { get; set; } = new ClusterId("default");

		/// <summary>
		/// Name of the namespace to use
		/// </summary>
		public string NamespaceId { get; set; } = "horde.compute";

		/// <summary>
		/// Name of the input bucket
		/// </summary>
		public string RequestBucketId { get; set; } = "requests";

		/// <summary>
		/// Name of the output bucket
		/// </summary>
		public string ResponseBucketId { get; set; } = "responses";

		/// <summary>
		/// Filter for agents to include
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// Access control list
		/// </summary>
		public AclConfig Acl { get; set; } = new AclConfig();

		/// <summary>
		/// Callback post loading this config file
		/// </summary>
		/// <param name="globalConfig">The global config instance</param>
		public void PostLoad(GlobalConfig globalConfig)
		{
			GlobalConfig = globalConfig;
			Acl.PostLoad(globalConfig.Acl, $"compute:{Id}");
		}

		/// <summary>
		/// Authorizes a user to perform a given action
		/// </summary>
		/// <param name="action">The action being performed</param>
		/// <param name="user">The principal to validate</param>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
		{
			return Acl?.Authorize(action, user) ?? GlobalConfig.Authorize(action, user);
		}
	}

	/// <summary>
	/// How frequently the maintence window repeats
	/// </summary>
	public enum ScheduledDowntimeFrequency
	{
		/// <summary>
		/// Once
		/// </summary>
		Once,

		/// <summary>
		/// Every day
		/// </summary>
		Daily,

		/// <summary>
		/// Every week
		/// </summary>
		Weekly,
	}

	/// <summary>
	/// Settings for the maintenance window
	/// </summary>
	public class ScheduledDowntime
	{
		/// <summary>
		/// Start time
		/// </summary>
		public DateTimeOffset StartTime { get; set; }

		/// <summary>
		/// Finish time
		/// </summary>
		public DateTimeOffset FinishTime { get; set; }

		/// <summary>
		/// Frequency that the window repeats
		/// </summary>
		public ScheduledDowntimeFrequency Frequency { get; set; } = ScheduledDowntimeFrequency.Once;

		/// <summary>
		/// Gets the next scheduled downtime
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>Start and finish time</returns>
		public (DateTimeOffset StartTime, DateTimeOffset FinishTime) GetNext(DateTimeOffset now)
		{
			TimeSpan offset = TimeSpan.Zero;
			if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 1.0)
				{
					days -= days % 1.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if (days >= 7.0)
				{
					days -= days % 7.0;
				}
				offset = TimeSpan.FromDays(days);
			}
			return (StartTime + offset, FinishTime + offset);
		}

		/// <summary>
		/// Determines if this schedule is active
		/// </summary>
		/// <param name="now">The current time</param>
		/// <returns>True if downtime is active</returns>
		public bool IsActive(DateTimeOffset now)
		{
			if (Frequency == ScheduledDowntimeFrequency.Once)
			{
				return now >= StartTime && now < FinishTime;
			}
			else if (Frequency == ScheduledDowntimeFrequency.Daily)
			{
				double days = (now - StartTime).TotalDays;
				if (days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 1.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else if (Frequency == ScheduledDowntimeFrequency.Weekly)
			{
				double days = (now - StartTime).TotalDays;
				if (days < 0.0)
				{
					return false;
				}
				else
				{
					return (days % 7.0) < (FinishTime - StartTime).TotalDays;
				}
			}
			else
			{
				return false;
			}
		}
	}

	/// <summary>
	/// Path to a platform and stream to use for syncing AutoSDK
	/// </summary>
	public class AutoSdkWorkspace
	{
		/// <summary>
		/// Name of this workspace
		/// </summary>
		public string? Name { get; set; }

		/// <summary>
		/// The agent properties to check (eg. "OSFamily=Windows")
		/// </summary>
		public List<string> Properties { get; set; } = new List<string>();

		/// <summary>
		/// Username for logging in to the server
		/// </summary>
		public string? UserName { get; set; }

		/// <summary>
		/// Stream to use
		/// </summary>
		[Required]
		public string? Stream { get; set; }
	}

	/// <summary>
	/// Information about an individual Perforce server
	/// </summary>
	public class PerforceServer
	{
		/// <summary>
		/// The server and port. The server may be a DNS entry with multiple records, in which case it will be actively load balanced.
		/// </summary>
		public string ServerAndPort { get; set; } = "perforce:1666";

		/// <summary>
		/// Whether to query the healthcheck address under each server
		/// </summary>
		public bool HealthCheck { get; set; }

		/// <summary>
		/// Whether to resolve the DNS entries and load balance between different hosts
		/// </summary>
		public bool ResolveDns { get; set; }

		/// <summary>
		/// Maximum number of simultaneous conforms on this server
		/// </summary>
		public int MaxConformCount { get; set; }

		/// <summary>
		/// Optional condition for a machine to be eligable to use this server
		/// </summary>
		public Condition? Condition { get; set; }

		/// <summary>
		/// List of properties for an agent to be eligable to use this server
		/// </summary>
		public List<string>? Properties { get; set; }
	}

	/// <summary>
	/// Credentials for a Perforce user
	/// </summary>
	public class PerforceCredentials
	{
		/// <summary>
		/// The username
		/// </summary>
		public string UserName { get; set; } = String.Empty;

		/// <summary>
		/// Password for the user
		/// </summary>
		public string? Password { get; set; } = String.Empty;

		/// <summary>
		/// Login ticket for the user (will be used instead of password if set)
		/// </summary>
		public string? Ticket { get; set; } = String.Empty;
	}

	/// <summary>
	/// Information about a cluster of Perforce servers. 
	/// </summary>
	[DebuggerDisplay("{Name}")]
	public class PerforceCluster
	{
		/// <summary>
		/// The default cluster name
		/// </summary>
		public const string DefaultName = "Default";

		/// <summary>
		/// Name of the cluster
		/// </summary>
		[Required]
		public string Name { get; set; } = null!;

		/// <summary>
		/// Username for Horde to log in to this server. Will use the first account specified below if not overridden.
		/// </summary>
		public string? ServiceAccount { get; set; }

		/// <summary>
		/// Whether the service account can impersonate other users
		/// </summary>
		public bool CanImpersonate { get; set; } = true;

		/// <summary>
		/// Whether to use partitioned workspaces on this server
		/// </summary>
		public bool SupportsPartitionedWorkspaces { get; set; } = false;

		/// <summary>
		/// List of servers
		/// </summary>
		public List<PerforceServer> Servers { get; set; } = new List<PerforceServer>();

		/// <summary>
		/// List of server credentials
		/// </summary>
		public List<PerforceCredentials> Credentials { get; set; } = new List<PerforceCredentials>();

		/// <summary>
		/// List of autosdk streams
		/// </summary>
		public List<AutoSdkWorkspace> AutoSdk { get; set; } = new List<AutoSdkWorkspace>();
	}
}

