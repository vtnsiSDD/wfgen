
#include <iostream>
#include <stdio.h>
#include <string>
#include <csignal>
#include <atomic>
#include <functional>
#include <fstream>

#include "logger.hh"

static std::atomic<bool> continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "\nLOGGER_SERVER ---> ctrl+c received --> exiting\n";
    continue_running = false;
}


int main(int arg_c, char **arg_v){
    if(arg_c < 2){
        std::cout << " Usage : " << arg_v[0] << " <log filename> [binding addr:port]\n";
    }
    std::string filename("logger_file.log");
    std::string frontend("tcp://127.0.0.1:40000");
    if(arg_c >= 3)
        frontend = std::string(arg_v[2]);
    if(arg_c >= 2)
        filename = std::string(arg_v[1]);
    std::fstream fout{filename, fout.binary | fout.trunc | fout.in | fout.out};
    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "Starting the logger server -- spinning up workers\n";
    logger_server log_s("",fout,frontend,"tcp://127.0.1.1:40001");
    log_s << logger::set_level(logger::INFO) << "logger server -- started\n";
    log_s.commit();log_s.flush();

    double flush_period = 1.0;
    auto last_flush = std::chrono::high_resolution_clock::now();

    while(continue_running){
        // if(log_s.hold_for(10.0)){
        std::function<bool()> hmmm = std::function<bool()>([&log_s]()
            {return !log_s.empty();});
        bool can_flush = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now()-last_flush).count()/1e6 > flush_period;
        if(log_s.hold_for(0.1, hmmm) && can_flush){
            log_s.flush();
        }
    }

    log_s << logger::set_level(logger::INFO) << "Stopping the logger server workers\n";
    log_s.commit();log_s.flush();
    log_s.stop();
    std::cout << "logger server -- stopped\n";
    log_s.wait();
    std::cout << "logger server -- finished\n";

    return 0;
}