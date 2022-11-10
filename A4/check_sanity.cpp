#include <string>
#include <iostream>

class output_line {
    bool arrived;   // true for arrived, false for exit
    int dir;
};

int isdeadlock = 0;
int num_trains[4];

int safety_check()