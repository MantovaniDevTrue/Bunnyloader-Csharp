from pathlib import Path
import json
import os
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
script=root/'Split/Bunny-Loader/tools/ci/verify_private_delivery.py'
sha='a'*40
gradle=(root/'Split/Bunny-Loader/app/build.gradle.kts').read_text()
version=re.search(r'versionName\s*=\s*"([A-Za-z0-9._-]+)"',gradle).group(1)
example=json.loads((root/'Split/Bunny-Loader/samples/ExampleModCSharp/Playable/manifest.json').read_text())['version']
assets=[{'name':n,'size':100} for n in ['BunnyLoader-CSharp-'+version+'-SignedV3.apk','BunnyLoader-CSharp-'+version+'-SignedV3.apk.sha256','ExampleModCSharp-v'+example+'.bmod','ExampleModCSharp-v'+example+'.bmod.sha256']]
release={'tag_name':'csharp-v3-run-123-a1','body':'Source '+sha,'assets':assets}
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);fake=p/'gh'
    fake.write_text('#!/usr/bin/env python3\nimport os,sys\nassert sys.argv[1]=="api", "No comments or writes allowed"\nif "tags/" in sys.argv[2]:print(open(os.environ["TEST_RELEASE"]).read())\nelse:print(os.environ.get("TEST_PRIVATE",\'{"private":true}\'))\n')
    fake.chmod(0o755);fixture=p/'release.json'
    env=dict(os.environ,PATH=tmp+os.pathsep+os.environ['PATH'],GH_TOKEN='test-only',BUNNY_BUILD_RESULT='success',BUNNY_SOURCE_SHA=sha,BUNNY_RELEASE_TAG=release['tag_name'],TEST_RELEASE=str(fixture),GITHUB_STEP_SUMMARY=str(p/'summary.md'))
    def run(payload,success,reason=''):
        fixture.write_text(json.dumps(payload))
        result=subprocess.run(['python3',str(script)],cwd=root,env=env,capture_output=True,text=True)
        assert (result.returncode==0)==success,result.stdout+result.stderr
        if reason:assert reason in result.stdout,result.stdout
    run(release,True)
    run(dict(release,body='different source'),False,'ReleaseSourceIdentityMismatch')
    run(dict(release,assets=assets[1:]),False,'ExpectedOneNonemptyAsset')
    run(dict(release,assets=assets+assets[:1]),False,'ExpectedOneNonemptyAsset')
    env['TEST_PRIVATE']='{"private":false}';run(release,False,'PrivateDestinationRequired');env.pop('TEST_PRIVATE')
    env['BUNNY_BUILD_RESULT']='failure';run(release,False,'AndroidBuildNotSuccessful');env['BUNNY_BUILD_RESULT']='success'
    env['GH_TOKEN']='';run(release,False,'PrivateReadTokenMissing')
print('PRIVATE_DELIVERY_READ_ONLY_IDENTITY_ASSETS_AND_FAILURES_PASS cases=7')
