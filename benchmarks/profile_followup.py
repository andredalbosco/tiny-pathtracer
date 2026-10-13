#!/usr/bin/env python3
"""Small diagnostic passes; authenticate sudo separately before running."""
import csv
import json
import pathlib
import subprocess
import time

root = pathlib.Path(__file__).resolve().parents[1]
out = root / 'build' / 'perf'
configs = {x['config']['name']: x['config'] for x in json.loads((out / 'hardware-stat.json').read_text())['configurations']}


def command(config):
    args = [str(root / 'build' / config.get('binary', 'pathtracer_simple')), '128', '320', str(out / 'render.hdr'),
            '--kernel', config['kernel'], '--threads', str(len(config['cpus'])),
            '--cpus', ','.join(map(str, config['cpus'])), '--batch', str(config.get('batch', 1)),
            '--runtime', config.get('runtime', 'std')]
    if config.get('refill'):
        args.append('--refill')
    return ['/usr/bin/taskset', '-c', ','.join(map(str, config['cpus'])), '/usr/sbin/runuser', '-u', 'blacknoir', '--'] + args


def collect(name, options, workload):
    path = out / (name + '.csv')
    path.touch()
    args = ['sudo', '-n', '/usr/bin/perf', 'stat', '--no-big-num', '-x,', '-o', str(path)] + options + ['--'] + workload
    start = time.perf_counter()
    run = subprocess.run(args, capture_output=True, text=True)
    elapsed = time.perf_counter() - start
    (out / (name + '-stderr.txt')).write_text(run.stderr)
    rows = []
    for row in csv.reader(path.read_text().splitlines()):
        if len(row) < 3 or row[0].startswith('#'):
            continue
        try:
            value = float(row[0].strip())
        except ValueError:
            value = None
        rows.append(dict(value=value, value_text=row[0].strip(), unit=row[1], event=row[2],
                         running_ns=row[3] if len(row) > 3 else None,
                         running_percent=row[4] if len(row) > 4 else None, extra=row[5:]))
    print(name + ' ' + str(run.returncode), flush=True)
    return dict(name=name, command=args, wall_seconds=elapsed, returncode=run.returncode, stderr=run.stderr, events=rows)


results = []
for name in ['scalar-P1', 'rays-refill-P1']:
    results.append(collect('topdown-' + name, ['--cputype', 'core', '--all-user', '-M', 'TopdownL1'], command(configs[name])))
events = ','.join('cpu_atom/' + event + '/u' for event in ['cycles', 'topdown-fe-bound', 'topdown-bad-spec', 'topdown-be-bound', 'topdown-retiring'])
results.append(collect('topdown-rays-refill-E1', ['-e', events], command(configs['rays-refill-E1'])))
for name in ['rays-refill-P1', 'rays-refill-P8', 'rays-refill-E1', 'rays-refill-E16', 'rays-refill-physical24', 'rays-refill-logical32']:
    config = configs[name]
    pmus = ['cpu_core'] if max(config['cpus']) < 16 else ['cpu_atom'] if min(config['cpus']) >= 16 else ['cpu_core', 'cpu_atom']
    events = ','.join(pmu + '/' + event + '/u' for pmu in pmus for event in ['cycles', 'ref-cycles', 'instructions'])
    results.append(collect('frequency-' + name, ['-e', events], command(config)))
(out / 'followup-core.json').write_text(json.dumps(results, indent=2) + '\n')

# Controller events are system-wide. Idle windows bracket the loaded windows.
# Each loaded window executes ten ordinary-user renders, preserving workload IDs.
repeat = out / 'repeat_render.py'
# Derive from the separator instead of depending on the launcher argument count.
launcher = command(configs['rays-refill-logical32'])
renderer = launcher[launcher.index('--') + 1:]
repeat.write_text('import subprocess\nargs = ' + repr(renderer) + '\nfor i in range(10):\n subprocess.run(args, check=True, stdout=subprocess.DEVNULL)\n')
imc_events = ','.join('uncore_imc_free_running_' + str(controller) + '/' + event + '/' for controller in [0, 1] for event in ['data_read', 'data_write'])
imc = [collect('imc-idle-before', ['-a', '-e', imc_events], ['/usr/bin/sleep', '3'])]
for repeat_id in range(3):
    imc.append(collect('imc-loaded-' + str(repeat_id), ['-a', '-e', imc_events], ['/usr/sbin/runuser', '-u', 'blacknoir', '--', '/usr/bin/python3', str(repeat)]))
imc.append(collect('imc-idle-after', ['-a', '-e', imc_events], ['/usr/bin/sleep', '3']))
(out / 'controller-traffic.json').write_text(json.dumps(dict(scope='System-wide controller traffic, including unrelated work; loaded windows contain ten full renders.', renders_per_loaded_window=10, results=imc), indent=2) + '\n')
print('Followup complete', flush=True)
