// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Horde.Server.Agents.Utilization
{
	/// <summary>
	/// Collection of utilization collection
	/// </summary>
	public interface IUtilizationDataCollection
	{
		/// <summary>
		/// Adds entries for the given utilization
		/// </summary>
		/// <param name="data">Telemetry data to add</param>
		/// <returns>Async task</returns>
		Task AddUtilizationDataAsync(IUtilizationData data);

		/// <summary>
		/// Finds utilization data matching the given criteria
		/// </summary>
		/// <param name="startTimeUtc">Start time to query utilization for</param>
		/// <param name="finishTimeUtc">Finish time to query utilization for</param>
		/// <returns>The utilization data</returns>
		Task<List<IUtilizationData>> GetUtilizationDataAsync(DateTime startTimeUtc, DateTime finishTimeUtc);

		/// <summary>
		/// Finds the latest utilization data
		/// </summary>
		/// <returns>The utilization data</returns>
		Task<IUtilizationData?> GetLatestUtilizationDataAsync();
	}
}
