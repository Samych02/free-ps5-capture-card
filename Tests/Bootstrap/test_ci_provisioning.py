import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CIProvisioningTests(unittest.TestCase):
    def test_ci_installs_locked_prerequisites_before_bootstrap(self):
        workflow = (ROOT / '.github/workflows/macos.yml').read_text()
        self.assertIn('brew install', workflow, 'CI must provision dependencies instead of assuming runner contents')
        provision = workflow.index('brew install')
        bootstrap = workflow.index('run: scripts/bootstrap.sh')
        self.assertLess(provision, bootstrap)
        installation = workflow[provision:bootstrap]
        formulas = json.loads((ROOT / 'Dependencies.lock.json').read_text())['systemDependencies']['formulae']
        for formula in formulas:
            self.assertIn(formula, installation, 'Missing CI prerequisite: ' + formula)
