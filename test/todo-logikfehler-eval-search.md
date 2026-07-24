# To-do: Logikfehler und Lücken in `eval/` und `search/` — vertiefte Analyse

Stand: 24.07.2026, 2. Durchgang. Jeder Befund wurde direkt am Code verifiziert (nicht nur
aus dem ersten Agenten-Durchlauf übernommen); pro Befund sind die **Wechselwirkungen** mit
anderen Codestellen analysiert, die den Effekt kompensieren oder verschärfen. Mehrere
Befunde des ersten Durchgangs wurden dabei korrigiert oder neu bewertet (markiert mit ⚠),
drei Befunde sind neu hinzugekommen (N1–N3).

Konfidenz: **sicher** = Mechanismus vollständig am Code nachvollzogen.
ELO-Angaben sind Erwartungswerte unter Vorbehalt: jede Änderung einzeln per SPRT testen;
Effekte addieren sich nicht.

---

## Teil 1: Suche (`search/`)

### 1.1 Extension-Komplex: Check-/Singular-Extensions wirken nur an PV-Knoten — differenzierte Neubewertung ⚠
- **Dateien:** [search.cpp:424](../search/search.cpp#L424) (einzige Aufrufstelle `extendSearch`), [search.cpp:405](../search/search.cpp#L405) (`se()` unconditional), [extension.h:35-48](../search/extension.h#L35-L48), [search.cpp:280](../search/search.cpp#L280) (SE-Gate, siehe 1.2)
- **Faktenlage (verifiziert):** `depth = node.extendSearch(...)` läuft nur bei `TYPE == SearchRegion::PV`. `se()` wird dagegen an *jedem* Knoten mit `depth >= 4` aufgerufen (Zeile 405); sein Ergebnis wird an Nicht-PV-Knoten verworfen.
- **Wechselwirkungen, die den fehlenden Check-Extension-Effekt an Nicht-PV-Knoten kompensieren** (das hatte der erste Durchgang ignoriert):
  1. Schachgebende Züge werden **nie** reduziert oder geprunt: `doMovePrunings = moveNumber > 3 && !node.isCheckMove(...)` ([search.cpp:429](../search/search.cpp#L429)) schaltet LMR, Move-Count- und Futility-Pruning für Schachzüge ab — an allen Knotentypen.
  2. Knoten **im Schach** überspringen Forward-Futility und Nullmove komplett (`checkEvalReleatedCutoffsAndSetEval` returnt bei `isInCheck()` vor beiden Cutoffs, [search.cpp:58-61](../search/search.cpp#L58-L61)); Nullmove hat zusätzlich einen eigenen In-Check-Guard.
  3. Am Horizont fängt die Quiescence Schachfolgen auf: `EVADES_CHECK_IN_QUIESCENSE = true`, im Schach werden **alle** Evasionen (nicht nur Schlagzüge) generiert und ohne Stand-Pat gesucht ([quiescencese.cpp:98-122](../search/quiescencese.cpp#L98-L122)).
  4. `se()` hat einen Doppel-Extension-Guard (`sideToMoveIsInCheck → return 0`, [search.cpp:266](../search/search.cpp#L266)).
- **Was wirklich übrig bleibt:**
  - (a) *Check-Extension nur in PV:* Forcierte Schachfolgen werden in Scout-Suchen effektiv 1 Ply flacher gesehen als in der PV. Das erzeugt systematische PV/Scout-Inkonsistenz (mehr Re-Search-Überraschungen, Suchinstabilität) und verzögert das Finden erzwungener Matt-/Gewinnfolgen in CUT-Knoten um Iterationen. Moderne Engines haben generische Check-Extensions aber weitgehend abgeschafft, weil die o.g. Kompensationen (kein Pruning von Schachzügen + Evasion-Qsearch) den Großteil abdecken. Erwartung: klein, Vorzeichen unklar ohne Test. **~0–20 ELO**, nicht 40–100.
  - (b) *Verschwendete `se()`-Suche an Nicht-PV-Knoten:* An jedem INNER-Knoten mit Tiefe ≥ 4, TT-Zug und passendem TT-Eintrag läuft eine echte Null-Window-Suche mit `depth/2` — deren einziges Produkt (die Extension) verworfen wird. Reiner Overhead (leicht gemildert dadurch, dass die SE-Suche TT/Killer füllt). **~5–10 ELO** durch Entfernen oder Verwerten.
  - (c) *SE-Extension an Nicht-PV-Knoten verwerten:* In modernen Engines wirken Singular Extensions gerade an CUT-Knoten. Zusammen mit dem Gate-Bug aus 1.2 ist SE hier de facto ein PV-only-Feature. Korrekt verdrahtet (Gate fixen + Ergebnis überall nutzen) ist das der wertvollste Teil des Komplexes: **~15–40 ELO**, zwingend per SPRT zu validieren.
- **Empfohlene Reihenfolge:** zuerst 1.2 (Gate-Inversion) fixen, dann `se()`-Ergebnis an allen Knotentypen verwenden (bzw. Aufruf an Nicht-PV-Knoten streichen, falls Test negativ), Check-Extension für Nicht-PV separat testen.
- **Konfidenz:** sicher (Codefakten); ELO-Wirkung testpflichtig.

### 1.2 SE-/Futility-Gate `ttValueIsUpperBound`: semantisch invertiert und im Hauptpfad zusätzlich tot — verschärft ⚠
- **Dateien:** [searchvariables.h:194](../search/searchvariables.h#L194) (Set), [searchvariables.h:101](../search/searchvariables.h#L101) (Reset), [searchvariables.h:266](../search/searchvariables.h#L266) (`forewardFutility`-Guard), [search.cpp:280](../search/search.cpp#L280) (SE-Gate)
- **Befund A — Inversion (live und schädlich in `se()`):** `probeTT` setzt `ttValueIsUpperBound = entry.isGreaterOrEqualBeta()` — das ist per Definition ([ttentry.h:171](../search/ttentry.h#L171)) ein **Lower**-Bound-Indikator. In `se()` (dort ist die Reihenfolge `setFromParentNode` → `probeTT` korrekt, das Flag also aktiv) bedeutet `if (node.ttValueIsUpperBound) return 0;` daher: **SE wird genau dann übersprungen, wenn der TT-Eintrag ein Fail-High (Wert ≥ beta) war** — also exakt bei den Standard-SE-Kandidaten (in Stockfish ist ein Lower-Bound-TT-Eintrag *Voraussetzung* für SE). Übrig bleiben EXACT-Einträge (an INNER-Knoten selten) und Upper-Bound-Einträge (deren TT-Zug meist ohnehin leer ist → `return 0` bei Zeile 288). Konsequenz: Singular Extensions feuern fast nur noch an PV-Knoten — konsistent mit Befund 1.1(c).
- **Befund B — toter Guard im Hauptpfad:** In `negaMax` läuft `probeTT` (Schritt 3, Zeile 395) **vor** `setFromParentNode` (Schritt 6, Zeile 412), das das Flag bedingungslos auf `false` zurücksetzt. Der Guard `if (ttValueIsUpperBound) return false;` in `forewardFutility` (aufgerufen aus Schritt 7) kann also nie feuern. Nach Korrektur der Semantik (echtes `isLessOrEqualAlpha`) wäre das ein sinnvoller Schutz: kein Reverse-Futility-Prune, wenn der TT sagt, der wahre Wert liegt ≤ alpha (statische Eval also zu optimistisch ist).
- **Fix:** Flag aus `isLessOrEqualAlpha()` speisen; in `negaMax` nach `setFromParentNode` erneut setzen (oder Reset entfernen); in `se()` Kondition auf „nur bei Lower-Bound/EXACT fortfahren" drehen.
- **Konfidenz:** sicher (beide Mechanismen vollständig verfolgt)
- **ELO-Schätzung:** Teil des SE-Komplexes aus 1.1(c) (~15–40 gemeinsam); der Futility-Guard allein ~3–8.

### 1.3 Quiescence nutzt den TT-Zug nie zur Zugsortierung — bestätigt
- **Dateien:** [quiescencese.cpp:95](../search/quiescencese.cpp#L95), [moveprovider.h:53-59](../search/moveprovider.h#L53-L59), [moveprovider.h:140-156](../search/moveprovider.h#L140-L156)
- **Verifizierter Mechanismus:** Der lokale `MoveProvider` startet per Konstruktor mit `selectStage = MoveType::PV + 1` (= `WEIGHT_CAPTURES`); weder `computeCaptures` noch `computeEvades` setzen die Stage zurück (nur `computeMoves` — der Hauptsuchpfad — tut das). Im Nicht-Schach-Fall läuft zusätzlich `selectNextCapture()`, das `_ttMove` gar nicht kennt. `setTTMove(ttMove)` ist damit in der Quiescence wirkungslos.
- **Wechselwirkung, die den Schaden begrenzt:** Der aggressive TT-Cutoff eine Zeile später ([quiescencese.cpp:96](../search/quiescencese.cpp#L96), `if (ttValue != NO_VALUE) return ttValue;`) beendet viele Qsearch-Knoten mit verwertbarem TT-Eintrag ohnehin sofort — der TT-Zug hätte nur in den Fällen Wert, in denen der Eintrag keinen Cutoff erlaubt (Bound passt nicht). Außerdem ist die Capture-Sortierung (MVV + Recapture-Bonus) bereits brauchbar.
- **Fix:** In `computeCaptures`/`computeEvades` `selectStage = MoveType::PV` setzen, wenn ein TT-Zug vorhanden ist (im Capture-Pfad zusätzlich prüfen, dass der TT-Zug in der Non-Silent-Liste liegt).
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~3–10 (Knoteneffizienz im knotenreichsten Suchteil, aber durch TT-Cutoff gedämpft)

### 1.4 Stille Damen-Umwandlungen können per Move-Count-Pruning verworfen werden — bestätigt, mit engerem Anwendungsfenster ⚠
- **Dateien:** [search.cpp:197-212](../search/search.cpp#L197-L212) (`computeLMR`: nur `isCapture()`-Ausnahme), [search.cpp:441](../search/search.cpp#L441) (Move-Count-Prune), [movelist.h:89-96](../basics/movelist.h#L89-L96)
- **Verifizierter Mechanismus mit Einschränkungen:** Eine schlagfreie Damen-Umwandlung ist als **non-silent** klassifiziert (`addPromote`: Damen-Umwandlung → `addNonSilentMove`) und wird daher früh in der Capture-Stage angeboten — allerdings mit Sortiergewicht 0 (`computeCaptureWeight` zählt nur das geschlagene Material), also **nach** allen echten Schlagzügen. In Stellungen mit ≥ 3 vorher probierten Zügen ist dann `moveNumber > 3` erfüllt, und wenn die Umwandlung kein Schach gibt: `computeLMR` liefert lmr > 0 (kein `isPromote()`-Check) → bei `depth - lmr < 0` wird der Zug **komplett übersprungen**, ohne je gesucht zu werden.
- **Kompensationen (warum das seltener zuschlägt als zunächst behauptet):** (a) In reinen Bauernendspielen greift `!position.hasMoreThanPawns()` — dort ist das Pruning ganz aus; (b) `canPruneFutility` rechnet den Umwandlungswert korrekt ein (nur der Move-Count-Zweig nicht); (c) Umwandlungen mit Schach sind über `isCheckMove` ausgenommen; (d) die Quiescence generiert Damen-Umwandlungen und fängt viele Fälle am Horizont; (e) die LMR-*Reduktion* (statt Skip) ist meist harmlos, weil der Materialgewinn der Dame auch bei reduzierter Tiefe sofort sichtbar ist.
- **Verbleibendes Risiko:** Stellungen mit Figuren + weit vorgerücktem Freibauern, Umwandlung ohne Schach, tief in der Zugliste bei kleiner Resttiefe. Selten, aber die Fälle sind hochwertig.
- **Fix (Einzeiler):** `if (move.isCapture() || move.isPromote()) return 0;` in `computeLMR`. Zusätzlich lohnenswert: Umwandlungen im Capture-Weighting mit ~Damenwert boosten (siehe N3).
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~5–15

### 1.5 Toter Code: TT-Bound-Verfeinerung des Stand-Pat unerreichbar — bestätigt
- **Datei:** [quiescencese.cpp:96-109](../search/quiescencese.cpp#L96-L109)
- **Verifiziert:** `ttValue` stammt aus `getTTCutoffValue` (liefert nur dann ≠ `NO_VALUE`, wenn Bound + Fenster einen Cutoff rechtfertigen). Das frühe `return ttValue` bei Zeile 96 feuert für **jeden** solchen Wert; danach ist `ttValue == NO_VALUE` garantiert, und der Verfeinerungsblock (Z. 105–109, `bestValue = standPatValue = ttValue`) ist unerreichbar — `abs(NO_VALUE) > MIN_MATE_VALUE` schlägt immer fehl. Der auskommentierte `alpha + 1 == beta`-Guard in Zeile 96 zeigt die Historie: Ursprünglich sollte der frühe Return nur in Null-Window-Knoten greifen und der Verfeinerungsblock in PV-Knoten arbeiten.
- **Nebenbefund:** Der frühe Return feuert auch an PV-Qsearch-Knoten und akzeptiert (anders als der Hauptsuchpfad, der `cutoffValue != 0` verlangt, [searchvariables.h:206](../search/searchvariables.h#L206)) auch den Wert 0 — kleine Inkonsistenz bei Remis-Werten aus dem TT.
- **Fix:** Frühen Return an `alpha + 1 == beta` binden (Guard reaktivieren); dann arbeitet der Verfeinerungsblock wieder.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~3–8

### 1.6 Kein Zeit-Check in der Quiescence — bestätigt, aber Überziehung eng begrenzt ⚠
- **Dateien:** [quiescencese.cpp](../search/quiescencese.cpp) (kein `ClockManager`-Bezug), [search.cpp:351-356](../search/search.cpp#L351-L356), [clockmanager.h:96-124](../search/clockmanager.h#L96-L124)
- **Präzisierung gegenüber dem ersten Durchgang:** `stopOnNodeTarget` wird an **jedem** negaMax-Knoten (auch NEAR_LEAF) geprüft und returnt sofort `true`, sobald `isSearchStopped()` — d. h. nachdem `emergencyAbort()` an irgendeinem INNER/PV-Knoten einmal gefeuert hat, wickelt sich der gesamte Baum schnell ab. Die maximale Überziehung ist also die Bearbeitungszeit **eines** NEAR_LEAF-Teilbaums (Tiefe ≤ 2) inkl. seiner Qsearch — normalerweise Mikro- bis Millisekunden. Da die Quiescence keine stillen Schachs generiert (nur Schlagzüge bzw. Evasionen im Schach), sind Explosionen selten; pathologische lange Schlag-/Schach-Ketten bleiben das Restrisiko.
- **Fix:** günstiger Node-Counter-basierter Check in der Qsearch (z. B. alle 1024 Knoten).
- **Konfidenz:** sicher (Codefakt); Auswirkung klein
- **ELO-Schätzung:** ~0–5, primär Robustheit gegen Zeitüberschreitung in Extremstellungen

### 1.7 SEE entfernt alle gleichartigen Angreifer auf einmal — bestätigt, Wirkung kleiner als angenommen ⚠
- **Datei:** [see.h:325-337](../search/see.h#L325-L337) (`tryPiece`)
- **Verifizierter Mechanismus:** `allPiecesLeft &= ~pieceToTryBitBoard[COLOR]` entfernt alle Angreifer des aktuellen Typs aus der Occupancy, obwohl nur einer zieht (`pieceToTryBitBoard` selbst poppt korrekt einzeln). Konkreter Fehlerfall: Zwei weiße Bauern e4/g4 greifen f5 an; nach exf5 ist auch g4 aus der Occupancy entfernt → ein schwarzer Läufer h3 wird via g4-„Röntgenblick" verfrüht als f5-Angreifer gezählt, obwohl g4 noch auf dem Brett steht.
- **Wechselwirkung, die den Schaden begrenzt:** `QUIESCENSE_USE_SEE_PRUNINT = false` ([searchparameter.h:133](../search/searchparameter.h#L133)) — die volle SEE wird **nur** für die Capture-Sortierung im Hauptsuchpfad benutzt (`isLoosingCapture` in der GOOD_CAPTURES-Stage); die Quiescence verwendet nur die leichte Bauern-Heuristik `isLoosingCaptureLight`. Ein falscher SEE-Wert kostet also nur gelegentlich Sortierqualität, prunt aber nichts weg.
- **Fix:** nur den tatsächlich ziehenden Stein (`lsb`) aus `allPiecesLeft` entfernen.
- **Konfidenz:** sicher (Mechanismus); Randfall-Häufigkeit gering
- **ELO-Schätzung:** ~0–5

### 1.8 Remis-Marker im TT: Verdrängung über Slot-Kollisionen, nicht über Same-Hash-Overwrite — Mechanismus korrigiert ⚠
- **Dateien:** [movehistory.h:101-110](../search/movehistory.h#L101-L110), [searchvariables.h:189-192](../search/searchvariables.h#L189-L192), [ttentry.h:228-248](../search/ttentry.h#L228-L248), [tt.h:105-141](../search/tt.h#L105-L141)
- **Korrektur des ersten Durchgangs:** Die Behauptung „der erste Nicht-PV-Knoten mit gleichem Hash überschreibt den Marker" ist **falsch**: `probeTT` erzwingt bei `isMaxDephtEntry()` immer einen sofortigen Cutoff ([searchvariables.h:189](../search/searchvariables.h#L189)), bevor der Knoten je selbst suchen und speichern könnte — der Same-Hash-Overwrite-Pfad ist für Marker praktisch unerreichbar.
- **Realer Restmechanismus:** Marker sterben über gewöhnliche **Slot-Kollisionen** (andere Position, gleicher Index): Ein PV-Store verdrängt den EXACT-Marker aus dem Primary-Slot (kopiert ihn zwar nach Secondary, [tt.h:124-131](../search/tt.h#L124-L131)), aber der Secondary ist always-replace — der nächste kollidierende Store löscht den Marker endgültig. Über Millionen Stores einer langen Suche ist das für einzelne Marker wahrscheinlich. Nicht-PV-Stores können den EXACT-Marker dagegen nicht verdrängen (`isExact() → return false`).
- **Einordnung:** Kern-Remiserkennung (Pfad-Repetition, 50-Züge, Material) ist unabhängig und korrekt; betroffen ist nur die Bias-Heuristik über die Spielhistorie hinweg.
- **Fix:** `isNewBetterForPrimary`/Secondary-Replacement um einen `isMaxDephtEntry()`-Schutz ergänzen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~0–5

### 1.9 Root-Verhalten nach Fail-Low des ersten Zugs — kein Fehler, Designnotiz ⚠ (herabgestuft)
- **Dateien:** [search.cpp:541-588](../search/search.cpp#L541-L588), [iterativedeepening.h:226-255](../search/iterativedeepening.h#L226-L255), [rootmoves.cpp:75-86](../search/rootmoves.cpp#L75-L86)
- **Neubewertung:** Nach Fail-Low von Zug 0 bleiben die restlichen Züge im Null-Window bei der (zu hohen) Aspiration-Alpha — sie können aber **fail-high gehen und die Iteration retten** (Re-Search mit `setPVWindow`, Zeile 562–569). Das Weitersuchen ist also kein reiner Verlust, sondern eine legitime Rettungsstrategie; die Null-Window-Suchen unterhalb des echten Werts scheitern zudem schnell. Erst wenn *alle* Züge low failen, wiederholt die äußere Schleife die Iteration mit geweitetem Fenster — die Wiederholungssuchen profitieren dann von frischen TT-Einträgen. Auch die Sortierung ist abgesichert: `RootMove::operator<` hält den bisherigen Bestzug (höchste Tiefe, Fail-Low-Flag) vorn, sodass bei Zeitabbruch kein unterdurchsuchter Zug gespielt wird.
- **Verbleibende Optimierungsidee:** sofortiges Widening bei Root-Fail-Low (Stockfish-Verhalten) spart die Rest-Null-Window-Suchen; messbar wenige ELO über Zeitmanagement.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~0–3

### N1 (NEU) Aspiration-Window: Delta-Term ist durch Zuweisungsreihenfolge immer 0
- **Datei:** [aspirationwindow.h:98-99](../search/aspirationwindow.h#L98-L99)
- **Befund:** In `setSearchResult` wird `_positionValue = positionValue;` **vor** der Fensterberechnung `calculateWindowSize(..., _positionValue - positionValue)` ausgeführt — das Delta-Argument ist dadurch konstant 0, und `deltaRelatedSize` in [aspirationwindow.h:149](../search/aspirationwindow.h#L149) trägt nie bei. Die beabsichtigte Anpassung der Fensterbreite an die Größe des Wertsprungs (gerade im Rising-Zustand, wo `abs(delta)` voll zählen sollte) ist tot; das Widening lebt nur von `_retryCount * 30` und dem Alternating-Fallback auf ±MAX. Folge: Bei großen Wertsprüngen sind mehr Re-Search-Runden nötig als vorgesehen.
- **Fix:** Delta vor der Zuweisung berechnen (`const auto delta = _positionValue - positionValue;`).
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~0–5 (Zeitersparnis bei instabilen Werten)

### N2 (NEU) Verlierende Schlagzüge werden vor Killern und ruhigen Zügen einsortiert
- **Datei:** [moveprovider.h:329-345](../search/moveprovider.h#L329-L345), Stage-Reihenfolge [moveprovider.h:40-44](../search/moveprovider.h#L40-L44)
- **Befund:** In der GOOD_CAPTURES-Stage bekommen SEE-verlierende Schlagzüge einen Malus (−50000), werden aber weiterhin **in derselben Stage** zurückgegeben, sobald keine gewinnenden Captures mehr da sind (`while`-Bedingung `weight >= 0` beendet die Malus-Schleife, der Zug wird geliefert). Die Stage-Folge ist also: gute Captures → **verlierende Captures** → Killer 1/2 → sortierte ruhige Züge. Standard (und messbar besser) ist, verlierende Schlagzüge hinter Killern und History-Zügen einzuordnen („bad captures last"), weil Killer an CUT-Knoten deutlich häufiger Cutoffs produzieren als Materialopfer.
- **Fix:** verlierende Captures in eine eigene Stage nach KILLER2/SORT_MOVES verschieben.
- **Konfidenz:** sicher (Mechanismus); Nutzen testpflichtig
- **ELO-Schätzung:** ~3–10

### N3 (NEU) Stille Damen-Umwandlungen bekommen Sortiergewicht 0 in der Capture-Stage
- **Datei:** [moveprovider.h:284-291](../search/moveprovider.h#L284-L291) (`computeCaptureWeight`)
- **Befund:** Das Capture-Gewicht ist nur der Wert des geschlagenen Steins (+10 Recapture-Bonus). Eine schlagfreie Damen-Umwandlung (non-silent, siehe 1.4) erhält Gewicht 0 und wird damit hinter jedem Bauerngewinn einsortiert — obwohl sie ~800 cp Materialschwung bringt. Das verstärkt Befund 1.4 (hohe `moveNumber` → Pruning-Fenster) und kostet Sortierqualität.
- **Fix:** `if (move.isPromote()) weight += Damenwert − Bauernwert;` in `computeCaptureWeight` (wirkt auch in der Quiescence, die dieselbe Gewichtung nutzt).
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~2–8 (gemeinsam mit 1.4 zu testen)

### Geprüft und in Ordnung befunden (Suche)
- **Mate-Scores im TT:** Store `±ply`-Korrektur ([ttentry.h:321-335](../search/ttentry.h#L321-L335)), Probe-Rückrechnung ([ttentry.h:268-280](../search/ttentry.h#L268-L280)) — korrekt und konsistent.
- **TT-Zug-Legalität:** TT-Züge werden nur gespielt, wenn sie in der frisch generierten legalen Zugliste gefunden werden (`selectProposedMove` matcht gegen `moveList`) — kein Illegal-Move-Risiko durch Hash-Kollisionen.
- **Nullmove:** Eval≥beta-Gate, Material-Guard, Zugzwang-Schutz (`sideToMoveHasQueenRookBishop`), In-Check-Guard, Mate-Fenster-Guards, Verifikationssuche mit `isVerifyingNullmove`-Flag und `noNullmove`-Vererbung — vollständig und korrekt.
- **LMR-Re-Search:** Reduzierte Suche > alpha → volle Null-Window-Suche → ggf. PV-Re-Search; Fail-Low der reduzierten Suche verbessert `bestValue` (Schutz gegen falsche Mate-Werte) — korrekt.
- **Futility im Zugloop:** `canPruneFutility` rechnet Capture- **und** Promotionswert ein; Guards gegen Mate-Escape und Bauernendspiele vorhanden.
- **Qsearch-Mate-Erkennung:** Im Schach ohne Evasionen bleibt `bestValue = -MAX_VALUE + ply` (korrekter Mate-Score); Patt im Schach unmöglich.
- **Repetition/50-Züge/Material:** an jedem Knoten vor allem anderen geprüft ([search.cpp:336-344](../search/search.cpp#L336-L344)).
- **Butterfly-History:** Index Piece+Zielfeld (Farbe im Piece kodiert), Aging über Halbierung, Malus-Updates für erfolglose Züge — konsistent.

---

## Teil 2: Evaluation (`eval/`)

### 2.1 King-Attack-Damen-Term prüft die Dame der falschen Seite — bestätigt, Wirkung präzisiert ⚠
- **Datei:** [king-attack.h:170](../eval/king-attack.h#L170)
- **Verifiziert:** In `computeAttackValue<COLOR>` ist `COLOR` der **Verteidiger** (`kingSquare = getKingSquare<COLOR>()`, Angriffe kommen aus `results.piecesAttack[OPPONENT]`, Zeile 159). `(position.getPieceBB(QUEEN + COLOR) != 0) * queenFactor` erhöht den Gefahrenindex des COLOR-Königs also, wenn der **Verteidiger** noch seine eigene Dame hat — beabsichtigt war der klassische „Angriff ohne Dame ist harmlos"-Rabatt, d. h. `QUEEN + OPPONENT`.
- **Wechselwirkungen/Wirkungsfenster:**
  - Solange **beide** Seiten Damen haben (oder beide keine — inkl. vollständigem Damentausch), bekommen beide Indizes denselben +2-Versatz; wegen der nichtlinearen `attackWeight`-Tabelle hebt sich das nicht exakt, aber näherungsweise auf. Der Fehler manifestiert sich hauptsächlich bei **asymmetrischer** Damenpräsenz (Dame gegen Turm+Leichtfigur, nach Umwandlungen, Damenopfer-Angriffe): Dort wird die Königsgefahr der falschen Seite um 2 Indexstufen verschoben — auf der steilen Tabelle ([king-attack.h:205-208](../eval/king-attack.h#L205-L208), z. B. Index 8→10 ≈ 43 cp) sind das je nach Angriffsniveau 20–60 cp in die falsche Richtung, für **beide** Könige gleichzeitig und gleichsinnig falsch.
  - Wichtig: Der Fehler ist farbsymmetrisch — der vorhandene Symmetrie-Selbsttest (`assertSymetry`) **kann ihn prinzipiell nicht finden**, selbst wenn er aktiv wäre. Das erklärt, wie er seit 2021 überlebt hat, und zeigt, dass die Fehlerklasse „falsche Seite, aber symmetrisch" eigene gezielte Tests braucht (z. B. bekannte Q-vs-R+B-Teststellungen gegen Referenzbewertungen).
- **Fix:** `QUEEN + COLOR` → `QUEEN + OPPONENT` (Einzeiler), danach `queenFactor` ggf. neu tunen (SPSA), da die Tabelle mit dem Fehler mittrainiert/getunt wurde.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~5–20 (Frequenz asymmetrischer Damenstellungen begrenzt den Effekt; erster Durchgang mit 15–30 war zu optimistisch)

### 2.2 Endspiel-Signatur-Lookups nicht angeschlossen; keine Skalierung für ungleichfarbige Läufer — bestätigt
- **Dateien:** [evalendgame.cpp:48-77, 90-98, 111-114, 130](../eval/evalendgame.cpp#L48-L77) (alle `registerSym(...)`-Blöcke in `/* … */`), [eval.cpp:33](../eval/eval.cpp#L33) (`//#include "eval-correction.h"`), `piece-signature-def.h` (per Projekt-Grep in keiner einzigen Datei inkludiert)
- **Verifiziert (2. Durchgang, da grep zunächst das Gegenteil suggerierte):** Die `registerSym`-Zeilen stehen sämtlich innerhalb von Kommentarblöcken. Aktiv sind nur die handgeschriebenen `REGISTER`-Heuristiken (KQKR, KRBKR, KBNK, KBBK, KPsK, KPsKPs, drawValue-Fälle etc. — diese sind inhaltlich geprüft und plausibel, inkl. korrekter Farb-Spiegelung über `changeSide`).
- **Konkret fehlende Standard-Technik:** Es gibt **keine** Remis-Skalierung für ungleichfarbige Läufer mit Bauern (das Pattern `KBP*KBP*` → `KBKB`-Lookup ist auskommentiert; nichts anderes deckt es ab) und keine Gewinnwahrscheinlichkeits-Korrekturen für gängige Ungleichgewichte (KRKB, KRKN mit Bauern nur als reine `forceToAnyCornerButDraw`-Fälle ohne Bauern). OCB-Endspiele bewertet die Engine also allein nach Material+PST+Bauernstruktur — systematische Überbewertung von Mehrbauern-Stellungen, die remis sind.
- **Einordnung:** Der Code stammt aus einem explizit unfertigen WIP-Commit („not finished"). Vor dem Verdrahten validieren: Die Lookup-Werte wurden nie im Spielbetrieb getestet. Alternativ (kleinerer, sicherer Schritt): eine handgeschriebene OCB-Skalierung (Faktor ~0,5 auf den Bauern-Überschussanteil bei reinem OCB, abgeschwächt mit zusätzlichen Figuren) direkt in `EvalEndgame` registrieren.
- **Konfidenz:** sicher (toter Code); Nutzen der Aktivierung testpflichtig
- **ELO-Schätzung:** ~10–30 (OCB ist der werthaltigste Einzelteil)

### 2.3 Bauernschild berechnet, aber nie aufgerufen — bestätigt, Erwartung gedämpft ⚠
- **Datei:** [king-attack.h:100-123](../eval/king-attack.h#L100-L123) (Definitionen), [king-attack.h:286](../eval/king-attack.h#L286) (`pawnIndexFactor`), Aufrufstellen: keine (Projekt-Grep: nur Definition + interne Verwendung + `getIndexLookup`-Export als „kShield")
- **Verifiziert:** Weder `KingAttack::eval()` noch `King::eval()` (nur PST + Endspiel-Pawn-Distanz, [king.h:94-105](../eval/king.h#L94-L105)) enthalten einen Shelter-Term; auch sonst existiert keiner (kingpawnattack.cpp ist ein Endspiel-Königsrennen-Modul). Der Export der Gewichte in die Tuning-Schnittstelle belegt, dass die Aktivierung geplant war.
- **Dämpfender Faktor:** Die vorhandenen `pawnIndexFactor`-Gewichte spannen nur ~19 cp (−9…+10, skaliert mit `midgameInPercentV2`) — in dieser Größenordnung ist der direkte Effekt klein. Indirekt überschneidet sich der fehlende Shelter teilweise mit dem Attack-Term (offene Königsstellung ⇒ mehr Angriffsfelder ⇒ höherer Attack-Index), was den Verlust weiter dämpft. Ein *getunter* Shelter-Term (übliche Größenordnung 30–60 cp Spanne) wäre wertvoller als das bloße Einschalten der aktuellen Mini-Gewichte.
- **Fix:** `computePawnShieldValue` in `computeAttackValue` bzw. `eval()` einbeziehen, danach Gewichte per SPSA tunen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** ~3–10 mit aktuellen Gewichten; mehr nur nach Retuning

### 2.4 Raumbewertung durch Default-Gewicht 0 neutralisiert — bestätigt
- **Dateien:** [space.h:61-68](../eval/space.h#L61-L68), [space.cpp:80](../eval/space.cpp#L80)
- **Verifiziert:** `SPACE_WEIGHT_MG_DEFAULT = 0`; ohne `PARAM_OPTIMIZE_SPACE`-Build ist `SPACE_WEIGHT_MG` constexpr 0 → `Space::eval()` liefert immer `EvalValue(0,0)`. Der UCI-Setter existiert nur im Optimize-Build.
- **Einordnung:** Möglicherweise bewusst nach erfolglosem Tuning auf 0 gelassen (das Repo enthält SPSA-Infrastruktur und `mobility-spsa.ini`-Spuren); es gibt dazu aber keinen Kommentar. Wenn getestet und negativ: toten Code entfernen statt mitschleppen. Wenn ungetestet: tunen.
- **Konfidenz:** sicher (Mechanismus); Absicht unklar
- **ELO-Schätzung:** ~0–10

### 2.5 `computeIndexVector()` bricht nach dem ersten Feature ab (Tuning-Pfad) — bestätigt
- **Datei:** [eval.cpp:68-87](../eval/eval.cpp#L68-L87)
- **Verifiziert:** `return indexVector;` in Zeile 72, danach ~15 Zeilen unerreichbarer Feature-Aufbau (midgame, tempo, kingPST, alle Piece-Details, KingAttack, Threat). Betrifft ausschließlich den Trainings-/Tuning-Pfad (`boardadapter.h` → `computeEvalIndexVector`), nicht die Spielstärke im Match — aber jedes gradientenbasierte Nachtunen über diesen Pfad ist lahmgelegt, was indirekt alle Gewichts-Verbesserungen blockiert (u. a. relevant für 2.1/2.3-Retuning).
- **Fix:** Einzeiler (Return entfernen bzw. ans Ende verschieben).
- **Konfidenz:** sicher
- **ELO-Schätzung:** 0 direkt; hoher indirekter Wert als Tuning-Voraussetzung

### 2.6 Symmetrie-Selbsttest inaktiv, mit Parameterfehler — bestätigt, mit Reichweiten-Einschränkung ⚠
- **Datei:** [eval.h:46-57](../eval/eval.h#L46-L57), einziger (auskommentierter) Aufrufer [quiescencese.cpp:111](../search/quiescencese.cpp#L111)
- **Verifiziert:** `eval(symPosition, ttPtr, -MAX_VALUE)` übergibt `-MAX_VALUE` als drittes Positionsargument — das ist laut Signatur ([eval.h:62](../eval/eval.h#L62)) `ply`, nicht `alpha`. Bei Reaktivierung würden Endspiel-Korrekturwerte oberhalb `MIN_MATE_VALUE` um `-MAX_VALUE` verschoben ([eval.cpp:164-169](../eval/eval.cpp#L164-L169)) → falsche Assert-Feuer.
- **Wichtige Einschränkung (siehe 2.1):** Auch ein reparierter, aktiver Symmetrie-Test fängt nur **asymmetrische** Fehler. Farbsymmetrische Falsch-Seite-Fehler wie 2.1 bleiben unsichtbar. Empfehlung: zusätzlich eine kleine EPD-Suite mit bekannten Materialungleichgewichts-Stellungen und Erwartungsintervallen als Regressionstest in `tests/` aufnehmen.
- **Fix:** Parameter korrigieren (`0` als ply übergeben), Aufruf in Debug-Builds aktivieren, EPD-Regressionstest ergänzen.
- **Konfidenz:** sicher
- **ELO-Schätzung:** 0 direkt; Absicherung

### Geprüft und in Ordnung befunden (Eval)
- Negamax-Vorzeichenkonvention: `lazyEval` rechnet aus Weiß-Sicht, `eval()` spiegelt für Schwarz — konsistent; `attackWeight`-Vorzeichen (negativ = Gefahr für den bewerteten König) korrekt verrechnet.
- Farb-Spiegelung: `switchSide`/`^0x38` in PST-, Pawn-, King- und Endgame-Code (inkl. `isSquareInBB<BLACK>`) korrekt.
- Phasen-Interpolation: `midgameInPercent`-Tabellen mit geklemmtem Index, `EvalValue::getValue` multipliziert vor Division — kein Präzisions-/Überlaufproblem.
- Aktive Endspiel-Heuristiken in `evalendgame.cpp` (KBNK mit korrekter Ecke, KBBK-Remis bei gleichfarbigen Läufern, KPsK-Randbauern-Remis, KBsPsK-Falscher-Läufer, KNPsK-Festung, KQKR, regVal-Abwertungen KB/KN gegen Bauern) — inhaltlich plausibel und farbrichtig registriert.
- `lazyEval`-Sonderpfade: Endgame-Korrektur ersetzt (nicht addiert) den Wert und wendet dann Mate-Ply-Korrektur an; `result == 0 → 1`-Konvention gegen TT-Remis-Verwechslung; 50-Züge-Dämpfung nur im Normalpfad — konsistent.

---

## Priorisierte Reihenfolge (korrigiert, nach erwartetem Nutzen ÷ Risiko)

| Prio | Befund | Art | ELO (Erwartung) |
|---|--------|-----|-----------------|
| 1 | 1.2 + 1.1(c): SE-Gate-Inversion fixen, SE-Ergebnis an allen Knoten nutzen | Bugfix + Feature | ~15–40 |
| 2 | 2.2 OCB-Remis-Skalierung (zunächst handgeschrieben) | fehlende Technik | ~10–30 |
| 3 | 2.1 King-Attack: `QUEEN + OPPONENT` | Bugfix (Einzeiler) | ~5–20 |
| 4 | 1.4 + N3: Promotions von LMR/LMP ausnehmen + Ordering-Boost | Bugfix | ~5–15 |
| 5 | 1.1(b): `se()`-Aufruf an Nicht-PV-Knoten streichen/verwerten | Effizienz | ~5–10 |
| 6 | N2 Verlierende Captures hinter Killer/Quiets | Ordering | ~3–10 |
| 7 | 1.3 TT-Zug in Quiescence aktivieren | Ordering | ~3–10 |
| 8 | 2.3 Bauernschild aktivieren + tunen | Feature | ~3–10 |
| 9 | 1.5 Qsearch-TT-Kontrollfluss reparieren | Bugfix | ~3–8 |
| 10 | 1.2(B) Futility-Guard beleben (nach Semantik-Fix) | Bugfix | ~3–8 |
| 11 | N1 Aspiration-Delta-Reihenfolge | Bugfix (Einzeiler) | ~0–5 |
| 12 | 1.1(a) Check-Extension für Nicht-PV testen | Experiment | ~0–20, Vorzeichen offen |
| 13 | 2.4 Space-Gewicht tunen oder Code entfernen | Aufräumen/Tuning | ~0–10 |
| 14 | 1.6 Node-Counter-Zeitcheck in Qsearch | Robustheit | ~0–5 |
| 15 | 1.7 SEE-Einzelstein-Entfernung | Bugfix | ~0–5 |
| 16 | 1.8 Marker-Schutz im TT-Replacement | Bugfix | ~0–5 |
| 17 | 1.9 Sofortiges Widening bei Root-Fail-Low | Experiment | ~0–3 |
| 18 | 2.5 Tuning-Feature-Vektor (Einzeiler) | Infrastruktur | indirekt |
| 19 | 2.6 Symmetrie-Test fixen + EPD-Regressionssuite | Absicherung | indirekt |

**Methodischer Hinweis:** Die Schätzungen sind bewusst konservativer als im ersten Durchgang.
Insbesondere bei Extensions und Pruning gilt: Die Suche ist ein ausbalanciertes System —
mehrere Mechanismen (Check-Ausnahmen in LMR/Futility, Evasion-Qsearch, In-Check-Guards)
kompensieren sich gegenseitig, und die vorhandenen Gewichte/Margins wurden mit den
bestehenden Fehlern mitgetunt. Nach jedem Fix, der die Baumform ändert (Prio 1, 4, 5, 6, 7, 12),
sollten `semc/semf`, `fut/ffut` und die LMR-Formel per SPSA nachgezogen werden, sonst
unterschätzt der SPRT-Test den wahren Gewinn.
