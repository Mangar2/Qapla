# Refactoring-Vorschläge (Stand 2026-08-13)

Ergebnis eines systematischen Reviews über alle Subsysteme (ohne Bitbases, ohne toten Code).
Jeder Vorschlag wurde von einem zweiten, adversariellen Durchgang am Code verifiziert;
Korrekturen aus dieser Prüfung sind eingearbeitet. Vier Befunde fielen bei der Prüfung durch —
sie stehen am Ende, damit sie nicht noch einmal vorgeschlagen werden.

Zwei zulässige Gründe je Vorschlag:

- **einfacher** — der Code wird einfacher, ohne langsamer zu werden. Gate: identischer
  EPD-Nodecount, Laufzeit innerhalb 5 % (Pflichtlauf laut CLAUDE.md).
- **schneller** — der Code wird schneller. Gate: identischer Nodecount **plus** interleaved
  A/B-Serie gegen ein Referenz-Binary (Methodik in position-state-refactoring.md, Abschnitt 2).
  Ohne konsistentes Vorzeichen in allen Paaren wird der Umbau zurückgenommen.

Die gescheiterten Versuche aus position-state-refactoring.md Abschnitt 5 (Attack-Tables
auslagern, Getter mit Validity-Flag, Check-Flag-Cache, defensive Recomputes entfernen) wurden
als Sperrliste geprüft — kein Vorschlag hier berührt sie.

---

## 1. Schneller (Messkandidaten, interleaved A/B zwingend)

### 1.1 `enum Piece : int8_t` — `_board` von 256 auf 64 Bytes

**Dateien:** [types.h:184](../basics/types.h#L184), [board.h:556](../basics/board.h#L556)

`Piece` hat keinen Underlying-Type, also int (4 Bytes); `array<Piece, 64> _board` belegt
4 Cachelines. Alle Enumeratoren passen in int8_t (Maximum PIECE_MASK = 0x0F), alle Operatoren
rechnen ohnehin über int32_t. Mit `enum Piece : int8_t` schrumpft das meistgelesene Array des
Programms auf 1 Cacheline — Byte-Loads kosten auf M4/x86-64 dasselbe wie 32-Bit-Loads, keine
Indirektion, kein Branch. Grep-geprüft: kein `sizeof(Piece)`-Verlass, kein memcpy/Serialisierung
auf `_board`, alle Konversionsquellen auf 4 Bit maskiert (move.h:110-116). Eine Zeile Änderung,
repo-weite Rekompilierung. Gate: perft 5 (4865609), EPD-Nodecount identisch, dann A/B-Timing.

### 1.2 KingAttack-Gate auf `midgameInPercentV2` statt `midgameInPercent`

**Dateien:** [eval.cpp:116, 146-148](../eval/eval.cpp#L146), [eval.h:137-180](../eval/eval.h#L137), [king-attack.h:123-125, 179](../eval/king-attack.h#L123)

Bei V2 == 0 ist der KingAttack-Rückgabewert beweisbar exakt 0 (beide Anteile werden mit V2
multipliziert), und V2 > 0 impliziert V1 > 0 (V1-Tabelle positiv ab Index 6, V2 erst ab 12,
beide monoton). Das Gate `midgameInPercent > 0` auf `midgameInPercentV2 > 0` wechseln
überspringt die komplette Königsangriffs-Rechnung (2× computeAttacks, 2× computeAttackValue mit
Magic-Lookups und Popcounts) in allen Stellungen mit Figurenwert 6–11 — genau die Endspiele, in
denen die Suche am tiefsten geht. Danach hat V1 in lazyEval keinen Leser mehr und die Berechnung
eval.cpp:116 entfällt dort (fetchDetails behält sie für den kalten Detail-Pfad). Werte in jeder
Stellung identisch → Nodecount beweisbar gleich. Wechselwirkung mit 2.4 beachten.

### 1.3 `checkGivingSquares` nur dort berechnen, wo es gelesen wird

**Dateien:** [search-node.h:330-338](../search/search-node.h#L330), [search.cpp:260, 371, 515, 528, 633](../search/search.cpp#L515)

`computeMoves` ruft `computeCheckBitmapsForMovingColor()` bei jedem Aufruf; gelesen wird das
Ergebnis nur über `node.isCheckMove` in der Move-Loop von negaMax (search.cpp:528). Die Zeile
aus `computeMoves` herausnehmen und direkt hinter den computeMoves-Aufruf in negaMax (Zeile 515)
setzen. Spart die Berechnung (4 Magic-Lookups, Discovered-Check-Schleifen, 56-Byte-Write) in
se() — dort wird sie in Zeile 515 ohnehin für dieselbe Position wiederholt —, in
negaMaxPreSearch und an der Wurzel. Die wirkungslose Zeile `position.isWhiteToMove();`
(search-node.h:331) dabei mit entfernen. Nodecount beweisbar identisch.

### 1.4 SearchNode: heiße Tail-Member nach vorn, Mutex ans Ende

**Dateien:** [search-node.h:552-597](../search/search-node.h#L552)

Hinter dem heißen Skalarblock (bis `cutoff`) liegen Mutex (64 B, einziger Nutzer sind die nie
gerufenen *ThreadSafe-Wrapper), MoveProvider (~3,3 KB) und PV — und erst dahinter die ebenfalls
pro Knoten angefassten `checkGivingSquares`, `_nodeType` und `ttPtr`. Diese drei direkt hinter
`cutoff` einordnen, `mtxSearchResult` ans Strukturende. Reine Deklarationsreihenfolge, niemand
kopiert oder serialisiert das Layout. Präzedenz: in Attempt 1 (Abschnitt 5 des Plans) brachte
allein das Verschieben eines Blocks ans Ende von SearchNode ~1 Prozentpunkt — die Memberordnung
dieser Struktur ist nachweislich messbar. Bei neutralem Ergebnis zurücknehmen.

### 1.5 `selectProposedMove`: `break` nach Fund, Indextyp reparieren

**Dateien:** [moveprovider.h:386-397](../search/moveprovider.h#L386)

Die Scan-Schleife läuft nach dem Treffer bis zum Listenende weiter, obwohl jeder Zug in der
Liste nur einmal vorkommt (erster Treffer == letzter Treffer) — `break` einfügen. Läuft bis zu
dreimal pro Knoten (PV, KILLER1, KILLER2) mit Vollscan. Zusätzlich läuft der Index als `uint8_t`
gegen `getTotalMoveAmount()` (bis MAX_MOVE_AMOUNT = 300, laut movelist.h per FEN erreichbar):
bei > 255 Zügen wrappt er und die Schleife terminiert nie — Index auf `uint32_t`. Verhalten und
Nodecount identisch.

### 1.6 PawnTT-Index: 64-Bit-Modulo durch Multiply-Shift ersetzen

**Dateien:** [pawntt.h:78-80, 94-109](../eval/pawntt.h#L78)

`computeEntryIndex` rechnet `hashKey % _tt.capacity()` — eine udiv (~8–10 Zyklen) plus
capacity() als Pointer-Differenz, 1–2× pro Eval-Aufruf. Lemire-Reduktion
`(unsigned __int128)hashKey * count >> 64` ersetzt das durch mul+umulh (~3 Zyklen). Ändert nur,
welche Schlüssel sich einen Slot teilen; da der volle 64-Bit-Hash verglichen wird und ein Miss
denselben Wert berechnet, den ein Hit geliefert hätte, ist der Nodecount identisch. Zur
Konsistenz `_tt.size()` statt `capacity()` verwenden (das Haupt-TT macht das bereits,
tt.h:89). Beide Builds nutzen clang, `__int128` ist auf x64 und ARM64 verfügbar — Kommentar
dazu gehört an die Stelle. Erwartung ehrlich klein (< 1 %); ohne konsistentes Vorzeichen im
A/B-Lauf verwerfen.

### 1.7 Signatur-Lookup-Tabellen: 32 KB für Daten, die in 8 KB passen

**Dateien:** [piecesignature.h:417-429, 439-452](../basics/piecesignature.h#L417)

`futilityOnCaptureMap` ist `array<pieceSignature_t, 4096>` (16 KB), speichert aber nur
true/false → Elementtyp `bool`/`uint8_t` (4 KB, zugleich der semantisch richtige Typ).
`staticPiecesValue` ist `array<value_t, 4096>` (16 KB), Maximalwert 61 → `uint8_t` (4 KB);
Rückgabetyp bleibt value_t (implizite Erweiterung beim Load). Kein Interface, keine Zeile
Aufrufercode ändert sich; beide Tabellen werden pro Knoten angefasst (Quiescence-Captures bzw.
2× pro Eval-Aufruf). Die Schleifengrenzen-Anomalie in Zeile 442 (`< ALL` statt `< SIZE`) nicht
anfassen — das wäre eine separate Verhaltensfrage. Langsamer werden kann es nicht; Gewinn klein,
nur per A/B nachweisbar.

---

## 2. Einfacher, heißer Pfad (Gate: identischer Nodecount, Laufzeit ±5 %)

### 2.1 Pinned-Move-Generatoren per Template-Parameter zusammenlegen

**Dateien:** [movegenerator.cpp:404-488, 615, 657, 791-792](../movegenerator/movegenerator.cpp#L404)

`genPinnedMovesForAllPieces<COLOR>` und `genPinnedCapturesForAllPieces<COLOR>` sind bis auf zwei
Blöcke identisch (~40 duplizierte Zeilen subtiler Pin-Logik, inklusive wortgleich dupliziertem
Kommentar). Zu `template<Piece COLOR, moveGenType_t TYPE>` zusammenlegen, die zwei Blöcke unter
`if constexpr (TYPE == ALL)`. Compile-Time-Dispatch, jede Instanziierung instruktionsgleich mit
heute; der Enum-Wert ALL existiert bereits (movegenerator.h:210).

### 2.2 Doppelte attackMask-Zuweisung in `computeAttackMasksForBothColors` entfernen

**Dateien:** [movegenerator.cpp:123-149](../movegenerator/movegenerator.cpp#L123)

`computeAttackMask<COLOR>()` schreibt sein Ergebnis bereits selbst in `attackMask[COLOR]`
(Zeile 140); der Wrapper weist den Rückgabewert trotzdem noch einmal in dasselbe Element zu
(147-148). Äußere Zuweisungen streichen, `computeAttackMask` auf void (kein anderer Aufrufer,
privat). Läuft pro doMove; Streichen eines redundanten Stores kann Instruktionen nur senken.

### 2.3 Vierfach duplizierte Slider-Angriffs-Probe als Inline-Helfer

**Dateien:** [movegenerator.cpp:292-296, 534-537, 757-769](../movegenerator/movegenerator.cpp#L292)

Das Muster „Bishop-Maske & (B|Q) | Rook-Maske & (R|Q)" steht viermal wörtlich im Code (genEPMove,
genEvades, zweimal isCheckMove). Privater Inline-Helfer
`template<Piece ATTACKER_COLOR> bitBoard_t sliderAttacksToSquare(Square, bitBoard_t occupancy)`.
Die Farbe ist an allen vier Stellen compile-time bekannt; instruktionsgleich.
`computePinnedMask` und `computeCheckBitmaps` brauchen die Hälften getrennt und bleiben unberührt.

### 2.4 Doppelte Midgame-Phasenberechnung zusammenlegen

**Dateien:** [eval.h:96-113](../eval/eval.h#L96), [eval.cpp:40-41, 116-117](../eval/eval.cpp#L40)

`computeMidgameInPercent` und `computeMidgameV2InPercent` haben identische Rümpfe bis auf die
Tabelle, beide Aufrufstellen rufen sie direkt nacheinander. Ein Helfer berechnet den geclampten
Index einmal und setzt beide EvalResults-Felder. Reine Quelltext-Deduplizierung — die doppelte
Index-Berechnung eliminiert der Compiler unter -O3/-flto vermutlich schon selbst, also kein
Speedup-Anspruch. **Nach 1.2 umsetzen:** fällt V1 aus lazyEval heraus, betrifft dieser Punkt nur
noch den kalten fetchDetails-Pfad.

### 2.5 Gemeinsamer Attack-Akkumulations-Helfer für die vier Figuren-Evaluatoren

**Dateien:** [knight.h:122-130](../eval/knight.h#L122), [bishop.h:137-147](../eval/bishop.h#L137), [rook.h:152-163](../eval/rook.h#L152), [queen.h:129-141](../eval/queen.h#L129), [evalresults.h:40-76](../eval/evalresults.h#L40)

Die Vier-Zeilen-Buchhaltung (pieceAttack |= …; piecesDoubleAttack |= piecesAttack & attackBB;
piecesAttack |= attackBB; popCount(…)) ist viermal kopiert — inklusive der unkommentierten
Invariante, dass piecesDoubleAttack **vor** piecesAttack aktualisiert werden muss. Ein
monomorpher inline-Helfer auf EvalResults trägt sie genau einmal. Ebenso `isPinned` als
gemeinsamer Helfer (Rückgabetyp `bool` genügt allen vier Aufrufern — die heutigen Signaturen
differieren, sind aber äquivalent). Die Queen-Variante akkumuliert queenAttack heute *nach*
piecesAttack — reine OR-Akkumulationen sind untereinander vertauschbar, der Helfer darf eine
feste Reihenfolge wählen; der EPD-Lauf bestätigt es.

### 2.6 `updateTTandKiller`: gespeicherten `positionHash` verwenden statt neu rechnen

**Dateien:** [search-node.h:451-459](../search/search-node.h#L451), [search.cpp:408-412, 591](../search/search.cpp#L408)

`positionHash` wird am Knoteneintritt gesetzt (nonSearchingCutoff bzw. initSearchAtRoot), und
beim Aufruf von updateTTandKiller ist die Position wieder exakt die des Eintritts (alle
doMove/undoMove-Paare balanciert — genau die Invariante, die der B3-Shadow-Compare abgesichert
hat). `setTTEntry(positionHash, isPV)` plus Debug-`assert(positionHash ==
position.computeBoardHash())`. Strikt weniger Instruktionen, gleicher Hashwert, identisches
TT-Verhalten.

### 2.7 INNER/NEAR_LEAF-Dispatch in einen Helfer ziehen

**Dateien:** [search.cpp:175-177, 550-552, 570-572](../search/search.cpp#L175)

Dreimal derselbe Ternary mit je sieben wiederholten Argumenten; alle drei Schwellen sind
dieselbe Regel „Kindtiefe > 1 → INNER" (nachgerechnet: depth−R>2, moveDepth−lmr>2, moveDepth>2
sind mit cd = jeweilige Kindtiefe alle cd>1). Privater inline-Helfer
`template<SearchRegion TYPE> searchChild(...)` in search.cpp (derselben Übersetzungseinheit,
im Zweifel force-inline); Negation und WhatIf-Zeilen bleiben an den Aufrufstellen. Der fehlende
TYPE-Guard am Nullmove-Aufruf ist unschädlich (nur bei TYPE==INNER erreichbar).

### 2.8 `TTEntry::initialize` baut `_info` in einer Zuweisung auf

**Dateien:** [ttentry.h:54-68, 93-118](../search/ttentry.h#L54)

`setInfo` erhält per `_info &= PRECISION_MASK` absichtlich die alten Präzisionsbits des
überschriebenen Eintrags, `setValue` löscht sie danach wieder — funktioniert nur in genau dieser
Reihenfolge, und außerhalb von initialize hat keine der beiden Funktionen einen Aufrufer.
initialize berechnet die Präzision selbst und weist das komplette 16-Bit-Wort einmal zu
(Bitfelder überlappen nicht, geprüft; `_pv` ist ein eigener Member und kollidiert nicht).
Achtung: `setEntryAgeIndicator` bleibt — `clear()` benutzt es weiterhin. TT::setEntry ruft
initialize höchstens einmal pro Aufruf (die drei Stellen schließen sich aus).

### 2.9 MaterialBalance: Konfigurationstabellen statisch, doppelte Befüllung zusammenlegen

**Dateien:** [materialbalance.h:46-67, 199-205](../basics/materialbalance.h#L46), [materialbalance.cpp:80-97](../basics/materialbalance.cpp#L80)

Der #else-Zweig des Konstruktors ist zeilenidentisch mit `rebuildPieceValues` (16 Zeilen);
im Normal-Build sind die Tabellen Instanzmember (168 Bytes pro Board), im Optimize-Build bereits
statisch. Im Normal-Build als `inline static constexpr` anlegen. Damit die Befüllung wirklich
nur einmal existiert: eine gemeinsame constexpr-fähige Builder-Funktion im Header, die im
Normal-Build die constexpr-Tabellen initialisiert und im Optimize-Build von rebuildPieceValues
gerufen wird (constexpr-Funktionen sind zur Laufzeit aufrufbar). `abs()` ist in C++20 nicht
constexpr — im Builder ein Ternary. Der aufruferlose non-const `getPieceValues()` entfällt.
`pieceValues[piece]` bleibt ein einzelner Load, nur this-relativ → .rodata.

### 2.10 Endspielpfad: Statik-Aufruf über Dummy-Objekt, leerer out-of-line-Konstruktor

**Dateien:** [evalendgame.cpp:199-200, 218](../eval/evalendgame.cpp#L199), [kingpawnattack.cpp:25-27](../eval/kingpawnattack.cpp#L25)

KPsKPs legt `Pawn evalPawn;` an, nur um die statische Funktion `computePawnValueNoPiece`
aufzurufen → direkt `Pawn::computePawnValueNoPiece(...)`. `KingPawnAttack::KingPawnAttack()` ist
leer und out-of-line — Deklaration und Definition streichen, der implizite Default ist exakt
äquivalent (alle Member werden von initRace gefüllt, bevor sie gelesen werden; mit -flto ist der
Call ohnehin wegoptimiert). Reiner Klarheitsgewinn, es wird ausschließlich entfernt.

---

## 3. Einfacher, kalter Pfad (Gate: identischer Nodecount)

### 3.1 `IncrementalState::operator==` und EvalValue-Vergleich defaulten

**Dateien:** [board.h:53-63](../basics/board.h#L53), [evalvalue.h](../basics/evalvalue.h), [imbalance.h:80](../basics/imbalance.h#L80)

EvalValue bekommt `constexpr bool operator==(const EvalValue&) const = default;`, dann wird der
handgeschriebene Feldvergleich in IncrementalState zu `= default` — dasselbe Muster wie
`Imbalance::State`. Schließt die Fehlerklasse „neues Feld fällt aus dem Vergleich" aus (die der
Plan bei Imbalance dokumentiert). Läuft nur in Debug-Assertions.

### 3.2 Redundantes `bitBoardsPiece.fill(0)` in `MoveGenerator::clear()`

**Dateien:** [movegenerator.cpp:196-204](../movegenerator/movegenerator.cpp#L196)

`Board::clear()` hat bitBoardsPiece über clearBB() bereits genullt; die erneute Füllung in
Zeile 203 ist wirkungslos und verwischt die Zuständigkeit (bitBoardsPiece gehört Board).
Keine stillgelegte alte Berechnung, sondern eine versehentliche Doppelung — Zeile streichen.

### 3.3 ButterflyBoard nicht per Wert übergeben

**Dateien:** [computinginfo.h:100](../search/computinginfo.h#L100)

`initNewSearch` nimmt das 16-KB-ButterflyBoard per Wert; das darunterliegende
`RootMoves::setMoves` erwartet ohnehin eine Referenz, gelesen wird nur. Parameter auf
`ButterflyBoard&`. Einmal pro Suchstart, rein lesend, kein Dangling (der intern gespeicherte
Zeiger lebt nur innerhalb von setMoves).

### 3.4 RootMove-Kopie in `printNewPV`

**Dateien:** [computinginfo.h:222](../search/computinginfo.h#L222)

`const auto rootMove = _rootMoves.getMove(moveNo);` kopiert ~250 Bytes pro fertig gesuchtem
Root-Move; die Nachbarfunktion printSearchResult (Zeile 170) benutzt für denselben Zugriff
bereits `const auto&`. Angleichen. (Keine Heap-Allokation im Spiel — `_pvString` ist immer leer.)

### 3.5 KillerMove: Rule of Zero

**Dateien:** [killermove.h:36-42](../search/killermove.h#L36)

Handgeschriebener Copy-Konstruktor und operator= tun exakt das, was der Compiler generiert
(beide Member, nichts speziell). Streichen; die Klasse wird dabei trivially copyable, und ein
später ergänzter Member kann nicht mehr stillschweigend aus der Kopie herausfallen.

### 3.6 Doppelte Vorspul-Schleife in MoveHistory

**Dateien:** [movehistory.h:79-96, 145-160](../search/movehistory.h#L79)

Der Block „Board von startPosition kopieren, Züge vor dem 50-Züge-Fenster nachspielen" steht
zeichengleich in isDrawByRepetition und computeDrawHashes. Private Hilfsfunktion, beide rufen
sie. Die per-Knoten-Remisprüfung (isDrawByRepetitionInSearchTree) ist davon getrennt und bleibt
unberührt.

### 3.7 UciParameterProvider-Boilerplate tabellengesteuert

**Dateien:** [knight.cpp](../eval/knight.cpp), [bishop.cpp](../eval/bishop.cpp), [rook.cpp](../eval/rook.cpp), [queen.cpp](../eval/queen.cpp), [threat.cpp](../eval/threat.cpp), [pawn.cpp](../eval/pawn.cpp), [array-generator.h:137-146](../eval/array-generator.h#L137)

~93 Tuning-Parameter in sechs Klassen, jeder mit getUciParameters-Eintrag plus if-else-Arm in
setUciParameter (~250 Zeilen mechanische Ketten; rook.cpp allein 20 Arme, pawn.cpp 31). Ein
Binder mit Liste {name, Ziel, min, max} und regenerate()-Hook pro Klasse ersetzt beides — ein
neuer Parameter ist dann eine Tabellenzeile. Achtung: die fünf ppThreat-Multiplikatoren in
pawn.cpp sind int64_t, der Rest int32_t — Binder mit typisierten Zielen oder Member vereinheitlichen.
Zusätzlich ist generateMobilityMap fünfmal fast wortgleich (knight, bishop, rook, queen, threat)
— ein Template `generateEvalArrayBSpline<SIZE>` in array-generator.h ersetzt alle.
Alles in #ifdef PARAM_OPTIMIZE_*-Blöcken: die Release-Binary bleibt bitidentisch.

### 3.8 Winboard/Statistics: identische Kommando-Handler in ChessInterface heben

**Dateien:** [winboard.cpp](../interface/winboard.cpp), [statistics.cpp](../interface/statistics.cpp), [chessinterface.h](../interface/chessinterface.h)

Statistics ist als Kopie von Winboard entstanden: readLevelCommand, checkClockCommands,
setBoard, analyzeMove, handleRemove, handleInputWhileComputingMove sind byte-identisch
(~130-150 Zeilen); printGameResult differiert nur im ILLEGAL_MOVE-Fall, readMemory nur in
„Hash"/„hash" (folgenlos — setOption lowercased). In die Basisklasse heben. Dafür müssen auch
`_mode` und `_computerIsWhite` nach ChessInterface wandern (handleRemove/analyzeMove benutzen
sie; das Mode-Enum liegt dort schon) — mehr als reines Verschieben, aber machbar. Winboards
handleMove druckt zusätzlich das Spielergebnis; der Unterschied muss erhalten bleiben.

### 3.9 ResultPerPieceIndex = Kopie von PieceSignatureStatistic; Sparse-Serialisierung dreifach

**Dateien:** [self-play-manager.cpp](../interface/self-play-manager.cpp), [piece-signature-statistic.cpp](../training/piece-signature-statistic.cpp), [signature-eval-adjuster.cpp:323-369](../training/signature-eval-adjuster.cpp#L323)

Dieselben drei Vektoren, saveToFile/loadFromFile und die Auswertefunktionen doppelt; das
writeMap/readMap-Lambda-Paar existiert wortgleich ein drittes Mal im signature-eval-adjuster.
Eine Klasse in training/ behalten, die Serialisierung als freie Helfer extrahieren. **Bewusste
Entscheidung nötig:** computeStatistic ist semantisch verschieden — bei symmetrischen Signaturen
zählt die self-play-Variante das Total doppelt (Statistik löscht zu 0 aus), die training-Variante
behandelt den Fall korrekt. Die Zusammenlegung wählt eine Variante (vermutlich training/), die
Ausgabe des anderen Werkzeugs ändert sich dadurch — dokumentieren. Motorpfad folgenlos.

### 3.10 PGNTokenizer: defekt initialisierte `_charType`-Tabelle durch `isSymbolContinuation()` ersetzen

**Dateien:** [pgntokenizer.cpp:25-43, 101-121](../pgn/pgntokenizer.cpp#L25), [pgntokenizer.h:37-40, 119](../pgn/pgntokenizer.h#L37)

Das Instanz-Member `_charType` (256 Einträge) wird nur beim allerersten Objekt gefüllt (bewacht
von `static int aInit`); jede weitere Instanz — auch jede über den String-Konstruktor erzeugte —
liest eine uninitialisierte Tabelle, dazu mit signed char indiziert (negativ für Bytes ≥ 128).
Einziger Leser ist skipSymbolContinuation, und die Tabelle ist exakt aus isSymbolContinuation()
aufgebaut: Tabelle, Enum und Init-Muster streichen, direkt die Funktion rufen. Für die korrekt
initialisierte erste Instanz verhaltensgleich; für alle anderen ersetzt es UB durch das Gemeinte.

### 3.11 `convertPositionValueToWinboardFormat` doppelt — in ISendSearchInfo verschieben

**Dateien:** [uciprintsearchinfo.h:110-118](../interface/uciprintsearchinfo.h#L110), [winboardprintsearchinfo.h:105-113](../interface/winboardprintsearchinfo.h#L105)

Wortgleich in beiden Klassen, in der UCI-Variante nicht einmal aufgerufen. Eine Definition als
protected-Member der gemeinsamen Basis ISendSearchInfo (nötige Typen sind dort schon sichtbar).

### 3.12 ConsoleIO: String-Kopien und Verkettung pro Zeichen im Tokenizer

**Dateien:** [consoleio.h:168-170, 201-223](../interface/consoleio.h#L168)

isCharInString nimmt den String per Wert und nennt den Parameter `string` (verschattet den
Typnamen); readTokenFromBuffer baut `spaceString + separationString` in jeder Zeichen-Iteration
neu. Parameter auf `const std::string&`, Verkettung vor die Schleife (oder direkt
`str.find(ch) != npos`). Kleinster Befund der Liste.

---

## 4. Geprüft und verworfen

Damit sie nicht wieder vorgeschlagen werden — die Prüfung fand die Fakten korrekt, aber den
Vorschlag nicht tragfähig:

- **Imbalance-Lookups auf int16_t halbieren (160 KB → 80 KB).** Im Tuning-Build erreichen die
  Einträge mit Koeffizienten bis ±1000 rechnerisch ~43000 und sprengen int16_t; nötig wäre eine
  #ifdef-gespaltene Typedef — zusätzliche Komplexität für einen vermutlich unmessbaren Gewinn
  (pro Update werden nur 2 Einträge gelesen, die heißen liegen ohnehin auf residenten Cachelines).
- **updateStateOnDoMove aus dem Move dekodieren statt aus dem Brett.** Der Capture-Teil ist im
  Chess960-Fall eine echte Verhaltensänderung (Rochade-Turm auf Königszielfeld: Halbzugzähler
  divergiert, Remis-Erkennung dahinter), und weder EPD-Lauf noch 960-perft prüfen den
  Halbzugzähler. Der verbleibende Pawn-Teil allein wäre Umbau ohne Vereinfachung.
- **PieceSignatureLookup-Stride 256 → 16 (256 KB → 1 KB).** Der Code wird von keiner Datei
  inkludiert und nie kompiliert — Laufzeit- und Compile-Zeit-Effekt exakt null, keine Zeile
  entfällt. Arbeit an bewusst deaktiviertem Code; falls die Lookups je reaktiviert werden,
  gehört die Stride-Änderung in denselben, ohnehin zu messenden Schritt.
- **Tote EPMask-Initialisierung in BitBoardMasks::InitStatics streichen.** Faktisch korrekt
  (die Stores sind wirkungslos), aber die alte EPMask-Berechnung wurde erkennbar bewusst per
  Überschreiben stillgelegt statt entfernt — strukturell dasselbe Muster wie das deaktivierte
  Bitbase-Probing. Löschen nur auf ausdrücklichen Auftrag.

---

## Umsetzungshinweise

- Jeder Punkt einzeln, eigener Commit, eigener EPD-Vergleichslauf (CLAUDE.md-Pflicht).
- Reihenfolge-Abhängigkeit: 1.2 vor 2.4 (sonst wird 2.4 doppelt angefasst).
- Die Messkandidaten in Abschnitt 1 zusätzlich per interleaved A/B-Serie gegen ein
  Referenz-Binary (Methodik: position-state-refactoring.md Abschnitt 2, concurrency=1,
  erste Paarung verwerfen, gleiches Vorzeichen in allen Paaren). Neutral gemessen → zurücknehmen.
- 1.1 (enum Piece) und 1.4 (SearchNode-Layout) sind die beiden Kandidaten mit dem
  klarsten Mechanismus und Präzedenz im Projekt — als Erste messen.
