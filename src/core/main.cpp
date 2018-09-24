#include "utilities/Parallel.h"
#include "Solver.h"

#include <iostream>
#include <thread>
#include <utilities/SerializationUtils.h>

// Gases masses
// 133 - 133 Cs
// 88 -  88 Kr -> Rb -> Sr
// 138 - 138 Xe -> Cs -> Ba
// 88 - 88 Rb
// 88 - 88 Sr
// 138 - 138 Cs
// 138 - 138 Ba

// Beta chains setup
// 1, 3, 4, 6.78e-5, 6.49e-4
// 2, 5, 6, 6.78e-5, 6.49e-4

int main(int argc, char* argv[]) {
    Parallel::init(&argc, &argv);

    std::string configFilename = "../../config3D.json";

    std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> startTime;
    if (Parallel::isMaster()) {
        startTime = std::chrono::steady_clock::now();
    }

    // Print off a hello world message
    if (Parallel::isMaster()) {
        std::cout << "Starting solver with " << Parallel::getSize() << " nodes" << std::endl << std::endl;
    }

    // Create config
    if (Parallel::isSingle() == false) {
        if (Parallel::isMaster() == true) {

            // load config
            Config::getInstance()->load(configFilename);
            Config::getInstance()->init();

            // send to other processes
            for (int processor = 1; processor < Parallel::getSize(); processor++) {
                Parallel::send(SerializationUtils::serialize(Config::getInstance()), processor, Parallel::COMMAND_CONFIG);
            }
        } else {

            // get mesh from master process
            Config* config = nullptr;
            SerializationUtils::deserialize(Parallel::recv(0, Parallel::COMMAND_CONFIG), config);
            config->getImpulseSphere()->init();
            Config::setInstance(config);
        }
    } else {

        // load mesh
        Config::getInstance()->load(configFilename);
        Config::getInstance()->init();
    }

    if (Parallel::isMaster()) {
        std::cout << "Configuration ------------------------------------------------------------" << std::endl;
        std::cout << *Config::getInstance() << std::endl;
        std::cout << "--------------------------------------------------------------------------" << std::endl;
        std::cout << std::endl;
    }

    try {
        Solver solver;
        solver.init();
        solver.run();
    } catch (const std::exception& e) {
        std::cout << std::endl;
        std::cout << "Exception ----------------------------------------------------------------" << std::endl;
        std::cout << e.what() << std::endl;
        std::cout << "--------------------------------------------------------------------------" << std::endl;
        std::cout << std::endl;

        Parallel::abort();
    }

    if (Parallel::isMaster()) {
        auto now = std::chrono::steady_clock::now();
        auto wholeTime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::cout << "It took - " << wholeTime << " seconds" << std::endl;
    }

    Parallel::finalize();
}
