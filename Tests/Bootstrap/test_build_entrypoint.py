"""Dependency-free tests for the native build entry point."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/build-native.sh"


class BuildEntrypointTests(unittest.TestCase):
    def invoke(self, *args):
        self.assertTrue(SCRIPT.is_file(), "native build entry point is not implemented")
        return subprocess.run(["bash", str(SCRIPT), *args], cwd=ROOT,
                              text=True, capture_output=True)

    def test_help_does_not_require_build_dependencies(self):
        result = self.invoke("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--config", result.stdout)

    def test_unknown_option_is_rejected_before_configuration(self):
        result = self.invoke("--unknown")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Unknown option", result.stderr)

    def test_missing_configuration_is_rejected(self):
        result = self.invoke("--config")
        self.assertEqual(result.returncode, 2)
        self.assertIn("requires", result.stderr)

    def test_invalid_configuration_is_rejected(self):
        result = self.invoke("--config", "Typo")
        self.assertEqual(result.returncode, 2)
        self.assertIn("Invalid configuration", result.stderr)

    def test_configuration_switches_use_independent_build_directories(self):
        with tempfile.TemporaryDirectory(prefix="capture build paths ") as temporary:
            root=Path(temporary);(root/"scripts").mkdir();tools=root/"tools";tools.mkdir()
            script=root/"scripts/build-native.sh";script.write_text(SCRIPT.read_text())
            python=root/"build/python/bin/python3";python.parent.mkdir(parents=True)
            python.write_text("#!/bin/sh\nexit 0\n");python.chmod(0o755)
            stub=root/"stub.py"
            stub.write_text("""import json, os, sys
from pathlib import Path
tool=Path(sys.argv[1]).name
if tool=='cmake':
    with open(os.environ['CAPTURE_TEST_LOG'],'a') as log: log.write(json.dumps(sys.argv[2:])+'\\n')
elif tool=='brew': print(str(Path(os.environ['CAPTURE_TEST_ROOT'])/'dependencies'/sys.argv[-1]))
""")
            for name in ("cmake","ninja","brew","pkg-config"):
                tool=tools/name
                tool.write_text('#!/bin/sh\nexec "$CAPTURE_TEST_PYTHON" "$CAPTURE_TEST_STUB" "$0" "$@"\n')
                tool.chmod(0o755)
            log=root/"commands.jsonl";environment=os.environ.copy()
            environment.update(PATH=str(tools)+os.pathsep+environment["PATH"],CAPTURE_TEST_PYTHON=sys.executable,
                               CAPTURE_TEST_STUB=str(stub),CAPTURE_TEST_LOG=str(log),CAPTURE_TEST_ROOT=str(root))
            for configuration in ("Debug","Release","Debug"):
                result=subprocess.run(["bash",str(script),"--config",configuration,"--tests"],env=environment,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
            calls=[json.loads(line) for line in log.read_text().splitlines()]
            for offset,configuration in enumerate(("Debug","Release","Debug")):
                configure,build=calls[offset*2:offset*2+2]
                expected=str(root/"build/native"/configuration)
                self.assertEqual(configure[configure.index("-B")+1],expected)
                self.assertIn("-DCMAKE_BUILD_TYPE="+configuration,configure)
                self.assertEqual(build,["--build",expected,"--parallel"])

    def test_consumers_select_the_matching_configuration(self):
        project=(ROOT/"project.yml").read_text()
        self.assertEqual(project.count("framework: build/native/$(CONFIGURATION)/frameworks/CaptureEngine.framework"),2)
        for name in ("scripts/test.sh","scripts/test-app-unit-direct.sh","Tests/Support/integration.py"):
            self.assertIn("build/native/Debug",(ROOT/name).read_text(),name)
        packaging=(ROOT/"scripts/package-dev-app.sh").read_text()
        self.assertIn('python3 - "$ROOT" "$APP" "$CONFIG"',packaging)
        self.assertIn("'build/native' / configuration / '_deps/chiaki-src'",packaging)


if __name__ == "__main__":
    unittest.main()
