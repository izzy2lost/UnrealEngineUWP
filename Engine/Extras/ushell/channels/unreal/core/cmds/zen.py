# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import shutil
import unreal
import unrealcmd
import unreal.cmdline
import http.client
import json
import hashlib

#-------------------------------------------------------------------------------
class _Base(unrealcmd.Cmd):
    def perform_project_completion(self, prefix, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            conn.request("GET", "/prj", headers=headers)
            response = conn.getresponse()
            projects = json.loads(response.read().decode())
        except socket.timeout:
            return

        for project in projects:
            yield project['Id']

    def perform_oplog_completion(self, prefix, project_filter, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            if project_filter:
                conn.request("GET", f"/prj/{project_filter}", headers=headers)
                response = conn.getresponse()
                projects.append(json.loads(response.read().decode()))
            else:
                conn.request("GET", "/prj", headers=headers)
                response = conn.getresponse()
                projects = json.loads(response.read().decode())
        except socket.timeout:
            return

        for project in projects:
            for oplog in project['oplogs']:
                yield oplog['id']

    def get_project_id_from_project(self, project):
        if not project:
            self.print_error("An active project is required for this operation")

        normalized_project_path = str(project.get_path()).replace("\\","/")
        project_id = project.get_name() + "." + hashlib.md5(normalized_project_path.encode('utf-8')).hexdigest()[:8]
        return project_id;


    def get_project_or_default(self, explicit_project):
        if explicit_project:
            return explicit_project

        ue_context = self.get_unreal_context()
        return self.get_project_id_from_project(ue_context.get_project())


#-------------------------------------------------------------------------------
class _ZenUETargetBase(_Base):
    variant = unrealcmd.Arg("development", "Build variant to launch")
    runargs  = unrealcmd.Arg([str], "Additional arguments to pass to the process being launched")
    build    = unrealcmd.Opt(True, "Build the target prior to running it")

    def _build(self, target, variant, platform):
        if target.get_type() == unreal.TargetType.EDITOR:
            build_cmd = ("_build", "editor", variant)
        else:
            platform = platform or unreal.Platform.get_host()
            build_cmd = ("_build", "target", target.get_name(), platform, variant)

        import subprocess
        ret = subprocess.run(build_cmd)
        if ret.returncode:
            raise SystemExit(ret.returncode)

    def get_binary_path(self, target, platform=None):
        ue_context = self.get_unreal_context()

        if isinstance(target, str):
            target = ue_context.get_target_by_name(target)
        else:
            target = ue_context.get_target_by_type(target)

        if self.args.build:
            self._build(target, self.args.variant, platform)

        variant = unreal.Variant.parse(self.args.variant)
        if build := target.get_build(variant, platform):
            if build := build.get_binary_path():
                return build

        raise EnvironmentError(f"No {self.args.variant} build found for target '{target.get_name()}'")

    def run_ue_program_target(self, target_name, cmdargs = None):
        binary_path = self.get_binary_path(target_name)

        final_args = []
        if cmdargs: final_args += cmdargs
        if self.args.runargs: final_args += (*unreal.cmdline.read_ueified(*self.args.runargs),)

        launch_kwargs = {}
        if not sys.stdout.isatty():
            launch_kwargs = { "silent" : True }
        exec_context = self.get_exec_context()
        runnable = exec_context.create_runnable(str(binary_path), *final_args);
        runnable.launch(**launch_kwargs)
        return True if runnable.is_gui() else runnable.wait()


#-------------------------------------------------------------------------------
class _ZenUtilityBase(_Base):
    zenargs  = unrealcmd.Arg([str], "Additional arguments to pass to zen utility")

    def get_zen_utility_command(self, cmdargs):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        platform = unreal.Platform.get_host()
        exe_filename = "zen"
        if platform == "Win64":
            exe_filename = exe_filename + ".exe"

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/{platform}/{exe_filename}"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        final_args = []
        if cmdargs: final_args += cmdargs
        if self.args.zenargs: final_args += self.args.zenargs

        return bin_path, final_args


    def run_zen_utility(self, cmdargs):
        bin_path, final_args = self.get_zen_utility_command(cmdargs)

        cmd = self.get_exec_context().create_runnable(bin_path, *final_args)
        return cmd.run()

#-------------------------------------------------------------------------------
class Dashboard(_ZenUETargetBase):
    """ Starts the zen dashboard GUI."""

    def main(self):
        return self.run_ue_program_target("ZenDashboard")

#-------------------------------------------------------------------------------
class Start(_ZenUETargetBase):
    """ Starts an instance of zenserver."""
    SponsorProcessID = unrealcmd.Opt("", "Process ID to be added as a sponsor process for the zenserver process")

    def main(self):

        args = []
        if self.args.SponsorProcessID:
            args.append("-SponsorProcessID=" + str(self.args.SponsorProcessID))
        else:
            args.append("-SponsorProcessID=" + str(os.getppid()))

        return self.run_ue_program_target("ZenLaunch", args)

#-------------------------------------------------------------------------------
class Stop(_ZenUtilityBase):
    """ Stops any running instance of zenserver."""

    def main(self):
        return self.run_zen_utility(["down"])

#-------------------------------------------------------------------------------
class Status(_ZenUtilityBase):
    """ Get the status of running zenserver instances."""

    def main(self):
        return self.run_zen_utility(["status"])

#-------------------------------------------------------------------------------
class Version(_ZenUtilityBase):
    """ Get the version of the in-tree zenserver executable."""

    def main(self):
        return self.run_zen_utility(["version"])

#-------------------------------------------------------------------------------
class ImportSnapshot(_ZenUtilityBase):
    """ Imports an oplog snapshot into the running zenserver process."""
    snapshotdescriptor = unrealcmd.Arg(str, "Snapshot descriptor file path to import from")
    snapshotindex = unrealcmd.Arg(0, "0-based index of the snapshot within the snapshot descriptor to import from")
    projectid = unrealcmd.Opt("", "The zen project ID to import into (defaults to an ID based on the current ushell project)")
    oplog = unrealcmd.Opt("", "The zen oplog to import into (defaults to the oplog name in the snapshot)")
    cloudauthservice = unrealcmd.Opt("", "Name of the service to authorize with when importing from a cloud source")
    asyncimport = unrealcmd.Opt(False, "Trigger import but don't wait for completion")

    def complete_projectid(self, prefix):
        return self.perform_project_completion(prefix)

    def complete_oplog(self, prefix):
        return self.perform_oplog_completion(prefix, self.get_project_or_default(self.args.projectid))

    def complete_snapshotdescriptor(self, prefix):
        prefix = prefix or "."
        for item in os.scandir(prefix):
            if item.name.endswith(".json"):
                yield item.name
                return

        for item in os.scandir(prefix):
            if item.is_dir():
                yield item.name + "/"

    def _lookup_service_name(self):
        if getattr(self, "_service_name", None):
            return

        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        if value := default.OAuthProviderIdentifier:
            self._service_name = value
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.OAuthProviderIdentifier:
                self._service_name = value

    def refresh_zen_token(self):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        bin_type = "win-x64"
        if sys.platform == "darwin": bin_type = "osx-x64"
        elif sys.platform == "linux": bin_type = "linux-x64"

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/DotNET/OidcToken/{bin_type}/OidcToken.exe"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        self._service_name = self.args.cloudauthservice
        self._lookup_service_name()
        if not self._service_name:
            raise ValueError("Unable to discover service name")

        oidcargs = (
            "--ResultToConsole=true",
            "--Service=" + str(self._service_name),
        )

        if project := ue_context.get_project():
            oidcargs = (*oidcargs, "--Project=" + str(project.get_path()))

        cmd = self.get_exec_context().create_runnable(bin_path, *oidcargs)
        ret, output = cmd.run2()
        if ret != 0:
            return False

        tokenresponse = json.loads(output)

        self._AccessToken = tokenresponse['Token']
        self.print_info("Cloud access token obtained for import operation.")

        return True

    def get_exec_context(self):
        context = super().get_exec_context()
        if hasattr(self, '_AccessToken') and self._AccessToken:
            context.get_env()["UE-CloudDataCacheAccessToken"] = self._AccessToken
        return context

    def add_args_from_descriptor(self, snapshot, args):
        snapshot_type = snapshot['type']

        # TODO: file, zen snapshot types
        if snapshot_type == 'cloud':
            args.append('--cloud')
            args.append(snapshot['host'])
            args.append('--namespace')
            args.append(snapshot['namespace'])
            args.append('--bucket')
            args.append(snapshot['bucket'])
            args.append('--key')
            args.append(snapshot['key'])
        else:
            self.print_error(f"Unsupported snapshot type {snapshot_type}")
            return False

        return True

    @unrealcmd.Cmd.summarise
    def main(self):
        try:
            with open(self.args.snapshotdescriptor, "rt") as file:
                descriptor = json.load(file)
        except FileNotFoundError:
            self.print_error(f"Error accessing snapshot descriptor file {self.args.snapshotdescriptor}")
            return False

        snapshot = descriptor["snapshots"][self.args.snapshotindex]
        snapshot_type = snapshot['type']

        args = [
        "oplog-import",
        self.get_project_or_default(self.args.projectid),
        self.args.oplog or snapshot['targetplatform'],
        ]

        if self.args.asyncimport:
            args.append("--async")

        if not self.add_args_from_descriptor(snapshot, args):
            return False

        if snapshot_type == 'cloud':
            if not self.refresh_zen_token():
                return False

        return self.run_zen_utility(args)
