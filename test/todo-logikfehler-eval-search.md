# To-do: Logikfehler und Lücken in `eval/` und `search/`

Ergebnis einer Code-Analyse vom 23.07.2026. Sortiert nach geschätztem ELO-Gewinn.
Konfidenz: **sicher** = konkreter Fehler nachvollzogen; **plausibel** = sieht falsch aus, hängt aber von nicht vollständig verfolgten Invarianten ab.

---

## Teil 1: Evaluation (`eval/`)

### 1.1 King-Attack-Rabatt prüft die Dame der falschen Seite
- **Datei:** `eval/king-attack.h:170`
- **Befund:** In `computeAttackValue<COLOR>` ist `COLOR` die Farbe des *verteidigenden* Königs, der Angreifer ist `OPPONENT`. Der Term `(position.getPieceBB(QUEEN + COLOR) != 0) * queenFactor` prüft aber die Dame des **Verteidigers** statt die des **Angreifers**. Beabsichtigt war (lt. Commit `bac2e8e`): Königsangriff abschwächen, wenn der Angreifer keine Dame mehr hat. Effekt: Nach einem einseitigen Damentausch wird die Damen-Präsenz der falschen Seite gutgeschrieben – der gesamte King-Safety-Term ist in Stellungen mit asymmetrischer Damenzahl verzerrt.
- **Fix:** `QUEEN + COLOR` → `QUEEN + OPPONENT`.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~15–30

### 1.2 Trainierte Endspiel-/Material-Signatur-Korrekturen sind nicht angeschlossen (toter Code)
- **Dateien:** `eval/eval-correction.h` (nirgends eingebunden; `eval/eval.cpp:33` ist das `#include` auskommentiert), `eval/piece-signature-def.h` (nirgends eingebunden), `eval/evalendgame.cpp:~48–130` (`registerSym(...)`-Aufrufe komplett auskommentiert)
- **Befund:** Zwei große, fertig implementierte Wissensblöcke laufen nie: (a) die ~3000-Einträge-Korrekturtabelle pro Materialsignatur, (b) ~90 empirische Gewinnwahrscheinlichkeits-Tabellen (KBKB, KRKB, KQKR, Turm-gegen-Leichtfigur-Ungleichgewichte …). Folge: Es gibt **keinerlei aktive Skalierung für ungleichfarbige Läufer** und keine feinen Korrekturen für gängige Materialungleichgewichte. Stammt aus WIP-Commit `48ec188` („not finished“).
- **Fix:** Verdrahtung fertigstellen und per Selbstspiel testen (explizit als unfertig markiert – vor Aktivierung validieren).
- **Konfidenz:** sicher (dass der Code tot ist); plausibel (dass Aktivierung ELO bringt)
- **ELO-Schätzung:** ~15–40 (größte einzelne Lücke: Remistendenz ungleichfarbiger Läufer)

### 1.3 Bauernschild vor dem König wird berechnet, aber nie aufgerufen
- **Datei:** `eval/king-attack.h:100–123` (`computePawnShieldIndex`/`computePawnShieldValue`), Gewichtstabelle `pawnIndexFactor` bei Zeile 286
- **Befund:** Weder `KingAttack::eval()` noch `evalWithDetails()` rufen die Pawn-Shield-Funktionen auf; `King::eval()` enthält nur König-PST und einen Endspiel-Distanzterm. Der klassische Term „Bauernschild vor dem rochierten König“ trägt damit nichts zur Bewertung bei.
- **Fix:** Pawn-Shield-Wert in `KingAttack::eval()` einbeziehen (Gewichte ggf. neu tunen).
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~10–25

### 1.4 Raumbewertung durch Default-Gewicht 0 komplett neutralisiert
- **Dateien:** `eval/space.cpp:80`, `eval/space.h:61`
- **Befund:** `SPACE_WEIGHT_MG_DEFAULT = 0`; in Nicht-`PARAM_OPTIMIZE_SPACE`-Builds ist `SPACE_WEIGHT_MG` constexpr an diesen Default gebunden. `Space::eval()` liefert daher immer `EvalValue(0,0)` – das vollständig implementierte Space-Subsystem ist in Produktion wirkungslos.
- **Fix:** Gewicht per SPSA/Selbstspiel tunen und einen sinnvollen Default setzen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~5–15

### 1.5 `computeIndexVector()` bricht nach dem ersten Feature ab (Tuning-Pfad defekt)
- **Datei:** `eval/eval.cpp:68–73`
- **Befund:** Frühes `return` nach dem ersten `push_back` – der gesamte restliche Feature-Vektor (Material, PST, Bauernstruktur, Königsangriff, Drohungen …) ist unerreichbarer Code. Betrifft nicht die Spielstärke direkt, aber der Tuning-/Trainingspfad (`search/boardadapter.h:440`) sieht nur noch ein einziges Feature pro Stellung – gradientenbasiertes Nachtunen der Gewichte ist damit faktisch lahmgelegt.
- **Fix:** Frühes `return` entfernen (Einzeiler).
- **Konfidenz:** sicher
- **ELO-Schätzung:** 0 direkt; indirekt hoch (Voraussetzung für künftiges Tuning)

### 1.6 Symmetrie-Selbsttest inaktiv und mit Parameterfehler
- **Datei:** `eval/eval.h:46–57` (`assertSymetry`), einziger Aufrufer `search/quiescencese.cpp:111` auskommentiert
- **Befund:** Das Sicherheitsnetz gegen genau die Fehlerklasse aus 1.1 (asymmetrische Eval) läuft nirgends. Zusätzlich übergibt die Funktion `-MAX_VALUE` als drittes Positionsargument – das ist `ply`, nicht `alpha` – und würde bei Reaktivierung Mate-Score-Anpassungen verfälschen.
- **Fix:** Parameter korrigieren, Aufruf in Debug-Builds aktivieren.
- **Konfidenz:** sicher
- **ELO-Schätzung:** 0 direkt; verhindert künftige Regressionen

### Geprüft und in Ordnung befunden
Farb-Spiegelung in `pawn.h`, Springer-Outpost-Masken beider Farben, Turm auf offener Linie / 7. Reihe, Läuferpaar-Bonus, Phasen-Interpolation (`EvalValue::getValue`), aktive Endspielfunktionen in `evalendgame.cpp` (KBNK, KBBK, KPsK, KQKR, drawValue-Registrierungen).

---

## Teil 2: Suche (`search/`)

### 2.1 Extensions (Schach/Singular/Freibauer) wirken nur an PV-Knoten
- **Datei:** `search/search.cpp:424` (einzige Aufrufstelle von `extendSearch`), `search.cpp:405`
- **Befund:** `depth = node.extendSearch(...)` wird nur bei `TYPE == SearchRegion::PV` ausgeführt. Da PV nur entlang der Erstzug-Kette propagiert, erhält der weitaus größte Teil des Baums (alle Null-Window-/Scout-Suchen in `INNER`/`NEAR_LEAF`) **keine** Schach-, Singular- oder Freibauer-Extensions – Schachfolgen werden dort systematisch zu flach gesucht, und die Scout-Bounds, die die PVS-Re-Search-Entscheidung steuern, sind unzuverlässig. Zusätzlich wird `se()` (die teure Singular-Extension-Subsuche) bei `search.cpp:405` **immer** ausgeführt, das Ergebnis aber an Nicht-PV-Knoten verworfen – verschwendete Rechenzeit plus fehlende Extension.
- **Fix:** `extendSearch` (mind. Check-Extension) auch an Nicht-PV-Knoten anwenden; `se()`-Aufruf und -Verwertung konsistent machen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~40–100+ (größter Einzelbefund der gesamten Analyse)

### 2.2 Stille Umwandlungen werden von LMR und Move-Count-Pruning nicht ausgenommen
- **Datei:** `search/search.cpp:197–212` (`computeLMR`), `429–444` (Aufrufstelle + `continue`-Pruning)
- **Befund:** `computeLMR` prüft nur `move.isCapture()`, nie `move.isPromote()`. Eine schlagfreie Umwandlung (Bauer zieht auf die 8. Reihe) ist weder Capture noch (zwingend) Schach und kann daher per LMR reduziert oder – da derselbe `lmr`-Wert das Move-Count-Pruning speist (`depth - lmr < 0` → `continue`) – ab Zug ~4 in der Sortierung **komplett weggeschnitten** werden. Das Futility-Pruning (`canPruneFutility`) berücksichtigt den Umwandlungswert korrekt, dieser Pfad nicht.
- **Fix:** `move.isPromote()` in `computeLMR` ausnehmen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~10–30 (selten, aber hochwertige Fälle: gewonnene Bauernendspiele)

### 2.3 Quiescence nutzt den TT-Zug nie zur Zugsortierung
- **Datei:** `search/quiescencese.cpp:95`, `search/moveprovider.h:53–59, 140–156`
- **Befund:** `setTTMove(ttMove)` wird zwar gesetzt, aber der lokale `MoveProvider` startet mit `selectStage = MoveType::PV + 1` (überspringt die TT-Zug-Stufe), und der Nicht-Schach-Pfad geht ohnehin über `selectNextCapture()`, das `_ttMove` nie konsultiert. Der TT-Zug ist in der Quiescence damit tot – ausgerechnet im knotenreichsten Teil der Suche fehlt die beste Sortierinformation.
- **Fix:** TT-Zug-Stufe im Quiescence-Pfad aktivieren.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~5–15

### 2.4 `ttValueIsUpperBound`-Guard für Futility/SE ist tot bzw. semantisch invertiert
- **Datei:** `search/searchvariables.h:101, 142, 194, 266`, `search/search.cpp:280`
- **Befund:** (a) `probeTT` setzt `ttValueIsUpperBound`, aber das danach laufende `setFromParentNode` setzt es bedingungslos auf `false` zurück – der Guard `if (ttValueIsUpperBound) return false;` im `forewardFutility` kann nie feuern. (b) Zudem wird das Flag aus `isGreaterOrEqualBeta()` (Lower-Bound-Indikator) gespeist statt aus `isLessOrEqualAlpha()` – das Gegenteil dessen, was Name und Verwendung nahelegen; das betrifft auch den SE-Pfad (`search.cpp:280`).
- **Fix:** Reihenfolge korrigieren (Flag nach `setFromParentNode` setzen) und Semantik auf `isLessOrEqualAlpha()` umstellen.
- **Konfidenz:** sicher (Reset-Reihenfolge); plausibel (Invertierung)
- **ELO-Schätzung:** ~5–15 (gelegentlich zu aggressives Futility-Pruning)

### 2.5 Kein Zeit-/Abbruch-Check innerhalb der Quiescence-Suche
- **Datei:** `search/quiescence.h`, `search/quiescencese.cpp` (gesamte Datei), Gate in `search/search.cpp:351–356`
- **Befund:** `emergencyAbort()` wird bei `TYPE == NEAR_LEAF` übersprungen, und die Quiescence selbst ruft den `ClockManager` nie auf; einzige Grenze ist `MAX_SEARCH_DEPTH`. Lange forcierte Schach-/Schlagfolgen (bei Schach werden volle Evasion-Zuglisten generiert) können die Zeitkontrolle deutlich überziehen.
- **Fix:** Periodischen Abort-Check (z. B. Node-Counter-basiert) in die Quiescence einbauen.
- **Konfidenz:** sicher (Code-Fakt); Auswirkung situativ
- **ELO-Schätzung:** gering direkt, aber Zeitüberschreitungs-/Zeitnot-Risiko (korrektheitsrelevant)

### 2.6 SEE entfernt alle gleichartigen Angreifer auf einmal statt einen pro Tauschschritt
- **Datei:** `search/see.h:328–337`
- **Befund:** `allPiecesLeft &= ~pieceToTryBitBoard[COLOR]` entfernt **alle** Angreifer eines Figurentyps (z. B. beide Springer), obwohl nur einer zieht. Steht einer der noch nicht gezogenen Steine auf einer Linie zwischen eigenem Slider und Zielfeld, öffnet das verfrüht eine Röntgenlinie → falscher SEE-Wert in diesem Randfall. Betrifft Capture-Sortierung und Qsearch-Pruning (`isLoosingCaptureLight`).
- **Fix:** Nur den tatsächlich ziehenden Stein aus `allPiecesLeft` entfernen.
- **Konfidenz:** plausibel (Mechanismus nachvollzogen, konkrete Fehlstellung nicht konstruiert)
- **ELO-Schätzung:** ~5–15

### 2.7 Toter Code: TT-Bound-Verfeinerung des Stand-Pat in der Quiescence unerreichbar
- **Datei:** `search/quiescencese.cpp:93–109`
- **Befund:** Das frühe `return ttValue` bei `ttValue != NO_VALUE` macht den späteren Block (`bestValue = standPatValue = ttValue` unter Bound-Prüfung) unerreichbar – vermutlich Folge des auskommentierten `alpha+1==beta`-Guards. Die beabsichtigte Stand-Pat-Verfeinerung per TT-Bound läuft nie.
- **Fix:** Kontrollfluss klären: entweder frühen Return an Null-Window binden oder den Verfeinerungsblock entfernen/reparieren.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~5–10

### 2.8 Root-Schleife sucht nach Fail-Low des ersten Zugs sinnlos weiter
- **Datei:** `search/search.cpp` (`negaMaxRoot`, ~541–588), `search/iterativedeepening.h`
- **Befund:** Abbruch nur bei `isFailHigh()`. Failt Zug 0 gegen das Aspirationsfenster low, wird die Iteration vom äußeren Loop ohnehin mit geweitetem Fenster komplett wiederholt – das Weitersuchen der Züge 1..N mit ungültigem Alpha ist reine Zeitverschwendung.
- **Fix:** Bei Root-Fail-Low sofort mit geweitetem Fenster neu starten.
- **Konfidenz:** plausibel (evtl. bewusste Toleranz)
- **ELO-Schätzung:** wenige ELO (nur Zeitmanagement)

### 2.9 Spielhistorien-Remis-Marker im TT werden von normalen Knoten überschrieben
- **Datei:** `search/ttentry.h:228–248` (`isNewBetterForPrimary`), `search/movehistory.h:101–122`
- **Befund:** Die vor der Suche einmalig gesetzten `MAX_DEPTH`/`isPV`-Marker für bereits gespielte Stellungen (Remis-Bias über Spielgrenzen hinweg) werden vom ersten normalen Nicht-PV-Knoten mit gleichem Hash überschrieben (`if (sameHash) return true;`) – und für den Rest der Suche nicht neu gesetzt. Kern-Remiserkennung (eigener Pfad, 50-Züge, Material) ist davon unabhängig und korrekt.
- **Fix:** Marker-Einträge vor Überschreiben schützen (z. B. Flag in `isNewBetterForPrimary` respektieren).
- **Konfidenz:** plausibel
- **ELO-Schätzung:** gering

### Geprüft und in Ordnung befunden
Mate-Score-Anpassung beim TT-Store/-Probe, Nullmove-Struktur (Zugzwang-Guard, Verifikationssuche, Fenster-/Tiefenübergabe), TT-Zug-Legalitätsprüfung gegen vollständige legale Zugliste (kein Illegal-Move-Risiko durch Hash-Kollisionen), PVS-Alpha/Beta-Negation.

---

## Priorisierte Reihenfolge (nach erwartetem ELO-Gewinn)

| # | Befund | ELO-Schätzung |
|---|--------|---------------|
| 1 | 2.1 Extensions nur an PV-Knoten | ~40–100+ |
| 2 | 1.1 King-Attack prüft falsche Dame | ~15–30 |
| 3 | 1.2 Endspiel-Signaturkorrekturen nicht angeschlossen | ~15–40 |
| 4 | 2.2 Stille Umwandlungen in LMR/LMP | ~10–30 |
| 5 | 1.3 Bauernschild nie aufgerufen | ~10–25 |
| 6 | 2.3 TT-Zug in Quiescence tot | ~5–15 |
| 7 | 2.4 `ttValueIsUpperBound` tot/invertiert | ~5–15 |
| 8 | 2.6 SEE-Röntgenlinien-Randfall | ~5–15 |
| 9 | 1.4 Space-Gewicht = 0 | ~5–15 |
| 10 | 2.7 Stand-Pat-TT-Verfeinerung unerreichbar | ~5–10 |
| 11 | 2.5 Kein Zeitcheck in Quiescence | Zeitnot-Risiko |
| 12 | 2.8 Root-Fail-Low sucht weiter | wenige ELO |
| 13 | 1.5 Tuning-Feature-Vektor defekt | indirekt |
| 14 | 1.6 Symmetrie-Selbsttest inaktiv/fehlerhaft | Absicherung |
| 15 | 2.9 Remis-Marker überschreibbar | gering |

*Hinweis: ELO-Schätzungen sind grobe Erfahrungswerte; jede Änderung sollte einzeln per Selbstspiel (SPRT) validiert werden. Die Schätzungen addieren sich nicht einfach.*
