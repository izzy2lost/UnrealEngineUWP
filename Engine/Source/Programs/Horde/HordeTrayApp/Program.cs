// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using EpicGames.Core;

#pragma warning disable CA1806 // Not checking return code from MessageBox

namespace HordeTrayApp
{
	static class Program
	{
		const string MutexName = "Horde.Agent.TrayApp-Mutex";
		const string EventName = "Horde.Agent.TrayApp-Exit";

		[DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
		public static extern int MessageBox(IntPtr hWnd, string text, string? caption, uint type);

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
							MessageBox(IntPtr.Zero, $"Unable to copy app to temp location. Error:\n\n{ex}", null, 0);
							return 1;
						}
					}
				}
			}

			BuildAvaloniaApp()
				.StartWithClassicDesktopLifetime(Array.Empty<string>(), ShutdownMode.OnExplicitShutdown);

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

		// Avalonia configuration, don't remove; also used by visual designer.
		public static AppBuilder BuildAvaloniaApp()
		{
			return AppBuilder.Configure<App>()
				.UsePlatformDetect()
				.LogToTrace();
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
}
