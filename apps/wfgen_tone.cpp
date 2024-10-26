// transmit tone signal with offset
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <csignal>
#include <uhd/usrp/multi_usrp.hpp>

#include "labels.hh"
#include "liquid.h"
#include "writer.hh"

static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "TONE ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}

int main (int argc, char **argv)
{
    double      uhd_tx_freq = 2.46e9;
    double      uhd_tx_gain =   60.0;
    double      uhd_tx_rate =  100e3;
    std::string uhd_tx_args{"type=b200"};
    double      grange      =   0.0f;    // software cycle gain range
    double      gcycle      =   2.0f;    // gain cycle duration
    std::string json{""};
    std::string file_dump{""};
    double      duration    =   -1.0;    // total duration
    uint8_t     cut_radio   = 0;

    const int max_chrono = 5;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();


    uint8_t dry_run = 0;

    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:d:j:W:C:z:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -d <duration:%.3f s> ] [ -j <json:%s> ]\n", uhd_tx_args.c_str(), duration, json.c_str());
            printf("  [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ] [ -z <dry_run:%u> ]\n", file_dump.c_str(), cut_radio, dry_run);
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'z': dry_run     = strtoul(optarg, &strend, 10); break;
        default: exit(1);
        }
    }

    double bw_f = 5e3;
    double bw_nr = bw_f/uhd_tx_rate;

    printf("Using:\n");
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  bw:           %.6f\n",5e3f/uhd_tx_rate);
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    printf("  grange:       %.3f\n",grange);
    printf("  cycle:        %.3f\n",gcycle);

    if(dry_run == 1){
        return 0;
    }

    chrono_time[1] = get_time();

    std::string args(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // try to configure hardware
    usrp->set_tx_rate(uhd_tx_rate);
    usrp->set_tx_freq(uhd_tx_freq);
    usrp->set_tx_gain(0);
    usrp->set_tx_bandwidth(bw_f*1.05);

    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = false;  // never SOB when continuous
    md.end_of_burst   = true;  // 
    md.has_time_spec  = false;  // set to false to send immediately

    // stream
    std::vector<size_t> channel_nums;
    channel_nums.push_back(0);
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);
    // send a mini EOB packet
    tx_stream->send("",0,md);
    usrp->set_tx_gain(uhd_tx_gain);

    // vector buffer to send data to USRP
    unsigned int n(8192);
    unsigned int fc(0);
    std::vector<std::complex<float> > usrp_buffer(n);
    for (auto i=0U; i < n; i++) {
        float theta(2*M_PI*i*fc/(float)n);
        std::complex<float>v( cosf(theta), sinf(theta) );
        usrp_buffer[i] = 0.5f * v;
    }
    // buffer of buffers? I guess?
    std::vector<std::complex<float>*> bufs(channel_nums.size(), &usrp_buffer.front());
    std::vector<std::complex<float>*> bufs_ptr(channel_nums.size(), &usrp_buffer.front());

    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = false;   // 
    md.has_time_spec  = true;  // set to false to send immediately

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running ";
    if (duration > 0) std::cout << "for " << duration << " seconds ";
    std::cout << "(hit CTRL-C to stop)" << std::endl;
    chrono_time[2] = get_time();
    usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);

    labels* reporter=nullptr;
    if(!json.empty()){
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG1","TXDL S1");
        reporter->set_modulation("tone");
        reporter->eng_bw = 5e3;
    }
    md.time_spec = uhd::time_spec_t(chrono_time[2]+0.5);
    uint64_t xfer_counter = 0;
    uint64_t xfer = 0, xfer_idx = 0;
    size_t xfer_len = 0;
    while (continue_running) {
        // send the result to the USRP
        xfer = 0;
        xfer_idx = 0;
        xfer_len = usrp_buffer.size();
        for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
            bufs_ptr[cidx] = bufs[cidx];
        }
        while((xfer == 0 && xfer_len > 0) && continue_running){
            xfer = tx_stream->send(bufs_ptr, xfer_len, md);
            if(xfer < xfer_len){
                xfer_counter += xfer;
                xfer_len -= xfer;
                xfer_idx += xfer;
                for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
                    bufs_ptr[cidx] = &(bufs[cidx][xfer_idx]);
                }
                xfer = 0;
                if(md.start_of_burst){
                    md.start_of_burst = false;
                    md.end_of_burst   = false;
                    md.has_time_spec  = false;
                }
            }
        }
        xfer_counter += xfer;
        if(md.start_of_burst){
            md.start_of_burst = false;
            md.end_of_burst   = false;
            md.has_time_spec  = false;
        }

        if (duration > 0 && xfer_counter/uhd_tx_rate >= duration)
            break;
    }
    continue_running = false;
    // send a mini EOB packet
    md.start_of_burst = false;
    md.end_of_burst   = true;
    tx_stream->send("",0,md);

    chrono_time[3] = get_time();

    // sleep for a small amount of time to allow USRP buffers to flush
    // usleep(100000);
    while(get_time() < chrono_time[2]+0.5 + xfer_counter/uhd_tx_rate);
    usrp->set_tx_freq(6e9);
    usrp->set_tx_gain(0.0);

    //finished
    printf("usrp data transfer complete\n");
    chrono_time[4] = get_time();

    printf("Timestamp at program start: cpu sec: %15.9lf\n",chrono_time[0]);
    printf("Connecting to radio at: cpu sec: %15.9lf\n",chrono_time[1]);
    printf("Starting to send at: cpu sec: %15.9lf\n",chrono_time[2]);
    printf("Stopping send at: cpu sec: %15.9lf\n",chrono_time[3]);
    printf("Radio should be stopped at: cpu sec: %15.9lf\n",chrono_time[4]);
    // export to .json if requested
    if (!json.empty()) {
        char misc_buf[100];
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100,"        \"start_app\": %.9f,\n",chrono_time[0]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100,"        \"start_dev\": %.9f,\n",chrono_time[1]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100,"        \"start_tx\": %.9f,\n",chrono_time[2]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100,"        \"stop_tx\": %.9f,\n",chrono_time[3]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100,"        \"stop_app\": %.9f,\n",chrono_time[4]);
        reporter->cache_to_misc(std::string(misc_buf));
        
        std::string meta = "        \"command\": \"" + std::string(argv[0]);
        for(int arg_idx = 1; arg_idx < argc; arg_idx++){
            meta += (std::string(" ") + std::string(argv[arg_idx]));
        }
        reporter->cache_to_misc(meta+"\"\n");

        meta = "";
        reporter->start_reports();
        reporter->append(
            chrono_time[2]+0.5,
            double(xfer_counter)/chrono_time[2],
            uhd_tx_freq,
            5e3f,
            meta);
        reporter->activity_type="emanation_anomaly";
        reporter->protocol="unknown";
        reporter->set_modulation("tone");
        reporter->modality="single_carrier";
        reporter->device_origin=uhd_tx_args;
        reporter->modulation_src = "tone";
        
        reporter->finalize();
        delete reporter;
    }
    return 0;
}

