// transmit tone signal with offset
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <csignal>
// #include <time.h>
#include <uhd/usrp/multi_usrp.hpp>

#include "liquid.h"
#include "labels.hh"
#include "noisemodem.hh"
#include "writer.hh"

namespace c = wfgen::containers;
namespace w = wfgen::writer;
typedef ::wfgen::fc32 fc32;
const auto& gcs = c::get_empty_content_size;


static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "LINMOD ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}

int main (int argc, char **argv)
{
    double      uhd_tx_freq =       2.46e9;
    double      uhd_tx_gain =         50.0;
    double      uhd_tx_rate =       100e3f;
    std::string uhd_tx_args{"type=b200"};

    // double      grange         =   0.0f;    // software cycle gain range
    // double      gcycle         =   2.0f;    // gain cycle duration
    std::string modulation{"qpsk"};
    std::string json("");
    std::string file_dump{""};
    double      bw_nr         =   0.7f;
    double      bw_f          =   -1.f;
    uint8_t     cut_radio     =      0;

    const int max_chrono = 5;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();


    modulation_scheme ms = LIQUID_MODEM_QPSK;
    noise_scheme ms_n = LIQUID_NOISE_AWGN;
    uint8_t dry_run = 0;

    double duration = -1;

    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:M:B:b:d:j:W:C:z:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -M <modulation:%s> ] [ -B <bw_f:%.3f MHz> ]\n", uhd_tx_args.c_str(), modulation.c_str(), bw_f*1.0e-06);
            printf("  [ -b <bw_nr:%.3f NHz> ] [ -d <duration:%.3f s> ] [ -j <json:%s> ]\n", bw_nr, duration, json.c_str());
            printf("  [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ] [ -z <dry_run:%u> ]\n", file_dump.c_str(), cut_radio, dry_run);
            printf(" available modulation schemes:\n");
            liquid_print_modulation_schemes();
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 'M':{
            modulation.assign(optarg);
            ms = liquid_getopt_str2mod(optarg);
            if (ms == LIQUID_MODEM_UNKNOWN){
                fprintf(stderr,"error: %s, unknown/unsupported modulation scheme '%s'\n", argv[0], optarg);
                return 1;
            }
            break;
        }
        case 'B': bw_f        =  strtod(optarg, &strend); break;
        case 'b': bw_nr       =  strtod(optarg, &strend); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'z': dry_run     = strtoul(optarg, &strend, 10); break;
        default: exit(1);
        }
    }


    if(bw_nr > 0.0){
        bw_f = bw_nr * uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_nr specified directly as: " << bw_nr << " for a bw_f: " << bw_f << "Hz at rate: " << uhd_tx_rate << "Hz\n";
    }
    else if(bw_f > 0.0){
        bw_nr = bw_f / uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_f specified directly as: " << bw_f << " Hz at rate: " << uhd_tx_rate << "Hz for a bw_nr: " << bw_nr << std::endl;
    }
    else{
        bw_nr = 0.5;
        bw_f = bw_nr*uhd_tx_rate;
        std::cout << "bw not specified; set as: " << bw_f << " Hz at rate: " << uhd_tx_rate << "Hz for a bw_nr: " << bw_nr << std::endl;
    }


    printf("Using:\n");
    printf("  modulation:   %s\n",modulation.c_str());
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  bw:           %.3f\n",bw_nr);
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    // printf("  grange:       %.3f\n",grange);
    // printf("  cycle:        %.3f\n",gcycle);
    if(!json.empty())       printf("  json:         %s\n",json.c_str());
    if(!file_dump.empty())  printf("  file:         %s\n",file_dump.c_str());
    if(cut_radio)           printf("  radio connection cut\n");

    if(dry_run == 1){
        return 0;
    }
    chrono_time[1] = get_time();

    uhd::device_addr_t args(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp;
    if(!cut_radio){
        usrp = uhd::usrp::multi_usrp::make(args);

        // try to configure hardware
        usrp->set_tx_rate(uhd_tx_rate);
        usrp->set_tx_freq(uhd_tx_freq);
        usrp->set_tx_gain(0);
        usrp->set_tx_bandwidth(bw_f);
    }
    w::writer f_handle;
    if(!file_dump.empty()) f_handle = w::writer_create(w::WRITER_BURST, file_dump.c_str(), 0);

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
    uhd::tx_streamer::sptr tx_stream;
    if(!cut_radio){
        tx_stream = usrp->get_tx_stream(stream_args);
        tx_stream->send("",0,md);
        usrp->set_tx_gain(uhd_tx_gain);
    }

    // vector buffer to send data to USRP
    unsigned int buf_len = 800;
    // std::vector<fc32> usrp_buffer(buf_len);
    c::container iq_container = c::container_create(c::CFLOAT32 | c::POINTER | c::OWNER,
        buf_len*sizeof(fc32),NULL);

    // buffer of buffers? I guess?
    // std::vector<fc32*> bufs(channel_nums.size(), &usrp_buffer.front());
    std::vector<fc32*> bufs(channel_nums.size(), (fc32*)iq_container->ptr);

    // signal generator
    bool noise_mode = false;
    if (ms == LIQUID_MODEM_UNKNOWN){
        noise_mode = true;
        ms = LIQUID_MODEM_QPSK;
    }
    symstreamrcf gen = symstreamrcf_create_linear(
        LIQUID_FIRFILT_ARKAISER, bw_nr, 12, 0.25f, ms);
    symstreamrncf ngen = symstreamrncf_create_noise(
        LIQUID_FIRFILT_ARKAISER, bw_nr, 12, 0.25f, ms_n);
    if (noise_mode){
        ms = LIQUID_MODEM_UNKNOWN;
    }

    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = false;  // 
    md.has_time_spec  = true;  // set to false to send immediately

    // gain cycle
    if(!cut_radio) uhd_tx_rate = usrp->get_tx_rate(); // get actual rate
    // unsigned long int num_samples_cycle = (unsigned long int) (gcycle * uhd_tx_rate);
    // unsigned long int num_buffers_cycle = num_samples_cycle / buf_len;
    // std::cout << num_samples_cycle << "," << num_buffers_cycle << std::endl;
    // unsigned int buffer_counter=0;

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running (hit CTRL-C to stop)" << std::endl;
    // fc32 * buf = usrp_buffer.data();
    fc32 * buf = (fc32*)iq_container->ptr;
    fc32 sym;
    chrono_time[2] = get_time();
    if(!cut_radio)
        usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);
    
    labels* reporter;
    if(!json.empty()){
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG1","TXDL S1");
        reporter->set_modulation(modulation);
        reporter->eng_bw = bw_f;
    }
    double initial_start = chrono_time[2]+0.5;
    md.time_spec = uhd::time_spec_t(chrono_time[2]+0.5);
    uint64_t xfer_counter = 0;
    uint64_t xfer = 0;
    size_t xfer_len = 0;
    float gain = 0.5f;
    symstreamrcf_set_gain(gen, gain);
    while (continue_running) {
        // set the software gain
        // float gain_dB = -grange*(0.5f - 0.5f*cosf(2*M_PI*(float)buffer_counter/(float)num_buffers_cycle));
        //printf("%6u : gain=%8.3f\n", buffer_counter, gain_dB);
        // buffer_counter = (buffer_counter+1) % num_buffers_cycle;
        // float gain = 0.5f*powf(10.0f, gain_dB/20.0f);
        if(ms != LIQUID_MODEM_UNKNOWN){
            // symstreamrcf_set_gain(gen, gain);

            // generate samples to buffer
            symstreamrcf_write_samples(gen, buf, buf_len);
        }
        else{
            // symstreamrncf_set_gain(ngen, gain);

            // generate samples to buffer
            symstreamrncf_write_samples(ngen, buf, buf_len);
        }

        xfer = 0;
        xfer_len = buf_len;
        while((xfer == 0 && xfer_len > 0) && continue_running){
            // send the result to the USRP
            if(!cut_radio){
                xfer = tx_stream->send(bufs, xfer_len, md);
                if(!file_dump.empty()){
                    //write iq_container->ptr xfer
                    if(xfer != writer_store_head(f_handle,iq_container,xfer)){
                        throw std::runtime_error("Hmmm.");
                    }
                }
            }
            else if(!file_dump.empty()){
                xfer = writer_store_head(f_handle,iq_container,xfer_len);
            }
            if(xfer < xfer_len){
                xfer_counter += xfer;
                xfer_len -= xfer;
                memmove( buf, &buf[xfer], xfer_len*sizeof(fc32) );
                xfer = 0;
                if(md.start_of_burst){
                    md.start_of_burst = false;
                    md.end_of_burst = false;
                    md.has_time_spec = false;
                }
            }
        }
        ////////xfer == last transfer size
        xfer_counter += xfer;
        if(md.start_of_burst){
            md.start_of_burst = false;
            md.end_of_burst = false;
            md.has_time_spec = false;
        }
        if(duration > 0 && get_time() > initial_start+duration) break;
    }
    continue_running = false;
    // send a mini EOB packet
    md.start_of_burst = false;
    md.end_of_burst   = true;
    if(!cut_radio) tx_stream->send("",0,md);
    chrono_time[3] = get_time();


    // sleep for a small amount of time to allow USRP buffers to flush
    // usleep(100000);
    if(!cut_radio){
        while(get_time() < chrono_time[2]+0.5+xfer_counter/uhd_tx_rate);
        usrp->set_tx_freq(6e9);
        usrp->set_tx_gain(0.0);
    }
    if(!file_dump.empty()){
        writer_close(f_handle);
        container_destroy(&iq_container);
        writer_destroy(&f_handle);
    }

    //finished
    printf("usrp data transfer complete\n");
    symstreamrcf_destroy(gen);
    symstreamrncf_destroy(ngen);
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
            double(xfer_counter)/uhd_tx_rate,
            uhd_tx_freq,
            uhd_tx_rate*bw_nr,
            meta);
        reporter->activity_type="lowprob_anomaly";
        reporter->protocol="unknown";
        reporter->set_modulation(modulation);
        reporter->modulation_src = modulation;
        reporter->modality="single_carrier";
        reporter->device_origin=uhd_tx_args;

        reporter->finalize();
        delete reporter;
    }
    return 0;
}

