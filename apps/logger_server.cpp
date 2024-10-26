
#include <iostream>
#include <stdio.h>
#include <string>
#include <csignal>
#include <atomic>
#include <functional>

#include "logger.hh"

static std::atomic<bool> continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "\nLOGGER_SERVER ---> ctrl+c received --> exiting\n";
    continue_running = false;
}


int main(int arg_c, char **arg_v){
    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "Starting the logger server -- spinning up workers\n";
    logger_server log_s;
    log_s << logger::set_level(logger::INFO) << "logger server -- started @ tcp://127.0.0.1:40000\n";
    log_s.commit();log_s.flush();

    uint32_t tick_count = 0;
    while(continue_running){
        // if(log_s.hold_for(10.0)){
        std::function<bool()> hmmm = std::function<bool()>([&log_s]()
            {return !log_s.empty();});
        if(log_s.hold_for(0.1, hmmm)){
            if(tick_count > 100)
                std::cout << std::endl;
            log_s.flush();
            tick_count = 0;
        }
        else{
            // std::cout << "timeout " << log_s.empty() << std::endl;
            tick_count++;
            if(tick_count % 100 == 0){// toggle every 10s of idle.
                std::cout << "." << std::flush;
                if(tick_count >= 600){// new line every 60s of idle
                    std::cout << std::endl;
                    tick_count = 0;
                }
            }
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