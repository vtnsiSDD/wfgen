//
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <csignal>
#include <uhd/usrp/multi_usrp.hpp>

#include <random>

#include "liquid.h"
#include "labels.hh"
#include "wbofdmgen.hh"
#include "writer.hh"

// primitive burst definition
struct burst {
    float fc, bw, t0, dur;
    uint8_t type;
    modulation_scheme ms;
    burst(float _fc, float _bw, float _t0, float _dur, modulation_scheme _ms) :
        fc(_fc), bw(_bw), t0(_t0), dur(_dur), type(0), ms(_ms) {}
    void print() {
        printf("  fc:%9.3f MHz, bw:%9.3f kHz, t0:%9.3f ms, dur:%9.3f ms, mod=%s\n",
            fc*1e-6f, bw*1e-3f, t0*1e3f, dur*1e3f, modulation_types[ms].name);
    }
};

// generate a hop of a particular bw and center frequency
void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrcf gen, nco_crcf mixer);

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq=0.0f, float sample_rate=1.0f,
        modulation_scheme ms = LIQUID_MODEM_QPSK, bool sweep=false);


static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "WBOFDM ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}


int main (int argc, char **argv)
{
    /* +General */
    double uhd_tx_freq = 2.46e9;
    double uhd_tx_rate = 16.0e6f;
    double uhd_tx_gain = 60.0f;
    std::string uhd_tx_args{"type=b200"};
    std::string modulation{"qpsk"};
    double bw_nr = -1;
    double bw_f = -1;

    /* +Bursty */
    double dwell = 0.5;   // How long should the waveform be on per burst
    double squelch = 0.5; // How long should the burst be silent before next burst
    double period = 5;    // Used in place of 'leave' for a consistent period of bursts
    double duration    = -1;
    double span    = 0.9;

    unsigned int nfft = 2400;
    unsigned int ncar = 1920;
    unsigned int cplen = 20;
    unsigned int num_bursts  = 10;
    uint16_t min_syms = 0;
    uint16_t max_syms = 0;
    int          num_channels= -1;
    std::string  json{""};

    // init time keeping stuff
    const int max_chrono = 10;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono * sizeof(double));
    chrono_time[0] = get_time();

    modulation_scheme ms = LIQUID_MODEM_QPSK;

    // get cli options
    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc, argv, "hf:r:g:a:b:B:d:w:q:p:k:j:s:M:H:k:n:U:c:u:v:")) != EOF){
        switch (dopt){
        case 'h':
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -M <modulation:%s> ] [ -b <bw_nr:%.3f NHz> ]\n", uhd_tx_args.c_str(), modulation.c_str(), bw_nr);
            printf("  [ -B <bw_f:%.3f MHz> ] [ -d <duration:%.3f s> ] [ -p <period:%.3f s> ]\n", bw_f*1.0e-06, duration, period);
            printf("  [ -H <num_bursts:%u> ] [ -w <dwell:%.3f s> ] [ -q <squelch:%.3f s> ]\n",num_bursts, dwell, squelch);
            printf("  [ -k <num_channels:%u> ] [ -s <span:%.3f MHz> ] [ -n <nfft:%u> ]\n", num_channels, span, nfft);
            printf("  [ -U <used_carriers:%u> ] [ -c <cyclic_prefix:%u> ] [ -j <json:%s> ]\n", ncar, cplen, json.c_str());
            printf("  [ -u <min_symbols:%u> ] [ -v <max_symbols:%u> ]\n", min_syms, max_syms);
            printf(" available modulation schemes:\n");
            liquid_print_modulation_schemes();
            return 0;
        case 'f': uhd_tx_freq   = strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate   = strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain   = strtod(optarg, &strend); break;
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
        case 'b': bw_nr         = strtod(optarg, &strend); break;
        case 'B': bw_f          = strtod(optarg, &strend); break;
        case 'd': duration      = strtod(optarg, &strend); break;
        case 'p': period        = strtod(optarg, &strend); break;
        case 'w': dwell         = strtod(optarg, &strend); break;
        case 'q': squelch       = strtod(optarg, &strend); break;
        case 'H': num_bursts    = strtoul(optarg, &strend, 10); break;
        case 'k': num_channels  = strtoul(optarg, &strend, 10); break;
        case 'n': nfft          = strtoul(optarg, &strend, 10); break;
        case 'U': ncar          = strtoul(optarg, &strend, 10); break;
        case 'u': min_syms      = strtoul(optarg, &strend, 10); break;
        case 'v': max_syms      = strtoul(optarg, &strend, 10); break;
        case 'c': cplen         = strtoul(optarg, &strend, 10); break;
        case 's': span          = strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
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

    if(min_syms==0 && max_syms==0){
        min_syms = max_syms = 512;
    }
    else if (min_syms > 0 && max_syms == 0){
        max_syms = min_syms;
    }

    std::random_device *rd = new std::random_device;
    std::mt19937 rgen((*rd)());
    std::uniform_int_distribution<uint16_t> syms_dist(min_syms,max_syms);
    delete rd;

    double loop_time = dwell + squelch; // total time for each loop

    printf("Using:\n");
    printf("  modulation:    %s\n",modulation.c_str());
    printf("  freq:          %.3f\n",uhd_tx_freq);
    printf("  rate:          %.3f\n",uhd_tx_rate);
    printf("  gain:          %.3f\n",uhd_tx_gain);
    printf("  args:          %s\n",uhd_tx_args.c_str());
    printf("  num_bursts:    %d\n",num_bursts);
    printf("  dwell:         %.3f\n",dwell);
    printf("  squelch:       %.3f\n",squelch);
    printf("  period:        %.3f\n",period);
    printf("  bw_f:          %.3f\n",bw_f);
    printf("  bw_nr:         %.3f\n",bw_nr);
    printf("  span:          %.3f\n",span);
    printf("  channels:      %d\n",num_channels);
    printf("  nfft:          %u\n",nfft);
    printf("  used_carriers: %u\n",ncar);
    printf("  cplen:         %u\n",cplen);
    printf("  min_syms:      %u\n",min_syms);
    printf("  max_syms:      %u\n",max_syms);
    printf("  duration:      %.3f\n",duration);
    printf("  json:          %s\n",json.c_str());

    chrono_time[1] = get_time();

    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = false;  // never SOB when continuous
    md.end_of_burst   = true;  //
    md.has_time_spec  = false;  // set to false to send immediately

    uhd::device_addr_t args_uhd(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args_uhd);

    // try to configure hardware
    usrp->set_tx_rate(uhd_tx_rate);
    usrp->set_tx_freq(uhd_tx_freq);
    usrp->set_tx_gain(0);
    usrp->set_tx_bandwidth(bw_f*1.05);

    // signal generator
    wbofdmgen gen(nfft,cplen,ncar,ms);

    // stream
    std::vector<size_t> channel_nums;
    channel_nums.push_back(0);
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);
    // send a mini EOB packet
    tx_stream->send("",0,md);
    usrp->set_tx_gain(uhd_tx_gain);

    // get actual rate
    uhd_tx_rate = usrp->get_tx_rate();

    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = true;  //
    md.has_time_spec  = true;  // set to false to send immediately

    // vector buffer to send data to USRP
    auto buf_len = gen.get_buf_len(max_syms);
    std::vector<std::complex<float>> usrp_buffer(buf_len);

    // buffer of buffers? I guess?
    std::vector<std::complex<float> *> bufs(channel_nums.size(),
                                            &usrp_buffer.front());
    std::vector<std::complex<float> *> buf_ptr(channel_nums.size(),
                                               &usrp_buffer.front());

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running (hit CTRL-C to stop)" << std::endl;
    std::complex<float> *buf = usrp_buffer.data();
    unsigned int count = 0;
    chrono_time[2] = get_time();
    usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);

    labels *reporter;
    if (!json.empty())
    {
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG0000001","TXDL S0000001");
        reporter->modulation = "ofdm";
        reporter->modulation_src = modulation;
        reporter->start_reports();
    }
    chrono_time[6] = chrono_time[2] + 0.5;
    double send_at = chrono_time[6];
    md.time_spec = uhd::time_spec_t(chrono_time[6]);
    uint64_t xfer_counter = 0;
    uint64_t xfer = 0, xfer_idx = 0;
    size_t xfer_len = 0;
    uint16_t syms = syms_dist(rgen);

    while (continue_running)
    {
        // generate samples to buffer

        gen.generate(buf,syms);

        xfer = 0;
        xfer_idx = 0;
        xfer_counter = 0;
        // xfer_len = usrp_buffer.size();
        xfer_len = gen.get_buf_len(syms);
        for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
            buf_ptr[cidx] = bufs[cidx];
        }
        while((xfer == 0 && xfer_len > 0) && continue_running){
            // send the result to the USRP
            //? changed this to 1 because xfer kept returning 0, now it returns the same as xfer_len
            //? but still saying 'L' for late
            xfer = tx_stream->send(buf_ptr, xfer_len, md, 1); 
            // printf("xfer(%ld) = xfer_len(%ld)\n", xfer, xfer_len);

            if(xfer < xfer_len){
                // printf("xfer(%ld) < xfer_len(%ld)\n", xfer, xfer_len);
                xfer_counter += xfer;
                xfer_len -= xfer;
                xfer_idx += xfer;
                for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
                    buf_ptr[cidx] = &(bufs[cidx][xfer_idx]);
                }
                xfer = 0;
                if(md.start_of_burst){
                    md.start_of_burst = false;
                    md.has_time_spec  = false;
                }
            }
        }
        if (!json.empty()){
            reporter->append(
            send_at,
            double(xfer_len)/uhd_tx_rate,
            uhd_tx_freq,
            uhd_tx_rate*bw_nr,
            "");
        }

        md.start_of_burst = true;
        md.has_time_spec  = true;
        syms = syms_dist(rgen);
        send_at += period;
        md.time_spec = uhd::time_spec_t(send_at);
    }

    // send a mini EOB packet
    md.start_of_burst = false;
    md.end_of_burst = true;
    tx_stream->send("", 0, md);

    chrono_time[3] = get_time();

    // sleep for a small amount of time to allow USRP buffers to flush
    // usleep(100000);
    while (get_time() < chrono_time[6] + xfer_counter / uhd_tx_rate)
        ;

    // finished
    printf("usrp data transfer complete\n");

    chrono_time[4] = get_time();

    printf("Timestamp at program start: cpu sec: %15.9lf\n",chrono_time[0]);
    printf("Connecting to radio at: cpu sec: %15.9lf\n",chrono_time[1]);
    printf("Starting to send at: cpu sec: %15.9lf\n",chrono_time[2]);
    printf("Stopping send at: cpu sec: %15.9lf\n",chrono_time[3]);
    printf("Radio should be stopped at: cpu sec: %15.9lf\n",chrono_time[4]);

    // export to .json if requested
    if (!json.empty())
    {
        char misc_buf[100];
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100, "        \"start_app\": %.9f,\n", chrono_time[0]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100, "        \"start_dev\": %.9f,\n", chrono_time[1]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100, "        \"start_tx\": %.9f,\n", chrono_time[2]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100, "        \"stop_tx\": %.9f,\n", chrono_time[3]);
        reporter->cache_to_misc(std::string(misc_buf));
        memset(misc_buf, 0, 100);
        snprintf(misc_buf, 100, "        \"stop_app\": %.9f,\n", chrono_time[4]);
        reporter->cache_to_misc(std::string(misc_buf));
        
        std::string meta = "        \"command\": \"" + std::string(argv[0]);
        for(int arg_idx = 1; arg_idx < argc; arg_idx++){
            meta += (std::string(" ") + std::string(argv[arg_idx]));
        }
        reporter->cache_to_misc(meta+"\"\n");

        meta = "";
        reporter->append(
            send_at,
            double(xfer_counter)/uhd_tx_rate,
            uhd_tx_freq,
            uhd_tx_rate*bw_nr,
            "");
        reporter->activity_type = "unknown"; // FIXME:::: Come back and fix this
        reporter->set_modulation( "ofdm" );
        reporter->modulation_src = modulation;
        reporter->protocol = "unknown";
        reporter->modality = "multi_carrier";
        reporter->activity_type = "lowprob_anomaly";
        reporter->device_origin = uhd_tx_args;
        reporter->finalize();
    delete reporter;
    }


    return 0;
}

// generate a hop of a particular bandwidth and center frequency
void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrcf gen, nco_crcf mixer)
{
    auto delay = symstreamrcf_get_delay(gen);

    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // check values
    if (buf_len < 2*dead_time)
        throw std::runtime_error("requested hop duration too small");

    // fill buffer
    unsigned int num_samples_on = buf_len - 2*dead_time;

    symstreamrcf_set_gain(gen, 0.5f);
    symstreamrcf_write_samples(gen, buf, num_samples_on);
    symstreamrcf_set_gain(gen, 0.0f);
    symstreamrcf_write_samples(gen, buf + num_samples_on, 2*dead_time);

    // mix
    nco_crcf_set_frequency(mixer, 2*M_PI*fc);
    nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
}



float get_rand_fc(){
    return randf() - 0.5f;
}
float get_uniform_fc(unsigned int step, unsigned int total_steps){
    return (float)step/(float)total_steps - 0.5f;
}
float get_channel_fc(int step, float span, unsigned int channels){
    float m = span/(float) channels;
    if (step < 0 || (unsigned int) step >= channels)
        step = rand() % channels;
    return m*(float)step - 0.5f*(span-m);
}

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq, float sample_rate, modulation_scheme ms,
        bool sweep, float span, int num_channels)
{
    // initialize objects
    unsigned int m = 12;
    symstreamrcf gen = symstreamrcf_create_linear(LIQUID_FIRFILT_ARKAISER, bw, m, 0.25f, ms);
    nco_crcf mixer = nco_crcf_create(LIQUID_VCO);

    //
    std::vector<burst> bursts;

    // generate individual hops
    for (auto i=0U; i<num_hops; i++) {
        float fc;
        if(num_channels <= 0){
            fc = sweep ? span*(1-1.2*bw)*get_uniform_fc(i,num_hops) : span*(1-1.2*bw)*get_rand_fc();
        }
        else{
            fc = sweep ? get_channel_fc(i%num_channels,span,num_channels) : get_channel_fc(-1,span,num_channels);
        }
        generate_hop(fc, buf + i*hop_dur, hop_dur, gen, mixer);

        // append to labels
        bursts.emplace_back(center_freq + fc*sample_rate, bw*sample_rate,
            (float)(i*hop_dur)/sample_rate, (float)hop_dur/sample_rate, ms);
    }

    // free objects
    symstreamrcf_destroy(gen);
    nco_crcf_destroy(mixer);

    // return list of bursts
    return bursts;
}

