#!/bin/sh
set -e
SRC="balatro.cpp clone.cpp blinds.cpp balatro_rng.cpp hand_eval.cpp jokers/*.cpp consumables/*.cpp"
# debug: sanitizers turn UB into loud crashes
g++ -std=c++20 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-parameter \
    tests.cpp $SRC -o run_tests
g++ -std=c++20 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-parameter \
    main.cpp $SRC -o naneinf_debug
# fast: for training / monte carlo
g++ -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter \
    main.cpp $SRC -o naneinf
