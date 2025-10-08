#include "testing.hh"
#include "test.hh"
#include "sketch/FDFilter.hh"
#include "sketch/DelaySketch.hh"
#include "sketch/JitterSketch.hh"
#include "sketch/JitterSketchS1Opt.hh"
#include "utils/hash.hh"
#include <iostream>

void testFDFilter(std::shared_ptr<INIReader> config,
                  const std::vector<core::Record> &records,
                  long mem_size) {
    uint64_t delay_thres = config->GetInteger("FDFilter", "delay_thres", 0);
    double jitter_factor = config->GetReal("general", "jitter_factor", 2.0);
    uint64_t min_absolute_jitter_thres = config->GetInteger("general", "min_absolute_jitter_thres", 500);
    uint64_t max_ifpd_diff = config->GetInteger("general", "max_ifpd_diff", 1000000);
    int jitter_detection_mode = config->GetInteger("general", "jitter_detection_mode", 2);
    int frequency_threshold = config->GetInteger("general", "frequency_threshold", 30);

    int k = config->GetInteger("FDFilter", "k", 0);
    int kk = config->GetInteger("FDFilter", "kk", 0);
    int num_hash = config->GetInteger("FDFilter", "num_hash", 0);
    int gnum_hash = config->GetInteger("FDFilter", "gnum_hash", 0);
    long nbits_conf = config->GetInteger("FDFilter", "nbits", 0);
    long gnbits_conf = config->GetInteger("FDFilter", "gnbits", 0);
    double ifpd_map_ratio = config->GetReal("FDFilter", "ifpd_map_ratio", 0.1);
    double cm_sketch_ratio = config->GetReal("FDFilter", "cm_sketch_ratio", 0.1);
    int cm_depth = config->GetInteger("FDFilter", "cm_depth", 4);

    size_t ifpd_entry_size = sizeof(std::pair<FlowKey<13>, uint64_t>::first_type) + sizeof(std::pair<FlowKey<13>, uint64_t>::second_type);
    size_t cm_entry_size = sizeof(uint32_t);

    size_t ifpd_map_mem_bytes = static_cast<size_t>(mem_size * ifpd_map_ratio);
    size_t ifpd_map_size = ifpd_entry_size > 0 ? ifpd_map_mem_bytes / ifpd_entry_size : 0;

    size_t cm_sketch_mem_bytes = static_cast<size_t>(mem_size * cm_sketch_ratio);
    int cm_width = (cm_depth > 0 && cm_entry_size > 0) ? cm_sketch_mem_bytes / (cm_depth * cm_entry_size) : 0;

    long bf_mem_bytes = mem_size - ifpd_map_mem_bytes - cm_sketch_mem_bytes;
    uint64_t bf_mem_bits = bf_mem_bytes > 0 ? bf_mem_bytes * 8 : 0;
    uint64_t total_conf_ratio_units = (uint64_t)(k + 1) * kk * nbits_conf + gnbits_conf;
    int gnbits = 0;
    int nbits = 0;
    if (total_conf_ratio_units > 0 && bf_mem_bits > 0) {
        gnbits = static_cast<int>((bf_mem_bits * gnbits_conf) / total_conf_ratio_units);
        uint64_t all_bfs_bits = bf_mem_bits - gnbits;
        if ((k + 1) * kk > 0) {
            nbits = static_cast<int>(all_bfs_bits / ((k + 1) * kk));
        }
    }

    printf("--- FDFilter Test ---\n");
    sketch::FDFilter<hash::AwareHash> fd_filter(k, kk, nbits, num_hash, gnbits, gnum_hash,
                                                delay_thres, jitter_factor, min_absolute_jitter_thres,
                                                max_ifpd_diff, ifpd_map_size, cm_width, cm_depth, jitter_detection_mode, frequency_threshold);
    jitterTest(fd_filter, records, jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold, mem_size);
}

void testDelaySketch(std::shared_ptr<INIReader> config,
                     const std::vector<core::Record> &records,
                     long mem_size) {
    double jitter_factor = config->GetReal("general", "jitter_factor", 2.0);
    uint64_t min_absolute_jitter_thres = config->GetInteger("general", "min_absolute_jitter_thres", 500);
    uint64_t max_ifpd_diff = config->GetInteger("general", "max_ifpd_diff", 1000000);
    int jitter_detection_mode = config->GetInteger("general", "jitter_detection_mode", 2);
    int frequency_threshold = config->GetInteger("general", "frequency_threshold", 30);

    int d = config->GetInteger("DelaySketch", "d", 4);
    double ifpd_map_ratio = config->GetReal("DelaySketch", "ifpd_map_ratio", 0.3);
    double cm_sketch_ratio = config->GetReal("DelaySketch", "cm_sketch_ratio", 0.3);
    int cm_depth = config->GetInteger("DelaySketch", "cm_depth", 4);

    size_t ifpd_entry_size = sizeof(std::pair<FlowKey<13>, uint64_t>::first_type) + sizeof(std::pair<FlowKey<13>, uint64_t>::second_type);
    size_t cm_entry_size = sizeof(uint32_t);
    size_t ds_bucket_size = sizeof(sketch::DelaySketchBucket::fp) + sizeof(sketch::DelaySketchBucket::t);

    size_t ifpd_map_mem_bytes = static_cast<size_t>(mem_size * ifpd_map_ratio);
    size_t ifpd_map_size = ifpd_entry_size > 0 ? ifpd_map_mem_bytes / ifpd_entry_size : 0;

    size_t cm_sketch_mem_bytes = static_cast<size_t>(mem_size * cm_sketch_ratio);
    int cm_width = (cm_depth > 0 && cm_entry_size > 0) ? cm_sketch_mem_bytes / (cm_depth * cm_entry_size) : 0;

    long delay_sketch_mem_bytes = mem_size - ifpd_map_mem_bytes - cm_sketch_mem_bytes;
    int w = 0;
    if (d > 0 && delay_sketch_mem_bytes > 0 && ds_bucket_size > 0) {
        w = delay_sketch_mem_bytes / (d * ds_bucket_size);
    }

    printf("--- DelaySketch Test ---\n");
    sketch::DelaySketch<hash::AwareHash> delay_sketch(d, w, jitter_factor, min_absolute_jitter_thres,
                                                      max_ifpd_diff, ifpd_map_size, cm_width, cm_depth, jitter_detection_mode, frequency_threshold);
    jitterTest(delay_sketch, records, jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold, mem_size);
}

void testJitterSketch(std::shared_ptr<INIReader> config,
                      const std::vector<core::Record>& records,
                      long mem_size) {
    double jitter_factor = config->GetReal("general", "jitter_factor", 2.0);
    uint64_t min_absolute_jitter_thres = config->GetInteger("general", "min_absolute_jitter_thres", 500);
    uint64_t max_ifpd_diff = config->GetInteger("general", "max_ifpd_diff", 1000000);
    int jitter_detection_mode = config->GetInteger("general", "jitter_detection_mode", 2);
    int frequency_threshold = config->GetInteger("general", "frequency_threshold", 30);

    double s1_ratio = config->GetReal("JitterSketch", "stage_one_ratio", 0.2);
    double s2_ratio = config->GetReal("JitterSketch", "stage_two_ratio", 0.4);
    int d3 = config->GetInteger("JitterSketch", "d3", 4);

    size_t s1_bucket_size = sizeof(sketch::JitterSketchStageOneBucket);
    size_t s2_bucket_size = sizeof(sketch::JitterSketchStageTwoBucket);
    size_t s3_entry_size = sizeof(sketch::JitterSketchStageThreeEntry);

    size_t s1_mem_bytes = static_cast<size_t>(mem_size * s1_ratio);
    int w1 = s1_bucket_size > 0 ? s1_mem_bytes / s1_bucket_size : 0;

    size_t s2_mem_bytes = static_cast<size_t>(mem_size * s2_ratio);
    int w2 = s2_bucket_size > 0 ? s2_mem_bytes / s2_bucket_size : 0;

    long s3_mem_bytes = mem_size - s1_mem_bytes - s2_mem_bytes;
    int w3 = 0;
    if (d3 > 0 && s3_mem_bytes > 0 && s3_entry_size > 0) {
        w3 = s3_mem_bytes / (d3 * s3_entry_size);
    }

    printf("--- JitterSketch Test ---\n");
    sketch::JitterSketch<hash::AwareHash> jitter_sketch(w1, w2, w3, d3, jitter_factor,
                                                        min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold);
    jitterTest(jitter_sketch, records, jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold, mem_size);
}

void testJitterSketchS1Opt(std::shared_ptr<INIReader> config,
                           const std::vector<core::Record>& records,
                           long mem_size) {
    double jitter_factor = config->GetReal("general", "jitter_factor", 2.0);
    uint64_t min_absolute_jitter_thres = config->GetInteger("general", "min_absolute_jitter_thres", 500);
    uint64_t max_ifpd_diff = config->GetInteger("general", "max_ifpd_diff", 1000000);
    int jitter_detection_mode = config->GetInteger("general", "jitter_detection_mode", 2);
    int frequency_threshold = config->GetInteger("general", "frequency_threshold", 30);

    double s1_ratio = config->GetReal("JitterSketchS1Opt", "stage_one_ratio", 0.2);
    double s2_ratio = config->GetReal("JitterSketchS1Opt", "stage_two_ratio", 0.4);
    int d3 = config->GetInteger("JitterSketchS1Opt", "d3", 4);
    int s1_hash_num = config->GetInteger("JitterSketchS1Opt", "s1_hash_num", 3);
    size_t s1_bucket_size = sizeof(sketch::JitterSketchS1OptStageOneBucket);
    size_t s2_bucket_size = sizeof(sketch::JitterSketchS1OptStageTwoBucket);
    size_t s3_entry_size = sizeof(sketch::JitterSketchS1OptStageThreeEntry);

    size_t s1_mem_bytes = static_cast<size_t>(mem_size * s1_ratio);
    int w1 = s1_bucket_size > 0 ? s1_mem_bytes / s1_bucket_size : 0;

    size_t s2_mem_bytes = static_cast<size_t>(mem_size * s2_ratio);
    int w2 = s2_bucket_size > 0 ? s2_mem_bytes / s2_bucket_size : 0;

    long s3_mem_bytes = mem_size - s1_mem_bytes - s2_mem_bytes;
    int w3 = 0;
    if (d3 > 0 && s3_mem_bytes > 0 && s3_entry_size > 0) {
        w3 = s3_mem_bytes / (d3 * s3_entry_size);
    }

    printf("--- JitterSketchS1Opt Test ---\n");
    sketch::JitterSketchS1Opt<hash::AwareHash> jitter_sketch_s1_opt(w1, w2, w3, d3, s1_hash_num, jitter_factor,
                                                                    min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold);
    jitterTest(jitter_sketch_s1_opt, records, jitter_factor, min_absolute_jitter_thres, max_ifpd_diff, jitter_detection_mode, frequency_threshold, mem_size);
}