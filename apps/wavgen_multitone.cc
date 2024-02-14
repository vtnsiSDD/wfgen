//
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <vector>
#include <csignal>
#include <uhd/usrp/multi_usrp.hpp>

#include "labels.hh"
#include "liquid.h"

static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "MULTITONE ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}

int main (int argc, char **argv)
{
    double      uhd_tx_freq = 2.46e9;
    double      uhd_tx_rate =   4.0e6;
    double      uhd_tx_gain =  60.0;
    std::string uhd_tx_args{"type=b200"};
    double      grange      =   0.0f;    // software cycle gain range
    double      gcycle      =   2.0f;    // gain cycle duration
    std::string json{""};
    std::string file_dump{""};
    unsigned int num_tones  = 0;
    std::vector<double> tone_delta(0);
    double      duration    = -1;
    std::vector<double> tone_gain(0);
    uint8_t     cut_radio   = 0;

    const int max_chrono = 5;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();


    uint8_t dry_run = 0;

    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:t:E:A:d:j:W:C:z:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -t <num_tones:%u> ] [ -E <tone_delta:[] NHz> ]\n", uhd_tx_args.c_str(), num_tones);
            printf("  [ -A <tone_gain:[] dB> ] [ -d <duration:%.3f s> ] [ -j <json:%s> ]\n", duration, json.c_str());
            printf("  [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ] [ -z <dry_run:%u> ]\n", file_dump.c_str(), cut_radio, dry_run);
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 't': num_tones   = strtoul(optarg, &strend, 10); break;
        case 'E': tone_delta  .push_back( strtod(optarg, &strend)); break;
        case 'A': tone_gain   .push_back( strtod(optarg, &strend)); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'z': dry_run     = strtoul(optarg, &strend, 10); break;
        default: exit(1);
        }
    }

    if(num_tones != tone_delta.size()){
        std::cout << "Number of tones("<<num_tones
            <<") doesn't match the number of deltas given ("
            <<tone_delta.size()<<")\n";
        return 1;
    }
    if(num_tones != tone_gain.size()){
        std::cout << "Number of tones("<<num_tones
            <<") doesn't match the number of magnitudes given ("
            <<tone_gain.size()<<")\n";
        return 1;
    }
    double min_delta = 1.0;
    double max_delta = -1.0;
    for(unsigned int idx = 0; idx < num_tones; idx++){
        if(tone_delta[idx]<min_delta){
            min_delta = tone_delta[idx];
        }
        if(tone_delta[idx]>max_delta){
            max_delta = tone_delta[idx];
        }
    }

    printf("Using:\n");
    printf("  n-tones:      %u\n",num_tones);
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  bw:           %.3f\n",max_delta-min_delta+5e3);
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    printf("  grange:       %.3f\n",grange);
    printf("  cycle:        %.3f\n",gcycle);
    printf("  duration:     %.3f\n",duration);
    printf("  %10s:%14s:\n","deltas","magnitudes");
    for(unsigned int idx = 0; idx < num_tones; idx++){
        printf("  %*s%0.6f %*f\n",4,"",tone_delta[idx],14,tone_gain[idx]);
    }

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
    usrp->set_tx_bandwidth(max_delta-min_delta+5e3);

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
    tx_stream->send("",0,md);
     usrp->set_tx_gain(uhd_tx_gain);

    // vector buffer to send data to USRP
    unsigned int buf_len = 8192;
    std::vector<std::complex<float> > usrp_buffer(buf_len);
    // buffer of buffers? I guess?
    std::vector<std::complex<float>*> bufs(channel_nums.size(), &usrp_buffer.front());

    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = false;  // 
    md.has_time_spec  = true;  // set to false to send immediately

    // gain cycle
    uhd_tx_rate = usrp->get_tx_rate(); // get actual rate
    unsigned long int num_samples_cycle = (unsigned long int) (gcycle * uhd_tx_rate);
    unsigned long int num_buffers_cycle = num_samples_cycle / buf_len;
    std::cout << num_samples_cycle << "," << num_buffers_cycle << std::endl;
    // unsigned int buffer_counter=0;

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running (hit CTRL-C to stop)" << std::endl;
    // std::complex<float> * buf = usrp_buffer.data();
    std::complex<float> sym;
    chrono_time[2] = get_time();
    usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);
    
    labels* reporter;
    if(!json.empty()){
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG1","TXDL S1");
        if (num_tones == 1)
            reporter->set_modulation("tone");
        else
            reporter->set_modulation("no_answer");
        reporter->eng_bw = max_delta-min_delta+5e3;
    }
    // unsigned long long ticker = 0;
    float local_gain = 1.0f/float(num_tones);
    md.time_spec = uhd::time_spec_t(chrono_time[2]+0.5);
    uint64_t xfer_counter = 0;
    uint64_t xfer = 0;
    size_t xfer_len = 0;
    while (continue_running) {
        // set the software gain
        // FIXME -- do this

        // generate samples to buffer
        // std::cout << "start_gen " << buf_len << " " << num_tones << std::endl;
        memset( usrp_buffer.data(), 0, buf_len*sizeof(std::complex<float>) );
        for(unsigned int t = 0; t < num_tones; t++){
            float fc = tone_delta[t] < 0.0 ? tone_delta[t]+1.0f : tone_delta[t];
            float m = tone_gain[t];
            for(unsigned int idx = 0; idx < buf_len; idx++){
                float theta(2*M_PI*idx*fc);
                std::complex<float> v( cosf(theta), sinf(theta) );
                usrp_buffer[idx] += local_gain*0.5f*m*v;
            }
        }
        xfer = 0;
        xfer_len = buf_len;
        // std::cout << "start_send " << std::abs(usrp_buffer[10]) << std::endl;
        while((xfer == 0 && xfer_len > 0) && continue_running){
            // send the result to the USRP
            xfer = tx_stream->send(bufs, usrp_buffer.size(), md);
            if(xfer < xfer_len){
                xfer_counter += xfer;
                xfer_len -= xfer;
                memmove( usrp_buffer.data(), &usrp_buffer[xfer], xfer_len*sizeof(std::complex<float>) );
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
    while(get_time() < chrono_time[2]+0.5 + xfer_counter/uhd_tx_rate){};
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
        for(unsigned int idx = 0; idx < num_tones; idx++){
            float fc = tone_delta[idx];
            reporter->append(
                chrono_time[2]+0.5,
                double(xfer_counter)/uhd_tx_rate,
                uhd_tx_freq+fc*uhd_tx_rate,
                5e3f,
                meta);
        }
        reporter->activity_type="emanation_anomaly";
        reporter->protocol="unknown";
        reporter->set_modulation("no_answer");
        reporter->modality=num_tones > 1 ? "multi_carrier" : "single_carrier";
        reporter->device_origin=uhd_tx_args;
        reporter->modulation_src = "multitone";

        reporter->finalize();
        delete reporter;
    }

    return 0;
}

