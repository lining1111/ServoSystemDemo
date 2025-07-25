//
// Created by lining on 2/14/25.
//

#include "progressbar.h"
#include <chrono>

using namespace std::chrono_literals;

void test_progressbar() {
    indicators::ProgressBar bar;
    bar.set_option(indicators::option::PrefixText{"Loading"});
    bar.set_option(indicators::option::ShowPercentage{true});
    bar.set_option(indicators::option::BarWidth{50});
    bar.set_option(indicators::option::Fill{"█"});
    bar.set_option(indicators::option::Lead{"█"});
    bar.set_option(indicators::option::Remainder{"-"});

    for (size_t i = 0; i <= 100; ++i) {
        bar.set_progress(i);
        std::this_thread::sleep_for(25ms);
    }

    bar.mark_as_completed();
}

void printProgressBar(int completed, int total, char fillChar, int width) {
    // Calculate the completed and remaining parts of the progress bar.
    double progress = static_cast<double>(completed) / total;
    int filledWidth = static_cast<int>(width * progress);
    int emptyWidth = width - filledWidth;

    // print the progress bar.
    std::cout << "[";
    for (int i = 0; i < filledWidth; ++i) {
        std::cout << fillChar;
    }
    for (int i = 0; i < emptyWidth; ++i) {
        std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(2) << progress * 100.0 << " %\r";
    std::cout.flush();
}