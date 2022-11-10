#include <string>
#include <istream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <fstream>

// convert the input file into a list of event
typedef enum __dir_t {N, W, S, E} dir_t;

class event {
public:
    bool isdeadlock_msg = false;
    bool arrived;   // true for arrived, false for exit
    dir_t dir;        // 0 - 'N', 1 - 'W', 2 - 'S', 3 - 'E'

    event(bool arrived, dir_t dir, bool deadlk_msg = false) : isdeadlock_msg(deadlk_msg), arrived(arrived), dir(dir) {};
};

// stores number of trains waiting at a junction
int num_trains[4] = {0, 0, 0, 0};  

std::vector<event> parse_file(std::istream &input_file) {
    std::vector<event> input;
    std::string line;
    while (std::getline(input_file, line)) {
        if (line.find("Arrived") != std::string::npos) {
            switch(line.at(35)) {
                case 'N':
                    input.push_back(event(true, N));
                    break;
                case 'W':
                    input.push_back(event(true, N));
                    break;
                case 'S':
                    input.push_back(event(true, N));
                    break;
                case 'E':
                    input.push_back(event(true, N));
                    break;
                default:
                    fprintf(stderr, "invalid input : arrived format is not correct\n");
                    exit(1);
            }
            continue;
        }
        if (line.find("Exited") != std::string::npos) {
            switch(line.at(31)) {
                case 'N':
                    input.push_back(event(false, N));
                    break;
                case 'W':
                    input.push_back(event(false, N));
                    break;
                case 'S':
                    input.push_back(event(false, N));
                    break;
                case 'E':
                    input.push_back(event(false, N));
                    break;
                default:
                    fprintf(stderr, "invalid input : exited format is not correct\n");
                    exit(1);
            }
            continue;
        }
        // deadlock message
        assert(line.find("Deadlock") != std::string::npos);
        input.push_back(event(false, N, true)); // arrived/dir value don't signify anything
    }
    return input;
}


std::string get_str(dir_t dir) {
    switch (dir)
    {
    case N:
        return "North";
    case S:
        return "South";
    case E:
        return "East";
    case W:
        return "West";
    default:
        break;
    }
}

int safety_check(std::vector<event> &input) {
    unsigned int i = 0;
    bool deadlk_check = false;
    for(auto &evnt : input) {
        if (deadlk_check) {
            // prev line was deadlk resolve line
            if (evnt.isdeadlock_msg) {
                fprintf(stderr, "invalid output: double deadlock resolution messages at line : %d\n", i+1);
                exit(1);
            }
            if (evnt.arrived) {
                fprintf(stderr, "invalid output: a train arrived while resolving deadlock at line : %d\n", i+1);
                exit(1);
            }
            // evnt.arrived = false && evnt.isdeadlock_msg = false
            num_trains[evnt.dir]--; // allow the train to pass through regardless
            deadlk_check = false;
            continue;
        }

        // prev line is not deadlk resolve message
        if (!evnt.isdeadlock_msg) {
            if (!evnt.arrived) {
                // train exiting evnt.dir
                if (!num_trains[evnt.dir]) {
                    fprintf(stderr, "exiting from direction : %s", get_str(evnt.dir).c_str());
                    exit(1);
                }
                switch(evnt.dir) {
                    case N:
                        if (num_trains[W]!=0) {
                            fprintf(stderr, "train crash: train exiting north when train on west stop, line number : %d\n", i+1);
                            exit(1);
                        }
                        break;
                    case W:
                        if (num_trains[S]!=0) {
                            fprintf(stderr, "train crash: train exiting west when train on south stop, line number : %d\n", i+1);
                            exit(1);
                        }
                        break;
                    case S:
                        if (num_trains[E]!=0) {
                            fprintf(stderr, "train crash: train exiting south when train on east stop, line number : %d\n", i+1);
                            exit(1);
                        }
                        break;
                    case E:
                        if (num_trains[N]!=0) {
                            fprintf(stderr, "train crash: train exiting east when train on north stop, line number : %d\n", i+1);
                            exit(1);
                        }
                        break;
                }
                // no crash, release that train
                num_trains[evnt.dir]--;
            } else {
                // train arrived, put the train in waiting queue
                num_trains[evnt.dir]++;
            }
        } else {
            // evnt.is_deadlk message
            deadlk_check = true;
        }
        i++;
    }
    return 0;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage : ./check <filename>\n");
        exit(1);
    }

    char *filename = argv[1];
    std::filebuf fb;
    if (fb.open(filename, std::ios::in)) {
        std::istream input_stream(&fb);
        auto input = parse_file(input_stream);
        int retv = safety_check(input);
        if (retv == 0) {
            printf("safety check passed\n");
        }
        fb.close();
    }
    return 0;
}