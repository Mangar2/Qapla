# Qapla Chess Engine - AI Coding Guidelines

## Project Overview
Qapla is a UCI/WinBoard chess engine written in C++20, featuring bitboard-based move generation, alpha-beta search with transposition tables, and both classical evaluation and optional NNUE (neural network) evaluation. Target strength: ~2850 CCRL Elo.

## Core Architecture

### Major Components (Namespace Organization)
- **QaplaBasics** ([basics/](basics/)): Board representation, move encoding, hash, PST (piece-square tables)
  - [board.h](basics/board.h): Core `Board` class with `doMove`/`undoMove`, state tracking
  - [types.h](basics/types.h): Fundamental types (`bitBoard_t`, `square_t`, pieces, colors)
  - [move.h](basics/move.h): Move encoding (16-bit packed format)
  
- **QaplaMoveGenerator** ([movegenerator/](movegenerator/)): Legal move generation using magic bitboards
  - [movegenerator.h](movegenerator/movegenerator.h): Extends `Board`, generates only legal moves
  - [magics.h](movegenerator/magics.h): Sliding piece attack generation
  
- **QaplaSearch** ([search/](search/)): Alpha-beta search with iterative deepening
  - [iterativedeepening.h](search/iterativedeepening.h): Root search controller, time management
  - [search.h](search/search.h): Recursive alpha-beta with null-move, LMR, pruning
  - [tt.h](search/tt.h): Transposition table with replacement strategy
  
- **ChessEval** ([eval/](eval/)): Classical evaluation (material, mobility, king safety, pawns)
  - [eval.h](eval/eval.h): Main evaluation entry point, lazy eval
  - [pawn.h](eval/pawn.h)/[pawntt.h](eval/pawntt.h): Pawn structure cache
  
- **QaplaBitbase** ([bitbase/](bitbase/)): Endgame tablebases (KPK, etc.)
  
- **QaplaInterface** ([interface/](interface/)): UCI/WinBoard protocol handlers
  - [uci.h](interface/uci.h): UCI protocol implementation
  - [winboard.h](interface/winboard.h): Winboard/XBoard protocol

### Data Flow
1. Interface receives UCI/WinBoard commands → parses FEN/moves
2. `IterativeDeepening` orchestrates search (time control, depth management)
3. `Search::search()` recursively explores game tree with `MoveGenerator`
4. `Eval::eval()` scores positions (or NNUE if `USE_STOCKFISH_EVAL` defined)
5. Results stored in `TT` (transposition table), best move returned to interface

## Build System

### Platforms & Compilers
- **Windows**: clang-cl (MSVC mode) - see [Makefile](Makefile) Windows section
- **Linux**: clang++ - see [Makefile](Makefile) Unix section

### Build Targets (Makefile)
```powershell
# Windows builds (PowerShell/CMD)
make BUILD_TYPE=Debug          # Debug build with symbols
make BUILD_TYPE=Release        # Optimized release (-O2, -flto, AVX2)
make BUILD_TYPE=Release_NO_POPCOUNT  # For old CPUs without POPCNT
make BUILD_TYPE=WhatifRelease  # Debug-enabled release build
```

### Optional Features (Preprocessor Flags)
- `WHATIF_RELEASE`: Enable `whatif` debugging in release builds (see [whatIf.h](search/whatIf.h))
- Typically defined in Makefile or Visual Studio project

## Development Workflows

### CODE MODIFICATION DISCIPLINE - ABSOLUTE REQUIREMENT

**CRITICAL RULE: Minimize all code changes to the absolute minimum required for the task.**

Every single line changed triggers mandatory testing (see Testing Requirements below). Unnecessary changes waste time and risk introducing bugs.

#### When Adding New Features
- **ADD new code** - don't modify existing code unless absolutely necessary
- **Example**: Adding new parameters to a class
  - ✓ CORRECT: Insert new members/functions, leave existing lines untouched
  - ✗ WRONG: Reorder existing members, change formatting, move unrelated lines
  
- **Example**: Adding futility pruning
  - ✓ CORRECT: Add new function `canPruneFutility()` and new parameters
  - ✗ WRONG: Rename `forewardFutility()` or move `cmdLineParam` declaration

#### Before Every Edit, Ask:
1. **Is this change strictly necessary for the implementation?**
2. **Can I achieve the goal by ONLY adding new lines?**
3. **Am I touching any code unrelated to the task?**

If you modify even one existing line unnecessarily:
- wmtest sd 12 must prove identical node counts (for non-functional changes)
- OR full SPRT + gauntlet testing (for functional changes)
- = Hours of wasted testing time

#### Whitespace/Formatting
- **Never** reformat existing code
- **Never** adjust indentation of unchanged lines
- **Never** reorder existing declarations/definitions
- Match existing style for new code only

#### Performance: Avoid Redundant Computations
Chess engines execute millions of nodes per second. Redundant checks kill performance.

**Before adding any condition/check:**
1. **Search for similar checks** in the same code path (same function, caller, callee)
2. **Analyze the call flow**: Is this computation already done elsewhere?
3. **Study existing similar code**: How do other pruning methods (move count pruning, razoring, etc.) handle this?
4. **Find the optimal structure**: Sometimes restructure code flow to compute once and reuse

**Example from futility pruning:**
- Bad: Call `isCheckingMove()` in both `canPruneFutility()` AND move count pruning
- Analysis needed: Where is the best place to compute it once? Can result be shared?
- No universal solution: Each case requires careful analysis of the specific situation

**Never blindly copy checks without understanding why they exist and where they belong.**

### Code Research - MANDATORY BEFORE WRITING

**NEVER guess or invent method names, parameters, or behavior. ALWAYS research first.**

**Before using ANY method, class, or API:**
1. **grep_search** for existing usage in the codebase
2. **read_file** to verify actual signature, parameters, return types
3. **Study context** - read surrounding code to understand usage patterns
4. **If uncertain**: Ask the user for clarification

**This applies to:**
- Method names and their parameters
- Template parameters
- Class member access
- Return types and values
- API behavior and side effects

**Golden rule: If you haven't seen it used in the codebase, don't assume it exists or works a certain way.**

### Testing Requirements - CRITICAL
**All code changes must be rigorously tested before merging.** 
Whenever we change anything in the code, we must ensure no unintended side effects occur. 
When we add a new functionality we need to 
- Comment-out the line(s) that really create the impact and keep every other change
- This Comment-out must be minimal, so do not comment-out the whole function, only the impact lines inside the function
- Deliver every change with these commented-out lines
- Prove that without this impact lines the code computes exactly the same node-number in "wmtest sd 12" 

**Automated Testing Workflow:**
```powershell
# 1. ONE-TIME: Create baseline before making any code changes
Copy-Item build\Release\Qapla.exe build\Release\Qapla_baseline.exe
Write-Output "wmtest sd 12`nquit" | .\build\Release\Qapla_baseline.exe > .\build\Release\baseline.txt

# 2. Make code changes with impact lines commented out
# 3. Compile: make BUILD_TYPE=Release -j
# 4. Run automated test and comparison
.\compare-wmtest.ps1
```

**Expected Results:**
- With impact lines commented out: IDENTICAL (0 differences, time within 1%)
- With impact lines active: DIFFERENCES FOUND (shows node/time changes)

The `compare-wmtest.ps1` script automatically:
- Runs wmtest sd 12 on current Qapla.exe
- Compares against baseline.txt
- Reports node count differences (absolute + percentage)
- Reports time differences (absolute + percentage)
- Shows first differing position with details

### Version Management
- Version history tracked in [version.md](version.md) with Elo ratings and change notes
- Format: `0.4.005` (major.minor.patch) - patch increments frequently during tuning
- Current branch: `release0.4` (default: `master`)

## Code Conventions

### Naming Patterns
- **Member variables**: `_lowerCamelCase` (e.g., `_whiteToMove`, `_boardState`)
- **Functions**: `lowerCamelCase()` (e.g., `doMove()`, `computeMidgameInPercent()`)
- **Types/Classes**: `UpperCamelCase` (e.g., `MoveGenerator`, `BoardState`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_SEARCH_DEPTH`, `WHITE_KING`)

### Header Organization
- Always include license header (GPL v3) with author/copyright
- Use `#pragma once` (not include guards in new code, though old code has `#ifndef`)
- Forward declare when possible; include full headers in `.cpp` files

### Move Representation
**The following information is encoded in every move and accessible through:**
- `getDeparture()`, `getDestination()`: Get squares
- `getMovingPiece()`: The piece being moved
- `getCapture()`: Captured piece (NONE if no capture)
- `getPromotion()`: Promotion piece (NONE if not promoting)
- `isCapture()`: true, if move captures a piece
- `isPromote()`: true, if pawn promotion
- `isCastleMove()`: true, if castling
- `isEPMove()`: true, if en-passant

### Evaluation Values
- `value_t` (int32_t): Centipawn units (100 = 1 pawn)
- `EvalValue`: Midgame/endgame tapered pair (see [evalvalue.h](basics/evalvalue.h))
- Position eval always from white's perspective; negated for black to move

### Search Conventions
- Alpha-beta bounds: `[-MAX_VALUE, +MAX_VALUE]`
- Mate scores: `MATE_VALUE - ply` (distance to mate encoded)
- Node types: PV (principal variation), ALL (fail-low), CUT (fail-high)

## Critical Integration Points

### Board State Management
- Must preserve `BoardState` struct when calling `doMove()` to enable `undoMove()`
- State includes: castling rights, EP square, 50-move counter, hash
- Example pattern:
  ```cpp
  BoardState savedState = board.getBoardState();
  board.doMove(move);
  // ... recursive search ...
  board.undoMove(move, savedState);
  ```

### Hash/Transposition Table
- Zobrist hashing updated incrementally in `doMove()`/`undoMove()`
- TT stores: hash key, best move, score, depth, node type
- Size configurable via UCI `setoption name Hash value <MB>`

### Time Management
- Clock logic in [clockmanager.h](search/clockmanager.h), [stdtimecontrol.h](interface/stdtimecontrol.h)
- Iterative deepening checks time before starting next depth
- Factor: 4x estimated time for next depth (see `ESTIMATED_TIME_FACTOR_FOR_NEXT_DEPTH`)

## Common Tasks

### Adding Evaluation Terms
1. Add weight to [eval/](eval/) header (e.g., [bishop.h](eval/bishop.h))
2. Compute in `Eval::lazyEval()` (see [eval.cpp](eval/eval.cpp))
3. Test with `printEval()` method for debugging
4. Tune weights via [training/](training/) tools or manual testing

### Modifying Search Behavior
1. Key pruning/reduction parameters in [search.cpp](search/search.cpp)
2. Update [version.md](version.md) with patch version and change description
3. Test Elo change with gauntlet vs reference engines

### Protocol Extensions (UCI)
1. Add option declaration in `UCI::uciCommand()` (see [uci.h](interface/uci.h))
2. Parse in `processCommand()` or relevant handler
3. Update [README.md](README.md) feature list

## External Dependencies
- **LZ4**: Bitbase compression ([lz4.c](bitbase/lz4.c)/[lz4.h](bitbase/lz4.h))
- **Miniz**: Alternative compression ([miniz.c](bitbase/miniz.c)/[miniz.h](bitbase/miniz.h))
- **NNUE** (optional): Stockfish-style neural network evaluation in [nnue/](nnue/), [nnue768/](nnue768/)

## Avoid These Pitfalls
- **Don't break symmetry**: Eval must be identical for mirrored positions (test with `assertSymetry()`)
- **Don't forget state preservation**: Always save `BoardState` before `doMove()` for correct `undoMove()`
- **Don't mix namespaces carelessly**: Always specify `using namespace` explicitly or qualify with `::`

## License
GPL v3 - always include full license header in new files (see [LICENSE](LICENSE))
