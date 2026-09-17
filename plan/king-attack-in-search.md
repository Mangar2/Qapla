# Königsangriff in der Suche — Analyse

Frage: wie kommt der Wert des Königsangriffs, getrennt für beide Seiten, in die Suche, um dort
Futility‑Margins (`forewardFutility`, `canPruneFutility`, später `forewardFutility2`) zu verändern?
Idee 23 in `ideas-0.5.0.md` (Spike: `MaxFutilityKingThread`).

Stand: 0.4.0-76-gb87ae3a plus uncommitted Futility‑Tunables vom 2026-09-17.

## 1. Was der Eval heute hat

`KingAttack::computeAttackValue<COLOR>` (`eval/king-attack.h`) rechnet pro König:

- `attackIndex` (0..32): Summe aus angegriffenen Feldern im Königsbereich ohne Bauerndeckung,
  gedeckten/ungedeckten Doppelangriffen, Schachfeldern (sichere doppelt) und `queenFactor`
  (3), falls der *Angreifer* eine Dame hat. Phasenunabhängig.
- `attackValue` = `attackWeight[attackIndex] * midgameInPercentV2 / 100` + Bauernschild.
  `attackWeight` ist konvex (0, −1, −3, … −658).

Vorzeichen und Perspektive: `COLOR` ist der Besitzer des Königs, `OPPONENT` greift an.
`index[WHITE]` ist also der Druck *auf den weißen König* durch Schwarz. Im Knoten ist
„eigener König“ = `index[sideToMove]`, „gegnerischer König“ = `index[opponent]`.

Drei Eigenschaften, die den Transport bestimmen:

1. Der Index braucht `EvalResults.piecesAttack/piecesDoubleAttack/queenAttack/…`, die von
   `Rook/Bishop/Knight/Queen::eval` gefüllt werden. Er ist **nicht** billig alleine berechenbar —
   ein Standalone‑Aufruf kostet praktisch die ganze Figuren‑Eval.
2. `lazyEval` ruft `KingAttack::eval` nur bei `midgameInPercent > 0`. Im reinen Endspiel gibt es
   keinen Index; er ist dort per Definition 0.
3. `Eval::eval` ist statisch, gibt ein `value_t` zurück, `EvalResults` ist eine lokale Variable
   in `lazyEval`. Nichts davon ist nach dem Aufruf erreichbar. Die Suche läuft mehrfädig
   (`threadpool.h`), ein statischer Merker in `KingAttack` scheidet aus.

## 2. Welche Größe transportiert wird

Vier Kandidaten:

| Größe | Bereich | Phase | Bemerkung |
|---|---|---|---|
| a) roher `attackIndex` | 0..32 | nein | das, was Spike nutzte (dort Skala bis 39, Schwellen 8/12) |
| b) phasenskalierter Index `index * midgameV2 / 100` | 0..32 | ja | linear, 6 Bit |
| c) cp‑Wert `attackWeight[index] * midgameV2 / 100` | 0..−658 | ja | konvex, Form vom Eval vorgegeben |
| d) c) plus Bauernschild | | ja | Schild ist Schwäche, kein Angriff — taktisch nicht aussagekräftig |

Empfehlung **b)**. Gründe:

- Phase gehört hinein: Index 15 bei 6 % Mittelspiel ist im Eval −14 cp und taktisch nahezu
  belanglos, weil kaum Figuren da sind. Ohne Skalierung würde die Margin im Übergang zum
  Endspiel grundlos wachsen. Bei `midgameInPercent == 0` fällt der Wert automatisch auf 0.
- Linear statt konvex: die Margin bekommt ihren *eigenen* Koeffizienten (`Margin += k * Druck`),
  die Form soll CLOP bestimmen, nicht die Tabelle des Evals. Wer die konvexe Form will, kann
  sie in der Suche mit `attackWeight[druck]` nachschlagen — b) enthält a) und c) als Ableitung,
  umgekehrt nicht.
- 0..32 passt in 6 Bit, was für den TT‑Weg (Abschnitt 4) entscheidend ist.

Bauernschild bleibt draußen: es misst eine statische Schwäche, die der Eval schon voll bewertet,
und sagt nichts darüber, ob gerade Taktik in der Luft liegt.

## 3. Transportweg Eval → SearchNode

### 3a. Out‑Parameter an `Eval::eval` (Empfehlung)

```cpp
// eval/evalresults.h
struct EvalAux {
    std::array<uint8_t, 2> kingPressure{};   // [WHITE], [BLACK], phasenskalierter Index 0..32
};
// eval/eval.h
static value_t eval(MoveGenerator& position, PawnTT* pawnttPtr = nullptr, value_t ply = 0,
    value_t alpha = -MAX_VALUE, EvalAux* aux = nullptr);
```

- `EvalResults` bekommt `std::array<uint8_t, 2> kingAttackIndex` (roh), gesetzt in
  `computeAttackValue<COLOR, …>` — eine Zuweisung, `STORE_DETAILS` bleibt unberührt.
- `lazyEval` schreibt am Ende, falls `aux != nullptr`, den skalierten Wert hinein. Wird
  `KingAttack::eval` nicht aufgerufen (Phase 0), bleibt 0.
- Alle sechs bestehenden Aufrufer (`search.cpp` ×2, `quiescencese.cpp` ×2, `search-node.h`,
  `eval.h::assertSymetry`) laufen unverändert weiter; nur `checkEvalReleatedCutoffsAndSetEval`
  und `initSearchAtRoot` übergeben `&node.evalAux`.
- Kosten: ein Nullptr‑Test und zwei Byte‑Schreibzugriffe pro Eval. Fadensicher, weil alles
  über lokale Daten läuft.

### 3b. Verworfen

- **`thread_local` in `KingAttack`**: versteckte Kopplung, Reihenfolgeabhängigkeit (Wert vom
  *letzten* Eval, nicht vom Knoten), Fehlerquelle bei Quiescence‑Evals zwischen Knoten. Nein.
- **Rückgabe eines Structs statt `value_t`**: sauber, aber Änderungen an allen Aufrufern,
  ohne dass Quiescence oder der `MAX_SEARCH_DEPTH`‑Cutoff etwas davon brauchen. 3a ist die
  gleiche Information mit weniger Berührungsfläche.
- **Berechnung in `KingAttack` als eigene statische Funktion für die Suche**: siehe Punkt 1 in
  Abschnitt 1 — zu teuer.

### 3c. Ablage im Knoten

`SearchNode` bekommt `std::array<uint8_t, 2> kingPressure` **nach Farbe**, nicht relativ zum
Zügigen. Gründe: der TT speichert Stellungseigenschaften, nicht Perspektiven; `stack[ply - 2]`
(isImproving‑Muster) vergleicht dieselbe Seite. Zwei Accessor:

```cpp
uint8_t ownKingPressure(const MoveGenerator& p) const { return kingPressure[p.isWhiteToMove() ? WHITE : BLACK]; }
uint8_t oppKingPressure(const MoveGenerator& p) const { return kingPressure[p.isWhiteToMove() ? BLACK : WHITE]; }
```

Gesetzt in `setFromParentNode` auf 0 (wie `adjustedEval = NO_VALUE`), gefüllt an genau den
Stellen, die `eval` füllen: `probeTT` (Abschnitt 4) und `checkEvalReleatedCutoffsAndSetEval`.
Im Schach wird kein Eval gerechnet und kein Futility gemacht — 0 ist dort richtig.

## 4. Der eigentliche Knackpunkt: der Eval kommt oft aus dem TT

`SearchNode::probeTT` setzt `eval = entry.getEval()`. In `checkEvalReleatedCutoffsAndSetEval`
wird `Eval::eval` dann **nicht** aufgerufen. Ist der Eintrag exakt, wird sogar `adjustedEval`
direkt gesetzt und der ganze Eval‑Block übersprungen. Auf all diesen Knoten gäbe es mit 3a
allein keinen Königsdruck, und Futility läuft genau dort.

Wie oft das ist, sollte vor dem Bauen ein Zähler im Debug‑Build sagen (Knoten in
`forewardFutility` mit `eval` aus dem TT vs. gerechnet). Erwartung: ein großer Anteil — es sind
die Knoten, die schon einmal besucht wurden, und das sind bei iterativer Vertiefung die meisten
inneren Knoten.

Drei Wege:

### 4a. Im TT mitspeichern (Empfehlung)

`TTEntry` ist 16 Byte: `Move`, `_value`, `_eval`, `_info`, `_pv`, `_hash`. **`_pv` ist ein
`uint16_t`, der ein `bool` hält — 15 Bit sind frei.** Zwei Werte à 6 Bit passen hinein, ohne
dass der Eintrag wächst oder ein anderes Feld enger wird.

Regel: **Druck ist gültig genau dann, wenn `_eval != NO_VALUE`.** Das hält die Semantik
ohne eigenes „unbekannt“‑Bit:

- `setTTEntry` schreibt `node.eval` und `node.kingPressure` zusammen. Ist `eval` NO_VALUE
  (Knoten im Schach, `storePVToTT`), sind die Druckwerte bedeutungslos, werden aber nie
  gelesen, weil der Leser dann `Eval::eval` ruft und sie neu bekommt.
- Ausnahme: exakter Eintrag mit `eval == NO_VALUE` (kommt nur aus `storePVToTT`). Dort wird
  heute gar kein Eval gerechnet; der Druck bleibt 0. Selten, PV‑Stellungen, tolerierbar.
  **Nicht** an dieser Stelle einen Eval nachziehen, um `isImproving` nicht zu verändern.
- Ein Eintrag wird nur von derselben Stellung ersetzt (oder ist eine 32‑Bit‑Kollision, dann ist
  der Eval genauso Müll wie der Druck) — keine neue Inkonsistenz gegenüber heute.

Umfang: `TTEntry::initialize` und ein Getter/Setter, `TT::setEntry` (Signatur), beide
`SearchNode::setTTEntry`, der Aufruf in `movehistory.h:132` (0/0 übergeben), `probeTT` liest.
Mechanisch, überschaubar. Quiescence schreibt keine TT‑Einträge, bleibt außen vor.

### 4b. Kennzahl in der Suche aus den Angriffsmasken (Alternative)

Der Zuggenerator hält `attackMask[COLOR]` und `pawnAttack[COLOR]`; sie sind beim Knoteneintritt
gültig (`doMove` → `isInCheck()` liest sie ohne Neuberechnung). Damit ist

```cpp
popCount(KingAttack::_kingAttackBB[c][kingSq] & position.attackMask[opp] & ~position.pawnAttack[c])
```

ein Königsdruck für ein paar Instruktionen, ohne Eval, ohne TT, in jedem Knoten. Es ist aber
**eine andere Größe**: keine Doppelangriffe, keine Schachfelder, kein Damenfaktor, keine Phase —
gröber als der Eval‑Index. Sinnvoll als eigenständiges Experiment oder als Fallback für die
Knoten aus 4c, nicht als „der Wert des Königsangriffs“.

### 4c. Unbekannt = 0 (nur als Machbarkeits‑Spike)

Druck nur dort nutzen, wo er gerechnet wurde. Deterministisch und EPD‑prüfbar, aber das Feature
wäre auf einem TT‑abhängigen Bruchteil der Knoten aktiv; ein SPRT darauf misst vor allem die
Verdünnung. Als getestete Version nicht geeignet.

## 5. Verwendung in den Prunings

Die Richtung folgt aus der Frage „welche Taktik übersieht der statische Eval hier?“

| Stelle | Prunt wenn | Risiko | Druck‑Term |
|---|---|---|---|
| `forewardFutility` (Fail‑High) | `eval − margin ≥ beta` | Gegner hat Angriff auf *unseren* König, wir stehen nicht wirklich über beta | `margin += ffOwnKingPressure * ownKingPressure` |
| `canPruneFutility` (stiller Zug, Fail‑Low) | `eval + margin < alpha` | *unser* stiller Zug ist ein Angriffszug gegen den gegnerischen König | `margin += futOppKingPressure * oppKingPressure` |
| `forewardFutility2` (Fail‑Low, tot) | `eval + margin ≤ alpha` | wie `canPruneFutility` | `margin += ffOppKingPressure2 * oppKingPressure` |

Das deckt sich mit Spike: `DoRazoring` (Fail‑Low‑Seite) erlaubt Futility nur, wenn der
*gegnerische* König (`mWtm ? mBKingThreat : mWKingThreat`) unter der Schwelle liegt; der
Eigenkönig‑Test im `StaticNullmove` war auskommentiert.

Form nach `CLAUDE.md`, „Shape of a tunable value“: ein Koeffizient mit Spielraum (`0..2·x`),
keine Schwelle — die Schwelle ist eine 0/1‑Ausnahme, die CLOP nicht bewegen kann. Der Index
selbst ist schon in `adjustedEval` enthalten (über `attackWeight`); der Margin‑Term misst nicht
den Mittelwert, sondern die *Streuung* des Evals unter Angriff. Gruppenflag
`SearchConfig::optimizeFutility`.

Nicht in den ersten Lauf: Nullmove‑Reduktion bei eigenem Königsdruck (die Verifikation fängt
das heute ab), LMR (bräuchte den Druck *nach* dem Zug, also den des Kindes — erst nach dessen
Eval bekannt).

## 6. Kosten und Neutralitätsnachweis

Eval: zwei Byte‑Zuweisungen und ein Nullptr‑Test. TT: zwei Shifts beim Schreiben, zwei beim
Lesen. Nichts davon ändert den Suchbaum, solange die Koeffizienten 0 sind.

Deshalb zwei Commits:

1. **Plumbing** (3a, 3c, 4a, Koeffizienten 0, Terme im Code): EPD‑Vergleichslauf
   `test/epd/epd-wmtest-depth.ini`, Nodes **identisch**, Laufzeit innerhalb 5 %.
2. **Aktivierung** je Stelle: Koeffizient ≠ 0 → Nodes verschieden (Code erreicht) → CLOP,
   ein Parameter, ~2000 Samples, Bereich um den Startwert zentriert → SPRT
   `--sprt eloh0=-3 eloh1=2` gegen die Version vor der Aktivierung. Erst `forewardFutility`,
   dann `canPruneFutility` gegen den Sieger. Mehr als zwei SPRTs → Abschlusslauf gegen den
   Stand vor dem Plumbing.

Startwert für die CLOP‑Mitte: die Margin ist in cp, der Druck 0..32; bei `ffBase = 69` und
`ffDepthFactor = 83` ist ein Koeffizient von ~10 (Druck 10 → +100 cp) eine plausible Mitte,
Bereich 0..20. Landet CLOP bei 0, ist der Eintrag ohne SPRT erledigt.

## 7. Fallstricke

- Perspektive: `index[WHITE]` = Angriff *auf* Weiß. `ownKingPressure` = `index[sideToMove]`.
  Ein vertauschtes Vorzeichen fällt im EPD‑Lauf nicht auf (Nodes ändern sich so oder so).
  Einen Test mit einer Stellung, in der nur ein König angegriffen ist, vor dem CLOP machen.
- `isImproving` darf durch das Plumbing nicht anders gesetzt werden — nur dort Eval rechnen, wo
  heute Eval gerechnet wird.
- `_pv`‑Bits: `isPV()` prüft `_pv != 0`, muss auf das eine Bit maskiert werden, sobald die
  anderen 15 belegt sind.
- Debug‑`assertSymetry` gilt weiter; `EvalAux` symmetrisch mitprüfen ist billig und findet
  Perspektivfehler sofort.
