// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend which communicates with the Horde server over HTTP for content
	/// </summary>
	public sealed class HttpStorageBackend : IStorageBackend
	{
		class WriteBlobResponse
		{
			public string Blob { get; set; } = String.Empty;
			public Uri? UploadUrl { get; set; }
			public bool? SupportsRedirects { get; set; }
		}

		readonly string _basePath;
		readonly Func<HttpClient> _createClient;
		readonly ILogger _logger;
		bool _supportsUploadRedirects = true;

		/// <inheritdoc/>
		public bool SupportsRedirects => _supportsUploadRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public HttpStorageBackend(string basePath, Func<HttpClient> createClient, ILogger logger)
		{
			_basePath = basePath.TrimEnd('/');
			_createClient = createClient;
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public async Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			if (offset == 0 && length == null)
			{
				_logger.LogDebug("Reading {Locator}", locator);
			}
			else if (length == null)
			{
				_logger.LogDebug("Reading {Locator} ({Offset}..)", locator, offset);
			}
			else
			{
				_logger.LogDebug("Reading {Locator} ({Offset}+{Length})", locator, offset, length);
			}

			if (length.HasValue && length.Value == 0)
			{
				return new MemoryStream(Array.Empty<byte>());
			}

			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Get, $"{_basePath}/blobs/{locator}"))
				{
					if (offset != 0 || length != null)
					{
						request.Headers.Range = new RangeHeaderValue(offset, (length == null) ? null : (offset + (length - 1)));
					}

					HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken);
					response.EnsureSuccessStatusCode();
					return await response.Content.ReadAsStreamAsync(cancellationToken);
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
		{
			using (Stream stream = await OpenBlobAsync(locator, offset, length, cancellationToken))
			{
				byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
				return ReadOnlyMemoryOwner.Create(data);
			}
		}

		/// <inheritdoc/>
		public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using StreamContent streamContent = new StreamContent(stream);

			if (_supportsUploadRedirects)
			{
				using (HttpClient redirectHttpClient = _createClient())
				{
					// Don't send the auth header if we're following a redirect
					redirectHttpClient.DefaultRequestHeaders.Authorization = null;

					WriteBlobResponse redirectResponse = await SendWriteRequestAsync(null, prefix, cancellationToken);
					if (redirectResponse.UploadUrl != null)
					{
						using (HttpResponseMessage uploadResponse = await redirectHttpClient.PutAsync(redirectResponse.UploadUrl, streamContent, cancellationToken))
						{
							if (!uploadResponse.IsSuccessStatusCode)
							{
								string body = await uploadResponse.Content.ReadAsStringAsync(cancellationToken);
								throw new StorageException($"Unable to upload data to redirected URL: {body}");
							}
						}
						_logger.LogDebug("Written {Locator} (using redirect)", redirectResponse.Blob);
						return new BlobLocator(redirectResponse.Blob);
					}
				}
			}

			WriteBlobResponse response = await SendWriteRequestAsync(streamContent, prefix, cancellationToken);
			_supportsUploadRedirects = response.SupportsRedirects ?? false;
			_logger.LogDebug("Written {Locator} (direct)", response.Blob);
			return new BlobLocator(response.Blob);
		}

		async Task<WriteBlobResponse> SendWriteRequestAsync(StreamContent? streamContent, string? prefix = null, CancellationToken cancellationToken = default)
		{
			using (HttpClient httpClient = _createClient())
			{
				using (HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, $"{_basePath}/blobs"))
				{
					using StringContent stringContent = new StringContent(prefix ?? String.Empty);

					MultipartFormDataContent form = new MultipartFormDataContent();
					if (streamContent != null)
					{
						form.Add(streamContent, "file", "filename");
					}
					form.Add(stringContent, "prefix");

					request.Content = form;
					using (HttpResponseMessage response = await httpClient.SendAsync(request, cancellationToken))
					{
						response.EnsureSuccessStatusCode();
						WriteBlobResponse? data = await response.Content.ReadFromJsonAsync<WriteBlobResponse>(cancellationToken: cancellationToken);
						return data!;
					}
				}
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			// We don't currently have any need for getting a read redirect explicitly, though ReadAsync() calls may redirect us automatically.
			return default;
		}

		/// <inheritdoc/>
		public async ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default)
		{
			if (_supportsUploadRedirects)
			{
				WriteBlobResponse redirectResponse = await SendWriteRequestAsync(null, prefix, cancellationToken);
				if (redirectResponse.UploadUrl != null)
				{
					return (new BlobLocator(redirectResponse.Blob), redirectResponse.UploadUrl);
				}
			}
			return null;
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}
