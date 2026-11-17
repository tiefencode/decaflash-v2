# Task 6.4 comparison (docs/slm-prototype.md): model run against the baseline of
# the same session, also cut to the same number of windows as the model run.
# Usage, from the repository root:
#   python3 docs/slm-captures/compare_6_4.py docs/slm-captures/2026-11-17_6.4_run1_no_model.log.gz \
#     docs/slm-captures/2026-11-17_6.4_run1b_no_model.log.gz \
#     docs/slm-captures/2026-11-17_6.4_run1c_no_model_text.log.gz \
#     docs/slm-captures/2026-11-17_6.4_run2_model.log.gz
# Baseline: idle from run1, show from run1 + run1b, text from run1c.
import os, statistics as st, sys

paths = sys.argv[1:]
if len(paths) != 4:
    sys.exit(__doc__ if __doc__ else "four capture paths needed")
sys.argv = [sys.argv[0]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "summarize.py")).read())


def states(path):
    ws = load(path)
    panel = lambda d: int(d['eye_panel'].split('/')[0])
    idle = [(t, d) for t, d in ws if d['show'] == '0' and panel(d) == 0]
    show = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and panel(d) == 0 and d['panel'] == '0'][3:]
    text = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and panel(d) > 0 and int(d['eye_frame'].split('/')[0]) == 0]
    return dict(idle=idle, show=show, text=text)


def stat(ws, k):
    v = sorted(int(d[k]) for _, d in ws)
    return f"{v[-1]}/{v[int(0.95 * (len(v) - 1))]}/{st.median(v):.0f}"


b1, b1b, b1c, m = (states(p) for p in paths)
base = dict(idle=b1['idle'], show=b1['show'] + b1b['show'], text=b1c['text'])
print("state label windows | audio gap max/p95/median | imu gap | beat lateness | core 1 busy | backlog | tx | not accepted")
for s in ['idle', 'show', 'text']:
    n = len(m[s])
    for label, ws in [('baseline all', base[s]), ('baseline first n', base[s][:n]), ('model', m[s])]:
        busy = sum(c * a for c, a in zip(sec(ws, 'busy', 0), sec(ws, 'busy', 1))) / sum(int(d['win_us']) for _, d in ws) * 100
        print(s, label, len(ws), '| audio', stat(ws, 'audio_gap_ms'), '| imu', stat(ws, 'imu_gap_ms'),
              '| beat', stat(ws, 'beat_late_max_ms'), f'| busy {busy:.1f}%',
              '| backlog', max(int(d['backlog_max']) for _, d in ws),
              '| tx', sum(int(d['tx']) for _, d in ws), '| rejected', sum(int(d['tx_rejected']) for _, d in ws))
