// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using EpicGames.Horde;
using FluentAvalonia.UI.Controls;
using Microsoft.Extensions.DependencyInjection;

namespace UnrealToolbox
{
	partial class GeneralSettingsViewModel : ObservableObject, IDisposable
	{
		record StateRecord(Uri ServerUrl, bool Connected);

		readonly SettingsContext _context;
		readonly IHordeClientProvider _hordeClientProvider;

		[ObservableProperty]
		string _serverUrl = String.Empty;

		[ObservableProperty]
		string _serverStatus = String.Empty;

		[ObservableProperty]
		bool? _isLoginEnabled;

		public ToolCatalogViewModel ToolCatalog { get; }

		public GeneralSettingsViewModel()
			: this(SettingsContext.Default)
		{
		}

		public GeneralSettingsViewModel(SettingsContext context)
		{
			_context = context;

			_hordeClientProvider = context.ServiceProvider.GetRequiredService<IHordeClientProvider>();
			_hordeClientProvider.OnStateChanged += OnStateChangedAsync;
			_hordeClientProvider.OnAccessTokenStateChanged += OnStateChangedAsync;

			ToolCatalog = new ToolCatalogViewModel(context.ServiceProvider.GetService<IToolCatalog>());

			OnStateChangedAsync();
		}

		public void Dispose()
		{
			_hordeClientProvider.OnStateChanged -= OnStateChangedAsync;
			_hordeClientProvider.OnAccessTokenStateChanged -= OnStateChangedAsync;
		}

		async void OnStateChangedAsync()
		{
			using IHordeClientRef? hordeClient = await _hordeClientProvider.GetClientRefAsync();
			if (hordeClient == null)
			{
				ServerUrl = "No server configured";
				ServerStatus = "Status unavailable";
				IsLoginEnabled = false;
			}
			else
			{
				ServerUrl = hordeClient.Client.ServerUrl.ToString();
				ServerStatus = hordeClient.Client.HasValidAccessToken() ? "Connected" : "Session Expired";
				IsLoginEnabled = !hordeClient.Client.HasValidAccessToken();
			}
		}

		public void OpenServer()
		{
			OpenBrowser(ServerUrl);
		}

		public async Task LoginAsync()
		{
			using IHordeClientRef? hordeClientRef = await _hordeClientProvider.GetClientRefAsync();
			if (hordeClientRef != null)
			{
				await hordeClientRef.Client.LoginAsync(true, CancellationToken.None);
			}
		}

		public async Task ConfigureServerAsync()
		{
			string serverUrl = ServerUrl;

			string? errorMessage = null;
			for (; ; )
			{
				TextBox serverUrlTextBox = new TextBox();
				serverUrlTextBox.Text = serverUrl;
				serverUrlTextBox.SelectionStart = serverUrl.Length;
				serverUrlTextBox.SelectionEnd = serverUrl.Length;

				Control content = serverUrlTextBox;
				if (!String.IsNullOrEmpty(errorMessage))
				{
					TextBlock errorBlock = new TextBlock();
					errorBlock.Text = errorMessage;

					StackPanel stackPanel = new StackPanel() { Spacing = 10 };
					stackPanel.Children.Add(serverUrlTextBox);
					stackPanel.Children.Add(errorBlock);

					content = stackPanel;
				}

				ContentDialog dialog = new ContentDialog()
				{
					Title = "Connect to Server",
					Content = content,
					PrimaryButtonText = "Connect",
					CloseButtonText = "Cancel"
				};

				ContentDialogResult result = await dialog.ShowAsync(_context.SettingsWindow);
				if (result == ContentDialogResult.None)
				{
					return;
				}

				serverUrl = serverUrlTextBox.Text;

				try
				{
					HordeOptions.SetDefaultServerUrl(new Uri(serverUrl));

					await _hordeClientProvider.RecreateAsync();

					using IHordeClientRef? clientRef = await _hordeClientProvider.GetClientRefAsync();
					if (clientRef != null)
					{
						await clientRef.Client.LoginAsync(true, CancellationToken.None);
					}

					return;
				}
				catch (Exception ex)
				{
					errorMessage = ex.ToString();
				}
			}
		}

		static void OpenBrowser(string url)
		{
			if (OperatingSystem.IsWindows())
			{
				string escapedUrl = url.Replace("&", "^&", StringComparison.Ordinal);
				Process.Start(new ProcessStartInfo("cmd", $"/c start {escapedUrl}") { CreateNoWindow = true });
			}
			else if (OperatingSystem.IsLinux())
			{
				Process.Start("xdg-open", url);
			}
			else if (OperatingSystem.IsMacOS())
			{
				Process.Start("open", url);
			}
		}
	}
}
