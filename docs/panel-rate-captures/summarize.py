"""Summarize a mainframe-panel-rate-sweep or mainframe-loop-load capture.

Every one-second LOOP_* window is assigned to a state from the report itself:
idle (show not started, no panel frame), show without text at frame_ms, and
text throughout (every eye frame a panel frame) at panel_ms. Windows with
sleep, a debug screen or mixed frames are left out.
"""
import re, sys, statistics as st


def load(path):
    recs = {}
    for line in open(path, errors="replace"):
        m = re.search(r'(LOOP_[A-F]) t=(\d+) (.*)', line)
        if not m:
            continue
        d = recs.setdefault(int(m.group(2)), {})
        for k, v in re.findall(r'(\w+)=(\S+)', m.group(3)):
            d[k] = v
    need = ['busy', 'eye_frame', 'psram_free', 'tx']
    return [d for _, d in sorted(recs.items()) if all(k in d for k in need)]


def sec(ws, name, i):
    return [int(d[name].split('/')[i]) for d in ws]


def pct(v, q):
    v = sorted(v)
    return v[int(q * (len(v) - 1))]


def summarize(label, ws):
    if not ws:
        print(f"{label}: no windows")
        return
    n = len(ws)
    win = sum(int(d['win_us']) for d in ws)
    busy_us = sum(c * a for c, a in zip(sec(ws, 'busy', 0), sec(ws, 'busy', 1)))
    frames = sum(sec(ws, 'eye_frame', 0)) + sum(sec(ws, 'eye_panel', 0))
    eye_us = sum(c * a for c, a in zip(sec(ws, 'eye_frame', 0), sec(ws, 'eye_frame', 1))) + \
        sum(c * a for c, a in zip(sec(ws, 'eye_panel', 0), sec(ws, 'eye_panel', 1)))
    out = [f"{label}: windows={n}",
           f"busy_share={busy_us / win * 100:.1f}%",
           f"passes/s={sum(sec(ws, 'busy', 0)) / (win / 1e6):.1f}",
           f"busy_max_ms={max(sec(ws, 'busy', 2)) / 1000:.1f}",
           f"eye_frames/s={frames / (win / 1e6):.1f}",
           f"eye_frame_avg_ms={eye_us / frames / 1000 if frames else 0:.1f}",
           f"eye_share={eye_us / win * 100:.1f}%"]
    for k in ['audio_gap_ms', 'imu_gap_ms', 'beat_late_max_ms']:
        v = [int(d[k]) for d in ws]
        out.append(f"{k}: max={max(v)} p95={pct(v, 0.95)} median={st.median(v):.0f}")
    out.append(f"backlog_max={max(int(d['backlog_max']) for d in ws)}")
    out.append(f"audio_drops={max(int(d['audio_drops']) for d in ws)}")
    out.append(f"int_free_min={min(int(d['int_free']) for d in ws)}")
    print("\n  ".join(out))


def main():
    for path in sys.argv[1:]:
        ws = load(path)
        last = ws[-1]
        print("==", path.split('/')[-1], "windows", len(ws),
              "dropped_reports", last.get('dropped_reports'),
              "audio_drops", last.get('audio_drops'))
        clean = [d for d in ws if d['sleeping'] == '0' and d['view'] == '0']
        panel_frames = lambda d: int(d['eye_panel'].split('/')[0])
        plain_frames = lambda d: int(d['eye_frame'].split('/')[0])
        idle = [d for d in clean if d['show'] == '0' and panel_frames(d) == 0]
        running = [d for d in clean if d['show'] == '1' and d['audio_on'] == '1']
        summarize("idle", idle)
        for ms in sorted({d.get('frame_ms', '25') for d in running}, key=int):
            show = [d for d in running if d.get('frame_ms', '25') == ms and d['panel'] == '0'
                    and panel_frames(d) == 0]
            summarize(f"show, no text, frame {ms} ms", show[3:] if ms == '25' else show)
        for ms in sorted({d.get('panel_ms', '25') for d in running}, key=int):
            text = [d for d in running if d.get('panel_ms', '25') == ms and panel_frames(d) > 0
                    and plain_frames(d) == 0]
            summarize(f"text throughout, panel {ms} ms", text)


main()
