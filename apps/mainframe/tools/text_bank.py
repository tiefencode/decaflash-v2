#!/usr/bin/env python3
"""Generates the text bank table of the firmware (add-text-bank, design.md Decision 2).

The text bank holds every corpus line, grouped by event and lead value. The
lead value of a planned line comes from the plan file of its event; the lead
values of the fixed texts come from apps/mainframe/text_model/plan/fixed.tsv.
Texts are written in panel order (words, then the emoji), so the panel lays out
the emoji as its leading element as for every other message.

The verse bank (add-verse-text-bank) comes from apps/mainframe/text_model/
verse_corpus/: one file per event, lines `LEAD<TAB>FORM<TAB>TEXT`, with ` / `
between verse lines. A verse line break becomes `\n` in the bank. Verse bodies
longer than 47 characters (line breaks not counted) give a warning only.

Usage (any Python 3, no packages):
  text_bank.py            write text_bank_data.h and text_bank_verse_data.h
  text_bank.py --check    exit 1 if a header is not up to date
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab  # noqa: E402

REPO = vocab.REPO
HEADER = REPO / "apps/mainframe/src/text_bank_data.h"
VERSE_HEADER = REPO / "apps/mainframe/src/text_bank_verse_data.h"
VERSE_CORPUS = REPO / "apps/mainframe/text_model/verse_corpus"
VERSE_BREAK = " / "
VERSE_FORMS = ("a", "b", "c")  # free short verse, terse lines, saying
FIXED = corpus.PLAN / "fixed.tsv"
LEADS = corpus.LEADS  # ENERGY_LOW, ENERGY_HIGH, ANNOYANCE_LOW, ...


def fixed_leads():
    """{(event, text): lead} from plan/fixed.tsv (event file stem, lead, text)."""
    leads = {}
    for number, line in enumerate(FIXED.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        stem, lead, text = line.split("\t")
        event = stem.upper()
        if event not in vocab.EVENTS or lead not in LEADS:
            raise ValueError(f"{FIXED.name}:{number}: bad event or lead")
        leads[(event, text)] = lead
    return leads


def planned_leads(event, levels_in_order):
    """Lead value per planned line: the plan lines in order, skipping lines that
    the review removed (their levels no longer appear)."""
    plan = [line.split("\t") for line in
            (corpus.PLAN / corpus.file_name(event)).read_text(encoding="utf-8").splitlines()
            if line]
    leads = []
    at = 0
    for levels in levels_in_order:
        while at < len(plan) and plan[at][1] != levels:
            at += 1
        if at == len(plan):
            raise ValueError(f"{corpus.file_name(event)}: a line does not follow the plan")
        leads.append(plan[at][0])
        at += 1
    return leads


def panel_order(text):
    """'<emoji> <words>' -> '<words> <emoji>'."""
    emoji, _, words = text.partition(" ")
    return f"{words} {emoji}"


def build():
    """{(event index, lead index): [panel texts]} for every corpus line."""
    fixed = fixed_leads()
    groups = {}
    for e, event in enumerate(vocab.EVENTS):
        path = corpus.CORPUS / corpus.file_name(event)
        rows = [line.split("\t") for line in path.read_text(encoding="utf-8").splitlines()
                if line.strip()]
        planned = [(levels, text) for levels, text in rows if (event, text) not in fixed]
        leads = planned_leads(event, [levels for levels, _ in planned])
        lead_of = {text: lead for (_, text), lead in zip(planned, leads)}
        for levels, text in rows:
            lead = fixed.get((event, text)) or lead_of[text]
            value, _, level = lead.rpartition("_")
            if f"{value}_{level}" not in levels.split():
                raise ValueError(f"{path.name}: lead {lead} does not match the levels of {text!r}")
            groups.setdefault((e, LEADS.index(lead)), []).append(panel_order(text))
    return groups


def build_verse(warn=print):
    """{(event index, lead index): [panel texts]} for every verse corpus line."""
    emoji = {utf8 for utf8, _ in vocab.curated_emoji()}
    groups = {}
    for e, event in enumerate(vocab.EVENTS):
        path = VERSE_CORPUS / corpus.file_name(event)
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip():
                continue
            where = f"{path.name}:{number}"
            lead, form, text = line.split("\t")
            if lead not in LEADS or form not in VERSE_FORMS:
                raise ValueError(f"{where}: bad lead or form")
            first, _, body = text.partition(" ")
            if first not in emoji:
                raise ValueError(f"{where}: does not start with a curated emoji")
            verses = body.split(VERSE_BREAK)
            for verse in verses:
                if not verse or verse != verse.strip() or "/" in verse:
                    raise ValueError(f"{where}: bad verse line break")
                vocab.split_body(verse.replace(":", ""))  # alphabet check
            if len(" ".join(verses)) > vocab.MAX_TEXT_CHARACTERS:
                warn(f"warning: {where}: {len(' '.join(verses))} characters "
                     f"(guide {vocab.MAX_TEXT_CHARACTERS})")
            groups.setdefault((e, LEADS.index(lead)), []).append(
                panel_order(f"{first} " + "\n".join(verses)))
    return groups


def c_string(text):
    return ('u8"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
            + '"')


def render(groups, namespace="text_bank_data", source="the corpus",
           title="every corpus line"):
    events, leads = len(vocab.EVENTS), len(LEADS)
    starts, lines = [], []
    for e in range(events):
        for lead in range(leads):
            starts.append(len(lines))
            lines += groups.get((e, lead), [])
    starts.append(len(lines))
    out = [
        f"// Generated by apps/mainframe/tools/text_bank.py from {source}. Do not edit.",
        f"// Text bank (add-text-bank): {title}, grouped by event and lead value.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        f"namespace decaflash::mainframe::{namespace} {{",
        "",
        f"constexpr uint8_t kEvents = {events};  // TextModelEvent order",
        f"constexpr uint8_t kLeads = {leads};   // value * 2 + (high ? 1 : 0), Mood order",
        f"constexpr uint16_t kLineCount = {len(lines)};",
        "",
        "// The lines of group (event, lead) are kLines[kGroupStart[g] .. kGroupStart[g + 1]),",
        "// g = event * kLeads + lead.",
        f"inline constexpr uint16_t kGroupStart[kEvents * kLeads + 1] = {{",
    ]
    for row in range(0, len(starts), 10):
        out.append("  " + ", ".join(str(s) for s in starts[row:row + 10]) + ",")
    out += ["};", "", "inline constexpr const char* kLines[kLineCount] = {"]
    out += [f"  {c_string(text)}," for text in lines]
    out += ["};", "", f"}}  // namespace decaflash::mainframe::{namespace}", ""]
    return "\n".join(out)


def main():
    check = "--check" in sys.argv[1:]
    headers = [
        (HEADER, render(build())),
        (VERSE_HEADER, render(build_verse(warn=(lambda _: None) if check else print),
                              "text_bank_verse_data", "the verse corpus",
                              "every verse corpus line (add-verse-text-bank)")),
    ]
    stale = 0
    for path, header in headers:
        if check:
            current = path.read_text(encoding="utf-8") if path.exists() else ""
            if current != header:
                print(f"{path.relative_to(REPO)} is not up to date; run {sys.argv[0]}")
                stale = 1
        else:
            path.write_text(header, encoding="utf-8")
    return stale


if __name__ == "__main__":
    sys.exit(main())
