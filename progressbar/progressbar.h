//
// Created by lining on 2/14/25.
//

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <indicators/progress_bar.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>

void test_progressbar();

void printProgressBar(int completed, int total, char fillChar, int width = 50);

#endif //PROGRESSBAR_H
