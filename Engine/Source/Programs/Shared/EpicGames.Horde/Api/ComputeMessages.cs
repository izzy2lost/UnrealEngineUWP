// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Horde.Compute;

#pragma warning disable CA2227 // Collection properties should be read only

namespace EpicGames.Horde.Api
{
	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AssignComputeRequest
	{
		/// <summary>
		/// Condition to identify machines that can execute the request
		/// </summary>
		public Requirements? Requirements { get; set; }
		
		/// <summary>
		/// Arbitrary ID to correlate the same request over multiple calls.
		/// It's recommended to pick something globally unique, such as a UUID.
		/// </summary>
		public string? RequestId { get; set; }
	}

	/// <summary>
	/// Request a machine to execute compute requests
	/// </summary>
	public class AssignComputeResponse
	{
		/// <summary>
		/// IP address of the remote machine
		/// </summary>
		public string Ip { get; set; } = String.Empty;

		/// <summary>
		/// Port number on the remote machine
		/// </summary>
		public int Port { get; set; }

		/// <summary>
		/// Cryptographic nonce to identify the request, as a hex string
		/// </summary>
		public string Nonce { get; set; } = String.Empty;

		/// <summary>
		/// AES key for the channel, as a hex string
		/// </summary>
		public string Key { get; set; } = String.Empty;

		/// <summary>
		/// Identifier for the remote machine
		/// </summary>
		public AgentId AgentId { get; set; }

		/// <summary>
		/// Identifier for the new lease on the remote machine
		/// </summary>
		public LeaseId LeaseId { get; set; }

		/// <summary>
		/// Resources assigned to this machine
		/// </summary>
		public Dictionary<string, int> AssignedResources { get; set; } = new Dictionary<string, int>();

		/// <summary>
		/// Properties of the agent assigned to do the work
		/// </summary>
		public IReadOnlyList<string> Properties { get; set; } = new List<string>();
	}
}
