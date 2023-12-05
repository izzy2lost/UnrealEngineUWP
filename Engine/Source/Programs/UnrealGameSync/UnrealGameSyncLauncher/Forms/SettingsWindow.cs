// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	partial class SettingsWindow : Form
	{
		[DllImport("user32.dll")]
		private static extern IntPtr SendMessage(IntPtr hWnd, int msg, int wParam, [MarshalAs(UnmanagedType.LPWStr)] string lParam);

		public delegate Task SyncAndRunDelegate(IPerforceConnection perforce, string? depotPath, bool preview, ILogger logWriter, CancellationToken cancellationToken);

		const int EmSetcuebanner = 0x1501;

		string? _logText;
		readonly SyncAndRunDelegate _syncAndRun;

		public SettingsWindow(string? prompt, string? logText, string? serverAndPort, string? userName, string? depotPath, bool preview, SyncAndRunDelegate syncAndRun)
		{
			InitializeComponent();

			if(prompt != null)
			{
				PromptLabel.Text = prompt;
			}

			_logText = logText;
			ServerTextBox.Text = serverAndPort ?? String.Empty;
			UserNameTextBox.Text = userName ?? String.Empty;
			DepotPathTextBox.Text = depotPath ?? String.Empty;
			UsePreviewBuildCheckBox.Checked = preview;
			_syncAndRun = syncAndRun;

			ViewLogBtn.Visible = logText != null;
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			SendMessage(ServerTextBox.Handle, EmSetcuebanner, 1, "Default Server");
			SendMessage(UserNameTextBox.Handle, EmSetcuebanner, 1, "Default User");
		}

		private void ViewLogBtn_Click(object sender, EventArgs e)
		{
			using LogWindow log = new LogWindow(_logText ?? String.Empty);
			log.ShowDialog(this);
		}

		private void ConnectBtn_Click(object sender, EventArgs e)
		{
			// Update the settings
			LauncherSettings launcherSettings = new LauncherSettings();

			launcherSettings.PerforceServerAndPort = ServerTextBox.Text.Trim();
			if(launcherSettings.PerforceServerAndPort.Length == 0)
			{
				launcherSettings.PerforceServerAndPort = null;
			}

			launcherSettings.PerforceUserName = UserNameTextBox.Text.Trim();
			if(launcherSettings.PerforceUserName.Length == 0)
			{
				launcherSettings.PerforceUserName = null;
			}

			launcherSettings.PerforceDepotPath = DepotPathTextBox.Text.Trim();
			if(launcherSettings.PerforceDepotPath.Length == 0)
			{
				launcherSettings.PerforceDepotPath = null;
			}

			launcherSettings.PreviewBuild = UsePreviewBuildCheckBox.Checked;
			launcherSettings.Save();

			PerforceSettings perforceSettings = new PerforceSettings(PerforceSettings.Default);
			if (!String.IsNullOrEmpty(launcherSettings.PerforceServerAndPort))
			{
				perforceSettings.ServerAndPort = launcherSettings.PerforceServerAndPort;
			}
			if (!String.IsNullOrEmpty(launcherSettings.PerforceUserName))
			{
				perforceSettings.UserName = launcherSettings.PerforceUserName;
			}
			perforceSettings.PreferNativeClient = true;

			// Create the P4 connection
			CaptureLogger logger = new CaptureLogger();

			// Create the task for connecting to this server
			ModalTask? task = PerforceModalTask.Execute(this, "Updating", "Checking for updates, please wait...", perforceSettings, (p, c) => _syncAndRun(p, launcherSettings.PerforceDepotPath, launcherSettings.PreviewBuild, logger, c), logger);
			if (task != null)
			{
				if(task.Succeeded)
				{
					launcherSettings.Save();
					DialogResult = DialogResult.OK;
					Close();
				}
				PromptLabel.Text = task.Error;
			}

			_logText = logger.Render(Environment.NewLine);
			ViewLogBtn.Visible = true;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
