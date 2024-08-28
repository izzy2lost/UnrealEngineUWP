// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO.Pipes;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using EpicGames.Core;
using HordeTrayApp.Forms;
using HordeAgent;
using Microsoft.Win32;

namespace HordeTrayApp
{
	record struct IdleStat(string Name, long Value, long MinValue);

	class AgentPlugin : TrayAppPluginBase
	{
		readonly ITrayAppHost _host;

		readonly BackgroundTask _clientTask;
		readonly BackgroundTask _tickPauseStateTask;

		readonly ToolStripMenuItem _enrollMenuItem;
		readonly ToolStripMenuItem _statusEnabled;
		readonly ToolStripMenuItem _statusDisabled;
		readonly ToolStripMenuItem _statusWhenIdle;

		readonly ToolStripMenuItem _statusMenuItem;
		readonly ToolStripMenuItem _logsMenuItem;

		readonly Settings _settings;

		AgentSettingsMessage? _agentSettings;
		IdleForm? _idleForm;

		void EnrollWithServer()
		{
			Uri? serverUrl = _agentSettings?.ServerUrl;
			if (serverUrl != null)
			{
				Process.Start(new ProcessStartInfo(new Uri(serverUrl, "agents/registration").ToString()) { UseShellExecute = true });
			}
		}

		public AgentPlugin(ITrayAppHost host)
		{
			_host = host;
			_settings = LoadSettings();

			_enrollMenuItem = new ToolStripMenuItem("Enroll with Server...");
			_enrollMenuItem.Click += (s, e) => EnrollWithServer();

			_statusEnabled = new ToolStripMenuItem("Enabled");
			_statusEnabled.Click += (s, e) => SetUserStatus(UserStatus.Enabled);

			_statusDisabled = new ToolStripMenuItem("Disabled");
			_statusDisabled.Click += (s, e) => SetUserStatus(UserStatus.Disabled);

			_statusWhenIdle = new ToolStripMenuItem("When Idle");
			_statusWhenIdle.Click += (s, e) => SetUserStatus(UserStatus.WhenIdle);

			ToolStripMenuItem showStatsMenuItem = new ToolStripMenuItem("Stats...");
			showStatsMenuItem.Click += Status_Stats_OnClick;

			_statusMenuItem = new ToolStripMenuItem("Status");
			_statusMenuItem.DropDownItems.Add(_statusEnabled);
			_statusMenuItem.DropDownItems.Add(_statusDisabled);
			_statusMenuItem.DropDownItems.Add(_statusWhenIdle);
			_statusMenuItem.DropDownItems.Add(new ToolStripSeparator());
			_statusMenuItem.DropDownItems.Add(showStatsMenuItem);

			_logsMenuItem = new ToolStripMenuItem("Open logs dir");
			_logsMenuItem.Click += OnOpenLogs;

			_clientTask = BackgroundTask.StartNew(StatusTaskAsync);
			_tickPauseStateTask = BackgroundTask.StartNew(ctx => TickPauseStateAsync(ctx));
		}

		private static Settings LoadSettings()
		{
			Settings? result = null;

			DirectoryReference? settingsRoot = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
			if (settingsRoot != null)
			{
				FileReference settingsPath = FileReference.Combine(settingsRoot, "Epic", "Horde", "TrayApp", "Settings.json");
				if (FileReference.Exists(settingsPath))
				{
					try
					{
						using FileStream stream = FileReference.Open(settingsPath, FileMode.Open, FileAccess.Read);
						result = JsonSerializer.Deserialize<Settings>(stream);
					}
					catch (Exception)
					{
					}
				}
				else
				{
					// File not found, create a file containing the default settings
					DirectoryReference.CreateDirectory(settingsPath.Directory);
					using FileStream stream = FileReference.Open(settingsPath, FileMode.OpenOrCreate, FileAccess.Write);
					JsonSerializer.Serialize(stream, new Settings(), new JsonSerializerOptions() { WriteIndented = true });
				}
			}

			return result ?? new Settings();
		}

		public override void PopulateMenu(ContextMenuStrip contextMenu)
		{
			base.PopulateMenu(contextMenu);

			contextMenu.Items.Add(_enrollMenuItem);
			contextMenu.Items.Add(_statusMenuItem);
			contextMenu.Items.Add(new ToolStripSeparator());
			contextMenu.Items.Add(_logsMenuItem);
		}

		public override void UpdateMenu()
		{
			base.UpdateMenu();

			UserStatus status = GetUserStatus();
			_statusEnabled.Checked = (status == UserStatus.Enabled);
			_statusDisabled.Checked = (status == UserStatus.Disabled);
			_statusWhenIdle.Checked = (status == UserStatus.WhenIdle);
		}

		public override async ValueTask DisposeAsync()
		{
			await base.DisposeAsync();

			if (_idleForm != null)
			{
				_idleForm.Dispose();
				_idleForm = null;
			}

			_enrollMenuItem.Dispose();
			_statusEnabled.Dispose();
			_statusDisabled.Dispose();
			_statusWhenIdle.Dispose();

			_logsMenuItem.Dispose();
			_statusMenuItem.Dispose();

			await _tickPauseStateTask.DisposeAsync();
			await _clientTask.DisposeAsync();
		}

		private void OnOpenLogs(object? sender, EventArgs e)
		{
			DirectoryReference? programDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
			if (programDataDir != null)
			{
				DirectoryReference logsDir = DirectoryReference.Combine(programDataDir, "Epic", "Horde", "Agent");
				if (DirectoryReference.Exists(logsDir))
				{
					Process.Start(new ProcessStartInfo { FileName = logsDir.FullName, UseShellExecute = true });
				}
				else
				{
					MessageBox.Show("Unable to open logs dir " + logsDir.FullName, "Horde Tray App", MessageBoxButtons.OK, MessageBoxIcon.Error);
				}
			}
		}

		enum UserStatus
		{
			Enabled = 0,
			Disabled = 1,
			WhenIdle = 2,
		}

		const string RegistryKey = "HKEY_CURRENT_USER\\Software\\Epic Games\\Horde\\TrayApp";
		const string RegistryStatusValue = "Status";

		private static UserStatus GetUserStatus()
		{
			return (UserStatus)((Registry.GetValue(RegistryKey, RegistryStatusValue, null) as int?) ?? 0);
		}

		private void SetUserStatus(UserStatus status)
		{
			Registry.SetValue(RegistryKey, RegistryStatusValue, (int)status);
			_statusChangedEvent.Set();
		}

		private void Status_Stats_OnClick(object? sender, EventArgs e)
		{
			if (_idleForm == null)
			{
				_idleForm = new IdleForm();
				_idleForm.FormClosed += (s, e) =>
				{
					_idleForm.Dispose();
					_idleForm = null;
				};
				_idleForm.Show();
			}
		}

		TrayAppPluginStatus? _status;

		void SetStatus(AgentStatusMessage status)
		{
			if (!status.Healthy)
			{
				string message = String.IsNullOrEmpty(status.Detail) ? "Error. Check logs." : (status.Detail.Length > 100) ? status.Detail.Substring(0, 100) : status.Detail;
				_status = new TrayAppPluginStatus(TrayAppPluginState.Error, message);
			}
			else if (status.NumLeases > 0)
			{
				string message = (status.NumLeases == 1) ? "Currently handling 1 lease" : $"Currently handling {status.NumLeases} leases";
				_status = new TrayAppPluginStatus(TrayAppPluginState.Busy, message);
			}
			else if (_enabled)
			{
				string message = "Agent is operating normally";
				_status = new TrayAppPluginStatus(TrayAppPluginState.Ok, message);
			}
			else
			{
				string message = "Agent is paused";
				_status = new TrayAppPluginStatus(TrayAppPluginState.Paused, message);
			}
			_host.UpdateStatus();
		}

		public override TrayAppPluginStatus GetStatus()
			=> _status ?? base.GetStatus();

		async Task StatusTaskAsync(CancellationToken cancellationToken)
		{
			SetStatus(AgentStatusMessage.Starting);
			for (; ; )
			{
				try
				{
					await PollForStatusUpdatesAsync(cancellationToken);
				}
				catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
				{
					break;
				}
				catch
				{
					SetStatus(new AgentStatusMessage(false, 0, "Unable to connect to Agent. Check the service is running."));
					await Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken);
				}
			}
		}

#pragma warning disable IDE1006
		[StructLayout(LayoutKind.Sequential)]
		struct LASTINPUTINFO
		{
			public int cbSize;
			public uint dwTime;
		}

		[DllImport("user32.dll")]
		static extern bool GetLastInputInfo(ref LASTINPUTINFO plii);

		[DllImport("kernel32.dll")]
		static extern uint GetTickCount();

		[StructLayout(LayoutKind.Sequential)]
		struct FILETIME
		{
			public uint dwLowDateTime;
			public uint dwHighDateTime;

			public readonly ulong Total => dwLowDateTime | ((ulong)dwHighDateTime << 32);
		};

		[StructLayout(LayoutKind.Sequential)]
		struct MEMORYSTATUSEX
		{
			public int dwLength;
			public uint dwMemoryLoad;
			public ulong ullTotalPhys;
			public ulong ullAvailPhys;
			public ulong ullTotalPageFile;
			public ulong ullAvailPageFile;
			public ulong ullTotalVirtual;
			public ulong ullAvailVirtual;
			public ulong ullAvailExtendedVirtual;
		}

		[DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
		static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern bool GetSystemTimes(out FILETIME lpIdleTime, out FILETIME lpKernelTime, out FILETIME lpUserTime);
#pragma warning restore IDE1006

		bool _enabled;
		readonly AsyncEvent _statusChangedEvent = new AsyncEvent();
		readonly AsyncEvent _enabledChangedEvent = new AsyncEvent();

		async Task TickPauseStateAsync(CancellationToken cancellationToken)
		{
			await using BackgroundTask cpuStatsTask = BackgroundTask.StartNew(ctx => TickCpuStatsAsync(ctx));
			await using BackgroundTask criticalProcessTask = BackgroundTask.StartNew(ctx => TickCriticalProcessAsync(ctx));

			TimeSpan pollInterval = TimeSpan.FromSeconds(0.25);

			Stopwatch stateChangeTimer = Stopwatch.StartNew();
			while (!cancellationToken.IsCancellationRequested)
			{
				Task statusChangedTask = _statusChangedEvent.Task;

				UserStatus userStatus = GetUserStatus();
				if (userStatus == UserStatus.Enabled)
				{
					if (!_enabled)
					{
						_enabled = true;
						_enabledChangedEvent.Set();
					}
				}
				else if (userStatus == UserStatus.Disabled)
				{
					if (_enabled)
					{
						_enabled = false;
						_enabledChangedEvent.Set();
					}
				}

				DateTime utcNow = DateTime.UtcNow;
				IEnumerable<IdleStat> idleStats = GetIdleStats();

				bool idle = idleStats.All(x => x.Value >= x.MinValue);
				if (idle == _enabled)
				{
					stateChangeTimer.Restart();
				}

				const int WakeTimeSecs = 2;
				const int IdleTimeSecs = 30;
				int stateChangeTime = (int)stateChangeTimer.Elapsed.TotalSeconds;
				int stateChangeMaxTime = _enabled ? WakeTimeSecs : IdleTimeSecs;
				_idleForm?.TickStats(_enabled, stateChangeTime, stateChangeMaxTime, idleStats);

				if (userStatus == UserStatus.WhenIdle && stateChangeTime >= stateChangeMaxTime)
				{
					_enabled ^= true;
					_enabledChangedEvent.Set();
					stateChangeTimer.Restart();
				}

				await Task.WhenAny(statusChangedTask, Task.Delay(pollInterval, cancellationToken));
			}
		}

		IEnumerable<IdleStat> GetIdleStats()
		{
			// Check there has been no input for a while
			LASTINPUTINFO lastInputInfo = new LASTINPUTINFO();
			lastInputInfo.cbSize = Marshal.SizeOf<LASTINPUTINFO>();

			if (GetLastInputInfo(ref lastInputInfo))
			{
				yield return new IdleStat("LastInputTime", (GetTickCount() - lastInputInfo.dwTime) / 1000, _settings.Idle.MinIdleTimeSecs);
			}

			// Check that no critical processes are running
			if (_settings.Idle.CriticalProcesses.Any())
			{
				yield return new IdleStat("CriticalProcCount", -_idleCriticalProcessCount, 0);
			}

			// Only look at memory/CPU usage if we're not paused; executing jobs will increase them
			if (!_enabled)
			{
				// Check the CPU usage doesn't exceed the limit
				yield return new IdleStat("IdleCpuPct", _idleCpuPct, _settings.Idle.MinIdleCpuPct);

				// Check there's enough available virtual memory 
				MEMORYSTATUSEX memoryStatus = new MEMORYSTATUSEX();
				memoryStatus.dwLength = Marshal.SizeOf<MEMORYSTATUSEX>();

				if (GlobalMemoryStatusEx(ref memoryStatus))
				{
					yield return new IdleStat("VirtualMemMb", (long)(memoryStatus.ullAvailPhys + memoryStatus.ullAvailPageFile) / (1024 * 1024), _settings.Idle.MinFreeVirtualMemMb);
				}
			}
		}

		int _idleCpuPct = 0;
		int _idleCriticalProcessCount = 0;

		async Task TickCpuStatsAsync(CancellationToken cancellationToken)
		{
			const int NumSamples = 10;
			TimeSpan sampleInterval = TimeSpan.FromSeconds(0.2);
			(ulong IdleTime, ulong TotalTime)[] samples = new (ulong IdleTime, ulong TotalTime)[NumSamples];

			int sampleIdx = 0;
			for (; ; )
			{
				if (GetSystemTimes(out FILETIME idleTime, out FILETIME kernelTime, out FILETIME userTime))
				{
					(ulong prevIdleTime, ulong prevTotalTime) = samples[sampleIdx];
					(ulong nextIdleTime, ulong nextTotalTime) = (idleTime.Total, kernelTime.Total + userTime.Total);

					samples[sampleIdx] = (nextIdleTime, nextTotalTime);
					sampleIdx = (sampleIdx + 1) % NumSamples;

					if (prevTotalTime > 0 && nextTotalTime > prevTotalTime)
					{
						_idleCpuPct = (int)(((nextIdleTime - prevIdleTime) * 100) / (nextTotalTime - prevTotalTime));
					}
				}
				await Task.Delay(sampleInterval, cancellationToken);
			}
		}

		async Task TickCriticalProcessAsync(CancellationToken cancellationToken)
		{
			TimeSpan sampleInterval = TimeSpan.FromSeconds(1.0);

			for (; ; )
			{
				try
				{
					if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows) && _settings.Idle.CriticalProcesses.Any())
					{
						IEnumerable<int> hordeProcessIds = Process.GetProcessesByName("HordeAgent").Select(x => x.Id);
						IEnumerable<Process> criticalProcesses = _settings.Idle.CriticalProcesses
							.Select(x => Path.GetFileNameWithoutExtension(x).ToUpperInvariant())
							.Distinct()
							.SelectMany(x => Process.GetProcessesByName(x))
							.Where(x => !x.HasExited);

						// Ignore processes that are descendants of HordeAgent
						if (hordeProcessIds.Any() && criticalProcesses.Any())
						{
							criticalProcesses = criticalProcesses
								.Where(x => !ProcessUtils.GetAncestorProcesses(x)
								.Select(x => x.Id).Intersect(hordeProcessIds).Any())
								.Where(x => !x.HasExited);
						}

						_idleCriticalProcessCount = criticalProcesses.Count();
					}
				}
				catch (InvalidOperationException)
				{
					// If a process stops running Process.Id will throw an exception
				}
				await Task.Delay(sampleInterval, cancellationToken);
			}
		}

		async Task PollForStatusUpdatesAsync(CancellationToken cancellationToken)
		{
			AgentMessageBuffer message = new AgentMessageBuffer();
			using (NamedPipeClientStream pipeClient = new NamedPipeClientStream(".", AgentMessagePipe.PipeName, PipeDirection.InOut))
			{
				SetStatus(new AgentStatusMessage(false, 0, "Connecting to agent..."));
				await pipeClient.ConnectAsync(cancellationToken);

				SetStatus(new AgentStatusMessage(false, 0, "Waiting for status update."));
				for (; ; )
				{
					Task idleChangeTask = _enabledChangedEvent.Task;

					bool enabled = _enabled;
					message.Set(AgentMessageType.SetEnabledRequest, new AgentEnabledMessage(enabled));
					await message.SendAsync(pipeClient, cancellationToken);

					if (_agentSettings == null)
					{
						message.Set(AgentMessageType.GetSettingsRequest);
						await message.SendAsync(pipeClient, cancellationToken);

						if (!await message.TryReadAsync(pipeClient, cancellationToken))
						{
							break;
						}
						if (message.Type == AgentMessageType.GetSettingsResponse)
						{
							_agentSettings = message.Parse<AgentSettingsMessage>();
						}
					}

					message.Set(AgentMessageType.GetStatusRequest);
					await message.SendAsync(pipeClient, cancellationToken);

					if (!await message.TryReadAsync(pipeClient, cancellationToken))
					{
						break;
					}

					switch (message.Type)
					{
						case AgentMessageType.GetStatusResponse:
							AgentStatusMessage status = message.Parse<AgentStatusMessage>();
							SetStatus(status);
							break;
					}

					await Task.WhenAny(idleChangeTask, Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken));
				}
			}
		}
	}
}
