# Qapla

Qapla chess engine

The current version 0.2 - the second pre-release version

## What is a chess engine?

A chess engine is a console application playing chess. It has no graphical user interface. Use a popular chess UI and include Qapla in it.

## Hardware requirement

Qapla is available for 64 bit windows and 64 bit linux. Qapla uses hardware support for some strongly used routines in modern computer chess (popCount, bitScanForeward). They are supported by intel processors since more than 10 years. If you have older processors, use the OLD_HW version. It is about 20% less fast than the version using hardware and it has (whyever) double size.

## Compiling it yourself

- I deliver a visual studio project that can be used to compile it. Compile the "release" version.
- You can also compile it with nmake. The make file is generated with chat-gpt but it still works :-)
- There is a CMakeLists.txt to compile it with cmake for linux.

To compile it with linux: 
1. Install cmake and clang++ (or g++)
2. Create a build directory with release subdirectory
3. Change to the release directory
4. Run `cmake ../..`
5. Run `make -j$(nproc)`

or you can add the following to the tasks.json in the .vscode directory, if you use Visual Studio Code:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Release",
            "type": "shell",
            "command": "cmake -B ${workspaceFolder}/build/release -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build ${workspaceFolder}/build/release",
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        }
    ]
}
```

## Version numbering

Qapla's version numbers have three digits:

- 3rd digit: I create a "patch" version number for every build I test for improvements. Thus I might create 5 new versions in a day and run a test with five different versions afterwards.
- 2cnd digit: Once I have enough features, elo's together, I will built a new minor version, currently I plan to have at least 50 elo difference between minor versions
- 1st digit: I will create the 1.0.X only if it is full featured (mutil-cpu, multi-pv, tablebases, ...)

## How do I test

A typical test is made with 60 seconds +0.01 second per move and 1400 games agains 14 engines of about equal playing strength. Current test set:

- Ruffian 1.05
- Glaurung 1.21
- Fruit 2.3.1
- Critter 0.52
- WildCat 8
- Aristarch 4.50
- Spike 0.9a
- Spike 1.2 Turin
- Midnight 5
- Amoeba 2.1
- Marvin 3.0
- Laser 1.0
- viridithas 2.7.0
- Leorik 2.3

A big thank you to the engine developers to have and keep their engines and also older versions freely available. These engines are selected because they play in average with similar playing strength and because the run very stable and supports short time controls. The current version is plays slightly above the average of these engines.

I use the "Spike GUI" that is not public available for the tests. It is able to play up to 10 games in parallel and has the ability to switch off all view windows to use very little CPU (about 1% of a single core when handling 10 games in parallel). Sadly, I do not provide the latest source code any more and I need tricks to install it on windows 10. It is developed with MFC ...

## What interfaces Qapla supports?

It supports UCI and Winboard, UCI is prefered. Winboard is currently mostly used for debugging on text mode. UCI supports more features than Winboard.

The following features are already supported in UCI

- Play games
- Analyze
- Pondering
- MultiPV (needs some improvements here :-) )
- Time per move, time per game, moves to go, fixed search depth

### Not yet supported in UCI

- Exact node count
- Exclude moves in search

### The following "features" are only supported in winboard

- perft
- divide
- generate (to generate bitbases, but you cannot use them yet)
- whatif (switched off by compiler flag in release builds). Whatif is a debugging feature to debug the search. It supports to print the variables in dedicated nodes of the search tree

Perft and divide has multi cpu support and accepts the cores N command for perft. It has been tested with 32 threads in parallel on a 16 core hyperthreading machine

### Pondering in Winboard

Pondering is only rudimentary supported in Winboard. Instead of continuing on ponder-hit, Qapla will stop the search and start a new search profiting from the transposition table. This is not the best way to do it, but it is the easiest way to implement it. I will improve it later.

## Playing strength

Qapla never played in "official tournaments". I only have some raiting according my own test with short time control. I expect it to have abount 2700 "CCRL-ELo" or 2500 "CEGT-Elo". CCRL and CEGT are very popular chess engine raiting lists.

## Improvements on Qapla 0.2 (to 0.1.1)

- Minor changes in eval, mostly rooks and queen on #7 rank
- Implemented late move reduction
- Implemented kind of butterfly history boards (but with piece/target square instead of source/target square)
- Singular extension
- IID
- Verify on Null moves
- More consistend aspiration window
- Better primary variant line due to always extend the last ply on PV node search, if a transposition table entry is found
- Refactoring many code lines left due to the first non-recursive approach
- Bug fixing

## What to expect from Qapla?

My current plan is

- Refactoring eval to calculate an index per piece and then retrieve the pice value from a table. This shall improve the ability to automatically improve eval weights
- Further improve search and implement more of the "classic" functionality that is still missing (futility pruning, razoring, ...)
- The new features (eval-refinement history, continuation history, ...) will be implemented later
- Try to generate nnue training data with my engine and implement a nnue network and train it myself.

## More info on the chess engine

The chess engine is based on a bitboard move generator, which generates only legal moves.

### Bitbases

Bitbase support is included in a basic form starting with version 0.3.

⚠️ **Important:** 
- This is a simple first version. It will load all bitbases in uncompressed format in memory once you set the bitbase path. Make sure you have good memory, if you use it.
- It is currently limited on 5 pieces bitbases. 6 pieces may work, but it is untested.

You can generate bitbases using the following command:

**`bitgenerate pieces [cores n]`**

⚠️ **Important:** This must be the very first command after starting the engine.  
Any other command (e.g. `uci` or `xboard` or anything else as xboard is default) will activate one of the standard protocols, which do not support bitbase generation.  
Once you enter bitbase generation mode, the engine will remain in this mode and only accept further `bitgenerate` commands.

#### Parameters

- `pieces`: The piece signature to generate.
  - Example: `KPK` will generate the bitbase for **King and Pawn vs. King**.
- `cores n`: (optional) Number of CPU cores to use during generation.

The bitbase will be created in the current working directory.  
Each generated bitbase is saved as a separate file (e.g. `KPK.btb` for King–Pawn–King).
The starting bitbase (e.g. KPPK if you bitgenerate KPPK) will also be saved as KPPK.h - a compiled header file containing the binary data for the bitbase, if you wish to link it to the program. The file KPK is for example alread linked in that way beginning with version 0.3.

#### Note on `P` in Signatures

The character `P` in a signature acts like a **wildcard for promotable pieces**.  
For example, generating `KPPPK` will implicitly require other bitbases like `KQK`, `KRK`, `KBK`, `KNK` to determine promotion outcomes.

As a result, generating a bitbase with a signature like `KPPPK` may trigger the generation of several additional bitbases if they are not already available.

#### Note KPPKP is not identical to KPKPP

KPPKP will store which positions are won for the side with the two pawns, while KPKPP will store which positions are won for the side with one pawn. Note that trying to generate KKPP - thus the bitbase for the won positions for the side having only a king will generate nothing. But KNK will be generated even, if it will produce an nearly zero size file. 
To Generate:

- All 3-pieces: KPK (or just "3", eg. bitgenerate 3 cores 4)
- All 4-pieces: KPPK, KPKP (or "4")
- All 5-pieces: KPPKP, KPKPP, KPPPK (or "5" will also generate all 4 and all 3 pieces)

#### Delete the files not needed

You may delete the files that you dont want after generation or you can cherry-pick the files you want. The system only needs all files for generation, not for playing with them.

#### Generation result information

The engine will print something like:
KQQKQ using 16 threads .................................................c
Time spent: 0:0:14.976 Won: 44433482 (42%) 44433482 Not Won: 11561529 (11%) Mated: 9051 Illegal: 48897469 (46%)

Each dot is a moves to mate dot. So if the first dot show up, the engine computed all positions with mate in one. The last "c" shows that copression is started. It also shows the amount of positions Won, the amount of positions not Won (draw or lost), the amout of mate positions and the amount of illegal positions (king not to move in check ...).

### Table bases

Not yet

### Multi-Threading

Not yet

### Search

Qapla has a classic recursive negamax search including the basic search features

- Move ordering (PV, good captures, killer1, killer2, non-captures, bad-captures)
- Null-window search on PV nodes after one best move has been found with verification search
- Iterative deepening with aspiration window
- Singular extension
- Tansposition table (fixed size: 32MB) - due to not yet implemented parameter to change it. Note changing the size of the TT has little effect on the playing strength
- Quiescense search with a check-evades for all plies
- Late move reduction with piece/position history
- Late move pruning
- Reverse futility pruning (thus futility pruning after making the move)

### Time management

Using longer time for critical situation

### Eval

Qapla already has several eval terms

- Piece Mobility, counting the amount of fields a piece may move to - (+ pass through - e.g. a rook passes through another rook or queen of the same color)
- Double Pawn penalty
- Isolated Pawn penalty
- Passe pawn bonus + connecte passed pawn bonus + protected passed pawn bonus (a non passed pawn protects a passed pawn)
- King security
- Threat count evaluation
- Piece square tables
- Knight outpost
- Rook trapped by king penalty
- Rook on half and full open files
- Rooks on 7th rank

### Endgame eval terms

As it does not support tablebases Qapla implements algorithms to play "good enough" in several clear endgame positions. This is the current list
To read the list one example: KQR*B*N*P*KB means a King with one Queen + zero or more Rooks + zero or more Bishops + zero or more Knights + zero or more pawns against King and Bishop. The idea is to have an algorithm for a King and Queen winning against King and Bishop and then use the same algorithm, if the Queen side has more material.

- KQKR
- KQR*B*N*P*KB (Queen vs. Bishop)
- KQR*B*N*P*KN (Queen vs. Knight)
- KQ+R*B*N*K (Queen to mate a King)
- KQP+KRP+ (Queen with pawns agains Rook with pawns)
- KR+B*N*K (Rook to mate a King)
- KBBK (Two bishops to mate a King or draw value, if the bishops are of the same color)
- KB+P+K
- KBNK
- KNP+K
- KP+KP+

Combination getting a winnign value bonus to encurage the engine to exchange material to get to those situations

- KQ+R*B*N*P+K
- KR+B*N*P+K
- KNNPK

Combinations getting a draw value

- KBK
- KN+K
- KK

and more already ...
