"""Build CameraNearClarity offline with MSVC x64. Never install or run a game.
Copyright (c) 2026 CameraNearClarity contributors; SPDX-License-Identifier: MIT.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / 'build/native'
COMPILE_INPUTS = ('src/core.cpp', 'src/CameraNearClarity.h', 'src/LicenseNotice.h', 'src/pattern_scan.h', 'src/target_patterns.h',
                  'src/runtime_control.cpp', 'src/runtime_control.h')

def compiler_environment(sdk_root=None):
    finder = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    if not install:
        raise RuntimeError('MSVC x64 Build Tools are required')
    environment = os.environ.copy()
    environment['PATH'] = environment.get('PATH', '').replace('"', '')
    environment.update(TEMP=str(OUTPUT), TMP=str(OUTPUT), VSLANG='1033', VSCMD_SKIP_SENDTELEMETRY='1')
    setup = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    command = OUTPUT / 'toolchain_env.cmd'
    command.write_text(f'@echo off\ncall "{setup}" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='ascii')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(command)], cwd=OUTPUT, env=environment,
        capture_output=True, text=True, encoding='mbcs', check=True)
    for line in result.stdout.splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            environment[key] = value
    environment.update(TEMP=str(OUTPUT), TMP=str(OUTPUT))
    if sdk_root:
        sdk = Path(sdk_root).resolve()
        versions = sorted((sdk / 'Include').glob('10.*'), key=lambda p: tuple(int(x) for x in p.name.split('.')))
        if not versions:
            raise RuntimeError('SDK root must contain Include/10.x')
        inc = versions[-1]
        libraries = sdk / 'Lib' / inc.name
        if not libraries.is_dir():
            libraries = sdk  # The supplied workspace SDK uses um/x64 and ucrt/x64 directly.
        for path in (inc/'um/windows.h', inc/'ucrt/stdio.h', libraries/'um/x64/kernel32.lib', libraries/'ucrt/x64/libucrt.lib'):
            if not path.is_file():
                raise RuntimeError('Incomplete SDK: ' + str(path))
        environment['INCLUDE'] = ';'.join(str(inc / p) for p in ('ucrt', 'shared', 'um', 'winrt')) + ';' + environment.get('INCLUDE', '')
        environment['LIB'] = ';'.join(str(libraries / p / 'x64') for p in ('um', 'ucrt')) + ';' + environment.get('LIB', '')
        environment['WindowsSDKVersion'] = inc.name + chr(92)
    return environment

def run(arguments, environment, log):
    result = subprocess.run([str(a) for a in arguments], cwd=OUTPUT, env=environment,
        capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUTPUT/log).write_text(result.stdout+result.stderr, encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{Path(arguments[0]).name} failed; see {OUTPUT/log}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, help='Optional SDK root with Include/10.x and Lib/10.x (or um/ucrt)')
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT/'report.json').unlink(missing_ok=True)
    sources = {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in COMPILE_INPUTS}
    env = compiler_environment(args.sdk_root)
    compiler, linker, dumper = [shutil.which(n, path=env['PATH']) for n in ('cl.exe','link.exe','dumpbin.exe')]
    if not all((compiler, linker, dumper)):
        raise RuntimeError('Incomplete MSVC toolchain')
    obj=OUTPUT/'core.obj'; artifact=OUTPUT/'CameraNearClarity.dll'
    flags=['/std:c++17', '/O2', '/MT', '/W4', '/WX', '/EHsc', '/utf-8']
    compile_command = [compiler, '/nologo', '/Bv', '/c', *flags, '/Fo'+str(obj), str(ROOT/'src/core.cpp')]
    link_command = [linker, '/NOLOGO', '/DLL', '/MACHINE:X64', '/INCREMENTAL:NO', '/DYNAMICBASE', '/NXCOMPAT', '/Brepro',
        '/OPT:REF', '/OUT:'+str(artifact), str(obj), str(OUTPUT/'runtime_control.obj'), 'kernel32.lib', 'user32.lib']
    run(compile_command, env, 'compile.log')
    run([compiler, '/nologo', '/c', *flags, '/Fo'+str(OUTPUT/'runtime_control.obj'),
         str(ROOT/'src/runtime_control.cpp')], env, 'control_compile.log')
    run(link_command, env, 'link.log')
    run([dumper, '/NOLOGO', '/HEADERS', '/EXPORTS', '/DEPENDENTS', artifact], env, 'pe_inspection.txt')
    inspection=(OUTPUT/'pe_inspection.txt').read_text(encoding='utf-8')
    exports=sorted(re.findall(r'^\s*\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\w+)\s*$', inspection, re.MULTILINE))
    if exports != ['SoraCameraGetStatus', 'SoraCameraInitialize']:
        raise ValueError('Unexpected exports: '+repr(exports))
    dependencies=re.findall(r'^\s+([A-Za-z0-9_.-]+\.dll)\s*$', inspection, re.MULTILINE | re.IGNORECASE)
    if {d.lower() for d in dependencies} != {'kernel32.dll','user32.dll'}:
        raise ValueError('Unexpected dependencies: '+repr(dependencies))
    license_bytes=(ROOT/'LICENSE').read_text(encoding='utf-8').encode('utf-8')
    if license_bytes not in artifact.read_bytes():
        raise ValueError('Full MIT notice not found in DLL')
    report={'artifact':str(artifact), 'dll_sha256':hashlib.sha256(artifact.read_bytes()).hexdigest(),
        'dll_bytes':artifact.stat().st_size, 'compile_inputs':sources, 'compiler':compiler,
        'compiler_version_log':'compile.log', 'linker':linker,
        'toolchain_sha256':{name:hashlib.sha256(Path(path).read_bytes()).hexdigest() for name,path in [('compiler',compiler),('linker',linker)]},
        'compiler_flags':flags, 'compile_command':compile_command, 'link_command':link_command,
        'sdk_root':str(args.sdk_root) if args.sdk_root else None,
        'msvc_version':env.get('VCToolsVersion'), 'windows_sdk_version':env.get('WindowsSDKVersion'),
        'exports':exports, 'dependencies':dependencies, 'license_embedded':True,
        'patch':json.loads((ROOT/'src/patch_context.json').read_text(encoding='utf-8')),
        'atomic_write':'InterlockedCompareExchange8 at signature+3, native conditional opcode <-> EB; branch operand preserved',
        'runtime_control':'Foreground-only edge-triggered key; 25 ms timer; INI content checked every 500 ms; no config writes',
        'game_executed':False, 'game_directory_written':False, 'dll_executed':False,
        'runtime_visual_verified':False,
        'target_lookup':'Hinkiii-style masked signature scan; no EXE name/size/hash whitelist',
        'scope':'all chara::Actor shared-path callers and child actors; scene bit 0x10 near-zero behavior bypassed'}
    (OUTPUT/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('artifact','dll_sha256','exports','dependencies')}))
if __name__=='__main__':
    main()
