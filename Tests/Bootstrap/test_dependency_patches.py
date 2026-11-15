import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DependencyPatchTests(unittest.TestCase):
    def test_all_locked_patch_digests_match(self):
        for patch in json.loads((ROOT / 'Dependencies.lock.json').read_text())['chiaki']['patches']:
            self.assertEqual(hashlib.sha256((ROOT / patch['file']).read_bytes()).hexdigest(), patch['sha256'])

    def test_populated_sources_revalidate_patches_on_every_configure(self):
        source = (ROOT / 'cmake/ChiakiDependency.cmake').read_text()
        after_population = source.split('FetchContent_MakeAvailable(chiaki)', 1)[1]
        self.assertIn('ApplyChiakiPatches.cmake', after_population,
                      'FetchContent may skip PATCH_COMMAND for an already populated source tree')
        self.assertIn('COMMAND_ERROR_IS_FATAL ANY', after_population)
