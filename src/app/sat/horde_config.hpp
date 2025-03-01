
#ifndef DOMPASCH_MALLOB_HORDE_CONFIG_HPP
#define DOMPASCH_MALLOB_HORDE_CONFIG_HPP

#include <map>
#include <sstream>
#include <iostream>

#include "util/params.hpp"

class Job; // forward definition

struct HordeConfig {

    float starttime;
    int apprank;
    int mpirank;
    int mpisize;

    int jobid;
    bool incremental;
    int firstrev;
    int threads;
    int maxBroadcastedLitsPerCycle;

    HordeConfig() {}
    HordeConfig(const Parameters& params, const Job& job);
    HordeConfig(const std::string& packed) {
        std::stringstream s_stream(packed);
        std::string substr;
        getline(s_stream, substr, ','); starttime = atof(substr.c_str());
        getline(s_stream, substr, ','); apprank = atoi(substr.c_str());
        getline(s_stream, substr, ','); mpirank = atoi(substr.c_str());
        getline(s_stream, substr, ','); mpisize = atoi(substr.c_str());
        getline(s_stream, substr, ','); jobid = atoi(substr.c_str());
        getline(s_stream, substr, ','); incremental = substr == "1";
        getline(s_stream, substr, ','); firstrev = atoi(substr.c_str());
        getline(s_stream, substr, ','); threads = atoi(substr.c_str());
        getline(s_stream, substr, ','); maxBroadcastedLitsPerCycle = atoi(substr.c_str());
    }

    std::string toString() const {
        std::string out = "";
        out += std::to_string(starttime) + ",";
        out += std::to_string(apprank) + ",";
        out += std::to_string(mpirank) + ",";
        out += std::to_string(mpisize) + ",";
        out += std::to_string(jobid) + ",";
        out += std::to_string(incremental?1:0) + ",";
        out += std::to_string(firstrev) + ",";
        out += std::to_string(threads) + ",";
        out += std::to_string(maxBroadcastedLitsPerCycle);
        return out;
    }

    std::string getJobStr() const {
        return "#" + std::to_string(jobid) + ":" + std::to_string(apprank);
    }
};

#endif