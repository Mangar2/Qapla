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

## Erledigt

### 1. forwardFutility depth limit — H1 accepted

- **Baseline:** 0.5.0-028-3-g3f460e1, EPD wmtest Tiefe 18: 248740566 Nodes.
- **CLOP (M4, 2026-11-13):** `optimizeFutility = true`, nur `ffDepthLimit min=0 max=20`, 2000 Samples,
  `test/clop/clop-standard.ini`, Concurrency 8. Schätzung **12.39**, gerundet **12**.
- **Zugeschaltet:** in `forewardFutility()` (`search/search-node.h`)
  `tunable<OPT, "ffDepthLimit", 12, 0, 20>()` statt `10`. EPD: 250812256 Nodes, anders - erreicht.
- **SPRT (M4, 2026-11-13 11:58 bis 2026-11-14 15:27):** gegen das Baseline-Binary,
  `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1`, `--sprt eloh0=-3 eloh1=2`, Concurrency 8:
  **H1 accepted nach 16509 Partien** (W 3467 D 9685 L 3357), Grenzen −3/+2.
- **Abgeschaltet:** Default wieder 10, `git diff` leer.

### 2. ffBestValueDivisor — H1 accepted

- **Baseline:** 0.5.0-028-3-g3f460e1, EPD wmtest Tiefe 18: 248740566 Nodes.
- **CLOP (M4, 2026-11-14):** `optimizeFutility = true`, nur `ffBestValueDivisor min=1 max=39`, 2000 Samples,
  `test/clop/clop-standard.ini` mit `--each tc=2+0.004` (Zeitkontrolle für den M4 skaliert: eine M4-Partie hat etwa
  2,5-mal die Knoten/s einer Linux-Partie unter Volllast), Concurrency 8. Schätzung **23.89**, gerundet **24**.
- **Zugeschaltet:** in `forewardFutility()` (`search/search-node.h`)
  `tunable<OPT, "ffBestValueDivisor", 24, 1, 41>()` statt `20`. EPD: 252714475 Nodes, anders - erreicht.
- **SPRT (M4, 2026-11-14 16:35 bis 2026-11-15 00:33):** gegen das Baseline-Binary,
  `test/sprt/sprt-standard.ini` mit `--each tc=8+0.04` (20+0.1 auf den M4 skaliert), `--sprt eloh0=-3 eloh1=2`,
  Concurrency 8: **H1 accepted nach 12053 Partien** (W 2756 D 6663 L 2634), Grenzen −3/+2.
- **Abgeschaltet:** Default wieder 20, `git diff` leer.

### 3. „No pruning on silent TT move“ entfernt — unentschieden

- **Baseline:** 0.5.0-028-3-g3f460e1, EPD wmtest Tiefe 18: 248740566 Nodes.
- **Zugeschaltet:** in `forewardFutility()` (`search/search-node.h`) die Zeile
  `if (!getTTMove().isEmpty() && !getTTMove().isCapture()) return false;` auskommentiert. EPD: 249230983 Nodes,
  anders - erreicht.
- **SPRT (Linux, 2026-11-14 20:04 bis 2026-11-15 04:59):** gegen das Baseline-Binary,
  `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1`, `--sprt eloh0=-3 eloh1=2`, Concurrency 30:
  **unentschieden nach 20000 Partien**, LLR 1.32 (W 4368 D 11304 L 4328), Grenzen −3/+2.
- **Abgeschaltet:** Zeile wieder aktiv, `git diff` leer.

### 4. forewardFutility2 — H0 accepted

- **CLOP (M4, 2026-11-15 01:55 bis 03:55):** `return false;` auskommentiert, `constexpr bool OPT =
  SearchConfig::optimizeFutility` statt `false`, `optimizeFutility = true`; die vier Parameter in einem Lauf (Mitten
  100/100/100/20), 4000 Samples, `test/clop/clop-standard.ini` mit `--each tc=2+0.004`, Concurrency 8. Schätzungen
  ffDepthFactor2 **186.26**, ffBase2 **178.75**, ffImprovingBonus2 **119.94**, ffBestValueDivisor2 **21.91** -
  gerundet 186 / 179 / 120 / 22. Die ersten beiden nahe an der Obergrenze 200.
- **Zugeschaltet:** auf 0.5.0-028-3-g3f460e1 `return false;` in `forewardFutility2()` auskommentiert, Flag false,
  Defaults 186 / 179 / 120 / 22. EPD: 238856561 Nodes (Baseline 248740566), anders - erreicht.
- **SPRT (Studio, 2026-11-15 07:29 bis 07:43):** gegen das Baseline-Binary, `test/sprt/sprt-standard.ini` von
  release0.5 mit `--each tc=8+0.04` (Studio mit 12 Engines ~ Linux 20+0.1), `--sprt eloh0=-3 eloh1=2`,
  Concurrency 12: **H0 accepted nach 514 Partien** (W 56 D 222 L 236), Grenzen −3/+2.
- **Abgeschaltet:** auf release0.5 nie eingeschaltet, `git diff` leer.

### 5. ffMargin — H1 accepted

- **Baseline:** 0.5.0-028-3-g3f460e1, EPD wmtest Tiefe 18: 248740566 Nodes.
- **CLOP (Linux, 2026-11-15 07:30 bis 09:45):** `optimizeFutility = true`, die drei Koeffizienten
  `ffDepthFactor min=0 max=166`, `ffBase min=0 max=138`, `ffImprovingBonus min=0 max=202` (Mitten = Defaults 83 / 69 /
  101), 3000 Samples, `test/clop/clop-standard.ini` (5+0.01), Concurrency 15. Schätzungen ffDepthFactor **89.68**,
  ffBase **84.13**, ffImprovingBonus **122.36** - gerundet 90 / 84 / 122.
- **Zugeschaltet:** auf 0.5.0-028-3-g3f460e1 in `forewardFutility()` (`search/search-node.h`) die Defaults 90 / 84 /
  122 statt 83 / 69 / 101. EPD: 257368343 Nodes, anders - erreicht.
- **SPRT (Linux, 2026-11-15 13:04 bis 19:52):** gegen das Baseline-Binary, `test/sprt/sprt-standard.ini` mit
  `--each tc=20+0.1`, `--sprt eloh0=-3 eloh1=2`, Concurrency 30: **H1 accepted nach 15304 Partien** (W 3406 D 8610
  L 3288), Grenzen −3/+2.
- **Abgeschaltet:** auf release0.5 nie eingeschaltet (gebaut auf einem Checkout von 3f460e1), `git diff` leer.

### 6. forewardFutility3 — unentschieden

- **Baseline:** 0.5.0-028-3-g3f460e1, EPD wmtest Tiefe 18: 248740566 Nodes.
- **CLOP (M4, 2026-11-15 07:33 bis 09:43):** `false &&` im Aufruf in `search.cpp` entfernt, `return false;` in
  `forewardFutility3()` auskommentiert, `OPT = SearchConfig::optimizeFutility`, `optimizeFutility = true`; die vier
  Parameter in einem Lauf (Mitten 100/100/100/20), 4000 Samples, `test/clop/clop-standard.ini` mit `--each tc=2+0.004`,
  Concurrency 8. EPD bei den Defaults: 227060322 Nodes, anders - erreicht. Schätzungen ffDepthFactor3 **106.19**,
  ffBase3 **109.03**, ffImprovingBonus3 **106.89**, ffBestValueDivisor3 **17.28** - gerundet 106 / 109 / 107 / 17.
- **Zugeschaltet:** auf 0.5.0-028-3-g3f460e1 die Methode eingeschaltet, Flag false, Defaults 106 / 109 / 107 / 17.
  EPD: 244268755 Nodes, anders - erreicht.
- **SPRT (Studio, 2026-11-15 13:05 bis 22:59):** gegen das Baseline-Binary, `test/sprt/sprt-standard.ini` von release0.5
  mit `--each tc=9+0.045` (Studio mit 12 Engines ~ Linux 20+0.1), `--sprt eloh0=-3 eloh1=2`, Concurrency 12:
  **unentschieden nach 20000 Partien**, LLR 0.32 (W 4244 D 11508 L 4248), Grenzen −3/+2.
- **Abgeschaltet:** auf release0.5 nie eingeschaltet (gebaut auf einem Checkout von 3f460e1), `git diff` leer.
