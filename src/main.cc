#include "utils/core.hh"
#include "experiment/testing.hh"
#include "experiment/JitterControlExperiment.hh"
#include "optimizer/OLDCOptimizer.hh"
#include "optimizer/JitterSketchOptimizer.hh"
#include <string>
#include <memory>
#include <algorithm>
#include <set>
int main(int argc, char *argv[]) {
    std::string config_file;
    if (argc > 1) {
        config_file = argv[1];
    } else {
        printf("Please set config path!\n");
        return 1;
    }

    auto config = core::load_settings(config_file);
    if (!config || config->ParseError() < 0) {
        printf("Can't load '%s', trying 'settings.conf' in current directory.\n", config_file.c_str());
        return 1;
    }
    printf("Successfully loaded config file: %s\n\n", config_file.c_str());

    std::string data_file = config->Get("general", "data_file", "");
    auto records = core::load_records(data_file);

    long mem_size = config->GetInteger("general", "mem_size", 0);


    std::set<FlowKey<13>> s;
    std::transform(records.begin(), records.end(), std::inserter(s, s.begin()),
                   [](auto &record) { return record.flowkey_; });
    printf("Flow number is %ld\n", s.size());

    printf("\n\n###########################################################\n");
    printf("#####         STARTING JITTER DETECT EXPERIMENT       #####\n");
    printf("###########################################################\n\n");


    testFDFilter(config, records, mem_size);
    testDelaySketch(config, records, mem_size);
    testJitterSketch(config, records, mem_size);
    testJitterSketchS1Opt(config, records, mem_size);

    printf("\n\n###########################################################\n");
    printf("#####         STARTING JITTER DETECT EXPERIMENT       #####\n");
    printf("###########################################################\n\n");


    printf("\n\n###########################################################\n");
    printf("#####         STARTING JITTER CONTROL EXPERIMENT       #####\n");
    printf("###########################################################\n\n");

    {
        printf("--- Running Standard OLDC Experiment ---\n");
        auto algo_b_optimizer = std::make_shared<OLDCOptimizer>();
        algo_b_optimizer->configure(config);
        JitterControlExperiment experiment(records, algo_b_optimizer, config);
        experiment.run();
    }

    {
        printf("\n--- Running JitterSketch-Optimized Experiment ---\n");
        auto dj_optimized_algo_b = std::make_shared<JitterSketchOptimizer>();
        dj_optimized_algo_b->configure(config);
        JitterControlExperiment experiment(records, dj_optimized_algo_b, config);
        experiment.run();
    }

    printf("\n###########################################################\n");
    printf("#####          JITTER CONTROL EXPERIMENT END         #####\n");
    printf("###########################################################\n\n\n");

    return 0;
}