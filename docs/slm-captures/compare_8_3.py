# Task 8.3 of openspec/changes/add-text-model: load capture of `mainframe-slm-capture`
# without the model image (baseline, text bank texts) against the run with the image
# (model texts), same build and session. Prints the limits of design.md Decision 13.
# Usage, from the repository root:
#   python3 docs/slm-captures/compare_8_3.py <baseline.log[.gz]> <model.log[.gz]>
import os, sys

paths = sys.argv[1:]
if len(paths) != 2:
    sys.exit("two capture paths needed: baseline, model")
sys.argv = [sys.argv[0]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "summarize.py")).read())


def states(path):
    ws = load(path)
    panel = lambda d: int(d['eye_panel'].split('/')[0])
    show = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and panel(d) == 0 and d['panel'] == '0'][3:]
    text = [(t, d) for t, d in ws if d['show'] == '1' and d['audio_on'] == '1' and panel(d) > 0]
    return ws, dict(show=show, text=text)


runs = [(label, path) + states(path) for label, path in zip(['baseline', 'model'], paths)]
print("run | resets (time back / banners) | audio drops | tx / rejected")
for label, path, ws, _ in runs:
    n, b = resets(path)
    print(label, '|', n, '/', b, '|', ws[-1][1]['audio_drops'], '|',
          sum(int(d['tx']) for _, d in ws), '/', sum(int(d['tx_rejected']) for _, d in ws))
print("state run windows | max audio gap | max imu gap | max beat lateness | backlog max | fallbacks")
for s in ['show', 'text']:
    for label, _, _, st_ in runs:
        w = st_[s]
        if not w:
            print(s, label, 'no windows'); continue
        mx = lambda k: max(int(d[k]) for _, d in w)
        print(s, label, len(w), '|', mx('audio_gap_ms'), '|', mx('imu_gap_ms'), '|', mx('beat_late_max_ms'),
              '|', mx('backlog_max'), '|', w[-1][1].get('fallbacks', '-'))
