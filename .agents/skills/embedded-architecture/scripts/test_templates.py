#!/usr/bin/env python3
"""Compile real C templates in an isolated target; test rule persistence, no hardware."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

SKILL = Path(__file__).resolve().parents[1]


def run(command: list[str], expected: int = 0, **kwargs) -> dict:
    result = subprocess.run(command, text=True, capture_output=True, **kwargs)
    if result.returncode != expected:
        raise AssertionError(f'{command}\nrc={result.returncode}\n{result.stdout}\n{result.stderr}')
    return dict(command=command, rc=result.returncode, stdout=result.stdout, stderr=result.stderr)


def file_hashes(root: Path) -> dict[str, str]:
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, help='可选JSON验证记录，不保留临时工程')
    args = parser.parse_args()
    checks = []
    with tempfile.TemporaryDirectory(prefix='embedded-port-') as folder:
        work = Path(folder)
        project = work / 'target_project'
        project.mkdir()
        user_agents = '# 项目约定\r\n\r\n保留已有的任务说明。\r\n'
        (project / 'AGENTS.md').write_bytes(user_agents.encode('utf-8'))
        command = [sys.executable, str(SKILL / 'scripts/install_project_rules.py'),
                   '--project', str(project), '--scaffold']
        checks.append(run(command))
        assert (project / 'AGENTS.md').read_bytes().startswith(user_agents.encode('utf-8'))
        for document in (project / 'docs/embedded_rules').glob('*.md'):
            for link in re.findall(r'\]\(([^)]+)\)', document.read_text()):
                if '://' not in link and not link.startswith('#'):
                    assert (document.parent / link.split('#')[0]).is_file(), (document, link)
        assert len(list((project / '.embedded_architecture_backups').glob('*/AGENTS.md'))) == 1
        first = file_hashes(project)
        checks.append(run(command))
        assert file_hashes(project) == first, '重复安装不是幂等的'
        for name in ['AA_app', 'AB_service', 'AC_module', 'AD_platform', 'AE_common', 'AF_config']:
            assert (project / name).is_dir()
        doc = project / 'docs/embedded_rules/errors-and-fsm.md'
        doc.write_text(doc.read_text() + '\n用户扩展规则。\n')
        changed = file_hashes(project)
        checks.append(run(command, expected=1))
        assert file_hashes(project) == changed, '拒绝更新后仍修改了工程'
        checks.append(run(command + ['--update']))
        backups = list((project / '.embedded_architecture_backups').glob(
            '*/docs/embedded_rules/errors-and-fsm.md'))
        assert any('用户扩展规则。' in p.read_text() for p in backups)
        assert '用户扩展规则。' not in doc.read_text()
        assert (project / 'AGENTS.md').read_bytes().startswith(user_agents.encode('utf-8'))
        checks.append(dict(test='安装、幂等、拒绝静默覆盖、更新前备份、保留AGENTS其他内容', passed=True))

        for name, content in [('missing_end', '<!-- embedded-architecture:begin -->'),
                              ('reversed', '<!-- embedded-architecture:end -->\n'
                               '<!-- embedded-architecture:begin -->')]:
            bad = work / name
            bad.mkdir()
            (bad / 'AGENTS.md').write_text(content)
            checks.append(run([sys.executable, command[1], '--project', str(bad)], expected=1))
            assert not (bad / 'docs').exists()
        outside = work / 'outside'
        outside.mkdir()
        (outside / 'marker').write_text('keep')
        linked = work / 'linked'
        linked.mkdir()
        (linked / 'docs').symlink_to(outside, target_is_directory=True)
        checks.append(run([sys.executable, command[1], '--project', str(linked)], expected=1))
        assert not (linked / 'AGENTS.md').exists() and (outside / 'marker').read_text() == 'keep'
        collision = work / 'collision'
        collision.mkdir()
        (collision / 'AF_config').write_text('existing file')
        checks.append(run([sys.executable, command[1], '--project', str(collision), '--scaffold'], expected=1))
        assert not (collision / 'docs').exists()
        checks.append(dict(test='损坏区块、倒序区块、工程外链接、目录冲突在写入前拒绝', passed=True))

        # Compile the copied implementation, not a separately reimplemented model.
        shutil.copytree(SKILL / 'assets/templates', project, dirs_exist_ok=True)
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        includes = ['-I' + str(project / 'AE_common'),
                    '-I' + str(project / 'AC_module/sample/driver/inc')]
        flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wpedantic',
                 '-Wconversion', '-Wshadow', '-Wstrict-prototypes']
        executable = work / 'test_sample_driver'
        sources = ['AE_common/fw_error.c', 'AC_module/sample/driver/src/bsp_sample_driver.c',
                   'tests/test_sample_driver.c']
        checks.append(run(compiler + flags + ['-fsanitize=undefined', '-fno-sanitize-recover=all']
                          + includes + [str(project / p) for p in sources]
                          + ['-o', str(executable)]))
        checks.append(run([str(executable)]))
        for header in ['fw_error.h', 'bsp_sample_driver.h']:
            checks.append(run(compiler + flags + includes + ['-x', 'c', '-fsyntax-only', '-'],
                              input=f'#include "{header}"\n'))
        cpp = shutil.which('c++')
        if cpp:
            checks.append(run([cpp, '-std=c++11', '-Wall', '-Wextra', '-Werror'] + includes
                              + ['-x', 'c++', '-fsyntax-only', '-'],
                              input='#include "fw_error.h"\n#include "bsp_sample_driver.h"\n'))
        checks.append(dict(test='复制后生产模板、C11严格告警、UBSan、头文件独立编译及C++引用', passed=True))
    report = dict(hardware_accessed=False, actual_project_modified=False, checks=checks,
                  scope='模板与规范安装器验证；不是目标MCU构建、Doxygen语义全校验或真实RTOS/硬件验证。')
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    for check in checks:
        if 'test' in check:
            print('PASS ' + check['test'])
        if check.get('stdout', '').startswith('PASS request'):
            print(check['stdout'], end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
