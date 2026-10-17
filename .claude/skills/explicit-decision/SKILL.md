---
name: explicit-decision
description: How to make a decision myself when Volker leaves it to me as the AI - the next value of a search (WDL weight, parameter), which run comes next, what to abort, what a result means for the plan. Apply at every decision point, whenever a result comes in that something depends on, and never replace it by a rule, a script or a decision tree fixed in advance.
---

# Explicit decision by myself

Volker (2026-10-17): "Der beste Entscheidungsalgorithmus, der dir zur Verfügung steht, bist du selber." A script or a
rule fixed in advance does not weigh everything that matters; I do, if I go through these steps every time.

## 1. Find my bias

What would I do by default, what did I do the last times? Name it, and question it especially: is it still good, or
is it only what has always been done?

My known defaults (seen 2026-10-16/17):
- continuing the current pattern in small "safe" steps (WDL 0.95 -> 0.90 -> ... in fixed 0.05 steps although every
  step won fast and nothing flattened)
- letting a run finish before using what it already shows
- turning the decision into a rule: a chain script, or an "if A then B, else C" announced in advance - a script in
  my head
- reading a magnitude out of a single measurement (an SPRT score is not an Elo figure)
- starting the next thing fast without checking its premise (a book whose score meant "before the move" combined with
  counts that mean "after the move"; a net put into the reference field before it had won its head to head)

The first idea that comes up usually follows one of these. Say which, then argue against it.

## 2. Decide as late as sensible - and collect everything first

Decide when the results the decision depends on are in, or when waiting longer costs more than it can bring. Never
fix the next step before.

Then collect, for myself, every piece of information that bears on the decision - in my own reasoning, not as a
report to Volker:
- every value tried so far and its result
- every SPRT: result, bounds, games to the decision, score, W/D/L - and the running ones with their current state
  (a running SPRT is less biased than a stopped one)
- the reference field: Elo with its error bar, how it moved with each step, draw rate
- the trend over all steps, not just the last one
- what runs where (GPU, game cores, every machine), what each will deliver and when, what would become useless
- what each next step costs (training ~2 h, SPRT 0.5-2 h, reference field ~2 h) and what it can tell

## 3. Make the decision

Ask: on the basis of all of this, what is the most efficient next step - the one that brings the most information or
the most strength for its cost? Consider bold steps (bracketing, bisection, a measurement of magnitudes instead of
another pairwise test), not only the next small one.

## 4. Question it once more - then decide finally

Did I really take everything into account? Does the decision follow one of my defaults from step 1 after all? Is
there information that will be available soon and would change it? Does it break a rule of CLAUDE.md or of the
other skills (one factor per comparison, nothing chained behind a measurement, the reference field only for a
winner)? Only then is the decision final.

In the report to Volker: what was decided and why, in a few lines - not the whole list and not what the next
decision is going to be.
