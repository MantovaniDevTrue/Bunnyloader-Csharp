"""Read-only verification. A comment permission cannot invalidate a delivered APK."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def verify():
    if os.environ.get('BUNNY_BUILD_RESULT') != 'success':
        raise ValueError('AndroidBuildNotSuccessful')
    if not os.environ.get('GH_TOKEN'):
        raise ValueError('PrivateReadTokenMissing')
    repo = 'MantovaniDevTrue/Bunnyloader-Csharp-Builds'
    tag = os.environ.get('BUNNY_RELEASE_TAG', '')
    sha = os.environ.get('BUNNY_SOURCE_SHA', '')
    if not re.fullmatch(r'csharp-v3-run-\d+-a\d+', tag) or not re.fullmatch(r'[0-9a-f]{40}', sha):
        raise ValueError('InvalidRunIdentity')
    def fetch(endpoint):
        result = subprocess.run(['gh', 'api', endpoint], capture_output=True, text=True)
        if result.returncode:
            raise ValueError('PrivateReleaseReadFailed Endpoint=' + endpoint + ' ExitCode=' + str(result.returncode))
        return json.loads(result.stdout)
    if fetch('repos/' + repo).get('private') is not True:
        raise ValueError('PrivateDestinationRequired')
    release = fetch('repos/' + repo + '/releases/tags/' + tag)
    if release.get('tag_name') != tag or sha not in release.get('body', ''):
        raise ValueError('ReleaseSourceIdentityMismatch')
    gradle = Path('Split/Bunny-Loader/app/build.gradle.kts').read_text()
    match = re.search(r'versionName\s*=\s*"([A-Za-z0-9._-]+)"', gradle)
    if not match: raise ValueError('VersionNameMissing Path=Split/Bunny-Loader/app/build.gradle.kts')
    version = match.group(1)
    apk = 'BunnyLoader-CSharp-' + version + '-SignedV3.apk'
    assets = release.get('assets', [])
    def require(name):
        matching = [a for a in assets if a.get('name') == name and a.get('size', 0) > 0]
        if len(matching) != 1:
            raise ValueError('ExpectedOneNonemptyAsset Stage=VerifyAssets Expected=1 Actual=' + str(len(matching)) + ' Name=' + name)
    require(apk)
    require(apk + '.sha256')
    manifest = json.loads(Path('Split/Bunny-Loader/samples/ExampleModCSharp/Playable/manifest.json').read_text())
    example = 'ExampleModCSharp-v' + manifest['version'] + '.bmod'
    require(example)
    require(example + '.sha256')
    return tag, sha, apk, example, 'https://github.com/' + repo + '/releases/tag/' + tag


def main():
    try:
        tag, sha, apk, example, url = verify()
        message = 'Private APK delivery verified: [' + tag + '](' + url + ')\n\n'
        message += '- Source: `' + sha + '`\n- APK: `' + apk + '`\n- ExampleMod: `' + example + '`\n'
        message += '\nMetadata verification; signing/content checks remain in the Android build. Gameplay is not certified.\n'
        print('BUNNY_PRIVATE_DELIVERY_METADATA=PASS ' + tag)
    except (ValueError, KeyError, AttributeError, OSError, json.JSONDecodeError) as error:
        message = 'Private delivery verification failed: ' + str(error) + '\n'
        print('::error::' + message.strip())
        if os.environ.get('GITHUB_STEP_SUMMARY'):
            with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary: summary.write(message)
        return 1
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary: summary.write(message)
    return 0


if __name__ == '__main__': sys.exit(main())
