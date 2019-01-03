//
// Created by Vincent on 30/09/2017.
//

#include "eval.h"
#include "board.h"
#include "endgame.h"
#include <sstream>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Utility tables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

U64 BB_PASSED[2][64] = {{0}};
U64 BB_ISOLATED[2][64] = {{0}};
U64 BB_IN_FRONT[2][64] = {{0}};
U64 BB_HOLE[2][64] = {{0}};

U64 BB_KING_CIRCLE[64] = {0};

U64 BB_CENTRE = 0;

void evaluator_t::eval_init() {
    for (uint8_t sq = 0; sq < 64; sq++) {
        BB_HOLE[WHITE][sq] = 0xffffffffffffffff;

        for (uint8_t rank = rank_index(sq) + uint8_t(1); rank < 8; rank++) {
            // Passed
            if (file_index(sq) > 0) BB_PASSED[WHITE][sq] |= single_bit(square_index(uint8_t(file_index(sq) - 1), rank));
            if (file_index(sq) < 7) BB_PASSED[WHITE][sq] |= single_bit(square_index(uint8_t(file_index(sq) + 1), rank));
            BB_PASSED[WHITE][sq] |= single_bit(square_index(file_index(sq), rank));

            // Candidate
            BB_IN_FRONT[WHITE][sq] |= single_bit(square_index(file_index(sq), rank));
        }

        // Hole
        BB_HOLE[WHITE][sq] ^= BB_PASSED[WHITE][sq];
        BB_HOLE[WHITE][sq] ^= BB_IN_FRONT[WHITE][sq];

        // Isolated
        for (uint8_t rank = 0; rank < 8; rank++) {
            if (file_index(sq) > 0)
                BB_ISOLATED[WHITE][sq] |= single_bit(square_index(uint8_t(file_index(sq) - 1), rank));
            if (file_index(sq) < 7)
                BB_ISOLATED[WHITE][sq] |= single_bit(square_index(uint8_t(file_index(sq) + 1), rank));
        }

        // Centre
        if (file_index(sq) <= 4 && file_index(sq) >= 3 && rank_index(sq) <= 4 && rank_index(sq) >= 3) {
            BB_CENTRE |= single_bit(sq);
        }

        // King circle
        BB_KING_CIRCLE[sq] |= find_moves<KING>(WHITE, sq, 0);
        BB_KING_CIRCLE[sq] |= find_moves<KNIGHT>(WHITE, sq, 0);
    }

    // Mirror
    for (uint8_t i = 0; i < 64; i++) {
        // Tables
        // ... none yet

        for (uint8_t square = 0; square < 64; square++) {
            if (BB_PASSED[WHITE][i] & single_bit(square))
                BB_PASSED[BLACK][MIRROR_TABLE[i]] |= single_bit(MIRROR_TABLE[square]);
            if (BB_ISOLATED[WHITE][i] & single_bit(square))
                BB_ISOLATED[BLACK][MIRROR_TABLE[i]] |= single_bit(MIRROR_TABLE[square]);
            if (BB_IN_FRONT[WHITE][i] & single_bit(square))
                BB_IN_FRONT[BLACK][MIRROR_TABLE[i]] |= single_bit(MIRROR_TABLE[square]);
            if (BB_HOLE[WHITE][i] & single_bit(square))
                BB_HOLE[BLACK][MIRROR_TABLE[i]] |= single_bit(MIRROR_TABLE[square]);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Main evaluation functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

evaluator_t::evaluator_t(eval_params_t params, size_t pawn_hash_entries) {
    this->pawn_hash_entries = pawn_hash_entries;
    pawn_hash_table = new pawn_entry_t[pawn_hash_entries];

    // Material tables
    for (int i = 0; i < 5; ++i) {
        mat[i][MG] = params.mat_mg[i];
        mat[i][EG] = params.mat_eg[i];
    }

    // Four-way mirrored tables
    constexpr uint8_t square_mapping[16][4] = {{A4, A5, H4, H5},
                                               {B4, B5, G4, G5},
                                               {C4, C5, F4, F5},
                                               {D4, D5, E4, E5},
                                               {A3, A6, H3, H6},
                                               {B3, B6, G3, G6},
                                               {C3, C6, F3, F6},
                                               {D3, D6, E3, E6},
                                               {A2, A7, H2, H7},
                                               {B2, B7, G2, G7},
                                               {C2, C7, F2, F7},
                                               {D2, D7, E2, E7},
                                               {A1, A8, H1, H8},
                                               {B1, B8, G1, G8},
                                               {C1, C8, F1, F8},
                                               {D1, D8, E1, E8}};
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 4; j++) {
            pst[WHITE][KNIGHT][square_mapping[i][j]][MG] = params.n_pst_mg[i];
            pst[WHITE][KNIGHT][square_mapping[i][j]][EG] = params.n_pst_eg[i];
            pst[WHITE][QUEEN][square_mapping[i][j]][MG] = params.q_pst_mg[i];
            pst[WHITE][QUEEN][square_mapping[i][j]][EG] = params.q_pst_eg[i];
            pst[WHITE][KING][square_mapping[i][j]][EG] = params.k_pst_eg[i];
        }
    }

    // Vertically mirrored tables
    for (uint8_t sq = 0; sq < 64; sq++) {
        int param_index = 32 - ((file_index(sq) % 4) + rank_index(sq) * 4);

        if (sq >= 8 && sq < 56) {
            pst[WHITE][PAWN][sq][MG] = params.p_pst_mg[param_index - 4];
            pst[WHITE][PAWN][sq][EG] = params.p_pst_eg[param_index - 4];
        }
        pst[WHITE][BISHOP][sq][MG] = params.b_pst_mg[param_index];
        pst[WHITE][BISHOP][sq][EG] = params.b_pst_eg[param_index];
        pst[WHITE][ROOK][sq][MG] = params.r_pst_mg[param_index];
        pst[WHITE][ROOK][sq][EG] = params.r_pst_eg[param_index];
    }

    // King middlegame pst
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; ++j) {
            pst[WHITE][KING][(2 - i) * 8 + j][MG] = params.k_pst_mg[i * 8 + j];
        }
    }

    for (int i = 24; i < 64; i++) {
        pst[WHITE][KING][i][MG] = params.k_pst_exposed_mg;
    }

    // Mirror PST for black
    for (int piece = 0; piece < 6; piece++) {
        for (int square = 0; square < 64; square++) {
            pst[BLACK][piece][MIRROR_TABLE[square]][MG] = pst[WHITE][piece][square][MG];
            pst[BLACK][piece][MIRROR_TABLE[square]][EG] = pst[WHITE][piece][square][EG];
        }
    }
}

evaluator_t::~evaluator_t() {
    delete[] pawn_hash_table;
}

int evaluator_t::evaluate(const board_t &board) const {
    // Check endgame
    eg_eval_t eg_eval = eval_eg(board);
    if (eg_eval.valid) {
        return board.record[board.now].next_move ? -eg_eval.eval : +eg_eval.eval;
    }

    // Middlegame and endgame scores
    int mg = 0;
    int eg = 0;

    // Main evaluation function
    eval_material(board, mg, eg);
    eval_pst(board, mg, eg);
    eval_pawns(board, mg, eg);

    // Interpolate between middlegame and endgame scores
    double game_process = double(pop_count(board.bb_all) - 2) / 30.0;
    int total = int(game_process * eg + (1 - game_process) * mg);

    return board.record[board.now].next_move ? -total : total;
}

void evaluator_t::eval_material(const board_t &board, int &mg, int &eg) const {
    const int pawn_balance = (pop_count(board.bb_pieces[WHITE][PAWN]) - pop_count(board.bb_pieces[BLACK][PAWN]));
    mg += mat[PAWN][MG] * pawn_balance;
    eg += mat[PAWN][EG] * pawn_balance;

    const int knight_balance = (pop_count(board.bb_pieces[WHITE][KNIGHT]) - pop_count(board.bb_pieces[BLACK][KNIGHT]));
    mg += mat[KNIGHT][MG] * knight_balance;
    eg += mat[KNIGHT][EG] * knight_balance;

    const int bishop_balance = (pop_count(board.bb_pieces[WHITE][BISHOP]) - pop_count(board.bb_pieces[BLACK][BISHOP]));
    mg += mat[BISHOP][MG] * bishop_balance;
    eg += mat[BISHOP][EG] * bishop_balance;

    const int rook_balance = (pop_count(board.bb_pieces[WHITE][ROOK]) - pop_count(board.bb_pieces[BLACK][ROOK]));
    mg += mat[ROOK][MG] * rook_balance;
    eg += mat[ROOK][EG] * rook_balance;

    const int queen_balance = (pop_count(board.bb_pieces[WHITE][QUEEN]) - pop_count(board.bb_pieces[BLACK][QUEEN]));
    mg += mat[QUEEN][MG] * queen_balance;
    eg += mat[QUEEN][EG] * queen_balance;
}

void evaluator_t::eval_pst(const board_t &board, int &mg, int &eg) const {
    U64 pieces;
    for (int type = 1; type < 5; type++) {
        pieces = board.bb_pieces[WHITE][type];
        while (pieces) {
            uint8_t sq = pop_bit(pieces);
            mg += pst[WHITE][type][sq][MG];
            eg += pst[WHITE][type][sq][EG];
        }

        pieces = board.bb_pieces[BLACK][type];
        while (pieces) {
            uint8_t sq = pop_bit(pieces);
            mg -= pst[BLACK][type][sq][MG];
            eg -= pst[BLACK][type][sq][EG];
        }
    }
}

void evaluator_t::eval_pawns(const board_t &board, int &mg, int &eg) const {
    // PST
    U64 pawns;
    pawns = board.bb_pieces[WHITE][PAWN];
    while (pawns) {
        uint8_t sq = pop_bit(pawns);
        mg += pst[WHITE][PAWN][sq][MG];
        eg += pst[WHITE][PAWN][sq][EG];
    }

    pawns = board.bb_pieces[BLACK][PAWN];
    while (pawns) {
        uint8_t sq = pop_bit(pawns);
        mg -= pst[BLACK][PAWN][sq][MG];
        eg -= pst[BLACK][PAWN][sq][EG];
    }
}

