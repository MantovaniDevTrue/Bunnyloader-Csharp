from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
script=root/'tools/ci/publish_csharp_private.sh'
body=script.read_text()
assert '${{' not in body, 'Runtime shell must receive GitHub values through env'
assert 'MantovaniDevTrue/Bunnyloader-Csharp-Builds' in body
assert 'Nine regression suites required' in body and '--prerelease' in body
subprocess.run(['bash','-n',str(script)],check=True)
with tempfile.TemporaryDirectory(prefix='bunny-delivery-contract-') as directory:
    fake=Path(directory)/'gh'
    fake.write_text('#!/usr/bin/env bash\nset -eu\nif [ "$1" != api ]; then exit 91; fi\nprintf "false\\n"\n')
    fake.chmod(0o755)
    # Exercise token/private-destination failures independently of the branch release hold.
    env=dict(os.environ,PATH=directory+os.pathsep+os.environ['PATH'],GH_TOKEN='',
             GITHUB_REF='refs/heads/unit-test-delivery',GITHUB_HEAD_REF='')
    missing=subprocess.run(['bash',str(script)],env=env,cwd=root,capture_output=True,text=True)
    assert missing.returncode!=0 and 'missing' in missing.stdout
    env['GH_TOKEN']='unit-test-placeholder'
    public=subprocess.run(['bash',str(script)],env=env,cwd=root,capture_output=True,text=True)
    assert public.returncode!=0 and 'destination is not private' in public.stderr
print('PRIVATE_DELIVERY_SCRIPT_SYNTAX_MISSING_TOKEN_PUBLIC_DESTINATION_REFUSAL_PASS')
