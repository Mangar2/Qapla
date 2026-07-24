# Arbeitsliste: Umsetzung der Befunde aus todo-logikfehler-eval-search.md

Jeder Schritt ist **vereinzelt** (eine Änderung = ein Schritt = ein Test) und durchläuft
den unten definierten Workflow. Grundlage: [todo-logikfehler-eval-search.md](todo-logikfehler-eval-search.md).

---

## Workflow (gilt für jeden Schritt)

**Basiszweig (MAIN):** `release0.4` (aktueller Entwicklungszweig).
**Test-Versions-Tag:** aktuell **27**; der nächste erfolgreiche Schritt bekommt **28**, dann fortlaufend.
**Sammelbranch für unklare Gewinne:** `tv-unclear` (einer für alle; wird von MAIN abgezweigt, sobald der erste unklare Fall auftritt, und danach immer auf den aktuellen MAIN-Stand rebased, bevor ein weiterer unklarer Schritt aufgesetzt wird).

1. **Vorbereitung (einmalig):** aktuellen MAIN-Stand bauen (`make Release`) und als Baseline sichern:
   `cp build/Release/Qapla test/engines/Qapla-baseline`
2. **Implementieren:** Änderung des Schritts auf MAIN umsetzen, `make Release`.
3. **CLOP (nur wenn der Schritt Parameter hat):**
   - `make ReleaseOpt` (Parameter sind nur im PARAM_OPTIMIZE-Build UCI-Optionen; Namen prüfen mit `echo uci | build/ReleaseOpt/Qapla`).
   - `~/bin/qet --settingsfile=test/clop/clop-standard.ini --clop samples=<N> --clopvalue name=<Opt> min=<min> max=<max> [...]`
   - Samples: 1 Parameter → 1000, 2 Parameter → 2000–3000, ≥3 Parameter → 5000.
   - Gefundenes Optimum als neuen Default in den Code übernehmen, `make Release`.
4. **SPRT (immer):**
   `~/bin/qet --settingsfile=test/sprt/sprt-standard.ini --sprt file=test/log/sprt-<ID>.state`
   (H0=0, H1=3 Elo, α=β=0.05, max. 10000 Games, tc=20+0.01, concurrency=10, rapid=true, Engine-Logging aus — alles im Settingsfile.)
5. **Entscheidung:**
   - **H1 accepted** → Änderung bleibt in MAIN. Tag vergeben: `git tag tv<N>` (N = nächste Nummer, beginnend mit 28); der Engine-Name übernimmt das Tag automatisch beim nächsten Build (Makefile: `git describe --tags`). Neu bauen und Baseline aktualisieren: `cp build/Release/Qapla test/engines/Qapla-baseline`. Schritt hier mit ✅ tv\<N\> markieren.
   - **H0 accepted** oder **keine Entscheidung mit Score ≤ 50,2 %** → Änderung zurückbauen (`git revert`/reset), Schritt hier mit ❌ verworfen markieren.
   - **Keine Entscheidung, Score > 50,2 %** → Änderung per cherry-pick auf `tv-unclear` bringen, aus MAIN zurückbauen, Schritt hier mit ➕ Sammelbranch markieren. MAIN bleibt unverändert.
6. **Abschluss (Schritt F1, ganz am Ende):** `tv-unclear` (alle unklaren Änderungen zusammen) gegen MAIN:
   `~/bin/qet --settingsfile=test/sprt/sprt-standard.ini --sprt file=test/log/sprt-final-unclear.state eloh1=5 maxgames=20000`
   H1 accepted (≥ 5 Elo) → Branch komplett nach MAIN mergen (+ Tag); sonst verwerfen.

**Wichtig bei baumformändernden Schritten (A-, B-, E-Gruppe):** Nach einem Accept prüfen, ob Nachtunen der Margins sinnvoll ist (Schritte A5, E4) — Gewichte wurden mit den alten Fehlern mitgetunt.

Status-Legende: ☐ offen · 🔄 in Arbeit · ✅ tv\<N\> in MAIN · ❌ verworfen · ➕ Sammelbranch

---

## Gruppe S: Infrastruktur-Voraussetzungen (kein SPRT nötig, kein Spielstärke-Einfluss)

### ☐ S1 — UCI-Export für Suchparameter im ReleaseOpt-Build
Die Suchparameter (`semc`, `semf`, `fut`, `ffut`) leben in `SearchParameter::parameters` und sind nur per Kommandozeile setzbar ([searchparameter.h:39-50](../search/searchparameter.h#L39-L50)); CLOP braucht sie aber als UCI-Optionen. Einen `UciParameterProvider` für die SearchParameter-Map ergänzen und in `collectUciProviders()` ([uci.cpp:51-65](../interface/uci.cpp#L51-L65)) registrieren (nur `#ifdef PARAM_OPTIMIZE`). Voraussetzung für A5, E2, E4.

### ☐ S2 — Tuning-Feature-Vektor reparieren (Befund 2.5)
Frühes `return` in `computeIndexVector()` entfernen ([eval.cpp:72](../eval/eval.cpp#L72)). Einzeiler, betrifft nur den Trainingspfad.

### ☐ S3 — Symmetrie-Selbsttest reparieren (Befund 2.6)
`assertSymetry`: drittes Argument ist `ply`, nicht `alpha` — `-MAX_VALUE` durch `0` ersetzen ([eval.h:49](../eval/eval.h#L49)); Aufruf in Debug-Builds aktivieren ([quiescencese.cpp:111](../search/quiescencese.cpp#L111)).

### ☐ S4 — EPD-Regressionssuite für farbsymmetrische Eval-Fehler
Kleine EPD-Suite mit Materialungleichgewichts-Stellungen (Q vs R+B, OCB-Remis-Stellungen) in `tests/` anlegen; Prüfung per `~/bin/qet --epd file=...`. Fängt die Fehlerklasse, die `assertSymetry` prinzipiell nicht sehen kann (Befund 2.1).

---

## Gruppe A: SE-/TT-Gate-Komplex (Befunde 1.1(b/c), 1.2) — höchste Priorität

### ☐ A1 — SE-Gate-Inversion beheben
`ttValueIsUpperBound` aus `entry.isLessOrEqualAlpha()` statt `entry.isGreaterOrEqualBeta()` speisen ([searchvariables.h:194](../search/searchvariables.h#L194)). Wirkung: SE läuft wieder bei Lower-Bound/EXACT-TT-Einträgen (Standard-Kandidaten) und wird bei Upper-Bounds übersprungen.
**Parameter:** keine → **SPRT:** `sprt-A1.state`

### ☐ A2 — SE-Ergebnis an Nicht-PV-Knoten verwenden
`extendSearch` nicht mehr an `TYPE == PV` binden, sondern die `seExtension` an allen Knotentypen anwenden ([search.cpp:424](../search/search.cpp#L424)); Check-/PP-Anteile von `calculateExtension` dabei zunächst unverändert PV-only lassen (eigene Schritte B1/B2), d. h. Extension-Berechnung aufteilen: SE-Anteil überall, Rest wie bisher.
**Abhängigkeit:** nach A1. **Parameter:** keine → **SPRT:** `sprt-A2.state`

### ☐ A3 — Fallback: `se()`-Aufruf an Nicht-PV-Knoten streichen
**Nur ausführen, wenn A2 ❌ ist:** Wenn die SE-Extension außerhalb der PV nichts bringt, dann den unnötigen `se()`-Aufruf (teure Subsuche mit verworfenem Ergebnis) an Nicht-PV-Knoten entfernen ([search.cpp:405](../search/search.cpp#L405)) — reiner Speedup.
**Parameter:** keine → **SPRT:** `sprt-A3.state`

### ☐ A4 — Futility-Guard im Hauptpfad beleben
`ttValueIsUpperBound` in `negaMax` nach `setFromParentNode` erneut aus dem TT-Eintrag setzen (Reihenfolge-Bug: [searchvariables.h:101](../search/searchvariables.h#L101) vs. [search.cpp:395/412](../search/search.cpp#L395)), damit `if (ttValueIsUpperBound) return false;` in `forewardFutility` ([searchvariables.h:266](../search/searchvariables.h#L266)) wieder greift.
**Abhängigkeit:** nach A1 (korrekte Semantik). **Parameter:** keine → **SPRT:** `sprt-A4.state`

### ☐ A5 — SE-Margin nachtunen
Nach Abschluss von A1/A2: `semc`/`semf` ([searchparameter.h:144-149](../search/searchparameter.h#L144-L149)) neu optimieren.
**Abhängigkeit:** S1, A1, A2 entschieden. **Parameter:** 2 → **CLOP:** `--clop samples=3000 --clopvalue name=semc min=0 max=60 --clopvalue name=semf min=0 max=12`, dann **SPRT:** `sprt-A5.state`

---

## Gruppe B: Extensions einzeln (Befund 1.1(a)) — je Extension-Typ ein Schritt

### ☐ B1 — Check-Extension an Nicht-PV-Knoten
Check-Anteil von `calculateExtension` auch an INNER/NEAR_LEAF anwenden. Vorzeichen offen (Kompensationen: Schachzüge sind bereits von LMR/Pruning ausgenommen, Evasion-Qsearch). Ergebnisoffen testen.
**Abhängigkeit:** nach A2 (saubere Trennung der Extension-Anteile). **Parameter:** keine → **SPRT:** `sprt-B1.state`

### ☐ B2 — Passed-Pawn-Extension aktivieren (PV)
`DO_PASSED_PAWN_EXTENSIONS = true` ([searchparameter.h:151](../search/searchparameter.h#L151)); vorher zwingend den Debug-`std::cout` aus [extension.h:41-44](../search/extension.h#L41-L44) entfernen (UCI-Protokoll-Verschmutzung).
**Parameter:** keine (Ziel-Ränge fest lassen) → **SPRT:** `sprt-B2.state`

### ☐ B3 — Passed-Pawn-Extension an Nicht-PV-Knoten
**Nur wenn B2 ✅:** analog B1 für den PP-Anteil.
**Parameter:** keine → **SPRT:** `sprt-B3.state`

---

## Gruppe C: King-Attack (Befund 2.1)

### ☐ C1 — Damen-Term auf die richtige Seite stellen
`QUEEN + COLOR` → `QUEEN + OPPONENT` in [king-attack.h:170](../eval/king-attack.h#L170). Einzeiler.
**Parameter:** keine → **SPRT:** `sprt-C1.state`

### ☐ C2 — queenFactor nachtunen
Nach C1-Entscheidung: `queenFactor` (Default 2) neu optimieren, da die `attackWeight`-Tabelle mit dem Fehler mitgetunt wurde. UCI-Name im ReleaseOpt-Build prüfen (KingAttack-Provider).
**Abhängigkeit:** C1. **Parameter:** 1 → **CLOP:** `--clop samples=1000 --clopvalue name=<queenFactor-Option> min=0 max=6`, dann **SPRT:** `sprt-C2.state`

---

## Gruppe D: Endspiel / OCB (Befund 2.2)

### ☐ D1 — Handgeschriebene OCB-Skalierung
Remis-Skalierung für ungleichfarbige Läufer in `EvalEndgame` registrieren (Pattern `KBP*KBP*`): bei reinem OCB den Bewertungsüberschuss mit Faktor `ocbScalePercent` skalieren. Parameter als UCI-Option im ReleaseOpt-Build anlegen.
**Parameter:** 1 → **CLOP:** `--clop samples=1000 --clopvalue name=ocbScalePercent min=20 max=90`, dann **SPRT:** `sprt-D1.state`

### ☐ D2 — OCB-Skalierung mit zusätzlichen Figuren
**Nur wenn D1 ✅:** abgeschwächte Skalierung, wenn neben den ungleichfarbigen Läufern weitere Figuren (z. B. je ein Turm) auf dem Brett sind.
**Parameter:** 1 → **CLOP:** `--clop samples=1000 --clopvalue name=ocbWithPiecesPercent min=60 max=100`, dann **SPRT:** `sprt-D2.state`

### ☐ D3 — Signatur-Lookups verdrahten (WIP-Commit 48ec188 fertigstellen)
`registerSym`-Blöcke in [evalendgame.cpp:48-114](../eval/evalendgame.cpp#L48-L114) aktivieren, `piece-signature-def.h` einbinden. Großer Schritt, nie im Spielbetrieb getestet — separat und zuletzt in dieser Gruppe.
**Parameter:** keine (Tabellen sind fix) → **SPRT:** `sprt-D3.state`

### ☐ D4 — eval-correction.h anschließen
Include in [eval.cpp:33](../eval/eval.cpp#L33) aktivieren und Korrektur in `lazyEval` anwenden. Eigener Schritt, unabhängig von D3 testen.
**Parameter:** keine → **SPRT:** `sprt-D4.state`

---

## Gruppe E: Promotions & Move-Ordering (Befunde 1.4, N2, N3)

### ☐ E1 — Promotions von LMR/Move-Count-Pruning ausnehmen
`if (move.isCapture() || move.isPromote()) return 0;` in `computeLMR` ([search.cpp:205](../search/search.cpp#L205)). Einzeiler.
**Parameter:** keine → **SPRT:** `sprt-E1.state`

### ☐ E2 — Ordering-Boost für Damen-Umwandlungen
In `computeCaptureWeight` ([moveprovider.h:284-291](../search/moveprovider.h#L284-L291)) Umwandlungen mit `promoBoost` gewichten (Skala an `getPieceValueForMoveSorting` orientieren; wirkt auch in der Quiescence).
**Abhängigkeit:** S1 (falls Boost als Suchparameter exportiert wird). **Parameter:** 1 → **CLOP:** `--clop samples=1000 --clopvalue name=promoBoost min=200 max=1200`, dann **SPRT:** `sprt-E2.state`

### ☐ E3 — Verlierende Schlagzüge hinter Killer/Quiets
Eigene Stage `BAD_CAPTURES` nach `KILLER2`/`SORT_MOVES` in [moveprovider.h](../search/moveprovider.h) (aktuell werden sie am Ende der GOOD_CAPTURES-Stage geliefert, [moveprovider.h:329-345](../search/moveprovider.h#L329-L345)).
**Parameter:** keine → **SPRT:** `sprt-E3.state`

### ☐ E4 — Futility-Margins nachtunen
Nach Abschluss der Gruppen A/B/E: `fut`/`ffut` ([searchparameter.h:155-168](../search/searchparameter.h#L155-L168)) neu optimieren.
**Abhängigkeit:** S1; A/B/E entschieden. **Parameter:** 2 → **CLOP:** `--clop samples=3000 --clopvalue name=fut min=25 max=150 --clopvalue name=ffut min=25 max=150`, dann **SPRT:** `sprt-E4.state`

---

## Gruppe Q: Quiescence (Befunde 1.3, 1.5)

### ☐ Q1 — TT-Zug-Stage in der Quiescence aktivieren
`selectStage = MoveType::PV` in `computeCaptures`/`computeEvades` setzen, wenn TT-Zug vorhanden (Capture-Pfad: nur wenn der TT-Zug in der Non-Silent-Liste liegt); Nicht-Schach-Pfad von `selectNextCapture()` auf den Stage-Selektor umstellen ([moveprovider.h:140-156](../search/moveprovider.h#L140-L156), [quiescencese.cpp:95,123](../search/quiescencese.cpp#L95)).
**Parameter:** keine → **SPRT:** `sprt-Q1.state`

### ☐ Q2 — TT-Kontrollfluss reparieren (Stand-Pat-Verfeinerung)
Frühen TT-Return an `alpha + 1 == beta` binden (auskommentierten Guard in [quiescencese.cpp:96](../search/quiescencese.cpp#L96) reaktivieren) — dann arbeitet der bisher tote Verfeinerungsblock (Z. 105–109) wieder.
**Parameter:** keine → **SPRT:** `sprt-Q2.state`

---

## Gruppe K: Eval-Features (Befunde 2.3, 2.4)

### ☐ K1 — Bauernschild aktivieren (bestehende Gewichte)
`computePawnShieldValue` in `KingAttack::eval()`/`computeAttackValue` einbeziehen ([king-attack.h:117-123](../eval/king-attack.h#L117-L123)).
**Parameter:** keine (erst mit Ist-Gewichten testen) → **SPRT:** `sprt-K1.state`

### ☐ K2 — Bauernschild-Gewichte tunen
**Nur wenn K1 ✅ oder ➕:** die 8 `pawnIndexFactor`-Werte ([king-attack.h:286](../eval/king-attack.h#L286)) als UCI-Optionen exportieren und optimieren.
**Parameter:** 8 → **CLOP:** `--clop samples=5000` mit 8 `--clopvalue`-Gruppen (min=-40 max=40), dann **SPRT:** `sprt-K2.state`

### ☐ K3 — Space-Gewicht tunen
`spaceWeightMg` (Default 0 = Feature aus) optimieren; UCI-Option existiert bereits im ReleaseOpt-Build ([space.cpp:40](../eval/space.cpp#L40)). Bei ❌: Space-Code als toten Code entfernen (eigener Aufräum-Commit ohne SPRT).
**Parameter:** 1 → **CLOP:** `--clop samples=1000 --clopvalue name=spaceWeightMg min=0 max=100`, dann **SPRT:** `sprt-K3.state`

---

## Gruppe R: Robustheit / Kleinigkeiten (Befunde N1, 1.6, 1.7, 1.8, 1.9)

### ☐ R1 — Aspiration-Delta-Reihenfolge fixen
Delta vor der Zuweisung berechnen ([aspirationwindow.h:98-99](../search/aspirationwindow.h#L98-L99)). Einzeiler.
**Parameter:** keine → **SPRT:** `sprt-R1.state`

### ☐ R2 — Zeitcheck in der Quiescence
Node-Counter-basierter `isSearchStopped()`-Check in `Quiescence::search` (z. B. alle 1024 Knoten). Intervall fest, kein Tuning.
**Parameter:** keine → **SPRT:** `sprt-R2.state` (Absicherung gegen Regression)

### ☐ R3 — SEE: nur den ziehenden Stein entfernen
In `tryPiece` nur das LSB aus `allPiecesLeft` entfernen statt des ganzen Typ-Bitboards ([see.h:333](../search/see.h#L333)).
**Parameter:** keine → **SPRT:** `sprt-R3.state`

### ☐ R4 — Remis-Marker im TT-Replacement schützen
`isMaxDephtEntry()`-Schutz in `isNewBetterForPrimary`/Secondary-Pfad ([ttentry.h:228-261](../search/ttentry.h#L228-L261), [tt.h:105-141](../search/tt.h#L105-L141)).
**Parameter:** keine → **SPRT:** `sprt-R4.state`

### ☐ R5 — Sofortiges Aspiration-Widening bei Root-Fail-Low
Bei Fail-Low des ersten Root-Zugs Iteration abbrechen und sofort mit geweitetem Fenster neu suchen ([search.cpp:541-588](../search/search.cpp#L541-L588)). Ergebnisoffen (aktuelles Verhalten ist verteidigbar).
**Parameter:** keine → **SPRT:** `sprt-R5.state`

---

## Gruppe F: Finale

### ☐ F1 — Sammeltest des Unclear-Branches
Alle ➕-Schritte liegen gemeinsam auf `tv-unclear`. Test gegen aktuellen MAIN:
`~/bin/qet --settingsfile=test/sprt/sprt-standard.ini --sprt file=test/log/sprt-final-unclear.state eloh1=5 maxgames=20000`
**H1 accepted (≥ 5 Elo):** Branch nach MAIN mergen + nächstes Tag. **Sonst:** Branch verwerfen.

---

## Empfohlene Bearbeitungsreihenfolge

S1 → S2 → S3 → S4 (Infrastruktur, parallelisierbar) →
A1 → A2 (→ ggf. A3) → A4 → A5 →
C1 → C2 →
E1 → E2 → E3 →
D1 → D2 →
Q1 → Q2 →
K1 → K2 → K3 →
B1 → B2 → B3 →
R1 → R2 → R3 → R4 → R5 →
E4 (Margins zuletzt, wenn die Baumform steht) →
D3 → D4 (große Endspiel-Schritte) →
F1 (Finale).

## Ergebnisprotokoll

| Schritt | Datum | SPRT-Ergebnis | Score | Entscheidung | Tag |
|---------|-------|---------------|-------|--------------|-----|
| — | — | — | — | — | — |
