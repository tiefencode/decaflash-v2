"""Analyse a mainframe-panel-frame-timing capture: PANEL_F and GAP events."""
import gzip, re, sys, statistics as st

def kv(s):
    return dict(re.findall(r'(\w+)=(\S+)', s))

def load(path):
    op = gzip.open if path.endswith('.gz') else open
    frames, gaps, g = [], [], []
    for line in op(path, 'rt', errors='replace'):
        m = re.search(r'(PANEL_F|GAP|LOOP_G|LOOP_A) (t=\d+ .*)', line)
        if not m: continue
        d = kv(m.group(2)); d['t'] = int(d['t'])
        {'PANEL_F': frames, 'GAP': gaps, 'LOOP_G': g, 'LOOP_A': []}[m.group(1)].append(d)
    return frames, gaps, g

def us(d, k):
    return int(d[k].split('/')[0])

def main(path):
    frames, gaps, g = load(path)
    print(f"PANEL_F lines={len(frames)} GAP lines={len(gaps)} LOOP_G windows={len(g)}")
    pf = sum(int(d['pf']) for d in g)
    print(f"panel frames total={pf} first={sum(int(d['first']) for d in g)} decodes={sum(int(d['decodes']) for d in g)} ev_dropped_last={g[-1]['ev_dropped'] if g else '-'}")
    gw = [d for d in g if int(d['pf']) > 0]
    for k in ['eye', 'shade', 'wrap', 'text', 'emoji', 'push']:
        avgs = [int(d[k].split('/')[0]) for d in gw]; mxs = [int(d[k].split('/')[1]) for d in gw]
        w = [int(d['pf']) for d in gw]
        mean = sum(a * n for a, n in zip(avgs, w)) / max(1, sum(w))
        print(f"  {k:6s} mean={mean/1000:6.2f} ms  window-max max={max(mxs)/1000 if mxs else 0:6.2f} ms")
    cols = ['total', 'eye', 'shade', 'wrap', 'text', 'inline', 'emoji', 'decode', 'push', 'motion', 'pass']
    print("\nfirst/slow panel frames (ms):")
    print("  t       first glyph " + " ".join(f"{c:>7s}" for c in cols) + "  decodes")
    for d in frames:
        print(f"  {d['t']:7d} {d['first']:>5s} {d['glyph']:>5s} " +
              " ".join(f"{us(d, c)/1000:7.1f}" for c in cols) + f"  {d['decode'].split('/')[1]}")
    print("\ngaps and the nearest preceding PANEL_F within 150 ms:")
    for gp in gaps:
        prev = [f for f in frames if 0 <= gp['t'] - f['t'] <= 150]
        f = prev[-1] if prev else None
        desc = (f"panel t={f['t']} first={f['first']} total={us(f,'total')/1000:.1f} "
                f"decode={us(f,'decode')/1000:.1f} motion={us(f,'motion')/1000:.1f} pass={us(f,'pass')/1000:.1f}") if f else "no first/slow panel frame"
        print(f"  t={gp['t']} {gp['kind']:5s} {gp['gap_ms']:>4s} ms  <- {desc}")
    firsts = [f for f in frames if f['first'] == '1']
    if firsts:
        print("\nfirst frames:", len(firsts),
              "total median", st.median(us(f,'total') for f in firsts)/1000,
              "decode median", st.median(us(f,'decode') for f in firsts)/1000)
    slow_nonfirst = [f for f in frames if f['first'] == '0']
    print("slow non-first frames:", len(slow_nonfirst))

main(sys.argv[1])
