// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Threading.Tasks;

namespace EpicGames.Horde.Replicators
{
	/// <summary>
	/// Information about a replicator
	/// </summary>
	public class GetReplicatorResponse
	{
		/// <summary>
		/// Identifier for this replicator
		/// </summary>
		public ReplicatorId Id { get; set; }

		/// <summary>
		/// The last change that was replicated
		/// </summary>
		public int? LastChange { get; set; }

		/// <summary>
		/// Time at which the last change was replicated
		/// </summary>
		public DateTime? LastChangeFinishTime { get; set; }

		/// <summary>
		/// The current change being replicated
		/// </summary>
		public int? CurrentChange { get; set; }

		/// <summary>
		/// Time at which the current change was replicated
		/// </summary>
		public DateTime? CurrentChangeStartTime { get; set; }

		/// <summary>
		/// Last error with replication, if there is one.
		/// </summary>
		public string? Error { get; set; }
	}

	/// <summary>
	/// Information about a replicator
	/// </summary>
	public class UpdateReplicatorRequest
	{
		/// <summary>
		/// Whether to pause replication
		/// </summary>
		public bool? Paused { get; set; }

		/// <summary>
		/// Resets the replication
		/// </summary>
		public bool? Reset { get; set; }

		/// <summary>
		/// Sets the next change to replicate from
		/// </summary>
		public int? Change { get; set; }
	}
}
