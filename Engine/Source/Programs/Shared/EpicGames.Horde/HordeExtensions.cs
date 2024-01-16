// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Net.Http;
using System.Net.Http.Headers;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.DependencyInjection;

namespace EpicGames.Horde
{
	/// <summary>
	/// Extension methods for Horde
	/// </summary>
	public static class HordeExtensions
	{
		/// <summary>
		/// Adds Horde-related services with the default settings
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		public static void AddHorde(this IServiceCollection serviceCollection)
		{
			AddHorde(serviceCollection, new HordeOptions());
		}

		/// <summary>
		/// Adds Horde-related services with the default settings
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		/// <param name="configure">Callback to configure options</param>
		public static void AddHorde(this IServiceCollection serviceCollection, Action<HordeOptions> configure)
		{
			HordeOptions options = new HordeOptions();
			configure(options);
			AddHorde(serviceCollection, options);
		}

		/// <summary>
		/// Adds Horde-related services with the default settings
		/// </summary>
		/// <param name="serviceCollection">Collection to register services with</param>
		/// <param name="options">Options for the collection</param>
		public static void AddHorde(this IServiceCollection serviceCollection, HordeOptions options)
		{
			void ConfigureHttpClient(HttpClient httpClient)
			{
				if (options.ServerUrl != null)
				{
					httpClient.BaseAddress = options.ServerUrl;
				}
				if (options.AccessToken != null)
				{
					httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", options.AccessToken);
				}

				options.ConfigureHttpClient?.Invoke(httpClient);
			}

			serviceCollection.AddHordeHttpClient(ConfigureHttpClient, options.AllowAuthPrompt);
			serviceCollection.AddSingleton<BundleCache>(sp => new BundleCache(options.BundleCache));
			serviceCollection.AddSingleton<StorageBackendCache>();
			serviceCollection.AddSingleton<HttpStorageBackendFactory>();
			serviceCollection.AddSingleton<HttpStorageClientFactory>();
		}
	}
}
