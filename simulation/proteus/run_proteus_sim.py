"""Smart IoT Irrigation Controller v2 - Proteus logic simulation (public proteus_automatic_api only).

Double-click to run. Everything is logged to run_log.txt / result.json in this folder.
Stand-in MCU: STM32F103R6 (Proteus cannot simulate the ESP32-S3).
  SW1 -> PA0  soil DRY        SW2 -> PA1  MANUAL water      SW3 -> PA2  TANK EMPTY
  PA5 -> D1 PUMP              PA6 -> D2 VALVE               PA7 -> D3 ALARM
  water = (DRY or MANUAL) and not TANK_EMPTY ; alarm = (DRY or MANUAL) and TANK_EMPTY
"""
import glob, hashlib, json, os, shutil, subprocess, sys, time, traceback, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
LOG = HERE / 'run_log.txt'
def log(*a):
    msg = ' '.join(str(x) for x in a)
    with open(LOG, 'a', encoding='utf-8') as f: f.write(time.strftime('%H:%M:%S ') + msg + '\n')
    print(msg, flush=True)

def stage1():
    """Runs under the system Python: install the library into .venv-proteus, then re-launch."""
    LOG.write_text('', encoding='utf-8')
    log('system python', sys.executable, sys.version)
    venv = HERE / '.venv-proteus'
    r = subprocess.run([sys.executable, str(HERE / 'ensure_library.py'), '--venv', str(venv)],
                       capture_output=True, text=True, cwd=HERE)
    log('ensure_library rc', r.returncode); log('stdout', r.stdout.strip()); log('stderr tail', r.stderr[-3000:])
    if r.returncode != 0: raise SystemExit('library install failed')
    info = json.loads(r.stdout.strip().splitlines()[-1])
    py = info['python']
    r2 = subprocess.run([py, '-m', 'pip', 'install', '-q', 'Pillow'], capture_output=True, text=True)
    log('pillow rc', r2.returncode, r2.stderr[-500:])
    log('relaunch in', py)
    r3 = subprocess.run([py, str(Path(__file__).resolve()), '--stage2'], cwd=HERE)
    log('stage2 exit', r3.returncode)

def find_paths():
    found = {}
    exes = []
    for pat in [r'C:\Program Files*\Labcenter Electronics\*\BIN\PDS.EXE', r'C:\Program Files*\*\Proteus*\BIN\PDS.EXE',
                r'D:\Proteus*\BIN\PDS.EXE', r'C:\Proteus*\BIN\PDS.EXE', r'D:\Program Files*\Labcenter Electronics\*\BIN\PDS.EXE',
                r'C:\Program Files*\Labcenter Electronics\*\*\BIN\PDS.EXE']:
        exes += glob.glob(pat)
    found['exe'] = exes
    roots = []
    for pat in [r'C:\ProgramData\Labcenter Electronics\*', r'C:\ProgramData\program', r'C:\ProgramData\Labcenter Electronics\*\*']:
        roots += [p for p in glob.glob(pat) if os.path.isdir(os.path.join(p, 'LIBRARY')) or os.path.isdir(os.path.join(p, 'SAMPLES'))]
    for e in exes:
        b = os.path.dirname(os.path.dirname(e))
        if os.path.isdir(os.path.join(b, 'LIBRARY')) or os.path.isdir(os.path.join(b, 'SAMPLES')): roots.append(b)
    found['roots'] = roots
    samples = []
    for r in roots:
        samples += glob.glob(os.path.join(r, 'SAMPLES', '**', 'STMCubeMX LED Blink.pdsprj'), recursive=True)
    found['stm32_sample'] = samples
    rescap = []
    for r in roots:
        rescap += glob.glob(os.path.join(r, 'SAMPLES', '**', 'Rescap.pdsprj'), recursive=True)
    found['rescap'] = rescap
    return found

def construction(c, log):
    c.add('STM32F103R6', 'U1', 'STM32F103R6', 1524000, 4572000)
    names = ['DRY', 'MANUAL', 'TANK_EMPTY']
    ys = [4318000, 2540000, 762000]
    for i in range(3):
        ref = f'SW{i+1}'
        c.add('BUTTON', ref, 'BUTTON', -6096000, ys[i], properties={'STATE': '0'})
        try:
            c.update(ref, label_offsets={'reference': (-508000, 762000), 'value': (1016000, 508000),
                                         'device': (1016000, 254000), 'properties': (1016000, -254000)})
        except Exception as e: log('label_offsets unsupported', e)
    rys = [-1270000, -4318000, -7366000]
    dys = [-2540000, -5588000, -8636000]
    for i in range(3):
        c.add('RES', f'R{i+1}', '470', -2540000, rys[i], rotation=90)
        c.add('LED-YELLOW', f'D{i+1}', 'LED-YELLOW', -6096000, dys[i])
    c.configure_power({'GND': 0, 'VCC/VDD': 3.3, 'VEE': -3.3},
                      {'VCC': 'VCC/VDD', 'VDD': 'VCC/VDD', 'VDDA': 'VCC/VDD', 'VSS': 'GND', 'VSSA': 'GND'})
    pins = c.pins('U1')
    pmap = {p['name']: p for p in pins}
    log('U1 pins', json.dumps([p for p in pins if p['name'].startswith('PA')], default=str))
    c.connect('SW1.2', 'U1.PA0-WKUP')
    c.connect('U1.PA1', 'SW2.2')
    c.connect('U1.PA2', 'SW3.2')
    lanes = [0, 508000, 762000]
    for i, pa in enumerate(('PA5', 'PA6', 'PA7')):
        pos = pmap[pa]['position']; px, py = pos[0], pos[1]
        rpins = {p['name']: p['position'] for p in c.pins(f'R{i+1}')}
        rx, ry = rpins['2'][0], rpins['2'][1]
        try:
            c.connect(f'U1.{pa}', f'R{i+1}.2', points=[(px, py), (lanes[i], py), (lanes[i], ry), (rx, ry)])
        except Exception as e:
            log('manual route failed, auto-route', pa, e); c.connect(f'U1.{pa}', f'R{i+1}.2')
        c.connect(f'R{i+1}.1', f'D{i+1}.A')
        c.add_terminal('ground', '', f'D{i+1}.K')
        c.add_terminal('power', 'VCC', f'SW{i+1}.1')
    for pin in ('VDD', 'VBAT', 'VREF+', 'NRST'):
        c.add_terminal('power', 'VCC', 'U1.' + pin, **({'position': (3048000, 5207000)} if pin == 'VDD' else {}))
    for pin in ('VSS', 'BOOT0'):
        c.add_terminal('ground', '', 'U1.' + pin, **({'position': (3048000, -4699000)} if pin == 'VSS' else {}))
    c.bind_controls('SW1', 'SW2', 'SW3')

def stage2():
    from proteus_automatic_api import Circuit, Project, Library, Session, Simulation, gpio_events
    import proteus_automatic_api as api
    report = {'passed': False, 'steps': []}
    out = HERE / ('run_' + time.strftime('%Y%m%d_%H%M%S'))
    out.mkdir()
    s = sim = None
    try:
        log('library', api.__file__, getattr(api, '__version__', '?'))
        paths = find_paths(); log('paths', json.dumps(paths)); report['paths'] = paths
        exe = paths['exe'][0]
        firmware = HERE / 'irrigation.hex'
        # 1. find sample projects that embed the devices we need (template + donors)
        need = ['STM32F103R6', 'BUTTON', 'RES', 'LED-YELLOW']
        sample_roots = [os.path.join(r, 'SAMPLES') for r in paths['roots']] + [r'C:\ProgramData\program\SAMPLES']
        holders = {n: [] for n in need}
        scanned = 0
        for root in dict.fromkeys(sample_roots):
            for p in glob.glob(os.path.join(root, '**', '*.pdsprj'), recursive=True):
                try:
                    with zipfile.ZipFile(p) as z:
                        dsn = z.read('ROOT.DSN')
                except Exception:
                    continue
                scanned += 1
                for n in need:
                    if (b'\x00' + n.encode() + b'\x00') in dsn or (n.encode() + b'\x00') in dsn:
                        holders[n].append(p)
        log('scanned samples', scanned, {n: len(v) for n, v in holders.items()})
        report['device_sources'] = {n: v[:5] for n, v in holders.items()}
        if not holders['STM32F103R6']:
            raise RuntimeError('No installed Proteus sample contains the STM32F103R6 (VSM for Cortex-M3 not installed?)')
        # template: a sample that has the STM32 and as many of the others as possible
        best = max(holders['STM32F103R6'], key=lambda p: sum(p in holders[n] for n in need))
        log('template sample', best)
        template = out / 'template.pdsprj'
        shutil.copy2(best, template)
        c = Circuit(template_project=template)
        assert c.components() == [], c.components()
        for n in need:
            try:
                c.import_device(n)
                log('imported from library', n); continue
            except Exception as e:
                msg = str(e)
                if 'already' in msg: log('already in template', n); continue
                log('library import failed', n, msg[:200])
            done = False
            for donor in holders[n][:15]:
                try:
                    c.import_device(n, donor_project=donor); log('imported', n, 'from', donor); done = True; break
                except Exception as e:
                    if 'already' in str(e): done = True; log('already in template', n); break
            if not done: log('could not import', n, '(will try to place anyway)')
        construction(c, log)
        project = out / 'Irrigation_Controller_Logic.pdsprj'
        c.save(project, title='Smart IoT Irrigation Controller v2 - logic simulation')
        log('saved', project)
        # 3. open, load firmware, trace, verify netlist
        s = Session(project, executable=exe); sim = Simulation(s)
        report['firmware'] = sim.set_firmware('U1', firmware)
        s.set_properties('U1', TRACE_GPIO='3')
        for i in range(3): s.set_properties(f'R{i+1}', VALUE='330')
        s.save(); s.close(); s = None
        s = Session(project, executable=exe); sim = Simulation(s)
        try:
            s.menu('View/Zoom To View Entire Sheet')
        except Exception: pass
        sdf = s.export_netlist(out / 'circuit.sdf')
        nets = [{(p['ref'], p['pin']) for p in n['pins']} for n in sdf['nets']]
        def same(*pins): assert any(set(pins) <= n for n in nets), pins
        same(('SW1', '2'), ('U1', 'PA0-WKUP')); same(('SW2', '2'), ('U1', 'PA1')); same(('SW3', '2'), ('U1', 'PA2'))
        for i, pa in enumerate(('PA5', 'PA6', 'PA7')):
            same(('U1', pa), (f'R{i+1}', '2')); same((f'R{i+1}', '1'), (f'D{i+1}', 'A'))
        report['netlist_verified'] = True; log('netlist verified', len(sdf['parts']), 'parts')
        try:
            from observe import capture
        except Exception as e:
            capture = None; log('no capture', e)
        # 4. scenario: (action, ref, expected [PUMP PA5, VALVE PA6, ALARM PA7])
        steps = [('initial', None, [0, 0, 0], 'All inputs off'),
                 ('press', 'SW1', [1, 1, 0], 'Soil becomes DRY -> pump + valve ON'),
                 ('press', 'SW3', [0, 0, 1], 'Tank EMPTY while dry -> pump OFF, ALARM ON'),
                 ('release', 'SW3', [1, 1, 0], 'Tank refilled -> watering resumes'),
                 ('release', 'SW1', [0, 0, 0], 'Soil wet again -> everything OFF'),
                 ('press', 'SW2', [1, 1, 0], 'MANUAL water button -> pump + valve ON'),
                 ('release', 'SW2', [0, 0, 0], 'Manual released -> OFF')]
        report['simulation'] = []
        for k, (action, ref, expected, text) in enumerate(steps):
            cmd = getattr(sim, action)(ref) if ref else None
            run = sim.run_for(0.02)
            time.sleep(0.4)
            logtxt = sim.log()
            (out / 'simulation.log').write_text(logtxt, encoding='utf-8')
            ev = [gpio_events(logtxt, ref='U1', port=0, pin=p) for p in (5, 6, 7)]
            levels = [e[-1]['value'] if e else None for e in ev]
            ok = [0 if v is None else v for v in levels] == expected
            row = dict(step=k, action=action, ref=ref, meaning=text, expected=expected, levels=levels, ok=ok,
                       command=cmd, run=run)
            report['simulation'].append(row)
            log(f'step {k}: {text} | PUMP,VALVE,ALARM = {levels} expected {expected} -> {"PASS" if ok else "FAIL"}')
            if capture:
                try: capture(pid=s.pid)[0].save(out / f'step{k}.png')
                except Exception as e: log('capture failed', e)
        report['passed'] = all(r['ok'] for r in report['simulation']) and report.get('netlist_verified', False)
        sim.stop(); s.save()
        log('RESULT', 'PASS' if report['passed'] else 'FAIL')
    except Exception:
        report['error'] = traceback.format_exc(); log('ERROR', report['error'])
    finally:
        try:
            if s and s.process.poll() is None:
                try:
                    if sim: sim.stop()
                except Exception: pass
                s.close()
        except Exception as e:
            log('close error', e)
        (out / 'result.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf-8')
        log('report', out / 'result.json')

if __name__ == '__main__':
    try:
        if '--stage2' in sys.argv: stage2()
        else: stage1()
    except BaseException:
        log('FATAL', traceback.format_exc())
    if '--stage2' not in sys.argv:
        log('done'); time.sleep(3)
