#ifndef TESTING_HH
#define TESTING_HH

#include "utils/core.hh"
#include <vector>
#include <memory>
#include <string>

void testFDFilter(std::shared_ptr<INIReader> config,
                  const std::vector<core::Record> &records,
                  long mem_size);

void testDelaySketch(std::shared_ptr<INIReader> config,
                     const std::vector<core::Record> &records,
                     long mem_size);

void testJitterSketch(std::shared_ptr<INIReader> config,
                      const std::vector<core::Record>& records,
                      long mem_size);

void testJitterSketchS1Opt(std::shared_ptr<INIReader> config,
                           const std::vector<core::Record>& records,
                           long mem_size);

#endif // TESTING_HH