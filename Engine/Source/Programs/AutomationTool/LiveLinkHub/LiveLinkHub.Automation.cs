// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.Linq;


namespace AutomationTool
{
	class MakeLiveLinkHubEditor : MakeCookedEditor
	{
		protected override ProjectParams MakeParams(string DLCName, string BasedOnReleaseVersion)
		{
			return new ProjectParams(
				Command: this
				, RawProjectPath: ProjectFile

				, NoBootstrapExe: true
				, DLCName: DLCName
				, BasedOnReleaseVersion: BasedOnReleaseVersion
				, DedicatedServer: bIsCookedCooker
				, NoClient: bIsCookedCooker
				, OptionalContent: true
				, ClientCookedTargets: new ParamList<string>() // Prevent AutodetectSettings from looking for a game target
				, EditorTargets: new ParamList<string>("LiveLinkHubEditor")
				, UbtArgs: "-SingleModulePlatform"
			);
		}

		protected override void ModifyParams(ProjectParams BuildParams)
		{
			base.ModifyParams(BuildParams);

			// We don't want the SDK dir / CookerSupportFiles
			BuildParams.CookerSupportFilesSubdirectory = null;
		}

		protected override void ModifyDeploymentContext(ProjectParams Params, DeploymentContext SC)
		{
			ModifyStageContext Context = CreateContext(Params, SC);

			DefaultModifyDeploymentContext(Params, SC, Context);

			Context.Apply(SC);

			string PlatName = SC.StageTargetPlatform.PlatformType.ToString();
			string ExeExtension = Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType);

			// Copy Zen binaries
			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"Engine/Binaries/{PlatName}/zen{ExeExtension}"),
				new FileReference($"Engine/Binaries/{PlatName}/zen{ExeExtension}"));

			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"Engine/Binaries/{PlatName}/zenserver{ExeExtension}"),
				new FileReference($"Engine/Binaries/{PlatName}/zenserver{ExeExtension}"));

			// Copy .target receipt to project and engine bin
			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"{Context.ProjectName}/Binaries/{PlatName}/{Context.ProjectName}.target"),
				new FileReference($"Engine/Binaries/{PlatName}/{Context.ProjectName}.target"));

			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"Engine/Binaries/{PlatName}/{Context.ProjectName}.target"),
				new FileReference($"Engine/Binaries/{PlatName}/{Context.ProjectName}.target"));

			// Stage TargetInfo and target script. Necessary to avoid "Running incorrect executable for target (...)"
			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"{Context.ProjectName}/Source/{Context.ProjectName}.Target.cs"),
				new FileReference($"Engine/Source/Programs/{Context.ProjectName}/Source/{Context.ProjectName}.Target.cs"));

			SC.FilesToStage.NonUFSFiles.Add(
				new StagedFileReference($"{Context.ProjectName}/Intermediate/TargetInfo.json"),
				new FileReference($"Engine/Source/Programs/{Context.ProjectName}/Intermediate/TargetInfo.json"));

			// Remove asset registry entry
			SC.FilesToStage.UFSFiles.Remove(new StagedFileReference($"{Context.ProjectName}/EditorClientAssetRegistry.bin"));

			// Move PDBs to debug (FIXME: should this be necessary? otherwise -nodebuginfo doesn't exclude them)
			SC.FilesToStage.NonUFSDebugFiles.Union(SC.FilesToStage.NonUFSFiles.Where(x => x.Key.HasExtension(".pdb")));

			// Remove any files from NonUFS that were also added to Debug
			SC.FilesToStage.NonUFSFiles = SC.FilesToStage.NonUFSFiles.Where(x => !SC.FilesToStage.NonUFSDebugFiles.ContainsKey(x.Key)).ToDictionary(x => x.Key, x => x.Value);
		}
	}
}
