// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Security.Claims;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Telemetry;
using HordeServer.Acls;
using HordeServer.Plugins;
using HordeServer.Telemetry.Metrics;

#pragma warning disable CA2227 // Change X to be read-only by removing the property setter

namespace HordeServer
{
	/// <summary>
	/// Config settings for analytics
	/// </summary>
	public class AnalyticsGlobalConfig : IPluginConfig
	{
		/// <summary>
		/// Metrics to aggregate on the Horde server
		/// </summary>
		public List<TelemetryStoreConfig> TelemetryStores { get; set; } = new List<TelemetryStoreConfig>();

		private AclConfig _parentAcl = null!;
		private readonly Dictionary<TelemetryStoreId, TelemetryStoreConfig> _telemetryStoreLookup = new Dictionary<TelemetryStoreId, TelemetryStoreConfig>();

		public void PostLoad(AclConfig parentAcl)
		{
			_parentAcl = parentAcl;

			_telemetryStoreLookup.Clear();
			foreach (TelemetryStoreConfig telemetryStore in TelemetryStores)
			{
				_telemetryStoreLookup.Add(telemetryStore.Id, telemetryStore);
				telemetryStore.PostLoad(parentAcl);
			}
		}

		/// <inheritdoc cref="AclConfig.Authorize(AclAction, ClaimsPrincipal)"/>
		public bool Authorize(AclAction action, ClaimsPrincipal user)
			=> _parentAcl.Authorize(action, user);


		/// <summary>
		/// Attempts to get configuration for a pool from this object
		/// </summary>
		/// <param name="telemetryStoreId">The pool identifier</param>
		/// <param name="config">Configuration for the telemetry store</param>
		/// <returns>True if the telemetry configuration was found</returns>
		public bool TryGetTelemetryStore(TelemetryStoreId telemetryStoreId, [NotNullWhen(true)] out TelemetryStoreConfig? config) 
			=> _telemetryStoreLookup.TryGetValue(telemetryStoreId, out config);
	}
}
