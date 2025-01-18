#include "engine.h"
#include "evaluate.h"
#include <iostream>

namespace Stockfish {

    Position Engine::pos;
    StateInfo Engine::state;
    Eval::NNUE::Networks* Engine::networks = nullptr;
    Eval::NNUE::AccumulatorCaches* Engine::caches = nullptr;

    void Engine::initialize() {
        // Netzwerke erstellen
        networks = new Eval::NNUE::Networks(
            Eval::NNUE::NetworkBig({ EvalFileDefaultNameBig, "None", "" }, Eval::NNUE::EmbeddedNNUEType::BIG),
            Eval::NNUE::NetworkSmall({ EvalFileDefaultNameSmall, "None", "" }, Eval::NNUE::EmbeddedNNUEType::SMALL));
    }

    void Engine::load_network(const std::string& evalFilePathBig, const std::string& evalFilePathSmall) {
        // Großes Netzwerk laden
        networks->big.load("", evalFilePathBig);

        // Kleines Netzwerk laden
        networks->small.load("", evalFilePathSmall);

        // Cache zurücksetzen
        if (caches)
            delete caches;
        caches = new Eval::NNUE::AccumulatorCaches(*networks);
    }


    void Engine::set_position(const std::string& fen) {
        pos.set(fen, false, &state);
    }

    Value Engine::evaluate() {
        // Führt die Evaluierung durch und gibt das Ergebnis zurück
        return Eval::evaluate(*networks, pos, *caches, 0);  
    }

    struct WinRateParams {
        double a;
        double b;
    };

    WinRateParams win_rate_params(const Position& pos) {

        int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>()
            + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();

        // The fitted model only uses data for material counts in [17, 78], and is anchored at count 58.
        double m = std::clamp(material, 17, 78) / 58.0;

        // Return a = p_a(material) and b = p_b(material), see github.com/official-stockfish/WDL_model
        constexpr double as[] = { -37.45051876, 121.19101539, -132.78783573, 420.70576692 };
        constexpr double bs[] = { 90.26261072, -137.26549898, 71.10130540, 51.35259597 };

        double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
        double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

        return { a, b };
    }

    // Turns a Value to an integer centipawn number,
    // without treatment of mate and similar special scores.
    ChessBasics::value_t Engine::to_cp(Value v) {

        // In general, the score can be defined via the WDL as
        // (log(1/L - 1) - log(1/W - 1)) / (log(1/L - 1) + log(1/W - 1)).
        // Based on our win_rate_model, this simply yields v / a.

        auto [a, b] = win_rate_params(pos);

        return int(std::round(100 * int(v) / a));
    }

}  // namespace Stockfish


