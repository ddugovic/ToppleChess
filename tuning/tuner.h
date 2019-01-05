//
// Created by Vincent Tang on 2019-01-04.
//

#ifndef TOPPLE_TUNER_H
#define TOPPLE_TUNER_H

#include <vector>

#include "../board.h"
#include "../eval.h"

class tuner_t {
    eval_params_t current_params;
    double scaling_constant;

    size_t entries;
    std::vector<board_t> positions;
    std::vector<double> results;
public:
    tuner_t(size_t entries, std::vector<board_t> positions, std::vector<double> results);
 
private:
    double find_scaling_constant();
    double momentum_optimise(int *parameter, double current_mea);
    int quiesce(board_t &board, int alpha, int beta, evaluator_t &evaluator);
    double sigmoid(double score);
    double mean_evaluation_error();
};


#endif //TOPPLE_TUNER_H
