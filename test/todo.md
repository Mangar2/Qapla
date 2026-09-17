# Test-ToDo

Aufgaben, die später abzuarbeiten sind — in der Regel Prüfungen, ob eine Änderung sinnvoll
ist. Vorgehen für CLOP und SPRT steht in `CLAUDE.md` (Abschnitte „Tuning parameter values
(CLOP)“ und „Trying out a new version (SPRT)“). Erledigte Einträge wandern mit Ergebnis in den
Abschnitt „Erledigt“, das Ergebnis zusätzlich in `plan/version-log.md`.

Ausführung: alle Läufe auf dem Linux-System mit 32 vCores (16 physische Kerne), nicht auf dem
Mac-Laptop. Concurrency-Budget 30 für die ganze Maschine, siehe `CLAUDE.md`; Binary
`build/Release/Qapla`, `qet` statt `qet.exe`.

Ergebnisformat: SPRT liefert nur „H0 accepted“ / „H1 accepted“ / „undecided“ plus Spielzahl und
Bounds — keine Elo-Zahl, keine Winrate.

## Offen

### 1. forwardFutility depth limit — CLOP, dann SPRT

- **Was:** Das Tiefenlimit von `forewardFutility()` in `search/search-node.h` war die Konstante
  `SearchConfig::FOREWARD_FUTILITY_DEPTH = 10` und ist jetzt der tunable
  `tunable<OPT, "ffDepthLimit", 10, 0, 20>()` (Gruppenflag `SearchConfig::optimizeFutility`
  in `search/search-config.h`). Uncommitted Stand vom 2026-09-17, Basis `0.4.0-76-gb87ae3a`.
- **Schritt 0:** EPD-Vergleichslauf (`test/epd/epd-wmtest-depth.ini`, kein `rapid`) vor/nach der
  Umstellung: Nodes müssen identisch sein (reine Umformung, Default 10 unverändert).
- **Schritt 1 (CLOP):** `optimizeFutility = true` setzen (nicht committen), `make Release -j`,
  Option `ffDepthLimit` per `uci` prüfen. Nur diesen einen Parameter tunen:
  `--clopvalue name=ffDepthLimit min=0 max=20` (Mitte = 10), ~2000 Samples,
  `test/clop/clop-standard.ini`. Ergebnis runden, als neuen Default eintragen, Flag zurück auf
  false. Hinweis: Range hat 21 Werte, ist also tunebar; falls CLOP bei 10 landet, ist der Eintrag
  ohne SPRT erledigt (keine Änderung).
- **Schritt 2 (SPRT):** neuen Default committen, taggen (nächste Nummer nach `0.4.0-…`), clean +
  rebuild, EPD-Lauf muss nun *andere* Nodes zeigen. Baseline = Build vor der Änderung in
  `new-versions/`. Lauf mit `test/sprt/sprt-standard.ini`, aber Zeitkontrolle und Bounds
  überschrieben:
  `--each tc=20+0.1` (vor den `--engine`-Blöcken!) und `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffDepthLimit.state`.
- **Entscheidung:** H1 → Wert bleibt. H0 → Commit nach `dead/ffDepthLimit`, Default zurück auf 10.
  Eintrag in `plan/version-log.md` als eigener Commit.

### 2. ffBestValueDivisor — CLOP, dann SPRT (Vorgehen wie Nr. 1)

- **Was:** In `forewardFutility()` (`search/search-node.h`) wird bei Pruning
  `bestValue = beta + (adjustedEval - beta) * 10 / tunable<OPT, "ffBestValueDivisor", 20, 1, 41>()`
  gesetzt — Default 20, d.h. bestValue liegt zur Hälfte zwischen beta und adjustedEval. Gleiches
  Gruppenflag `SearchConfig::optimizeFutility`. Uncommitted Stand vom 2026-09-17.
- **Reihenfolge:** erst Nr. 1 abschließen; Baseline für diesen Eintrag ist dann die Version nach
  der Entscheidung zu Nr. 1 (Zähler 10 bleibt fest, nur der Divisor wird getunt — Zähler und
  Nenner nie im selben Lauf).
- **Schritt 0:** EPD-Vergleichslauf, Nodes identisch (reine Umformung, Default 20).
- **Schritt 1 (CLOP):** `optimizeFutility = true` (nicht committen), `make Release -j`, Option
  per `uci` prüfen. Nur diesen Parameter: `--clopvalue name=ffBestValueDivisor min=1 max=39`
  (Mitte = 20; die UCI-Grenze 41 ist weiter als nötig, das ist erlaubt), ~2000 Samples,
  `test/clop/clop-standard.ini`. Runden, als Default eintragen, Flag zurück auf false. Landet
  CLOP bei 20: Eintrag ohne SPRT erledigt.
- **Schritt 2 (SPRT):** committen, taggen, clean + rebuild, EPD-Lauf muss andere Nodes zeigen.
  `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffBestValueDivisor.state`.
- **Entscheidung:** H1 → Wert bleibt. H0 → Commit nach `dead/ffBestValueDivisor`, Default zurück
  auf 20. Eintrag in `plan/version-log.md` als eigener Commit.

### 3. „No pruning on silent TT move“ in forewardFutility — SPRT (Ausnahme entfernen?)

- **Was:** `forewardFutility()` (`search/search-node.h`, Zeile ~273) prunt nicht, wenn ein
  stiller (nicht schlagender) TT-Zug vorliegt:
  `if (!getTTMove().isEmpty() && !getTTMove().isCapture()) return false;`
  Begründung im Kommentar: stille TT-Züge gibt es nur, wenn die Stellung schon im Fenster war.
  Frage: bringt diese Ausnahme etwas oder kann sie weg?
- **Kein CLOP** (boolesche Entscheidung), nur SPRT.
- **Reihenfolge:** nach Nr. 1 und 2; Baseline ist die Version nach deren Entscheidungen.
- **Schritt 1:** Zeile entfernen (Kommentar mit), committen, taggen, clean + rebuild. EPD-Lauf muss
  andere Nodes zeigen (Code ist erreicht).
- **Schritt 2 (SPRT):** `test/sprt/sprt-standard.ini` mit `--each tc=20+0.1` (vor den
  `--engine`-Blöcken) und `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-ffSilentTTMove.state`.
  Kandidat = Version *ohne* die Ausnahme.
- **Entscheidung:** H1 → Ausnahme bleibt entfernt. H0 → Ausnahme bleibt drin (Commit nach
  `dead/ffSilentTTMove`, revert), Kommentar im Code um das SPRT-Ergebnis ergänzen. Eintrag in
  `plan/version-log.md` als eigener Commit.

### 4. forewardFutility2 (Fail-Low-Pruning) aktivieren — CLOP, dann SPRT

- **Was:** `forewardFutility2()` in `search/search-node.h` ist die Fail-Low-Spiegelung von
  `forewardFutility()`: prunt, wenn `adjustedEval + margin <= alpha`, mit
  `bestValue = alpha - (alpha - adjustedEval) * 10 / ffBestValueDivisor2`. Die Methode ist
  über ein `return false;` in der ersten Zeile totgelegt; aufgerufen wird sie in
  `search.cpp` (`checkEvalReleatedCutoffsAndSetEval`, `forewardFutility(...) || forewardFutility2(...)`).
  Logik am 2026-09-17 geprüft und korrigiert (Vorzeichen bestValue, Guards); Stand uncommitted.
- **Reihenfolge:** nach Nr. 1–3; Baseline ist die Version nach deren Entscheidungen.
- **Schritt 1:** `return false;` entfernen und `constexpr bool OPT = false;` durch
  `SearchConfig::optimizeFutility` ersetzen (eigenes Flag ist nicht nötig, Nr. 1–2 sind dann
  durch). Für den CLOP-Lauf Flag auf true (nicht committen), `make Release -j`, Optionen
  `ffDepthFactor2`, `ffBase2`, `ffImprovingBonus2`, `ffBestValueDivisor2` per `uci` prüfen.
  EPD-Lauf muss andere Nodes zeigen als die Baseline (Code ist erreicht).
- **Schritt 2 (CLOP):** alle vier Parameter in einem Lauf, den Divisor eingeschlossen (sein
  Zähler 10 ist eine Konstante und läuft nicht mit, es kann sich also nichts wegkürzen):
  `--clopvalue name=ffDepthFactor2 min=0 max=200 --clopvalue name=ffBase2 min=0 max=200 --clopvalue name=ffImprovingBonus2 min=0 max=200 --clopvalue name=ffBestValueDivisor2 min=1 max=39`
  (Mitten 100/100/100/20), ~4000 Samples, `test/clop/clop-standard.ini`. Runden, als Defaults
  eintragen, Flag zurück auf false. Das Tiefenlimit `10 <= remainingDepth` bleibt in diesem Lauf
  fest.
- **Schritt 3 (SPRT):** committen, taggen, clean + rebuild. `test/sprt/sprt-standard.ini` mit
  `--each tc=20+0.1` (vor den `--engine`-Blöcken) und
  `--sprt eloh0=-3 eloh1=2 file=test/log/sprt-forewardFutility2.state`. Kandidat = Version mit
  aktiver Methode und getunten Werten.
- **Danach, unabhängig vom Ausgang:** das `return false;` wieder an den Anfang der Methode
  setzen (die Methode bleibt vorerst tot). Die getunten Werte bleiben als Defaults im Code stehen,
  das SPRT-Ergebnis kommt als Kommentar an die Methode. Eintrag in `plan/version-log.md` als
  eigener Commit.
- **Folgeaufgabe bei H1:** `forewardFutility2` mit `forewardFutility` zusammenführen — mehrere
  Guards sind identisch (Tiefenlimit, PV-Node, stiller TT-Zug). Erst dann wird die Fail-Low-Seite
  wirklich aktiv; die Zusammenführung bekommt einen eigenen Eintrag und eine eigene SPRT.
  Bei H0: Commit nach `dead/forewardFutility2`, kein Merge.

## Erledigt

(noch nichts)
