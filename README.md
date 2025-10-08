# JitterSketch: Finding Jittery Flows in Network Streams

This repository contains the source code for the research paper, "JitterSketch: Finding Jittery Flows in Network Streams". The project introduces JitterSketch, a novel sketch-based algorithm designed for the high-precision, real-time detection of jittery flows in high-speed networks.

## Repository Structure

The repository is organized into the following main directories:

* `src/`: Contains the main application logic and entry point.
    * `sketch/`: Implementation of JitterSketch and other sketch-based data structures.
    * `optimizer/`: Jitter control optimization algorithms, including the OLDC algorithm and a JitterSketch-enhanced version.
    * `experiment/`: Code for running jitter detection and control experiments.
    * `utils/`: Utility functions for hashing, data loading, and configuration parsing.
* `settings.conf`: The configuration file for setting experiment parameters.

## Getting Started

### Building the Project

1.  Clone the repository:
    ```bash
    git clone [https://github.com/jittersketch/JitterSketch.git](https://github.com/jittersketch/JitterSketch.git)
    cd JitterSketch
    ```

2.  Create a build directory and run CMake:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```

3.  Compile the source code:
    ```bash
    make
    ```
    This will generate an executable named `main` in the `build` directory.

### Running the Experiments

To run the experiments, execute the `main` program from the `build` directory, passing the path to the configuration file as an argument.

```bash
./main ../settings.conf