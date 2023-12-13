// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;

namespace EpicGames.Horde
{
	/// <summary>
	/// Options for configuring the Horde connection
	/// </summary>
	public class HordeOptions
	{
		/// <summary>
		/// Address of the Horde server
		/// </summary>
		public Uri? ServerUrl { get; set; }

		/// <summary>
		/// Access token to use for connecting to the server
		/// </summary>
		public string? AccessToken { get; set; }

		/// <summary>
		/// Whether to allow opening a browser window to prompt for authentication
		/// </summary>
		public bool AllowAuthPrompt { get; set; }

		/// <summary>
		/// Callback to allow configuring any HTTP client created for Horde
		/// </summary>
		public Action<HttpClient>? ConfigureHttpClient { get; set; }

		/// <summary>
		/// Options for creating new bundles
		/// </summary>
		public BundleOptions Bundle { get; } = new BundleOptions();

		/// <summary>
		/// Options for caching bundles 
		/// </summary>
		public BundleCacheOptions BundleCache { get; } = new BundleCacheOptions();
	}
}
