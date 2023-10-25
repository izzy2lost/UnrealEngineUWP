// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Serialization;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;

#pragma warning disable CA2227 // Collection properties should be read only

namespace EpicGames.Horde.Compute
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
		/// How to establish a connection to the remote machine
		/// </summary>
		public ConnectionMode ConnectionMode { get; set; }
		
		/// <summary>
		/// An optional address (host:port) to use when connecting depending on connection mode
		/// </summary>
		public string? ConnectionAddress { get; set; }

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
	
	/// <summary>
	/// Describe how to connect to the remote machine
	/// </summary>
	[JsonConverter(typeof(JsonStringEnumConverter))]
	public enum ConnectionMode
	{
		/// <summary>
		/// Connection is established directly to remote machine, behaving like a normal TCP/UDP connection
		/// </summary>
		Direct,
		
		/// <summary>
		/// Connection is tunneled through Horde server.
		/// When connecting, initiator must send a tunnel handshake request indicating which machine/IP to tunnel to.
		/// Once handshake is complete, TCP connection behaves as normal.
		/// </summary>
		Tunnel,
		
		/// <summary>
		/// Connection is established to remote machine via a relay.
		/// Forwarding is transparent and behaves like a normal TCP/UDP connection.
		/// </summary>
		Relay
	}
	
	/// <summary>
	/// Resource needs declaration request
	/// </summary>
	public class ResourceNeedsMessage
	{
		/// <summary>
		/// Unique session ID performing compute resource requests
		/// </summary>
		public string SessionId { get; set; } = String.Empty;
		
		/// <summary>
		/// Pool of agents requesting resources from
		/// </summary>
		public string Pool { get; set; } = String.Empty;

		/// <summary>
		/// Key/value of resources needed by session (such as CPU or memory, see KnownPropertyNames in Horde.Server)
		/// </summary>
		public Dictionary<string, int> ResourceNeeds { get; set; } = new();
	}
	
	/// <summary>
	/// Resource needs response
	/// </summary>
	public class GetResourceNeedsResponse
	{
		/// <summary>
		/// List of resource needs
		/// </summary>
		public List<ResourceNeedsMessage> ResourceNeeds { get; set; } = new();
	}
}
