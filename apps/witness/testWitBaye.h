
//testWitBaye.h

#pragma once
#include <queue>
#include <utility>

// forward declare — we only need to know it exists
enum WitnessAction;

struct WitnessMove {
    std::pair<int, int> posBefore;
    std::pair<int, int> posAfter;
    WitnessAction action;
};

// shared queue between Witness.h and Driver.cpp
inline std::queue<WitnessMove> witnessMoveQueue;
