"""Package 相机反虚化 independently; never install or execute the DLL."""
# Copyright (c) 2026 CameraNearClarity contributors; SPDX-License-Identifier: MIT
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/packages'
sys.path.insert(0, str(ROOT))
import build


def digest(data):
    return sha256(data).hexdigest()


def archive(name, kind, files):
    path = OUTPUT / name
    with ZipFile(path, 'w') as zipped:
        for relative, data in sorted(files.items()):
            entry = ZipInfo(relative, (2026, 1, 1, 0, 0, 0))
            entry.compress_type = ZIP_DEFLATED
            zipped.writestr(entry, data)
    with ZipFile(path) as zipped:
        if sorted(zipped.namelist()) != sorted(files):
            raise ValueError('ZIP members differ')
        for relative, data in files.items():
            if zipped.read(relative) != data:
                raise ValueError('ZIP payload differs: ' + relative)
    return dict(file=name, kind=kind, sha256=digest(path.read_bytes()), bytes=path.stat().st_size,
                members=[dict(path=n, bytes=len(d), sha256=digest(d)) for n,d in sorted(files.items())])


def main():
    plan = json.loads((ROOT/'release_plan.json').read_text(encoding='utf-8'))
    if (ROOT/'.git').exists():
        commit = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
        if subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']):
            raise ValueError('Commit the current source before packaging')
        names = subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode('utf-8').split('\0')
        names = [name for name in names if name]
    else:
        snapshot = json.loads((ROOT/'SOURCE_MANIFEST.json').read_text(encoding='utf-8'))
        commit, names = snapshot['source_commit'], list(snapshot['files'])
    sources = {name:(ROOT/name).read_bytes() for name in names}
    allowed_files = {'README.md','BUILDING.md','测试步骤.md','快捷键代码说明.txt','CameraNearClarity.ini','LICENSE','build.py','sora_mod.ini',
                     'release_plan.json','.gitignore','.gitattributes'}
    for name in sources:
        if name not in allowed_files and not (name.startswith('src/') or name == 'tools/package_native.py'):
            raise ValueError('Unexpected A source member: ' + name)
    dll = (ROOT/'build/native/CameraNearClarity.dll').read_bytes()
    fresh = ROOT/'build/native/report.json'
    report_path = fresh if fresh.exists() else ROOT/'build/native/accepted-report.json'
    raw = report_path.read_bytes()
    report = json.loads(raw)
    if digest(dll) != report['dll_sha256']:
        raise ValueError('DLL differs from original build report')
    inputs = report.get('compile_inputs') or {name:report['sources'][name] for name in build.COMPILE_INPUTS}
    if set(inputs) != set(build.COMPILE_INPUTS):
        raise ValueError('Actual compile input set differs')
    for name, expected in inputs.items():
        if digest(sources[name]) != expected:
            raise ValueError('Compiled input changed: ' + name)
    if (ROOT/'LICENSE').read_text(encoding='utf-8').encode('utf-8') not in dll:
        raise ValueError('Embedded MIT notice missing')
    source_manifest = dict(source_commit=commit, files={n:digest(d) for n,d in sorted(sources.items())})
    sources['SOURCE_MANIFEST.json'] = (json.dumps(source_manifest,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
    install = {'mods/CameraNearClarity/CameraNearClarity.dll':dll,
               'mods/CameraNearClarity/sora_mod.ini':sources['sora_mod.ini'],
               'mods/CameraNearClarity/CameraNearClarity.ini':sources['CameraNearClarity.ini']}
    expected_ini = b'[SoraMod]\nApi=1\nDLL=CameraNearClarity.dll\nInitialize=SoraCameraInitialize\n'
    if install['mods/CameraNearClarity/sora_mod.ini'] != expected_ini:
        raise ValueError('Camera declaration differs from the ABI contract')
    OUTPUT.mkdir(parents=True,exist_ok=True)
    (OUTPUT/'native-build-report.json').write_bytes(raw)
    version = plan['version']
    suffix = '-local-candidate' if plan.get('channel') == 'local-candidate' else ''
    assets = [archive(f'CameraNearClarity-v{version}{suffix}.zip','installation',install),
              archive(f'CameraNearClarity-v{version}{suffix}-source.zip','source',sources)]
    result = dict(name=plan['name'],version=version,source_commit=commit,artifacts=assets,
                  dll_sha256=digest(dll),compile_inputs=inputs,
                  binary_reused=report_path.name=='accepted-report.json',
                  native_build_report_sha256=digest(raw),
                  native_build_report=report,game_executed_by_packaging=False)
    # Keep the raw report separately; this package record never rewrites historical lineage.
    result['native_build_report_file'] = 'native-build-report.json'
    (OUTPUT/'manifest.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print(json.dumps({key:result[key] for key in ('version','source_commit','dll_sha256','binary_reused','artifacts')},ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
