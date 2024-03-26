// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Clients;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using UnrealGameSync.Forms;

namespace UnrealGameSync.UriHandlers
{
	class HordeHandler
	{
		class CopyProgressAdapter : IProgress<IExtractStats>
		{
			readonly IProgress<string> _inner;

			public CopyProgressAdapter(IProgress<string> inner)
			{
				_inner = inner;
			}

			public void Report(IExtractStats value)
			{
				_inner.Report($"Copied {value.Count} files ({value.Size / (1024.0 * 1024.0):n1}mb, {value.Rate / (1024.0 * 1024.0):n1}mb/s)...");
			}
		}

		static async Task DoSyncAsync(Uri server, NamespaceId namespaceId, RefName refName, DirectoryReference outputDir, IProgress<string> progress, CancellationToken cancellationToken)
		{
			DirectoryReference stateFolder = DirectoryReference.Combine(outputDir, ".ugs");
			DirectoryReference.CreateDirectory(stateFolder);

			using (ILoggerProvider loggerProvider = Logging.CreateLoggerProvider(FileReference.Combine(stateFolder, "sync.log")))
			{
				ServiceCollection services = new ServiceCollection();
				services.AddLogging(builder => builder.AddProvider(loggerProvider));
				services.AddHordeHttpClient((sp, client) => client.BaseAddress = server);
				services.AddSingleton<BundleCache>();
				services.AddSingleton<StorageBackendCache>();
				services.AddSingleton<IStorageClientFactory, HttpStorageClientFactory>();

				await using ServiceProvider serviceProvider = services.BuildServiceProvider();

				IStorageClientFactory factory = serviceProvider.GetRequiredService<IStorageClientFactory>();
				using IStorageClient storageClient = factory.CreateClient(namespaceId);

				progress.Report("Connecting to server...");

				DirectoryNode? node = await storageClient.ReadRefTargetAsync<DirectoryNode>(refName, cancellationToken: cancellationToken);

				progress.Report("Starting...");
				await node.CopyToDirectoryAsync(outputDir.ToDirectoryInfo(), new CopyProgressAdapter(progress), serviceProvider.GetRequiredService<ILogger<HordeHandler>>(), cancellationToken);
			}
		}

		[UriHandler(true)]
		public static UriResult Download(string server, string ns, string refName)
		{
			string defaultOutputDir = Directory.GetCurrentDirectory();

			string? outputDir;
			if (!DownloadSettingsWindow.Show(refName, defaultOutputDir, out outputDir))
			{
				return new UriResult() { Success = true };
			}

			try
			{
				DownloadProgressWindow.Execute((p, ctx) => DoSyncAsync(new Uri(server), new NamespaceId(ns), new RefName(refName), new DirectoryReference(outputDir), p, ctx), CancellationToken.None);
			}
			catch (Exception ex)
			{
				MessageBox.Show($"An error occurred while downloading the specified item.\n\n{ex}");
			}

			return new UriResult() { Success = true };
		}
	}
}
