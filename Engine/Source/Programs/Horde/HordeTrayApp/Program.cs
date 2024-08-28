// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using EpicGames.Core;
using HordeTrayApp.Properties;

namespace HordeTrayApp
{
	static class Program
	{
		const string MutexName = "Horde.Agent.TrayApp-Mutex";
		const string EventName = "Horde.Agent.TrayApp-Exit";

		[STAThread]
		public static int Main(string[] args)
		{
			using EventWaitHandle closeEvent = new EventWaitHandle(false, EventResetMode.AutoReset, EventName);
			using SingleInstanceMutex mutex = new SingleInstanceMutex(MutexName);

			if (args.Any(x => x.Equals("-close", StringComparison.OrdinalIgnoreCase)))
			{
				closeEvent.Set();
				return mutex.Wait(5000) ? 0 : 1;
			}

			if (!mutex.Wait(0))
			{
				return 1;
			}

			if (args.Any(x => x.Equals("-shadowcopy", StringComparison.OrdinalIgnoreCase)))
			{
				DirectoryReference? localAppData = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData);
				if (localAppData != null)
				{
					FileReference sourceExe = new FileReference(Assembly.GetExecutingAssembly().Location).ChangeExtension(".exe");
					if (FileReference.Exists(sourceExe) && !sourceExe.IsUnderDirectory(localAppData))
					{
						DirectoryReference sourceDir = sourceExe.Directory;
						DirectoryReference targetDir = DirectoryReference.Combine(localAppData, "Epic Games", "HordeTrayApp");
						try
						{
							DirectoryReference.CreateDirectory(targetDir);
							FileUtils.ForceDeleteDirectoryContents(targetDir);

							CopyFiles(sourceDir, targetDir);
							mutex.Release();

							FileReference targetExe = FileReference.Combine(targetDir, sourceExe.MakeRelativeTo(sourceDir)).ChangeExtension(".exe");
							using Process process = Process.Start(targetExe.FullName, args);

							return 0;
						}
						catch (Exception ex)
						{
							MessageBox.Show($"Unable to copy app to temp location. Error:\n\n{ex}");
							return 1;
						}
					}
				}
			}

			MainAsync(closeEvent).GetAwaiter().GetResult();
			return 0;
		}

		static void CopyFiles(DirectoryReference sourceDir, DirectoryReference targetDir)
		{
			DirectoryReference.CreateDirectory(targetDir);
			foreach (DirectoryReference sourceSubDir in DirectoryReference.EnumerateDirectories(sourceDir))
			{
				DirectoryReference targetSubDir = DirectoryReference.Combine(targetDir, sourceSubDir.GetDirectoryName());
				CopyFiles(sourceSubDir, targetSubDir);
			}
			foreach (FileReference sourceFile in DirectoryReference.EnumerateFiles(sourceDir))
			{
				FileReference targetFile = FileReference.Combine(targetDir, sourceFile.GetFileName());
				FileReference.Copy(sourceFile, targetFile, true);
			}
		}

		static async Task MainAsync(EventWaitHandle closeEvent)
		{
			ApplicationConfiguration.Initialize();

			await using (CustomApplicationContext appContext = new CustomApplicationContext(closeEvent))
			{
				Application.Run(appContext);
			}
		}
	}

	class SingleInstanceMutex : IDisposable
	{
		readonly Mutex _mutex;
		bool _locked;

		public SingleInstanceMutex(string name)
		{
			_mutex = new Mutex(true, name);
		}

		public void Release()
		{
			if (_locked)
			{
				_mutex.ReleaseMutex();
				_locked = false;
			}
		}

		public bool Wait(int timeout)
		{
			if (!_locked)
			{
				try
				{
					_locked = _mutex.WaitOne(timeout);
				}
				catch (AbandonedMutexException)
				{
					_locked = true;
				}
			}
			return _locked;
		}

		public void Dispose()
		{
			Release();
			_mutex.Dispose();
		}
	}

	class CustomApplicationContext : ApplicationContext, ITrayAppHost, IAsyncDisposable
	{
		readonly NotifyIcon _trayIcon;
		readonly Control _mainThreadInvokeTarget;
		readonly List<ITrayAppPlugin> _plugins = new List<ITrayAppPlugin>();
		readonly BackgroundTask _waitForExitTask;

		bool _disposed;

		public CustomApplicationContext(EventWaitHandle eventHandle)
		{
			_plugins.Add(new AgentPlugin(this));

			_mainThreadInvokeTarget = new Control();
			_mainThreadInvokeTarget.CreateControl();

			ToolStripMenuItem exitMenuItem = new ToolStripMenuItem("Exit");
			exitMenuItem.Click += OnExit;

			ContextMenuStrip menu = new ContextMenuStrip();
			foreach (ITrayAppPlugin plugin in _plugins)
			{
				plugin.PopulateMenu(menu);
			}
			menu.Items.Add(new ToolStripSeparator());
			menu.Items.Add(exitMenuItem);

			_trayIcon = new NotifyIcon()
			{
				Icon = Resources.StatusNormal,
				ContextMenuStrip = menu,
				Visible = true
			};
			_trayIcon.Click += TrayIcon_Click;

			_waitForExitTask = BackgroundTask.StartNew(ctx => WaitForExitAsync(eventHandle, ctx));
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if (disposing)
			{
				_mainThreadInvokeTarget.Dispose();
				_trayIcon.Dispose();
				_disposed = true;
			}
		}

		private void TrayIcon_Click(object? sender, EventArgs e)
		{
			foreach (AgentPlugin plugin in _plugins)
			{
				plugin.UpdateMenu();
			}
		}

		public async ValueTask DisposeAsync()
		{
			await _waitForExitTask.DisposeAsync();

			foreach (AgentPlugin plugin in _plugins)
			{
				await plugin.DisposeAsync();
			}

			Dispose();
			GC.SuppressFinalize(this);
		}

		private void OnExit(object? sender, EventArgs e)
		{
			ExitThread();
		}

		public void UpdateStatus()
		{
			_mainThreadInvokeTarget.BeginInvoke(() => SetStatus_MainThread());
		}

		void SetStatus_MainThread()
		{
			if (!_disposed)
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

				_trayIcon.Icon = state switch
				{
					TrayAppPluginState.Busy => Resources.StatusBusy,
					TrayAppPluginState.Paused => Resources.StatusPaused,
					TrayAppPluginState.Error => Resources.StatusError,
					_ => Resources.StatusNormal
				};
				_trayIcon.Text = String.Join("\n", messages);
			}
		}

		async Task WaitForExitAsync(EventWaitHandle eventHandle, CancellationToken cancellationToken)
		{
			await eventHandle.WaitOneAsync(cancellationToken);
			_mainThreadInvokeTarget.BeginInvoke(() => Exit_MainThread());
		}

		void Exit_MainThread()
		{
			if (!_disposed)
			{
				_trayIcon.Visible = false;
				Application.Exit();
			}
		}

		void Exit(object sender, EventArgs e)
		{
			Exit_MainThread();
		}
	}
}
