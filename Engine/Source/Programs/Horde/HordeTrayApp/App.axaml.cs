// Copyright Epic Games, Inc. All Rights Reserved.

using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;

namespace HordeTrayApp
{
	/// <summary>
	/// Main application class
	/// </summary>
	public sealed partial class App : Application, ITrayAppHost
	{
		readonly List<ITrayAppPlugin> _plugins = new List<ITrayAppPlugin>();

		WindowIcon? _normalIcon;
		WindowIcon? _busyIcon;
		WindowIcon? _pausedIcon;
		WindowIcon? _errorIcon;

		/// <summary>
		/// Constructor
		/// </summary>
		public App()
		{
		}

		/// <inheritdoc/>
		public override void Initialize()
		{
			AvaloniaXamlLoader.Load(this);

			_normalIcon = (WindowIcon)Resources["StatusNormal"]!;
			_busyIcon = (WindowIcon)Resources["StatusBusy"]!;
			_pausedIcon = (WindowIcon)Resources["StatusPaused"]!;
			_errorIcon = (WindowIcon)Resources["StatusError"]!;

			NativeMenuItem exitMenuItem = new NativeMenuItem("Exit");
			exitMenuItem.Click += TrayIcon_Exit;

			NativeMenu contextMenu = new NativeMenu();

			TrayIcon trayIcon = TrayIcon.GetIcons(this)![0];
			trayIcon.Menu = contextMenu;

//			_icon = new TrayIcon();
//			_icon.Icon = _normalIcon;
//			_icon.Menu = contextMenu;
//			_icon.ToolTipText = "Hello world";

//			TrayIcons icons = new TrayIcons();
//			icons.Add(_icon);

//			TrayIcon.SetIcons(this, icons);

			_plugins.Add(new AgentPlugin(this));

//			NativeMenu contextMenu = trayIcon.Menu!;
			foreach (ITrayAppPlugin plugin in _plugins)
			{
				plugin.PopulateContextMenu(contextMenu);
			}
			contextMenu.Items.Add(new NativeMenuItemSeparator());
			contextMenu.Items.Add(exitMenuItem);
		}

		private void TrayIcon_Exit(object? sender, EventArgs e)
		{
			((IClassicDesktopStyleApplicationLifetime)ApplicationLifetime!).Shutdown();
		}

		/// <inheritdoc/>
		public void UpdateStatus()
		{
			Dispatcher.UIThread.Post(() => UpdateStatusMainThread());
		}

		private void UpdateStatusMainThread()
		{
			TrayAppPluginState state = TrayAppPluginState.Undefined;
			List<string> messages = new List<string>();

			foreach (ITrayAppPlugin plugin in _plugins)
			{
				TrayAppPluginStatus status = plugin.GetStatus();
				if (status.State >= state)
				{
					if (status.State > state)
					{
						messages.Clear();
						state = status.State;
					}
					if (status.Message != null)
					{
						messages.Add(status.Message);
					}
				}
			}

			TrayIcon trayIcon = TrayIcon.GetIcons(this)![0];
			trayIcon!.Icon = state switch
			{
				TrayAppPluginState.Busy => _busyIcon,
				TrayAppPluginState.Paused => _pausedIcon,
				TrayAppPluginState.Error => _errorIcon,
				_ => _normalIcon
			};
			trayIcon.ToolTipText = String.Join("\n", messages);
		}
	}
}
