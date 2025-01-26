#ifndef ENGINE_H
#define ENGINE_H

// #define USE_STOCKFISH_EVAL

#include <string>
#include "evaluate.h"
#include "position.h"
#include "network.h"
#include "../basics/move.h"
#include "../basics/evalvalue.h"

namespace Stockfish {

    class Engine {
    public:
        // Initialisiert Netzwerke
        static void initialize();

        // Lädt die NNUE-Datei und initialisiert den Cache
        static void load_network(const std::string& evalFilePathBig, const std::string& evalFilePathSmall);

        // Setzt die aktuelle Position anhand eines FEN-Strings
        static void set_position(const std::string& fen);

		static Stockfish::Move makeStockfishMove(ChessBasics::Move m) {
            if (m.isEmpty()) return Stockfish::Move::none();
			if (m.isNullMove()) return Stockfish::Move::null();
			if (m.isPromote()) return Stockfish::Move::make<Stockfish::PROMOTION>(Stockfish::Square(m.getDeparture()), Stockfish::Square(m.getDestination()), Stockfish::PieceType(m.getPromotion() >> 1));
            if (m.isEPMove()) return Stockfish::Move::make<Stockfish::EN_PASSANT>(Stockfish::Square(m.getDeparture()), Stockfish::Square(m.getDestination()));
            if (m.isCastleMove()) {
                const auto action = m.getActionAndMovingPiece();
                auto pos = 
                    action == ChessBasics::Move::WHITE_CASTLES_KING_SIDE ? ChessBasics::H1 :
                    action == ChessBasics::Move::WHITE_CASTLES_QUEEN_SIDE ? ChessBasics::A1 :
                    action == ChessBasics::Move::BLACK_CASTLES_KING_SIDE ? ChessBasics::H8 : ChessBasics::A8;
                return Stockfish::Move::make<Stockfish::CASTLING>(
                    Stockfish::Square(m.getDeparture()),
                    Stockfish::Square(pos));
            };
            return Stockfish::Move(Stockfish::Square(m.getDeparture()), Stockfish::Square(m.getDestination()));
		}
#ifdef USE_STOCKFISH_EVAL
        static void doMove(ChessBasics::Move m, Stockfish::StateInfo& newSt) { m.isNullMove() ? pos.do_null_move(newSt) : pos.do_move(makeStockfishMove(m), newSt); }
        static void undoMove(ChessBasics::Move m) {  m.isNullMove() ? pos.undo_null_move() : pos.undo_move(makeStockfishMove(m)); }
#else 
        static void doMove(ChessBasics::Move m, Stockfish::StateInfo& newSt) { }
        static void undoMove(ChessBasics::Move m) {}
#endif
        // Bewertet die aktuelle Position
        static Value evaluate();

        static std::string trace() {
			return Eval::trace(pos, *networks);
        }

        static ChessBasics::value_t to_cp(Value v);

    private:
        static Position pos;
        static StateInfo state;

        static Eval::NNUE::Networks* networks;  // Dynamische Initialisierung
        static Eval::NNUE::AccumulatorCaches* caches;  // Initialisierung nach load_network
    };

}  // namespace Stockfish

#endif  // ENGINE_H


