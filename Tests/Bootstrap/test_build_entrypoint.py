"""Dependency-free tests for the native build entry point."""
from pathlib import Path
import subprocess
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


if __name__ == "__main__":
    unittest.main()
