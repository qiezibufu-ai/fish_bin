#!/usr/bin/env python3
"""Install persistent engineering rules; never moves code, builds or flashes."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
from pathlib import Path
import shutil
import sys

SKILL = Path(__file__).resolve().parents[1]
START = '<!-- embedded-architecture:begin -->'
END = '<!-- embedded-architecture:end -->'
BLOCK = f'''{START}
## 嵌入式代码实施规则

本工程的嵌入式移植、新增功能、修复和重构，必须遵循
[工程代码规则](docs/embedded_rules/code_rules.md)及其中链接的四份规范。
修改前先读相关规范；新增/修改涉及的.h公共接口使用中文Doxygen；
所有FSM错误终态必须记录具体可读错误码、底层原因和文件/函数/行号。
依照规范完成实际代码、构建与验证，不能以检查报告代替实现。
已有用户明确约定和更高优先级指令仍优先；保留本文件其他内容。
{END}'''


def safe_path(root: Path, relative: str) -> Path:
    path = root / relative
    if path.is_symlink() or not path.resolve().is_relative_to(root):
        raise ValueError(f'拒绝写入符号链接或工程外路径：{path}')
    if path.exists() and not path.is_file():
        raise ValueError(f'目标不是普通文件：{path}')
    return path


def install(project: Path, update: bool, scaffold: bool) -> list[str]:
    root = project.resolve()
    if not root.is_dir():
        raise ValueError(f'目标工程目录不存在：{root}')
    content = {'docs/embedded_rules/code_rules.md':
               (SKILL / 'assets/project-rules.md').read_bytes()}
    for name in ['layers-and-naming.md', 'interfaces-and-c-style.md',
                 'errors-and-fsm.md', 'runtime-and-build.md']:
        content['docs/embedded_rules/' + name] = (SKILL / 'references' / name).read_bytes()
    agents = safe_path(root, 'AGENTS.md')
    old = agents.read_bytes().decode('utf-8') if agents.exists() else ''
    if old.count(START) != old.count(END) or old.count(START) > 1:
        raise ValueError('AGENTS受管区块不完整或重复，请先处理该文件，未写入任何内容。')
    if START in old:
        begin = old.index(START)
        end_marker = old.index(END)
        end = end_marker + len(END)
        if begin > end_marker:
            raise ValueError('AGENTS区块顺序错误，未写入任何内容。')
        if old[begin:end] != BLOCK and not update:
            raise ValueError('AGENTS受管规则已变更；如需更新请使用--update。')
        new_agents = old[:begin] + BLOCK + old[end:]
    else:
        new_agents = old + ('\n\n' if old and not old.endswith('\n\n') else '') + BLOCK + '\n'
    content['AGENTS.md'] = new_agents.encode('utf-8')
    targets = {key: safe_path(root, key) for key in content}
    changed = [key for key, path in targets.items()
               if not path.exists() or path.read_bytes() != content[key]]
    conflicts = [key for key in changed if key != 'AGENTS.md' and targets[key].exists()]
    if conflicts and not update:
        raise ValueError('规范文件已有不同内容；使用--update后会先备份：' + ', '.join(conflicts))
    if scaffold:
        for name in ['AA_app', 'AB_service', 'AC_module', 'AD_platform', 'AE_common',
                     'AF_config', 'tests', 'tools']:
            path = root / name
            if path.is_symlink() or (path.exists() and not path.is_dir()):
                raise ValueError(f'规范目录名与现有文件/链接冲突：{path}')
    # Preflight above completes before any mutation. Preserve the complete old
    # AGENTS (including unmanaged content), even on its first managed addition.
    existing = [key for key in changed if targets[key].exists()]
    if existing:
        backup_root = root / '.embedded_architecture_backups'
        if backup_root.is_symlink() or not backup_root.resolve().is_relative_to(root):
            raise ValueError('备份目录不得指向工程外部。')
        backup = backup_root / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
        for key in existing:
            path = backup / key
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(targets[key], path)
        print(f'原文件备份：{backup}')
    for key in changed:
        path = targets[key]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content[key])
    if scaffold:
        for name in ['AA_app', 'AB_service', 'AC_module', 'AD_platform', 'AE_common',
                     'AF_config', 'tests', 'tools']:
            (root / name).mkdir(exist_ok=True)
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True, type=Path)
    parser.add_argument('--update', action='store_true', help='备份后更新已有规范')
    parser.add_argument('--scaffold', action='store_true', help='按需创建分层目录，不生成业务实现')
    args = parser.parse_args()
    try:
        changed = install(args.project, args.update, args.scaffold)
    except (ValueError, OSError, UnicodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(f'已安装工程内规则：{args.project.resolve()}（更新{len(changed)}个文件）')
    print('未迁移业务代码、未构建、未连接设备。继续完成用户指定范围的实际移植。')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
