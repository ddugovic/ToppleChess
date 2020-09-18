//
// Created by Vincent on 22/09/2017.
//

#ifndef TOPPLE_BOARD_H
#define TOPPLE_BOARD_H

#include <string>
#include <iostream>
#include <vector>

#include "move.h"
#include "types.h"
#include "bb.h"
#include "hash.h"

// Piece values: pnbrqk
// Used for SEE
constexpr int VAL[] = {100, 300, 300, 500, 900, INF};

/**
 * Represents a state in the game. It contains the move used to reach the state, and necessary variables within the state.
 */
struct game_record_t {
    move_t prev_move;

    Team next_move; // Who moves next?
    bool castle[2][2]; // [Team][0 for kingside, 1 for queenside]
    uint8_t ep_square; // Target square for en-passant after last double pawn move
    int halfmove_clock; // Moves since last pawn move or capture
    U64 hash;
    U64 kp_hash;
    material_data_t material;
};

/**
 * Represents the attacks on a certain square on the board. The team and piece fields are only meaningful if the
 * square is occupied - the occupied field is true.
 */
struct sq_data_t {
    bool occupied : 1;
    Team team : 1;
    Piece piece : 6;
};

/**
 * Internal board representation used in the Topple engine.
 * Uses bitboard representation only
 */
struct alignas(64) board_t {
    board_t() = default;
    explicit board_t(const std::string &fen);

    void move(move_t move);
    void unmove();

    move_t parse_move(const std::string &str) const;
    move_t to_move(packed_move_t packed_move) const;

    bool is_illegal() const;
    bool is_incheck() const;

    bool is_attacked(uint8_t sq, Team side) const;
    bool is_attacked(uint8_t sq, Team side, U64 occupied) const;
    U64 attacks_to(uint8_t sq, Team side) const; // Attacks to sq from side
    bool is_pseudo_legal(move_t move) const;
    bool is_legal(move_t move) const;
    bool gives_check(move_t move) const;

    bool is_repetition_draw(int search_ply) const;
    bool is_material_draw() const;

    int see(move_t move) const;
    U64 non_pawn_material(Team side) const;

    void mirror();

    /* Board representation (Bitboard) */
    U64 bb_pieces[2][6] = {}; // [Team][Piece]
    U64 bb_side[2] = {}; // [Team]
    U64 bb_all = {}; // All occupied squares
    /* Board representation (Square array) */
    sq_data_t sq_data[64] = {{}};

    /* Game history */
    std::vector<game_record_t> record;

    /* Internal methods */
    template<bool HASH>
    void switch_piece(Team side, Piece piece, uint8_t sq);

    void print();
};

inline std::ostream &operator<<(std::ostream &stream, const board_t &board) {
    stream << std::endl;

    for (int i = 1; i <= board.record.size(); i++) {
        if (i % 2 != 0) {
            stream << " " << ((i + 1) / 2) << ". ";
        }

        stream << board.record[i].prev_move << " ";
    }

    stream << std::endl;

    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            auto file = uint8_t(j);
            auto rank = uint8_t(i);
            if (board.sq_data[square_index(file, rank)].occupied) {
                switch (board.sq_data[square_index(file, rank)].piece) {
                    case PAWN:
                        stream << (board.sq_data[square_index(file, rank)].team ? "p" : "P");
                        break;
                    case KNIGHT:
                        stream << (board.sq_data[square_index(file, rank)].team ? "n" : "N");
                        break;
                    case BISHOP:
                        stream << (board.sq_data[square_index(file, rank)].team ? "b" : "B");
                        break;
                    case ROOK:
                        stream << (board.sq_data[square_index(file, rank)].team ? "r" : "R");
                        break;
                    case QUEEN:
                        stream << (board.sq_data[square_index(file, rank)].team ? "q" : "Q");
                        break;
                    case KING:
                        stream << (board.sq_data[square_index(file, rank)].team ? "k" : "K");
                        break;
                    default:
                        assert(false);
                }
            } else {
                stream << ".";
            }
        }

        stream << std::endl;
    }

    stream << std::endl;

    return stream;
}

#endif //TOPPLE_BOARD_H
