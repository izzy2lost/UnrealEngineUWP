// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls;
using EpicGames.Core;

namespace UnrealToolbox
{
	static class Program
	{
		const string MutexName = "UnrealToolbox-Mutex";
		const string EventName = "UnrealToolbox-Exit";

		public static SelfUpdateState? Update { get; private set; }

		[STAThread]
		public static int Main(string[] args)
		{
			if (!args.Any(x => x.Equals("-NoUpdate", StringComparison.OrdinalIgnoreCase)))
			{
				Update = SelfUpdateState.TryCreate("Unreal Toolbox", args);
			}

			for (; ; )
			{
				if (Update != null && Update.TryLaunchLatest())
				{
					return 0;
				}

				int result = RealMain(args);
				if (Update == null || !Update.IsUpdatePending())
				{
					return result;
				}
			}
		}

		static int RealMain(string[] args)
		{
			using SingleInstanceMutex mutex = new SingleInstanceMutex(MutexName);
			if (!mutex.Wait(0))
			{
				return 1;
			}

			BuildAvaloniaApp()
				.StartWithClassicDesktopLifetime(args, ShutdownMode.OnExplicitShutdown);

			return 0;
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
