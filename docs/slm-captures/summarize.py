import re, sys, statistics as st
def load(path):
    recs = {}
    for line in open(path, errors="replace"):
        m = re.search(r'(LOOP_[A-F]) t=(\d+) (.*)', line)
        if not m: continue
        d = recs.setdefault(int(m.group(2)), {})
        for k, v in re.findall(r'(\w+)=("[^"]*"|\S+)', m.group(3)): d[k] = v
    need = ['busy', 'eye_frame', 'psram_free', 'tx']
    return [(t, d) for t, d in sorted(recs.items()) if all(k in d for k in need)]
def sec(ws, name, i): return [int(d[name].split('/')[i]) for _, d in ws]
def summarize(label, ws):
    if not ws: print(label, "no windows"); return
    n = len(ws)
    win = sum(int(d['win_us']) for _, d in ws)
    passes = sum(sec(ws, 'busy', 0))
    busy_us = sum(c * a for c, a in zip(sec(ws, 'busy', 0), sec(ws, 'busy', 1)))
    out = [f"{label}: windows={n}", f"busy_share={busy_us/win*100:.1f}%", f"passes/s={passes/(win/1e6):.1f}",
           f"busy_max_ms={max(sec(ws,'busy',2))/1000:.1f}"]
    for k in ['audio_gap_ms', 'imu_gap_ms', 'beat_late_max_ms']:
        v = [int(d[k]) for _, d in ws]
        out.append(f"{k}: max={max(v)} median={st.median(v):.0f} p95={sorted(v)[int(0.95*(n-1))]}")
    out.append(f"backlog_max={max(int(d['backlog_max']) for _, d in ws)}")
    out.append(f"audio_drops={ws[-1][1]['audio_drops']}")
    out.append(f"tx={sum(int(d['tx']) for _, d in ws)} tx_rejected={sum(int(d['tx_rejected']) for _, d in ws)}")
    fr = sum(sec(ws, 'eye_frame', 0)) + sum(sec(ws, 'eye_panel', 0))
    out.append(f"eye_frames/s={fr/(win/1e6):.1f}")
    out.append(f"int_free_min={min(int(d['int_free']) for _, d in ws)} psram_free_min={min(int(d['psram_free']) for _, d in ws)}")
    m = [(int(d['fwd'].split('/')[0]), int(d['fwd'].split('/')[1]), int(d['fwd'].split('/')[2])) for _, d in ws if 'fwd' in d]
    if m and sum(c for c, _, _ in m):
        cnt = sum(c for c, _, _ in m)
        out.append(f"fwd/s={cnt/(win/1e6):.2f} fwd_avg_ms={sum(c*a for c,a,_ in m)/cnt/1000:.1f} fwd_max_ms={max(x for _,_,x in m)/1000:.1f}")
        out.append(f"mismatches={ws[-1][1]['mismatches']} runs={ws[-1][1]['runs']} stack_free={min(int(d['stack_free']) for _, d in ws)}")
    print("\n  ".join(out))
for path in sys.argv[1:]:
    ws = load(path)
    print("==", path.split('/')[-1], "windows", len(ws), "slm", ws[-1][1].get('slm'), "dropped_reports", ws[-1][1].get('dropped_reports'))
    idle = [(t, d) for t, d in ws if d['show'] == '0' and int(d['eye_panel'].split('/')[0]) == 0]
    show = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and int(d['eye_panel'].split('/')[0]) == 0 and d['panel'] == '0']
    text = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and int(d['eye_panel'].split('/')[0]) > 0 and int(d['eye_frame'].split('/')[0]) == 0]
    summarize("idle", idle); summarize("show", show[3:]); summarize("text (panel throughout)", text)
