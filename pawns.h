//
// Created by Vincent on 01/06/2019.
//

#ifndef TOPPLE_PAWNS_H
#define TOPPLE_PAWNS_H


#include <array>

#include "types.h"
#include "bb.h"

struct processed_params_t;
class board_t;

namespace pawns {
    constexpr U64 not_A = 0xfefefefefefefefe; // ~0x0101010101010101
    constexpr U64 not_H = 0x7f7f7f7f7f7f7f7f; // ~0x8080808080808080

    // Bitboard shifting
    template<Direction D>
    constexpr U64 shift(U64 bb);

    template<>
    constexpr U64 shift<D_N>(U64 b) { return b << 8u; }

    template<>
    constexpr U64 shift<D_S>(U64 b) { return b >> 8u; }

    template<>
    constexpr U64 shift<D_E>(U64 b) { return (b << 1u) & not_A; }

    template<>
    constexpr U64 shift<D_NE>(U64 b) { return (b << 9u) & not_A; }

    template<>
    constexpr U64 shift<D_SE>(U64 b) { return (b >> 7u) & not_A; }

    template<>
    constexpr U64 shift<D_W>(U64 b) { return (b >> 1u) & not_H; }

    template<>
    constexpr U64 shift<D_SW>(U64 b) { return (b >> 9u) & not_H; }

    template<>
    constexpr U64 shift<D_NW>(U64 b) { return (b << 7u) & not_H; }

    // General utility
    template<Team team> constexpr U64 fill_forward(U64 bb);
    template<> constexpr U64 fill_forward<WHITE>(U64 bb) {
        bb |= (bb << 8u);
        bb |= (bb << 16u);
        bb |= (bb << 32u);
        return bb;
    }
    template<> constexpr U64 fill_forward<BLACK>(U64 bb) {
        bb |= (bb >> 8u);
        bb |= (bb >> 16u);
        bb |= (bb >> 32u);
        return bb;
    }

    template<Team team>
    constexpr U64 stop_squares(U64 bb) {
        return shift<rel_offset(team, D_N)>(bb);
    }

    template<Team team>
    constexpr U64 front_span(U64 bb) {
        return fill_forward<team>(stop_squares<team>(bb));
    }

    template<Team team>
    constexpr U64 rear_span(U64 bb) {
        return front_span<Team(!team)>(bb);
    }

    template<Team team>
    constexpr U64 open_pawns(U64 own, U64 other) {
        return own & ~front_span<Team(!team)>(other);
    }

    template<Team team>
    constexpr U64 telestop_squares(U64 bb) {
        return front_span<team>(bb) ^ stop_squares<team>(bb);
    }

    constexpr U64 fill_file(U64 bb) {
        return fill_forward<WHITE>(bb) | fill_forward<BLACK>(bb);
    }

    // Files
    constexpr uint8_t file_set(U64 bb) {
        bb |= (bb >> 8u);
        bb |= (bb >> 16u);
        bb |= (bb >> 32u);
        return static_cast<uint8_t>(bb);
    }

    template<Direction D>
    constexpr uint8_t borders(uint8_t fs);

    template<>
    constexpr uint8_t borders<D_E>(uint8_t fs) {
        return fs & (fs ^ (fs >> 1u));
    }

    template<>
    constexpr uint8_t borders<D_W>(uint8_t fs) {
        return fs & (fs ^ (fs << 1u));
    }

    constexpr U64 half_or_open_files(U64 bb) {
        return ~fill_file(bb);
    }

    constexpr U64 open_files(U64 w, U64 b) {
        return half_or_open_files(w) & half_or_open_files(b);
    }

    // Metrics
    inline int island_count(U64 bb) {
        return pop_count(borders<D_E>(file_set(bb)));
    }

    template<Team team>
    inline int distortion(U64 bb) {
        U64 fill = fill_forward<Team(!team)>(bb);
        return pop_count((fill ^ (fill << 1u)) & not_A);
    }

    // Attacks
    template<Team team>
    constexpr U64 right_attacks(U64 bb) {
        return shift<rel_offset(team, D_NE)>(bb);
    }

    template<Team team>
    constexpr U64 left_attacks(U64 bb) {
        return shift<rel_offset(team, D_NW)>(bb);
    }

    template<Team team>
    constexpr U64 attacks(U64 bb) {
        return left_attacks<team>(bb) | right_attacks<team>(bb);
    }

    template<Team team>
    constexpr U64 double_attacks(U64 bb) {
        return left_attacks<team>(bb) & right_attacks<team>(bb);
    }

    template<Team team>
    U64 single_attacks(U64 bb) {
        return left_attacks<team>(bb) ^ right_attacks<team>(bb);
    }

    template<Team team>
    constexpr U64 holes(U64 bb) {
        return ~fill_forward<team>(attacks<team>(bb));
    }

    // Pawn identification
    template<Team team>
    constexpr U64 doubled(U64 bb) {
        return front_span<team>(bb) & bb;
    }

    constexpr U64 isolated(U64 bb) {
        return bb & ~fill_file(shift<D_E>(bb)) & ~fill_file(shift<D_W>(bb));
    }

    template<Team team>
    constexpr U64 backward(U64 own, U64 other) {
        return stop_squares<Team(!team)>(
                stop_squares<team>(own)
                & ~fill_forward<team>(left_attacks<team>(own) | right_attacks<team>(own))
                & (left_attacks<Team(!team)>(other) | right_attacks<Team(!team)>(other))
        );
    }

    template<Team team>
    constexpr U64 straggler(U64 own, U64 other, U64 own_backwards) {
        return own_backwards & open_pawns<team>(own, other) & (team ? 0x00ffff0000000000ull : 0x0000000000ffff00ull);
    }

    template<Team team>
    constexpr U64 forward_coverage(U64 bb) {
        U64 spans = front_span<team>(bb);
        return spans | shift<D_E>(spans) | shift<D_W>(spans);
    }

    template<Team team>
    constexpr U64 passed(U64 own, U64 other) {
        return own & ~forward_coverage<Team(!team)>(other);
    }

    template<Team team>
    constexpr U64 candidates(U64 own, U64 other) {
        U64 other_double_attacks = double_attacks<Team(!team)>(other);
        U64 other_single_attacks = single_attacks<Team(!team)>(other);
        U64 own_double_attacks = double_attacks<team>(own);
        U64 own_single_attacks = single_attacks<team>(own);

        return open_pawns<team>(own, other) & stop_squares<Team(!team)>
                ((own_single_attacks & other_single_attacks) | (own_double_attacks & other_double_attacks));
    }

    // Represents a pawn structure
    class structure_t {
    public:
        structure_t() = default;
        structure_t(const processed_params_t &evaluator, U64 pawn_hash, U64 w_pawns, U64 b_pawns);
        void eval_dynamic(const processed_params_t &evaluator, const board_t &board, int &mg, int &eg) const;

        inline U64 get_hash() const {
            return hash;
        }

        inline int32_t get_eval_mg() const {
            return eval_mg;
        }

        inline int32_t get_eval_eg() const {
            return eval_eg;
        }
    private:
        U64 hash = 0;

        int32_t eval_mg = 0;
        int32_t eval_eg = 0;
    };
}


#endif //TOPPLE_PAWNS_H
