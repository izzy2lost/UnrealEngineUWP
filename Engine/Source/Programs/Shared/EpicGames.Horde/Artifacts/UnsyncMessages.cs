// Copyright Epic Games, Inc.All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json.Serialization;

#pragma warning disable CA2227

namespace EpicGames.Horde.Artifacts
{
	/// <summary>
	/// Request to return a set of blobs for Unsync
	/// </summary>
	public class GetUnsyncDataRequest
	{
		/// <summary>
		/// The strong hash algorithm
		/// </summary>
		[JsonPropertyName("hash_strong")]
		public string? HashStrong { get; set; }

		/// <summary>
		/// Files to retrieve
		/// </summary>
		[JsonPropertyName("blocks")]
		public List<string> Blocks { get; set; } = new List<string>();
	}
}
