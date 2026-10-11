# Test-ToDo

Evaluierungen, die später abzuarbeiten sind — Messungen, ob eine Änderung sinnvoll wäre. Eine
Evaluierung ist keine geprüfte Version (siehe `CLAUDE.md`, „Evaluations are not tested
versions“); geprüfte Versionen entstehen erst danach, nach dem dortigen Vorgehen. CLOP- und
SPRT-Läufe selbst wie in `CLAUDE.md` („Tuning parameter values (CLOP)“, „Trying out a new
version (SPRT)“). Erledigte Einträge wandern mit Ergebnis in den Abschnitt „Erledigt“.

Ausführung: alle Läufe auf dem Linux-System mit 32 vCores (16 physische Kerne), nicht auf dem
Mac-Laptop. Concurrency-Budget 30 für die ganze Maschine, siehe `CLAUDE.md`; Binary
`build/Release/Qapla`, `qet` statt `qet.exe`.

Ergebnisformat: SPRT liefert nur „H0 accepted“ / „H1 accepted“ / „undecided“ plus Spielzahl und
Bounds — keine Elo-Zahl, keine Winrate.

## Jeder Punkt wird isoliert evaluiert

**Zuschalten, prüfen, abschalten, nächster Punkt.** Kein Punkt baut auf dem Ergebnis eines
anderen auf, und kein Ergebnis bleibt aktiv — auch H1 nicht.

- **Der Code ist fertig, bevor geprüft wird.** Alle Varianten und Alternativen stehen beim
  Anlegen eines Eintrags schon im Code, abgeschaltet (nicht aufgerufen, `return false;`,
  Default-Wert, Gruppenflag false). Dieser Stand ist committet; sein EPD-Lauf hat dieselben Nodes
  wie der Stand davor. Er ist die Baseline für *alle* Punkte, ihr Binary liegt in
  `new-versions/`.
- **Zuschalten:** genau die im Eintrag genannte Stelle ändern (Aufruf, Zeile, Default-Wert),
  sonst nichts. Kein Commit, kein Branch, kein Tag. `make Release -j`, EPD-Lauf: die Nodes müssen
  sich von der Baseline unterscheiden, sonst ist die Variante nicht erreicht.
- **Prüfen:** ggf. CLOP, dann SPRT gegen das Baseline-Binary, engine name `Qapla-<punkt>`.
- **Abschalten:** die Änderung zurücknehmen. Der Code ist danach exakt wie vorher —
  `git diff` ist leer. Auch getunte Werte kommen nicht in den Code, sie stehen nur in der
  Dokumentation.
- **Dokumentieren:** Ergebnis hier nach „Erledigt“ (kein Eintrag in `plan/version-log.md`, der
  gilt nur für getaggte Versionen). Jeder Eintrag enthält, was genau zugeschaltet war (Stelle, Werte), bei CLOP
  die geschätzten Werte (Schätzung und gerundeter Wert, mit Samplezahl), beim SPRT Ergebnis,
  Spielzahl und Bounds — so, dass sich der geprüfte Stand aus der Baseline exakt wiederherstellen
  lässt. Die CLOP-Werte werden **immer** dokumentiert: bei H0, H1 und undecided ebenso wie wenn
  CLOP beim alten Wert landet und gar kein SPRT folgt.
- **Reihenfolge** der Punkte ist beliebig. Teilen sich Punkte ein Gruppenflag, werden im CLOP-Lauf
  auch die Optionen der anderen Punkte sichtbar; sie stehen auf ihren Defaults und laufen nicht
  mit, das stört nicht.
- **Nach dem letzten Punkt:** keine weitere Aktion. Was mit den Ergebnissen geschieht, wird erst
  entschieden, wenn alle vorliegen.

## Offen

### 1. forwardFutility depth limit — CLOP, dann SPRT

- **Was:** Das Tiefenlimit von `forewardFutility()` in `search/search-node.h` war die Konstante
  `SearchConfig::FOREWARD_FUTILITY_DEPTH = 10` und ist jetzt der tunable
  `tunable<OPT, "ffDepthLimit", 10, 0, 20>()` (Gruppenflag `SearchConfig::optimizeFutility`
  in `search/search-config.h`).
- **CLOP:** `optimizeFutility = true`, `make Release -j`, Option `ffDepthLimit` per `uci` prüfen.
  Nur diesen einen Parameter tunen: `--clopvalue name=ffDepthLimit min=0 max=20` (Mitte = 10),
  ~2000 Samples, `test/clop/clop-standard.ini`. Ergebnis runden. Landet CLOP bei 10, ist der
  Eintrag ohne SPRT erledigt (CLOP-Wert trotzdem dokumentieren).
- **Zuschalten:** `optimizeFutility` zurück auf false, Default 10 durch den CLOP-Wert ersetzen.
- **SPRT:** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken!)
  und `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffDepthLimit.state`.
- **Abschalten:** Default wieder 10.

### 2. ffBestValueDivisor — CLOP, dann SPRT

- **Was:** In `forewardFutility()` (`search/search-node.h`) wird bei Pruning
  `bestValue = beta + (adjustedEval - beta) * 10 / tunable<OPT, "ffBestValueDivisor", 20, 1, 41>()`
  gesetzt — Default 20, d.h. bestValue liegt zur Hälfte zwischen beta und adjustedEval. Gleiches
  Gruppenflag `SearchConfig::optimizeFutility`. Zähler 10 bleibt fest, nur der Divisor wird
  getunt — Zähler und Nenner nie im selben Lauf.
- **CLOP:** `optimizeFutility = true`, `make Release -j`, Option per `uci` prüfen. Nur diesen
  Parameter: `--clopvalue name=ffBestValueDivisor min=1 max=39` (Mitte = 20; die UCI-Grenze 41
  ist weiter als nötig, das ist erlaubt), ~2000 Samples, `test/clop/clop-standard.ini`. Runden.
  Landet CLOP bei 20: Eintrag ohne SPRT erledigt (CLOP-Wert trotzdem dokumentieren).
- **Zuschalten:** Flag zurück auf false, Default 20 durch den CLOP-Wert ersetzen.
- **SPRT:** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffBestValueDivisor.state`.
- **Abschalten:** Default wieder 20.

### 3. „No pruning on silent TT move“ in forewardFutility — SPRT (Ausnahme entfernen?)

- **Was:** `forewardFutility()` (`search/search-node.h`, Zeile ~273) prunt nicht, wenn ein
  stiller (nicht schlagender) TT-Zug vorliegt:
  `if (!getTTMove().isEmpty() && !getTTMove().isCapture()) return false;`
  Begründung im Kommentar: stille TT-Züge gibt es nur, wenn die Stellung schon im Fenster war.
  Frage: bringt diese Ausnahme etwas oder kann sie weg?
- **Kein CLOP** (boolesche Entscheidung), nur SPRT.
- **Zuschalten:** die Zeile auskommentieren. Kandidat = Version *ohne* die Ausnahme.
- **SPRT:** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffSilentTTMove.state`.
- **Abschalten:** Zeile wieder aktiv.

### 4. forewardFutility2 (Fail-Low-Pruning) — CLOP, dann SPRT

- **Was:** `forewardFutility2()` in `search/search-node.h` ist die Fail-Low-Spiegelung von
  `forewardFutility()`: prunt, wenn `adjustedEval + margin <= alpha`, mit
  `bestValue = alpha - (alpha - adjustedEval) * 10 / ffBestValueDivisor2`. Die Methode ist
  über ein `return false;` in der ersten Zeile totgelegt; aufgerufen wird sie in
  `search.cpp` (`checkEvalReleatedCutoffsAndSetEval`, `forewardFutility(...) || forewardFutility2(...)`).
  Logik am 2026-09-17 geprüft und korrigiert (Vorzeichen bestValue, Guards). Die Parameter hängen
  am Gruppenflag `SearchConfig::optimizeFutility` (nicht an einem lokalen `constexpr bool OPT`).
- **CLOP:** `return false;` auskommentieren, `optimizeFutility = true`, `make Release -j`,
  Optionen `ffDepthFactor2`, `ffBase2`, `ffImprovingBonus2`, `ffBestValueDivisor2` per `uci`
  prüfen. Alle vier Parameter in einem Lauf, den Divisor eingeschlossen (sein Zähler 10 ist eine
  Konstante und läuft nicht mit, es kann sich also nichts wegkürzen):
  `--clopvalue name=ffDepthFactor2 min=0 max=200 --clopvalue name=ffBase2 min=0 max=200 --clopvalue name=ffImprovingBonus2 min=0 max=200 --clopvalue name=ffBestValueDivisor2 min=1 max=39`
  (Mitten 100/100/100/20), ~4000 Samples, `test/clop/clop-standard.ini`. Runden. Das Tiefenlimit
  `10 <= remainingDepth` bleibt in diesem Lauf fest.
- **Zuschalten:** `return false;` auskommentiert, Flag zurück auf false, die vier Defaults durch
  die CLOP-Werte ersetzen. Kandidat = aktive Methode mit getunten Werten.
- **SPRT:** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-forewardFutility2.state`.
- **Abschalten:** `return false;` wieder aktiv, Defaults wieder 100/100/100/20.

### 5. ffMargin (ffDepthFactor, ffBase, ffImprovingBonus) — CLOP, dann SPRT

- **Was:** Die Marge in `forewardFutility()` (`search/search-node.h`):
  `margin = ffDepthFactor * remainingDepth + ffBase - ffImprovingBonus * isImproving`,
  Defaults 83 / 69 / 101, UCI-Grenzen 0–166 / 0–138 / 0–202, Gruppenflag
  `SearchConfig::optimizeFutility`. Die drei Koeffizienten wurden noch nie gemeinsam getunt.
  ffDepthLimit und Divisor stehen auf ihren Defaults und laufen nicht mit.
- **Beobachtung (2026-09-17):** bei `isImproving` und kleiner Tiefe wird die Marge negativ
  (Tiefe 0: 83·0 + 69 − 101 = −32, Tiefe 1: 51 − 101 < 0 ab isImproving). Dann würde
  `adjustedEval - margin >= beta` schon bei `adjustedEval < beta` prunen; die Guard
  `if (adjustedEval < beta) return false;` fängt das ab. Sie bleibt im Lauf unverändert — der
  Lauf soll zeigen, ob die Koeffizienten so liegen sollen, dass die Marge negativ wird, oder nicht.
- **CLOP:** Flag auf true, `make Release -j`, Optionen per `uci` prüfen. Drei Parameter, Mitten =
  Defaults:
  `--clopvalue name=ffDepthFactor min=0 max=166 --clopvalue name=ffBase min=0 max=138 --clopvalue name=ffImprovingBonus min=0 max=202`
  ~3000 Samples, `test/clop/clop-standard.ini`. Runden.
- **Zuschalten:** Flag zurück auf false, die drei Defaults durch die CLOP-Werte ersetzen.
- **SPRT:** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffMargin.state`.
- **Abschalten:** Defaults wieder 83 / 69 / 101.

### 6. forewardFutility3 (Fail-Low-Pruning auf Quiescence-Wert) — CLOP, dann SPRT

- **Was:** `forewardFutility3()` in `search/search-node.h` prunt fail low wie
  `forewardFutility2()`, prüft aber nicht die Evaluierung, sondern einen Quiescence-Wert gegen
  alpha. Idee: die Quiescence schließt direkten Materialgewinn aus, das erlaubt ggf. eine kleinere
  Marge und damit aggressiveres Pruning. Ablauf in der Methode: Guards (Tiefe < 10,
  `adjustedEval < alpha`, kein PV-Knoten, kein stiller TT-Zug, Wertgrenzen), Marge
  `margin3 = ffDepthFactor3 * remainingDepth + ffBase3 + ffImprovingBonus3 * isImproving`,
  Vorfilter `adjustedEval + margin3 > alpha` → kein Pruning (der Quiescence-Wert ist mindestens
  stand pat), dann Quiescence mit Null-Window an der Schwelle `alpha - margin3` über einen
  Callback, Pruning bei `quiescenceValue <= alpha - margin3` mit
  `bestValue = alpha - (alpha - quiescenceValue) * 10 / ffBestValueDivisor3`. Aufgerufen in
  `search.cpp` (`checkEvalReleatedCutoffsAndSetEval`) für `remainingDepth > 1`.
  Defaults 100 / 100 / 100 / 20, UCI-Grenzen 0–200 / 0–200 / 0–200 / 1–41.
- **Zuschalten:** `false &&` in der Bedingung des Aufrufs in `search.cpp` entfernen und
  `return false;` am Anfang von `forewardFutility3()` auskommentieren. EPD-Lauf muss andere Nodes
  zeigen als die Baseline.
- **CLOP:** alle vier Parameter in einem Lauf, den Divisor eingeschlossen (sein Zähler 10 ist
  eine Konstante und läuft nicht mit):
  `--clopvalue name=ffDepthFactor3 min=0 max=200 --clopvalue name=ffBase3 min=0 max=200 --clopvalue name=ffImprovingBonus3 min=0 max=200 --clopvalue name=ffBestValueDivisor3 min=1 max=39`
  (Mitten 100/100/100/20), ~4000 Samples, `test/clop/clop-standard.ini`. Runden. Das
  Tiefenlimit 10 bleibt fest.
- **SPRT:** Kandidat = zugeschaltete Methode mit den CLOP-Werten als Defaults.
  `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-forewardFutility3.state`.
- **Abschalten:** `false &&` und `return false;` wieder rein, Defaults wieder
  100 / 100 / 100 / 20.

## Erledigt

(noch nichts)
