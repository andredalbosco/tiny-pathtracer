"""Convert expanded Make compiler recipes on stdin to a compilation database."""
import json
from pathlib import Path
import shlex
import shutil
import sys

root = Path.cwd()
entries = []
for line in sys.stdin:
    args = shlex.split(line)
    if not args:
        continue
    sources = [arg for arg in args if Path(arg).suffix in ('.cpp', '.cu')]
    if len(sources) != 1 or '-o' not in args:
        continue
    compiler = shutil.which(args[0])
    if compiler is None:
        raise SystemExit(f'Compiler not found: {args[0]}')
    args[0] = compiler
    entries.append({
        'directory': str(root),
        'file': str((root / sources[0]).resolve()),
        'arguments': args,
        'output': str((root / args[args.index('-o') + 1]).resolve()),
    })
if not entries:
    raise SystemExit('No compiler recipes found')
# clangd uses the first command for a file: prefer the host pass for navigation
# to host API headers. Retain the device pass with its real PTX build flags.
entries.sort(key=lambda entry: '--cuda-device-only' in entry['arguments']
             or '--ptx' in entry['arguments'])
Path('build').mkdir(exist_ok=True)
build_database = Path('build/compile_commands.json')
# Replace the symlink used by the previous root-level database layout.
if build_database.is_symlink():
    build_database.unlink()
build_database.write_text(json.dumps(entries, indent=2) + '\n')
print(f'Wrote {build_database} ({len(entries)} commands)')
