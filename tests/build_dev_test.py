"""Exercise DEV command forwarding without building or accessing private source."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("build_dev", ROOT / "scripts/build-dev.py")
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class BuildDev(unittest.TestCase):
    def invoke(self, target):
        with tempfile.TemporaryDirectory(prefix="stfc-dev-command-") as temporary:
            folder = Path(temporary)
            context = folder / "context.json"
            context.write_text(json.dumps({"source": "a" * 40, "aai_pin": "b" * 40, "tag": "",
                                           "run_id": "123", "run_number": 10, "run_attempt": 1}), encoding="utf-8")
            sysroot = folder / "selected-sysroot"
            sysroot.mkdir()
            arguments = ["build-dev.py", "--context", str(context), "--aai-source", str(folder), "--target", target]
            with patch.object(sys, "argv", arguments), \
                 patch.dict("os.environ", {"STFC_WINDOWS_MSVC_SYSROOT": str(sysroot)}), \
                 patch.object(BUILD.DEV, "command", side_effect=["b" * 40, ""]), \
                 patch.object(BUILD.DEV, "verify_windows"), patch.object(BUILD.DEV, "verify_mac"), \
                 patch.object(BUILD.subprocess, "run") as run:
                BUILD.main()
                return run.call_args_list[1].args[0], str(sysroot.resolve())

    def test_windows_passes_selected_sysroot_to_the_existing_zig_option(self):
        command, sysroot = self.invoke("x86_64-windows-msvc")
        self.assertIn(f"-Dmsvc-sysroot={sysroot}", command)

    def test_mac_does_not_receive_the_windows_sysroot(self):
        command, _ = self.invoke("aarch64-macos.14.6")
        self.assertFalse(any(argument.startswith("-Dmsvc-sysroot=") for argument in command))


if __name__ == "__main__":
    unittest.main()
