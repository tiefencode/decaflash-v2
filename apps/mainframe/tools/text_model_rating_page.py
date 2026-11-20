#!/usr/bin/env python3
"""Builds the Artifact page on which the user rates a blind pair set.

Input is a pairs.json as text_model_rating_set.py writes it (number, event,
mood, A, B; never the key). The page shows each pair, lets the user mark each
text usable or not and choose A, B or "equally good", and stores every answer
in the artifact's database (capability db), collection `answers`, one document
per pair:

  answers/p001  {"nr": 1, "a": "yes"|"no"|null, "b": "yes"|"no"|null,
                 "pref": "A"|"B"|"="|null, "at": ISO time}

A later session reads them back with the artifact tools (db_list on the
collection `answers`). The page is published once per rating set; the key
stays in key.json beside pairs.json.

Usage:
  text_model_rating_page.py <pairs.json> --title "..." [--intro "..."] [--out page.html]
"""

import argparse
import html
import json
import pathlib

PAGE = r"""<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@600&family=Inter:wght@400;600&display=swap" rel="stylesheet">
<style>
/* VOICE: a quiet reading desk, nothing between the user and the texts.
   IDEA: the Mainframe's message panel, one dark strip per text.
   PICTURE: the pair itself, two panel strips with their marks beside them.
   COLOUR and TYPE: white ground, ink, panel black with warm white letters, one green and one red for the marks; Barlow Condensed on the panel, Inter for the rest.
   LAYOUT: one calm column, a sticky progress line, pairs below each other. */
:root{color-scheme:light dark;
 --bg:light-dark(#ffffff,#121416);--ink:light-dark(#16181b,#e9eaec);--muted:light-dark(#5d636b,#a1a7ae);
 --line:light-dark(#e3e5e8,#2a2e33);--panel:#0b0c0d;--glow:#f4efe4;--card:light-dark(#f7f8f9,#1a1d20);
 --yes:light-dark(#1f7a4a,#46b37a);--no:light-dark(#b3361f,#e0644c);--pick:light-dark(#16181b,#e9eaec)}
:root[data-theme=light]{color-scheme:light}:root[data-theme=dark]{color-scheme:dark}
*{box-sizing:border-box;min-width:0}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.55 Inter,system-ui,sans-serif;overflow-x:clip}
.wrap{max-width:760px;margin:0 auto;padding:0 max(4vw,16px) 64px}
h1{font-size:clamp(28px,5vw,40px);line-height:1.05;margin:36px 0 10px;text-wrap:balance}
.intro{color:var(--muted);margin:0 0 20px;max-width:62ch}
.bar{position:sticky;top:0;z-index:2;background:var(--bg);border-bottom:1px solid var(--line);padding:12px 0;display:flex;flex-wrap:wrap;gap:10px 16px;align-items:center}
.meter{flex:1 1 200px;height:8px;border-radius:4px;background:var(--line);overflow:hidden}
.meter i{display:block;height:100%;background:var(--yes);width:0;transition:width .3s}
.count{font-variant-numeric:tabular-nums;white-space:nowrap}
.state{font-size:14px;color:var(--muted);white-space:nowrap}
.state.bad{color:var(--no)}
label.only{display:flex;gap:8px;align-items:center;font-size:15px;white-space:nowrap;cursor:pointer}
label.only input{width:18px;height:18px}
button{font:inherit;font-size:15px;min-height:44px;padding:0 14px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--ink);cursor:pointer}
button:focus-visible,input:focus-visible{outline:3px solid var(--yes);outline-offset:2px}
.pair{border-top:1px solid var(--line);padding:22px 0}
.pair.done .head b::after{content:" ✓";color:var(--yes)}
.head{display:flex;flex-wrap:wrap;gap:4px 12px;align-items:baseline;margin-bottom:12px}
.head b{font-size:18px}
.head span{color:var(--muted);font-size:15px}
.side{display:grid;grid-template-columns:28px minmax(0,1fr) auto;gap:10px;align-items:center;margin:10px 0}
.tag{font-weight:600;font-size:18px;text-align:center}
.panel{background:var(--panel);color:var(--glow);border-radius:10px;padding:12px 16px;font:600 22px/1.2 "Barlow Condensed",Inter,sans-serif;letter-spacing:.02em;box-shadow:inset 0 0 0 1px #ffffff14}
.marks{display:flex;gap:6px}
.marks button[aria-pressed=true].y{background:var(--yes);border-color:var(--yes);color:#fff}
.marks button[aria-pressed=true].n{background:var(--no);border-color:var(--no);color:#fff}
.pref{display:flex;flex-wrap:wrap;gap:6px;margin:14px 0 0 38px;align-items:center}
.pref em{font-style:normal;color:var(--muted);font-size:15px;margin-right:4px}
.pref button[aria-pressed=true]{background:var(--pick);border-color:var(--pick);color:var(--bg)}
.hidden{display:none}
.end{color:var(--muted);text-align:center;padding:32px 0}
@media(max-width:600px){.side{grid-template-columns:22px minmax(0,1fr)}.marks{grid-column:2}.pref{margin-left:0}.panel{font-size:20px}}
@media(prefers-reduced-motion:reduce){.meter i{transition:none}}
</style>
</head>
<body>
<div class="wrap">
<h1>__TITLE__</h1>
<p class="intro">__INTRO__</p>
<div class="bar">
 <div class="meter" aria-hidden="true"><i id="fill"></i></div>
 <span class="count" id="count">0 von 0 fertig</span>
 <label class="only"><input type="checkbox" id="only"> nur offene</label>
 <button id="next" type="button">Nächstes offenes</button>
 <span class="state" id="state">Verbinde …</span>
</div>
<div id="list"></div>
<p class="end" id="end"></p>
</div>
<script type="application/json" id="pairs">__PAIRS__</script>
<script>
{
const pairs = JSON.parse(document.getElementById('pairs').textContent).pairs;
const answers = {};            // id -> {a, b, pref}
const pending = {};            // id -> queued body while a write runs
const writing = {};
let db = null;
const list = document.getElementById('list');
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const idOf = nr => 'p' + String(nr).padStart(3, '0');
const done = a => a && a.a && a.b && a.pref;

list.innerHTML = pairs.map(p => `
<section class="pair" id="${idOf(p.nr)}" data-id="${idOf(p.nr)}">
 <div class="head"><b>Nr. ${p.nr} · ${esc(p.event)}</b><span>${esc(p.mood)}</span></div>
 ${['A','B'].map(s => `<div class="side"><span class="tag">${s}</span><div class="panel">${esc(p[s])}</div>
  <div class="marks" role="group" aria-label="Text ${s} brauchbar?">
   <button type="button" class="y" data-k="${s.toLowerCase()}" data-v="yes" aria-pressed="false">brauchbar</button>
   <button type="button" class="n" data-k="${s.toLowerCase()}" data-v="no" aria-pressed="false">nicht brauchbar</button>
  </div></div>`).join('')}
 <div class="pref" role="group" aria-label="Welcher Text ist besser?"><em>Besser:</em>
  <button type="button" data-k="pref" data-v="A" aria-pressed="false">A</button>
  <button type="button" data-k="pref" data-v="=" aria-pressed="false">gleich gut</button>
  <button type="button" data-k="pref" data-v="B" aria-pressed="false">B</button>
 </div>
</section>`).join('');

function paint(id) {
  const el = document.getElementById(id);
  const a = answers[id] || {};
  el.querySelectorAll('button[data-k]').forEach(b => b.setAttribute('aria-pressed', String(a[b.dataset.k] === b.dataset.v)));
  el.classList.toggle('done', !!done(a));
}
function summary() {
  const n = pairs.filter(p => done(answers[idOf(p.nr)])).length;
  document.getElementById('count').textContent = `${n} von ${pairs.length} fertig`;
  document.getElementById('fill').style.width = (100 * n / pairs.length) + '%';
  document.getElementById('end').textContent = n === pairs.length ? 'Alle Paare sind bewertet. Danke!' : '';
  filter();
}
function filter() {
  const only = document.getElementById('only').checked;
  list.querySelectorAll('.pair').forEach(el => el.classList.toggle('hidden', only && el.classList.contains('done')));
}
function setState(text, bad) {
  const s = document.getElementById('state');
  s.textContent = text; s.classList.toggle('bad', !!bad);
}
async function save(id) {
  if (!db) { setState('Speichern nicht verfügbar', true); return; }
  if (writing[id]) return;
  writing[id] = true;
  try {
    while (pending[id]) {
      const body = pending[id]; delete pending[id];
      setState('Speichere …');
      try { await db.collection('answers').doc(id).set(body); setState('Gespeichert'); }
      catch (e) {
        if (e && e.code === 'unavailable') { pending[id] = pending[id] || body; await new Promise(r => setTimeout(r, 800 + Math.random() * 800)); }
        else { setState('Nicht gespeichert: ' + (e && e.code || 'Fehler'), true); break; }
      }
    }
  } finally { writing[id] = false; }
}
list.addEventListener('click', ev => {
  const b = ev.target.closest('button[data-k]');
  if (!b) return;
  const id = b.closest('.pair').dataset.id;
  const a = Object.assign({a: null, b: null, pref: null}, answers[id]);
  a[b.dataset.k] = a[b.dataset.k] === b.dataset.v ? null : b.dataset.v;
  answers[id] = a;
  paint(id); summary();
  pending[id] = {nr: Number(id.slice(1)), a: a.a, b: a.b, pref: a.pref, at: new Date().toISOString()};
  save(id);
});
document.getElementById('only').addEventListener('change', filter);
document.getElementById('next').addEventListener('click', () => {
  const el = [...list.querySelectorAll('.pair')].find(e => !e.classList.contains('done'));
  if (el) el.scrollIntoView({behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ? 'auto' : 'smooth', block: 'start'});
});
summary();

(async () => {
  for (let i = 0; i < 30 && !window.claude; i++) await new Promise(r => setTimeout(r, 100));
  try { db = window.claude ? await window.claude.use('db') : null; } catch (e) { db = null; }
  if (!db) { setState('Speichern nicht verfügbar – bitte in claude.ai angemeldet öffnen', true); return; }
  const timer = setTimeout(() => setState('Antworten noch nicht erreichbar …', true), 10000);
  db.collection('answers').onSnapshot(snap => {
    if (!snap.metadata.fromCache) clearTimeout(timer);
    snap.docChanges().forEach(ch => {
      const id = ch.doc.id;
      if (pending[id] || writing[id]) return;   // keep the click the user just made
      if (ch.type === 'removed') delete answers[id];
      else { const d = ch.doc.data() || {}; answers[id] = {a: d.a || null, b: d.b || null, pref: d.pref || null}; }
      if (document.getElementById(id)) paint(id);
    });
    summary();
    if (!snap.metadata.fromCache) setState('Verbunden, Antworten werden gespeichert');
  }, e => { clearTimeout(timer); setState('Verbindung beendet: ' + (e && e.code || 'Fehler'), true); });
})();
}
</script>
</body>
</html>
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("pairs", help="pairs.json")
    parser.add_argument("--title", required=True)
    parser.add_argument("--intro", default="")
    parser.add_argument("--out", help="output HTML (default page.html beside pairs.json)")
    args = parser.parse_args()
    source = pathlib.Path(args.pairs)
    data = json.loads(source.read_text(encoding="utf-8"))
    for pair in data["pairs"]:
        if set(pair) - {"nr", "event", "mood", "A", "B"}:
            raise SystemExit(f"pair {pair.get('nr')}: unexpected fields, the key must stay hidden")
    payload = json.dumps({"pairs": data["pairs"]}, ensure_ascii=False).replace("</", "<\\/")
    page = (PAGE.replace("__TITLE__", html.escape(args.title))
            .replace("__INTRO__", html.escape(args.intro))
            .replace("__PAIRS__", payload))
    out = pathlib.Path(args.out) if args.out else source.with_name("page.html")
    out.write_text(page, encoding="utf-8")
    print(f"{len(data['pairs'])} pairs -> {out}")


if __name__ == "__main__":
    main()
