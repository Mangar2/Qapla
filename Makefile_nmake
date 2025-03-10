# NMAKE-kompatibles Makefile ohne GNU-spezifische Features
# Erstellt sämtliche Objekte im Unterverzeichnis "build" und linkt zu qapla.exe.

# ---------------------------------------------------------
# Compiler-Konfiguration
# ---------------------------------------------------------
CC = cl
CFLAGS = /permissive- /GS /GL /W3 /Gy /Zc:wchar_t /O2 /sdl /Zc:inline /fp:precise \
         /D "_T0" /D "NDEBUG" /D "_CONSOLE" /D "USE_AVX2" /D "USE_POPCNT" \
         /D "USE_SSE2" /D "USE_SSSE3" /D "USE_SSE41" /D "_MBCS" /WX- /Zc:forScope \
         /Gd /Oi /MD /std:c++20 /EHsc /nologo /Ot

# ---------------------------------------------------------
# Verzeichnis und Ziele
# ---------------------------------------------------------
BUILD_DIR = build
TARGET    = $(BUILD_DIR)\qapla.exe

OBJS = \
  $(BUILD_DIR)\qapla.obj \
  $(BUILD_DIR)\basicboard.obj \
  $(BUILD_DIR)\board.obj \
  $(BUILD_DIR)\hashconstants.obj \
  $(BUILD_DIR)\piecesignature.obj \
  $(BUILD_DIR)\pst.obj \
  $(BUILD_DIR)\bitbasegenerator.obj \
  $(BUILD_DIR)\bitbaseindex.obj \
  $(BUILD_DIR)\bitbasereader.obj \
  $(BUILD_DIR)\compress.obj \
  $(BUILD_DIR)\reverseindex.obj \
  $(BUILD_DIR)\eval-helper.obj \
  $(BUILD_DIR)\eval-map.obj \
  $(BUILD_DIR)\eval.obj \
  $(BUILD_DIR)\evalendgame.obj \
  $(BUILD_DIR)\kingAttack.obj \
  $(BUILD_DIR)\kingpawnattack.obj \
  $(BUILD_DIR)\pawn.obj \
  $(BUILD_DIR)\pawnrace.obj \
  $(BUILD_DIR)\rook.obj \
  $(BUILD_DIR)\stdtimecontrol.obj \
  $(BUILD_DIR)\winboard.obj \
  $(BUILD_DIR)\bitboardmasks.obj \
  $(BUILD_DIR)\magics.obj \
  $(BUILD_DIR)\movegenerator.obj \
  $(BUILD_DIR)\pgngame.obj \
  $(BUILD_DIR)\pgntokenizer.obj \
  $(BUILD_DIR)\perft.obj \
  $(BUILD_DIR)\quiescencese.obj \
  $(BUILD_DIR)\rootmoves.obj \
  $(BUILD_DIR)\search.obj \
  $(BUILD_DIR)\whatif.obj

# ---------------------------------------------------------
# Standard-Target
# ---------------------------------------------------------
all: $(TARGET)

# ---------------------------------------------------------
# Link-Schritt
# ---------------------------------------------------------
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) /Fe$(TARGET)

# ---------------------------------------------------------
# Einzelne Kompilierregeln (explizit, um 100% nmake-kompatibel zu sein)
# ---------------------------------------------------------
$(BUILD_DIR)\qapla.obj: qapla.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c qapla.cpp /Fo$(BUILD_DIR)\qapla.obj

$(BUILD_DIR)\basicboard.obj: basics\basicboard.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c basics\basicboard.cpp /Fo$(BUILD_DIR)\basicboard.obj

$(BUILD_DIR)\board.obj: basics\board.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c basics\board.cpp /Fo$(BUILD_DIR)\board.obj

$(BUILD_DIR)\hashconstants.obj: basics\hashconstants.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c basics\hashconstants.cpp /Fo$(BUILD_DIR)\hashconstants.obj

$(BUILD_DIR)\piecesignature.obj: basics\piecesignature.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c basics\piecesignature.cpp /Fo$(BUILD_DIR)\piecesignature.obj

$(BUILD_DIR)\pst.obj: basics\pst.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c basics\pst.cpp /Fo$(BUILD_DIR)\pst.obj

$(BUILD_DIR)\bitbasegenerator.obj: bitbase\bitbasegenerator.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c bitbase\bitbasegenerator.cpp /Fo$(BUILD_DIR)\bitbasegenerator.obj

$(BUILD_DIR)\bitbaseindex.obj: bitbase\bitbaseindex.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c bitbase\bitbaseindex.cpp /Fo$(BUILD_DIR)\bitbaseindex.obj

$(BUILD_DIR)\bitbasereader.obj: bitbase\bitbasereader.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c bitbase\bitbasereader.cpp /Fo$(BUILD_DIR)\bitbasereader.obj

$(BUILD_DIR)\compress.obj: bitbase\compress.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c bitbase\compress.cpp /Fo$(BUILD_DIR)\compress.obj

$(BUILD_DIR)\reverseindex.obj: bitbase\reverseindex.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c bitbase\reverseindex.cpp /Fo$(BUILD_DIR)\reverseindex.obj

$(BUILD_DIR)\eval-helper.obj: eval\eval-helper.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\eval-helper.cpp /Fo$(BUILD_DIR)\eval-helper.obj

$(BUILD_DIR)\eval-map.obj: eval\eval-map.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\eval-map.cpp /Fo$(BUILD_DIR)\eval-map.obj

$(BUILD_DIR)\eval.obj: eval\eval.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\eval.cpp /Fo$(BUILD_DIR)\eval.obj

$(BUILD_DIR)\evalendgame.obj: eval\evalendgame.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\evalendgame.cpp /Fo$(BUILD_DIR)\evalendgame.obj

$(BUILD_DIR)\kingAttack.obj: eval\kingAttack.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\kingAttack.cpp /Fo$(BUILD_DIR)\kingAttack.obj

$(BUILD_DIR)\kingpawnattack.obj: eval\kingpawnattack.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\kingpawnattack.cpp /Fo$(BUILD_DIR)\kingpawnattack.obj

$(BUILD_DIR)\pawn.obj: eval\pawn.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\pawn.cpp /Fo$(BUILD_DIR)\pawn.obj

$(BUILD_DIR)\pawnrace.obj: eval\pawnrace.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\pawnrace.cpp /Fo$(BUILD_DIR)\pawnrace.obj

$(BUILD_DIR)\rook.obj: eval\rook.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c eval\rook.cpp /Fo$(BUILD_DIR)\rook.obj

$(BUILD_DIR)\stdtimecontrol.obj: interface\stdtimecontrol.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c interface\stdtimecontrol.cpp /Fo$(BUILD_DIR)\stdtimecontrol.obj

$(BUILD_DIR)\winboard.obj: interface\winboard.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c interface\winboard.cpp /Fo$(BUILD_DIR)\winboard.obj

$(BUILD_DIR)\bitboardmasks.obj: movegenerator\bitboardmasks.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c movegenerator\bitboardmasks.cpp /Fo$(BUILD_DIR)\bitboardmasks.obj

$(BUILD_DIR)\magics.obj: movegenerator\magics.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c movegenerator\magics.cpp /Fo$(BUILD_DIR)\magics.obj

$(BUILD_DIR)\movegenerator.obj: movegenerator\movegenerator.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c movegenerator\movegenerator.cpp /Fo$(BUILD_DIR)\movegenerator.obj

$(BUILD_DIR)\pgngame.obj: pgn\pgngame.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c pgn\pgngame.cpp /Fo$(BUILD_DIR)\pgngame.obj

$(BUILD_DIR)\pgntokenizer.obj: pgn\pgntokenizer.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c pgn\pgntokenizer.cpp /Fo$(BUILD_DIR)\pgntokenizer.obj

$(BUILD_DIR)\perft.obj: search\perft.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c search\perft.cpp /Fo$(BUILD_DIR)\perft.obj

$(BUILD_DIR)\quiescencese.obj: search\quiescencese.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c search\quiescencese.cpp /Fo$(BUILD_DIR)\quiescencese.obj

$(BUILD_DIR)\rootmoves.obj: search\rootmoves.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c search\rootmoves.cpp /Fo$(BUILD_DIR)\rootmoves.obj

$(BUILD_DIR)\search.obj: search\search.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c search\search.cpp /Fo$(BUILD_DIR)\search.obj

$(BUILD_DIR)\whatif.obj: search\whatif.cpp
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) /c search\whatif.cpp /Fo$(BUILD_DIR)\whatif.obj

# ---------------------------------------------------------
# Aufräumen
# ---------------------------------------------------------
clean:
	if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
