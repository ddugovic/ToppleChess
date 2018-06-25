//
// Created by Vincent on 30/09/2017.
//

#ifndef TOPPLE_SEARCH_H
#define TOPPLE_SEARCH_H

#include <future>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <utility>
#include <vector>

#include "board.h"

namespace search_heur {
    // History heuristic
    class history_heur_t {
    public:
        history_heur_t() {
            memset(historyTable, 0, sizeof(historyTable));
        }

        void update(move_t good_move, move_t *previous, int len, int depth) {
            int bonus = depth * depth;

            table(good_move, bonus);

            for (int i = 0; i < len; i++) {
                move_t prev = previous[i];
                if (!prev.info.is_capture) {
                    table(prev, -depth);
                }
            }

            if (get(good_move) > 20000) {
                throttle();
            }
        }

        int get(move_t move) {
            return historyTable[move.info.team][move.info.piece][move.info.to];
        }

    private:
        int historyTable[2][6][64] = {0}; // Indexed by [TEAM][PIECE][TO]

        // Intenal update routine
        void table(move_t move, int delta) {
            historyTable[move.info.team][move.info.piece][move.info.to] += delta;
            if (historyTable[move.info.team][move.info.piece][move.info.to] < 0) {
                historyTable[move.info.team][move.info.piece][move.info.to] = 0;
            }
        }

        // Throttle the history heuristic to be bounded by +- 20000
        void throttle() {
            // Divide everything in the history table by 2.
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 64; j++) {
                    historyTable[0][i][j] /= 2;
                    historyTable[1][i][j] /= 2;
                }
            }
        }
    };

    // Killer heuristic
    class killer_heur_t {
    public:
        killer_heur_t() {
            memset(killers, 0, sizeof(killers));
        }

        void update(move_t killer, int ply) {
            if (!killer.info.is_capture) {
                killers[ply][1] = killers[ply][0];
                killers[ply][0] = killer;
            }
        }

        move_t primary(int ply) {
            return killers[ply][0];
        }

        move_t secondary(int ply) {
            return killers[ply][1];
        }

    private:
        move_t killers[MAX_PLY][2] = {{EMPTY_MOVE}};
    };
}

struct search_limits_t {
    // Game situation
    explicit search_limits_t(int now, int time, int inc, int moves_to_go) {
        // Set other limits
        depth_limit = MAX_PLY;
        node_limit = UINT64_MAX;

        // Handle time control
        {
            // Try and use a consistent amount of time per move
            time_limit = (time /
                         (moves_to_go > 0 ? moves_to_go + 1: std::max(60 - now, 40)));

            // Handle increment: Look to gain some time if there isn't much left
            time_limit += inc / 2;

            // Add a 25ms buffer to prevent losses from timeout.
            time_limit = std::min(time_limit, time - 25);

            // Ensure time is valid (if a time of zero is given, search will be depth 1)
            time_limit = std::max(time_limit, 0);
        }
    }

    // Custom search
    explicit search_limits_t(int move_time, int depth, U64 max_nodes, std::vector<move_t> root_moves) :
            time_limit(move_time), depth_limit(depth), node_limit(max_nodes), root_moves(std::move(root_moves)) {}

    int time_limit;
    int depth_limit;
    U64 node_limit;
    std::vector<move_t> root_moves = std::vector<move_t>();
};

class search_t {
    friend class movegen_t;

public:
    explicit search_t(board_t board, tt::hash_t *tt, unsigned int threads, search_limits_t limits);

    move_t think(const std::atomic_bool &aborted);

private:
    int search_aspiration(int prev_score, int depth, const std::atomic_bool &aborted);
    int search_root(board_t &board, int alpha, int beta, int depth, const std::atomic_bool &aborted);

    template<bool H> int search_ab(board_t &board, int alpha, int beta, int ply, int depth, bool can_null,
                  const std::atomic_bool &aborted);
    template<bool H> int search_qs(board_t &board, int alpha, int beta, int ply, const std::atomic_bool &aborted);

    void save_pv();
    bool has_pv();

    bool keep_searching(int depth);
    bool is_aborted(const std::atomic_bool &aborted);
    bool is_root_move(move_t move);

    void print_stats(int score, int depth, tt::Bound bound);

    // Data
    board_t board;
    tt::hash_t *tt;
    std::chrono::steady_clock::time_point start;

    // Limits
    unsigned int threads;
    search_limits_t limits;

    // PV
    int pv_table_len[MAX_PLY] = {0};
    move_t pv_table[MAX_PLY][MAX_PLY] = {{EMPTY_MOVE}};
    int last_pv_len = 0;
    move_t last_pv[MAX_PLY] = {EMPTY_MOVE};

    // Heuristics
    search_heur::killer_heur_t killer_heur;
    search_heur::history_heur_t history_heur;

    // Important information
    int sel_depth;

    // Stats
    U64 nodes = 0;
    U64 fhf = 0;
    U64 fh = 0;
    U64 nulltries = 0;
    U64 nullcuts = 0;
};

#endif //TOPPLE_SEARCH_H
