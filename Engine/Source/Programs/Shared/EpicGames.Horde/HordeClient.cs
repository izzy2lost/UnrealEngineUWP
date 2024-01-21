// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Http;

namespace EpicGames.Horde
{
	/// <summary>
	/// Default implementation of <see cref="IHordeClient"/>
	/// </summary>
	class HordeClient : IHordeClient
	{
		readonly IHttpClientFactory _httpClientFactory;

		/// <summary>
		/// Constructor
		/// </summary>
		public HordeClient(IHttpClientFactory httpClientFactory)
		{
			_httpClientFactory = httpClientFactory;
		}

		/// <inheritdoc/>
		public HordeHttpClient CreateHttpClient()
			=> _httpClientFactory.CreateHordeHttpClient();
	}
}
