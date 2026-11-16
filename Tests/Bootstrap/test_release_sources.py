import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('release_sources', ROOT / 'scripts/prepare-release.py')
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def manifest():
    return {'schema_version': 1, 'app_version': '0.1.0', 'app_build': '8',
            'project_revision': 'a' * 40, 'dependency_lock_sha256': digest(b'{}'),
            'app_tree_sha256': digest(b'fixture'),
            'components': [
                {'id': 'widget', 'kind': 'homebrew', 'version': '1.0',
                 'recipe_sha256': digest(b'recipe'), 'libraries': ['libwidget.dylib'],
                 'assets': [{'path': 'homebrew/widget/source.tar.gz',
                             'url': 'https://example.org/source.tar.gz', 'sha256': digest(b'source')}]},
                {'id': 'ffmpeg', 'kind': 'ffmpeg', 'version': '9.0.2',
                 'assets': [{'path': 'ffmpeg/source.tar.xz',
                             'url': 'https://example.org/ffmpeg.tar.xz', 'sha256': digest(b'ffmpeg')}]}]}


class Response(io.BytesIO):
    def __init__(self, data=b'source', url='https://example.org/source.tar.gz'):
        super().__init__(data)
        self.url = url


class ReleaseSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def app(self, data):
        app = self.root / 'App.app'
        contents = app / 'Contents'
        notices = contents / 'Resources/RuntimeNotices'
        embedded = contents / 'Resources/ProjectNotices/EmbeddedSources'
        notices.mkdir(parents=True)
        (embedded / 'ffmpeg').mkdir(parents=True)
        (contents / 'Info.plist').write_bytes(plistlib.dumps({
            'CFBundleShortVersionString': '0.1.0', 'CFBundleVersion': '8', 'CFBundleExecutable': 'App'}))
        (contents / 'MacOS').mkdir()
        (contents / 'MacOS/App').write_bytes(b'\xcf\xfa\xed\xfe' + b'synthetic app')
        (contents / 'Frameworks').mkdir()
        (contents / 'Frameworks/libwidget.dylib').write_bytes(b'\xcf\xfa\xed\xfe' + b'synthetic library')
        engine = contents / 'Frameworks/CaptureEngine.framework/Versions/A/CaptureEngine'
        engine.parent.mkdir(parents=True)
        engine.write_bytes(b'\xcf\xfa\xed\xfe' + b'synthetic first-party engine')
        (embedded / 'Dependencies.lock.json').write_bytes(b'{}')
        (embedded / 'ffmpeg/capture-build.json').write_text(json.dumps({
            'fixtures': False, 'source': {'sha256': digest(b'ffmpeg')}}))
        recipe = notices / 'widget-1.0/widget.rb'
        recipe.parent.mkdir()
        recipe.write_bytes(b'recipe')
        (notices / 'runtime-manifest.json').write_text(json.dumps([{
            'library': 'libwidget.dylib', 'source': '/opt/homebrew/Cellar/widget/1.0/lib/libwidget.dylib'}]))
        data['app_tree_sha256'] = release.app_tree_digest(app)
        return app

    def test_manifest_paths_urls_hashes_and_duplicate_coverage(self):
        release.validate_manifest(manifest())
        for path in ['../escape', '/absolute', 'a/../escape', 'a\\escape', 'a\nfile',
                     'homebrew/widget/a//escape', 'homebrew/widget/source.tar.gz/']:
            bad = manifest()
            bad['components'][0]['assets'][0]['path'] = path
            with self.subTest(path=path), self.assertRaises(ValueError):
                release.validate_manifest(bad)
        for url in ['http://example.org/x', 'file:///x', 'https://user:password@example.org/x']:
            bad = manifest()
            bad['components'][0]['assets'][0]['url'] = url
            with self.subTest(url=url), self.assertRaises(ValueError):
                release.validate_manifest(bad)
        bad = manifest()
        bad['components'][0]['assets'][0]['sha256'] = 'wrong'
        with self.assertRaises(ValueError):
            release.validate_manifest(bad)
        bad = manifest()
        bad['components'].append(copy.deepcopy(bad['components'][0]))
        with self.assertRaises(ValueError):
            release.validate_manifest(bad)

    def test_fetch_verifies_before_caching_and_rejects_redirect_downgrade(self):
        asset = manifest()['components'][0]['assets'][0]
        cache = self.root / 'cache'
        path = release.fetch_one(asset, cache, opener=lambda *a, **k: Response())
        self.assertEqual(path.read_bytes(), b'source')
        self.assertEqual(path, release.fetch_one(asset, cache, opener=lambda *a, **k: self.fail('cache missed')))
        path.write_bytes(b'corrupted')
        with self.assertRaises(ValueError):
            release.fetch_one(asset, cache)
        other = self.root / 'other'
        with self.assertRaises(ValueError):
            release.fetch_one(asset, other, opener=lambda *a, **k: Response(b'wrong'))
        self.assertFalse((other / asset['sha256']).exists())
        with self.assertRaises(ValueError):
            release.fetch_one(asset, other, opener=lambda *a, **k: Response(url='http://example.org/x'))

    def test_source_cache_symlink_is_rejected(self):
        asset = manifest()['components'][0]['assets'][0]
        cache = self.root / 'cache'
        cache.mkdir()
        target = self.root / 'outside'
        target.write_bytes(b'source')
        (cache / asset['sha256']).symlink_to(target)
        with self.assertRaises(ValueError):
            release.fetch_one(asset, cache)

    def test_formula_patch_order_and_unsupported_directives_fail_closed(self):
        component = manifest()['components'][0]
        source = '  sha256 "' + component['assets'][0]['sha256'] + '"\n'
        release.validate_recipe(component, source)
        with self.assertRaises(ValueError):
            release.validate_recipe(component, source + '  patch :DATA\n')
        patch = {'path': 'homebrew/widget/patches/one.patch',
                 'url': 'https://example.org/one.patch', 'sha256': digest(b'patch')}
        component['assets'].append(patch)
        recipe = (source + '  patch do\n    url "' + patch['url'] +
                  '"\n    sha256 "' + patch['sha256'] + '"\n  end\n')
        release.validate_recipe(component, recipe)
        for bad in [recipe.replace('one.patch', 'other.patch'),
                    recipe.replace(patch['sha256'], '0' * 64), source]:
            with self.assertRaises(ValueError):
                release.validate_recipe(component, bad)

    def test_uncovered_macos_formula_resource_is_not_silently_omitted(self):
        component = manifest()['components'][0]
        source = '  sha256 "' + component['assets'][0]['sha256'] + '"\n'
        resource = '    resource "extra-source" do\n      url "https://example.org/x"\n    end\n'
        release.validate_recipe(component, source + '  on_linux do\n' + resource + '  end\n')
        release.validate_recipe(component, source + '      resource(r).stage do\n      end\n')
        with self.assertRaises(ValueError):
            release.validate_recipe(component, source + resource)

    def test_app_requires_matching_build_lock_recipe_and_runtime_coverage(self):
        data = manifest()
        app = self.app(data)
        release.validate_app(app, data)
        for edit in ['build', 'lock', 'recipe', 'missing', 'version', 'ffmpeg']:
            bad = copy.deepcopy(data)
            if edit == 'build':
                bad['app_build'] = '7'
            elif edit == 'lock':
                bad['dependency_lock_sha256'] = '0' * 64
            elif edit == 'recipe':
                bad['components'][0]['recipe_sha256'] = '0' * 64
            elif edit == 'missing':
                bad['components'][0]['libraries'] = ['different.dylib']
            elif edit == 'version':
                bad['components'][0]['version'] = '2.0'
            else:
                bad['components'][1]['assets'][0]['sha256'] = '0' * 64
            with self.subTest(edit=edit), self.assertRaises(ValueError):
                release.validate_app(app, bad)

    def test_checksum_inventory_detects_changes_missing_files_extras_and_links(self):
        out = self.root / 'source'
        out.mkdir()
        (out / 'a').write_bytes(b'first')
        release.seal_tree(out)
        release.verify_tree(out)
        (out / 'a').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            release.verify_tree(out)
        (out / 'a').write_bytes(b'first')
        (out / 'extra').write_bytes(b'extra')
        with self.assertRaises(ValueError):
            release.verify_tree(out)
        (out / 'extra').unlink()
        (out / 'a').unlink()
        with self.assertRaises(ValueError):
            release.verify_tree(out)
        (out / 'a').symlink_to(self.root / 'outside')
        with self.assertRaises(ValueError):
            release.seal_tree(out)

    def test_missing_library_or_unlisted_framework_cannot_pass_source_coverage(self):
        data = manifest()
        app = self.app(data)
        release.validate_app(app, data)
        library = app / 'Contents/Frameworks/libwidget.dylib'
        original = library.read_bytes()
        library.unlink()
        with self.assertRaises(ValueError):
            release.validate_app(app, data)
        library.write_bytes(original)
        extra = app / 'Contents/Frameworks/Extra.framework/Extra'
        extra.parent.mkdir()
        extra.write_bytes(b'\xcf\xfa\xed\xfe' + b'unlisted code')
        with self.assertRaises(ValueError):
            release.validate_app(app, data)

    def test_frozen_app_tree_rejects_changed_or_extra_noncode_resources(self):
        data = manifest()
        app = self.app(data)
        resource = app / 'Contents/Resources/help.txt'
        resource.write_bytes(b'original resource')
        records = [{'path': p.relative_to(app).as_posix(), 'bytes': p.stat().st_size,
                    'mode': p.stat().st_mode & 0o777, 'sha256': digest(p.read_bytes())}
                   for p in sorted(app.rglob('*')) if p.is_file()]
        data['app_tree_sha256'] = digest(json.dumps(records, sort_keys=True, separators=(',', ':')).encode())
        release.validate_app(app, data)
        resource.write_bytes(b'changed resource')
        with self.assertRaises(ValueError):
            release.validate_app(app, data)
        resource.write_bytes(b'original resource')
        extra = app / 'Contents/Resources/unexpected.txt'
        extra.write_bytes(b'extra resource')
        with self.assertRaises(ValueError):
            release.validate_app(app, data)
        extra.unlink()
        executable = app / 'Contents/MacOS/App'
        executable.chmod((executable.stat().st_mode & 0o777) ^ 0o100)
        with self.assertRaises(ValueError):
            release.validate_app(app, data)

    def repository(self):
        repo = self.root / 'repo'
        subprocess.run(['git', 'init', '--quiet', str(repo)], check=True)
        (repo / 'source.txt').write_text('committed source')
        subprocess.run(['git', 'add', 'source.txt'], cwd=repo, check=True)
        subprocess.run(['git', '-c', 'user.name=Release Test', '-c', 'user.email=test@example.invalid',
                        'commit', '--quiet', '-m', 'fixture'], cwd=repo, check=True)
        return repo, subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()

    def test_project_export_uses_commit_not_working_tree_and_never_overwrites(self):
        repo, revision = self.repository()
        (repo / 'source.txt').write_text('uncommitted')
        (repo / 'private-canary.txt').write_text('synthetic untracked data')
        output = self.root / 'export'
        release.archive_project(repo, revision, output)
        self.assertEqual((output / 'source.txt').read_text(), 'committed source')
        self.assertFalse((output / 'private-canary.txt').exists())
        with self.assertRaises(FileExistsError):
            release.archive_project(repo, revision, output)

    def test_git_bundle_restores_expected_commit_without_network(self):
        repo, revision = self.repository()
        bundle = self.root / 'source.bundle'
        release.write_git_bundle(repo, revision, bundle)
        clone = self.root / 'clone'
        subprocess.run(['git', '-c', 'advice.detachedHead=false', 'clone', '--quiet', str(bundle), str(clone)], check=True)
        self.assertEqual(subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=clone, text=True).strip(), revision)
        with self.assertRaises(ValueError):
            release.write_git_bundle(repo, 'f' * 40, self.root / 'wrong.bundle')

    def test_release_manifest_covers_accepted_runtime_without_omitted_patches(self):
        data = json.loads((ROOT / 'docs/releases/v0.1.0-sources.json').read_text())
        release.validate_manifest(data)
        homebrew = [c for c in data['components'] if c['kind'] == 'homebrew']
        self.assertEqual(len(homebrew), 11)
        self.assertEqual(sum(len(c['libraries']) for c in homebrew), 13)
        ssh = next(c for c in homebrew if c['id'] == 'libssh2')
        self.assertEqual(len(ssh['assets']), 13)  # release archive + all 12 patches, in recipe order


if __name__ == '__main__':
    unittest.main()
