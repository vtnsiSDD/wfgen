// pre-generated FH/SS signal, transmitted on repeat
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <csignal>
#include <vector>
#include <random>
#include <uhd/usrp/multi_usrp.hpp>

#include "liquid.h"
#include "labels.hh"
#include "fskmodems.hh"
#include "afmodem.hh"
#include "noisemodem.hh"
#include "writer.hh"


// primitive burst definition
struct burst {
    float fc, bw, t0, dur;
    uint8_t type;
    modulation_scheme ms;
    fsk_scheme ms_f;
    analog_scheme ms_a;
    noise_scheme ms_n;
    burst(float _fc, float _bw, float _t0, float _dur, modulation_scheme _ms) :
        fc(_fc), bw(_bw), t0(_t0), dur(_dur), type(0), ms(_ms) {}
    burst(float _fc, float _bw, float _t0, float _dur, fsk_scheme _ms) :
        fc(_fc), bw(_bw), t0(_t0), dur(_dur), type(1), ms_f(_ms) {}
    burst(float _fc, float _bw, float _t0, float _dur, analog_scheme _ms) :
        fc(_fc), bw(_bw), t0(_t0), dur(_dur), type(2), ms_a(_ms) {}
    burst(float _fc, float _bw, float _t0, float _dur, noise_scheme _ms) :
        fc(_fc), bw(_bw), t0(_t0), dur(_dur), type(3), ms_n(_ms) {}
    void print() {
        if(type==0)
            printf("  fc:%9.3f MHz, bw:%9.3f kHz, t0:%9.3f ms, dur:%9.3f ms, mod=%s\n",
                fc*1e-6f, bw*1e-3f, t0*1e3f, dur*1e3f, modulation_types[ms].name);
        else if (type==1)
            printf("  fc:%9.3f MHz, bw:%9.3f kHz, t0:%9.3f ms, dur:%9.3f ms, mod=%s\n",
                fc*1e-6f, bw*1e-3f, t0*1e3f, dur*1e3f, fsk_types[ms_f].name);
        else if (type==2)
            printf("  fc:%9.3f MHz, bw:%9.3f kHz, t0:%9.3f ms, dur:%9.3f ms, mod=%s\n",
                fc*1e-6f, bw*1e-3f, t0*1e3f, dur*1e3f, analog_types[ms_a].name);
        else if (type==3)
            printf("  fc:%9.3f MHz, bw:%9.3f kHz, t0:%9.3f ms, dur:%9.3f ms, mod=%s\n",
                fc*1e-6f, bw*1e-3f, t0*1e3f, dur*1e3f, noise_types[ms_n].name);
    }
};

// generate a hop of a particular bandwidth and center frequency
// void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
//         symstreamrcf gen, nco_crcf mixer);
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch);
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamracf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch);
void generate_hop3(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrncf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch);

// generate a hop of a particular bandwidth and center frequency
// void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
//         symstreamrfcf gen, nco_crcf mixer);
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrfcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch);
void generate_hop3(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrfcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch);

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq=0.0f, float sample_rate=1.0f,
        modulation_scheme ms = LIQUID_MODEM_QPSK, bool sweep=false,
        float span=0.9, int num_channels=0, unsigned int dwell = 0, unsigned int squelch = 0);
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq=0.0f, float sample_rate=1.0f,
        fsk_scheme ms = LIQUID_MODEM_FSK4, bool sweep=false,
        float span=0.9, int num_channels=0, unsigned int dwell = 0, unsigned int squelch = 0,
        unsigned int k=2, double mod_index=1.0, unsigned int cpf_type=0);
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq=0.0f, float sample_rate=1.0f,
        analog_scheme ms = LIQUID_ANALOG_FM_WAV_FILE, bool sweep=false,
        float span=0.9, int num_channels=0, unsigned int dwell = 0, unsigned int squelch = 0,
        double mod_index=-1.0, double src_freq=0.2);
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq=0.0f, float sample_rate=1.0f,
        noise_scheme ms = LIQUID_NOISE_AWGN, bool sweep=false,
        float span=0.9, int num_channels=0, unsigned int dwell = 0, unsigned int squelch = 0);

void export_json(labels *reporter, std::vector<burst> bursts,
        double start, double loop_time,
        double center_freq, double sample_rate,
        uint64_t meant_to_send, uint64_t sent);

static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "FHSSGEN ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}


int main (int argc, char **argv)
{
    srand(std::random_device()());
    double       uhd_tx_freq = 2425e6;
    double       uhd_tx_gain = 60.0;
    double       uhd_tx_rate = 35.0e6f;
    std::string  uhd_tx_args{"type=b200"};
    std::string  modulation("qpsk");
    unsigned int num_bursts    = 10;      // number of hops in each burst
    float        hop_time    = -1;      // duration of each hop [seconds]
    float        bw_f        = 1e6f;    // bandwidth of each hop [Hz]
    float        loop_delay  = 500e-3f;  // delay between loops [seconds]
    bool         debug       = false;
    bool         sweep       = false;   // enable sweep for FH/SS
    double       duration    = -1;      // run time [s]
    double       span        = 0.9;     // frequency range relative to sample_rate (0,1)
    int          num_loops   = -1;      // number of loops to run
    int          num_channels= -1;      // number of channels to run
    double       src_fq      = -1;
    double       src_period  = -1;
    double       period      = -1;
    double       mod_index   = 1.0f;
    double       symbol_rate = -1;
    unsigned int cpf_type    =  0;
    float dwell              = -1;
    float squelch            = -1;
    std::string  json{""};
    std::string file_dump{""};
    modulation_scheme ms = LIQUID_MODEM_QPSK;
    fsk_scheme ms_f = LIQUID_FSK_UNKNOWN;
    analog_scheme ms_a = LIQUID_ANALOG_UNKNOWN;
    noise_scheme ms_n = LIQUID_NOISE_AWGN;
    uint8_t     cut_radio   =   0;

    const int max_chrono = 10;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();

    double bw_nr = -1;
    uint8_t analog_hop(0),digital_hop(1);
    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:M:b:B:d:p:J:K:x:R:H:w:q:s:k:S:l:L:j:W:C:P:F:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -M <modulation:%s> ] [ -b <bw_nr:%.3f NHz> ]\n", uhd_tx_args.c_str(), modulation.c_str(), bw_nr);
            printf("  [ -B <bw_f:%.3f MHz> ] [ -d <duration:%.3f s> ] [ -p <period:%.3f s> ]\n", bw_f*1.0e-06, duration, period);
            printf("  [ -J <digital_hop:%u> ] [ -K <analog_hop:%u> ] [ -x <mod_index:%.3f> ]\n", digital_hop, analog_hop, mod_index);
            printf("  [ -R <symbol_rate:%.3f kHz> ] [ -H <num_bursts:%u> ] [ -w <dwell:%.3f s> ]\n", symbol_rate*1.0e-03, num_bursts, dwell);
            printf("  [ -q <squelch:%.3f s> ] [ -s <span:%.3f MHz> ] [ -k <num_channels:%u> ]\n", squelch, span*1.0e-06, num_channels);
            printf("  [ -S <sweep:%u> ] [ -l <num_loops:%u> ] [ -L <loop_delay:%.3f s> ]\n", sweep, num_loops, loop_delay);
            printf("  [ -j <json:%s> ] [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ]\n", json.c_str(), file_dump.c_str(), cut_radio);
            printf("  [ -P <cpf_type:%d> ] [ -F <src_fq:%.3f MHz> ]\n", cpf_type, src_fq);
            printf(" available modulation schemes:\n");
            liquid_print_modulation_schemes();
            liquid_print_fsk_modulation_schemes();
            liquid_print_analog_modulation_schemes();
            liquid_print_noise_modulation_schemes();
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 'M':{
            modulation.assign(optarg);
            ms = liquid_getopt_str2mod(optarg);
            if (ms == LIQUID_MODEM_UNKNOWN){
                std::cout << "ignore the above error, treat as warning.\n";
                ms_f = liquid_getopt_str2fsk(optarg);
                if (ms_f == LIQUID_FSK_UNKNOWN){
                    ms_a = liquid_getopt_str2analog(optarg);
                    if (ms_a == LIQUID_ANALOG_UNKNOWN){
                        ms_n = liquid_getopt_str2noise(optarg);
                        if (ms_n == LIQUID_NOISE_UNKNOWN){
                            modulation = "tone";
                        }
                    }
                }
            }
            break;
        }
        case 'b': bw_nr       =  strtod(optarg, &strend); break;
        case 'B': bw_f        =  strtod(optarg, &strend); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'p': period      =  strtod(optarg, &strend); break;
        case 'J': digital_hop = strtoul(optarg, &strend, 10); break;
        case 'K': analog_hop  = strtoul(optarg, &strend, 10); break;
        case 'x': mod_index   =  strtod(optarg, &strend); break;
        case 'R': symbol_rate =  strtod(optarg, &strend); break;
        case 'H': num_bursts  = strtoul(optarg, &strend, 10); break;
        case 'w': dwell       =  strtod(optarg, &strend); break;
        case 'q': squelch     =  strtod(optarg, &strend); break;
        case 's': span        =  strtod(optarg, &strend); break;
        case 'k': num_channels= strtoul(optarg, &strend, 10); break;
        case 'S': sweep       = strtoul(optarg, &strend, 10); break;
        case 'l': num_loops   = strtoul(optarg, &strend, 10); break;
        case 'L': loop_delay  =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'P': cpf_type    =  strtol(optarg, &strend, 10); break;
        case 'F': src_fq      =  strtod(optarg, &strend); break;
        default: exit(1);
        }
}

    if(src_period > 0 && src_fq > 0) throw std::runtime_error("Only define one of (src_period, src_rate)");
    else if(src_period < 0 && src_fq < 0) src_fq = 0.2;
    else if(src_period > 0) src_fq = 1/src_period;
    else src_period = 1/src_fq;

    if(bw_nr > 0.0){
        bw_f = bw_nr * uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_nr specified directly as: " << bw_nr << " for a bw_f: " << bw_f << "Hz at rate: " << uhd_tx_rate << "Hz\n";
    }
    else if(bw_f > 0.0){
        bw_nr = bw_f / uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_f specified directly as: " << bw_f << " Hz at rate: " << uhd_tx_rate << "Hz for a bw_nr: " << bw_nr << std::endl;
    }
    else{
        bw_nr = 0.05;
        bw_f = bw_nr*uhd_tx_rate;
        std::cout << "bw not specified; set as: " << bw_f << " Hz at rate: " << uhd_tx_rate << "Hz for a bw_nr: " << bw_nr << std::endl;
    }

    hop_time = period;
    std::cout << "dwell: " << dwell << " | squelch: " << squelch << " | period: " << period << std::endl;
    if(dwell > 0 && squelch > 0 && hop_time > 0){
        if (std::abs(dwell+squelch - hop_time) > 1e-4)
            throw std::runtime_error("specify up to two of -q <squelch> -w <dwell> -p <period>");
    }
    else if (dwell <= 0 && squelch <= 0 && hop_time <= 0){
        dwell = 0.05;
        hop_time = dwell;
    }
    else if (dwell > 0 && squelch > 0){
        hop_time = dwell+squelch;
    }
    else if (dwell > 0 && hop_time > 0){
        squelch = (hop_time > dwell) ? hop_time-dwell : 0;
        hop_time = (hop_time > dwell) ? hop_time : dwell;
    }
    else if (squelch > 0 && hop_time > 0){
        dwell = (hop_time > squelch) ? hop_time-squelch : squelch;
        hop_time = (hop_time > squelch) ? hop_time : 2*squelch;
    }
    else if (dwell > 0){
        squelch = 0;
        hop_time = dwell;
    }
    else if (squelch > 0){
        dwell = squelch;
        hop_time = 2*squelch;
    }
    else if (hop_time > 0){
        dwell = hop_time;
        squelch = 0;
    }

    // derived values
    unsigned int hop_dur  = (unsigned int)(uhd_tx_rate * hop_time); // [samples]
              bw_nr       = bw_f / uhd_tx_rate; // relative bandwidth
    double loop_time = (hop_time * num_bursts) + loop_delay; // total time for each loop

    if (duration > 0 && num_loops > 0)
        throw std::runtime_error("specify only one of -D <duration> or -L <num loops>");
    else if (duration > 0)
        num_loops = (int)(round(duration / loop_time));
    else if (num_loops > 0)
        duration = loop_time * num_loops;

    while(num_loops == 0 && num_bursts > 0){
        num_bursts--;
        loop_time = (hop_time*num_bursts) + loop_delay;
        num_loops = (int)(round(duration/loop_time));
    }
    duration = loop_time*num_loops;

    unsigned int M;
    char good_config;
    char config_mode;
    unsigned int k;
    if(ms == LIQUID_MODEM_UNKNOWN){
        if(ms_f != LIQUID_FSK_UNKNOWN){
            //FSK waveform
            if(symbol_rate > 0 && bw_nr > 0){
                fprintf(stderr,"error: %s, specify symbol rate or bandwidth, not both\n",argv[0]);
                return 1;
            }
            if(symbol_rate < 0 && bw_nr < 0){
                fprintf(stderr,"error: %s, must specify symbol rate or bandwidth\n",argv[0]);
                return 1;
            }
            if(bw_nr >= 1.0){
                fprintf(stderr,"error: %s, must specify bandwidth in range (0,1)\n",argv[0]);
                return 1;
            }
            if(ms_f > 8 && cpf_type > 3){
                fprintf(stderr,"error: %s, invalid cpfsk type, must be in {0,1,2,3}\n",argv[0]);
                return 1;
            }
            else if((ms_f < 9 || ms_f > 16) && cpf_type > 3){
                printf("warn: %s, invalid cpfsk type, but value is ignored\n",argv[0]);
            }
            
            /// correct any user input that's invalid
            //// mod_index
            if(ms_f > 16) mod_index = 0.5;
            if(ms_f < 9) mod_index = 1.0;//???
            //// cpf_type
            if(ms_f < 9) cpf_type = 4;
            else if(ms_f > 16 && ms_f < 25) cpf_type = 0;
            else if(ms_f > 24) cpf_type = 3;
            else if (ms_f < 17 && cpf_type == 20) cpf_type = 0;
            M = 1<<((((unsigned int)ms_f)-1)%8+1);
            k;
            // double freq_dist = mod_index*symbol_rate
            // double bandwidth = (M-1)*freq_dist + 2*symbol_rate;
            // double bandwidth = (M-1)*mod_index*symbol_rate + 2*symbol_rate;
            // double bandwidth = ((M-1)*mod_index + 2)*symbol_rate;
            if(symbol_rate <= 0){
                if(ms_f == 25){
                    //gmsk
                    symbol_rate = bw_f/1.5;
                }
                else if(ms_f > 8){
                    //cpfsk
                    symbol_rate = bw_f/((M-1)*mod_index + 2);
                }
                else{
                    //fsk
                    symbol_rate = bw_f/(M + 1);
                }
                if (symbol_rate * M > uhd_tx_rate) symbol_rate = uhd_tx_rate/M;
            }
            if(symbol_rate * M > uhd_tx_rate){// M >= k error // k = uhd_tx_rate / symbol_rate
                fprintf(stderr,"error: %s, symbol_rate(%1.3e) and modulation order(%u) exceed sample_rate settings\n",argv[0],symbol_rate,M);
                return 1;
            }
            char good_config = 0;
            k = (unsigned int) roundf(uhd_tx_rate/symbol_rate);
            printf("Trying: sr(%.3e),bw(%.3f),rate(%.3e),modidx(%.3f),order(%u),sps(%u)\n",symbol_rate,bw_nr,uhd_tx_rate,mod_index,M,k);
            //finding a good bw
            //double sps_ideal = uhd_tx_rate/symbol_rate
            //uint sps_actual = round(uhd_tx_rate/symbol_rate)
            //bw = {  1.5/sps_actual ; gmsk
            //        (M+1)/sps_actual ; fsk
            //        ((M-1)*h + 2)/sps_actual ; general
            //
            double setting_bw;
            if(ms_f == 25) setting_bw = 1.5/float(k);
            else if(ms_f > 8){
                //cpfsk
                setting_bw = ((M-1)*mod_index + 2)/float(k);
            }
            else{
                //fsk
                setting_bw = (M + 1)/float(k);
            }
            if(ms_f < 9 || ms_f == 25){
                if(k >= M) good_config = 1;
            }
            else{
                if(k%2 == 0 && k>=M) good_config = 1;
            }
            if(!good_config) {
                if (k > M) k--; // must be odd number
                else k=M;//lower than M and M is even
            }
            if(setting_bw > bw_nr) good_config=0;
            while( ! good_config ){
                k++;
                symbol_rate = uhd_tx_rate/float(k);
                if(ms_f == 25) setting_bw = 1.5/float(k);
                else if(ms_f > 8){
                    //cpfsk
                    k += (k%2);
                    setting_bw = ((M-1)*mod_index + 2)/float(k);
                }
                else{
                    //fsk
                    setting_bw = (M + 1)/float(k);
                }
                if(setting_bw > bw_nr) good_config = 0;
                else good_config = 1;
            }
        }
        else{
            if(ms_a != LIQUID_ANALOG_UNKNOWN){
                // analog waveform
                if (ms_a >= 9 && mod_index >= 0.5){
                    throw std::runtime_error("not handling FM waveforms with mod_index >= 0.5 yet.");
                }
            }
            //tone or noise
        }
    }
    else{
        //LINMOD waveform
    }
    bw_f = bw_nr*uhd_tx_rate;

    if ((ms != LIQUID_MODEM_UNKNOWN || ms_f != LIQUID_FSK_UNKNOWN || ms_a != LIQUID_ANALOG_UNKNOWN) && !(modulation == "noise" || modulation == "awgn"))
        ms_n = LIQUID_NOISE_UNKNOWN;

    printf("Using:\n");
    printf("  modulation:   %s\n",modulation.c_str());
    printf("    %d    %d    %d    %d\n", (int)ms, (int)ms_f, (int)ms_a, (int)ms_n);
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    printf("  num_bursts:   %d\n",num_bursts);
    printf("  dwell:        %.3f\n",dwell);
    printf("  squelch:      %.3f\n",squelch);
    printf("  hop_time:     %.3f\n",hop_time);
    printf("  bw_f:         %.3f\n",bw_f);
    printf("  bw_nr:        %.3f\n",bw_nr);
    printf("  span:         %.3f\n",span);
    printf("  channels:     %d\n",num_channels);
    printf("  sweep:        %u\n",sweep);
    printf("  duration:     %.3f\n",duration);
    printf("  idle_time:    %.3f\n",loop_delay);
    printf("  loops:        %d\n",num_loops);
    printf("  json:         %s\n",json.c_str());
    if(ms == LIQUID_MODEM_UNKNOWN && ms_f != LIQUID_FSK_UNKNOWN){
        printf("  order:        %u\n",M);
        printf("  sps:          %u\n",k);
        printf("  h:            %.3f\n",mod_index);
        printf("  cpfsk_type:   ");
        switch (cpf_type){
            case 0: printf("Square\n"); break;
            case 1: printf("RCos Full\n"); break;
            case 2: printf("RCos Partial\n"); break;
            case 3: printf("GMSK\n"); break;
            default: printf("Ignoring\n"); break;
        }
    }
    else if(ms_a != LIQUID_ANALOG_UNKNOWN){
        printf("  h:            %.3f\n",mod_index);
        printf("  T:            %.3f\n",period);
        printf("  F:            %.3f\n",src_fq);
    }
    //
    printf("hop_dur:%u, bw:%.6f\n", hop_dur, bw_nr);

    // generate sequence
    unsigned long int num_samples = hop_dur * num_bursts;

    // vector buffer to send data to USRP
    std::vector<std::complex<float> > usrp_buffer(num_samples);

    // generate sequence and get labels
    std::vector<burst> bursts;
    if(ms == LIQUID_MODEM_UNKNOWN && ms_f == LIQUID_FSK_UNKNOWN && ms_a == LIQUID_ANALOG_UNKNOWN && ms_n != LIQUID_NOISE_UNKNOWN){
        //noise
        bursts = generate_sequence(bw_nr, hop_dur, num_bursts, usrp_buffer.data(), uhd_tx_freq, uhd_tx_rate, ms_n, sweep,
            span,num_channels,dwell*uhd_tx_rate,squelch*uhd_tx_rate);
    }
    else if(ms == LIQUID_MODEM_UNKNOWN && ms_a == LIQUID_ANALOG_UNKNOWN && ms_n == LIQUID_NOISE_UNKNOWN){//fskmod or tone
        bursts = generate_sequence(bw_nr, hop_dur, num_bursts, usrp_buffer.data(), uhd_tx_freq, uhd_tx_rate, ms_f, sweep,
            span,num_channels,dwell*uhd_tx_rate,squelch*uhd_tx_rate,k, mod_index, cpf_type);
    }
    else if(ms == LIQUID_MODEM_UNKNOWN && ms_a != LIQUID_ANALOG_UNKNOWN){
        bursts = generate_sequence(bw_nr, hop_dur, num_bursts, usrp_buffer.data(), uhd_tx_freq, uhd_tx_rate, ms_a, sweep,
            span,num_channels,dwell*uhd_tx_rate,squelch*uhd_tx_rate, mod_index, src_fq/uhd_tx_rate);
    }
    else{//linmod
        bursts = generate_sequence(bw_nr, hop_dur, num_bursts, usrp_buffer.data(), uhd_tx_freq, uhd_tx_rate, ms, sweep,
            span,num_channels,dwell*uhd_tx_rate,squelch*uhd_tx_rate);
    }

    float maxv= 0.;
    size_t fail_counter = 0;
    for(size_t buf_idx = 0; buf_idx < usrp_buffer.size(); buf_idx++){
        if (std::abs(usrp_buffer[buf_idx].real()) > maxv){
            maxv = std::abs(usrp_buffer[buf_idx].real());
            fail_counter++;
        }
        if (std::abs(usrp_buffer[buf_idx].imag()) > maxv){
            maxv = std::abs(usrp_buffer[buf_idx].imag());
            fail_counter++;
        }
    }
    std::cout << "FOUND " << fail_counter << "/" << usrp_buffer.size() << " buffer failures, with a max of " << maxv << std::endl;
    float fail_scale = 0.5/maxv;
    for(size_t buf_idx = 0; buf_idx < usrp_buffer.size(); buf_idx++){
        usrp_buffer[buf_idx] *= fail_scale;
    }

    // for (auto burst_info: bursts)
    //     burst_info.print();
    bursts[0].print();
    bursts[bursts.size()-1].print();

    if (debug) {
        // export output
        std::string fname{"usrp_fhssgen.dat"};
        FILE * fid = fopen(fname.c_str(),"wb");
        fwrite(usrp_buffer.data(), sizeof(std::complex<float>), hop_dur * num_bursts, fid);
        fclose(fid);
        printf("output written to %s\n", fname.c_str());
        //return 0;
    }


    chrono_time[1] = get_time();

    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = false;  // never SOB when continuous
    md.end_of_burst   = true;  // 
    md.has_time_spec  = false;  // set to false to send immediately

    uhd::device_addr_t args(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // try to configure hardware
    usrp->set_tx_gain(0);
    usrp->set_tx_rate(uhd_tx_rate);
    usrp->set_tx_freq(uhd_tx_freq);
    usrp->set_tx_bandwidth(uhd_tx_rate*span*1.05);

    // stream
    std::vector<size_t> channel_nums;
    channel_nums.push_back(0);
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);
    tx_stream->send("", 0, md);
    usrp->set_tx_gain(uhd_tx_gain);

    // get actual rate
    uhd_tx_rate = usrp->get_tx_rate();

    md.start_of_burst = true;  // never SOB when continuous
    // md.end_of_burst   = true;  // Trying to send the whole burst at once, sooooo all are true
    md.end_of_burst   = false;  // Trying to chunk things up now
    md.has_time_spec  = true;  // set to false to send immediately

    // buffer of buffers? I guess?
    std::vector<std::complex<float>*> bufs(channel_nums.size(), &usrp_buffer.front());
    std::vector<std::complex<float>*> buf_ptr(channel_nums.size(), &usrp_buffer.front());
    std::vector<std::complex<float> > usrp_zeros(tx_stream->get_max_num_samps(), std::complex<float>(0.0f,0.0f));

    // TODO: convert to int16?

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running ";
    if (duration > 0) std::cout << "for " << duration << " seconds (" << num_loops << " loops)";
    std::cout << "Using TX bandwidth: " << usrp->get_tx_bandwidth();
    std::cout << " (hit CTRL-C to stop)" << std::endl;

    unsigned int count = 0;
    chrono_time[2] = get_time();
    usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);
    std::cout << " ---- spinup time: " << chrono_time[2]-chrono_time[0] << " ----\n";

    labels* reporter = nullptr;
    if(!json.empty()){
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG1","TXDL S1");
        reporter->set_modulation( modulation );
        reporter->eng_bw = bw_f;
        reporter->start_reports();
    }
    chrono_time[6] = chrono_time[2]-loop_time+0.5;                // 'prev' TX time
    double initial_start = chrono_time[6] + loop_time;
    uint64_t xfer_counter = 0;
    uint64_t xfer = 0,xfer_idx = 0;
    size_t xfer_len = 0;
    size_t chunk_size = 4000;
    size_t send_size;
    while (continue_running) {
        xfer = 0;
        xfer_idx = 0;
        xfer_counter = 0;
        xfer_len = usrp_buffer.size();
        md.start_of_burst = true;
        md.has_time_spec  = true;
        chrono_time[6] += loop_time;
        md.time_spec = uhd::time_spec_t(chrono_time[6]);

        // Reset any shifts in the buffers
        for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
            buf_ptr[cidx] = bufs[cidx];
        }

        while((xfer == 0 && xfer_len > 0) && continue_running){
            // send the result to the USRP
            send_size = std::min(xfer_len,chunk_size);
            if (send_size == xfer_len){
                md.end_of_burst = true;
            }
            xfer = tx_stream->send(buf_ptr, send_size, md); // This blocks?
            if(xfer <= xfer_len){
                xfer_counter += xfer;
                xfer_len -= xfer;
                xfer_idx += xfer;

                if(xfer_len > 0){
                    // Not everything was sent, so shift one buffer set into the other (account for what was sent)
                    for(size_t cidx = 0; cidx < channel_nums.size(); cidx++){
                        buf_ptr[cidx] = &(bufs[cidx][xfer_idx]);
                    }
                }

                xfer = 0;
                if(md.start_of_burst){
                    md.start_of_burst = false;
                    md.has_time_spec  = false;
                }
            }
        }

        export_json(reporter, bursts, chrono_time[6], loop_time, uhd_tx_freq, uhd_tx_rate,
                    usrp_buffer.size(), xfer_counter);

        if (!continue_running){/// might have stopped before a loop finished
            std::cout << "Got to stop running with " << xfer_counter << " samples out of "
                << usrp_buffer.size() << " samples in the buffer\n";
        }

        // check early exit criterion
        count++;
        if ((int)count >= num_loops)
            break;
        if(duration > 0 && get_time() > initial_start + duration) continue_running=false;


        // // send zeros for now
        // size_t zero_count = (size_t)(loop_delay * usrp->get_tx_rate());
        // while (zero_count && continue_running) {
        //     auto zeros_to_send = std::min(zero_count, usrp_zeros.size());
        //     tx_stream->send(&usrp_zeros.front(), zeros_to_send, md);
        //     zero_count -= zeros_to_send;
        // }
    }
    continue_running = false;
 
    // send a mini EOB packet
    md.start_of_burst = false;
    md.end_of_burst   = true;
    tx_stream->send("",0,md);

    chrono_time[3] = get_time();

    // sleep for a small amount of time to allow USRP buffers to flush
    // usleep(100000);
    while(get_time() < chrono_time[6] + xfer_counter/uhd_tx_rate);
    usrp->set_tx_freq(6e9);
    usrp->set_tx_gain(0.0);

    // finished
    printf("usrp data transfer complete\n");

    chrono_time[4] = get_time();

    printf("Timestamp at program start: cpu sec: %15.9lf\n",chrono_time[0]);
    printf("Connecting to radio at: cpu sec: %15.9lf\n",chrono_time[1]);
    printf("Starting to send at: cpu sec: %15.9lf\n",chrono_time[2]);
    printf("Stopping send at: cpu sec: %15.9lf\n",chrono_time[3]);
    printf("Radio should be stopped at: cpu sec: %15.9lf\n",chrono_time[4]);
    // export to .json if requested
    if (!json.empty()){
        // export_json(json, bursts, start, loop_time, uhd_tx_freq, uhd_tx_rate);
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
        reporter->set_modulation( modulation );
        reporter->modulation_src = modulation;
        reporter->protocol = "unknown";
        if(num_channels == 1){
            reporter->modality = "single_carrier";
        }
        else{
            reporter->modality = "frequency_agile";
        }
        reporter->activity_type = "lowprob_anomaly";
        reporter->device_origin=uhd_tx_args;
        reporter->finalize();
        delete reporter;
    }

    return 0;
}

// // generate a hop of a particular bandwidth and center frequency
// void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
//         symstreamrcf gen, nco_crcf mixer)
// {
//     auto delay = symstreamrcf_get_delay(gen);
//
//     unsigned int dead_time = (unsigned int) (delay + 1.5f);
//
//     // check values
//     if (buf_len < 2*dead_time)
//         throw std::runtime_error("requested hop duration too small");
//
//     // fill buffer
//     unsigned int num_samples_on = buf_len - 2*dead_time;
//
//     symstreamrcf_set_gain(gen, 0.5f);
//     symstreamrcf_write_samples(gen, buf, num_samples_on);
//     symstreamrcf_set_gain(gen, 0.0f);
//     symstreamrcf_write_samples(gen, buf + num_samples_on, 2*dead_time);
//
//     // mix
//     nco_crcf_set_frequency(mixer, 2*M_PI*fc);
//     nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
// }

// generate a hop of a particular bandwidth and center frequency
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch)
{
    auto delay = symstreamrcf_get_delay(gen);
    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // buf_len is the hop_duration
    if (dwell == 0){
        squelch = 0;
        dwell = buf_len - 2*dead_time;
    }
    if (dwell > buf_len - 2*dead_time){
        dwell = buf_len - 2*dead_time;
    }

    unsigned int num_samples_on = dwell;
    dead_time = (2*dead_time > squelch) ? 2*dead_time : squelch;

    // check values
    if (buf_len < num_samples_on+dead_time)
        throw std::runtime_error("requested hop duration too small (-1) (d:"+std::to_string(buf_len)
            +",1:"+std::to_string(num_samples_on)+",0:"+std::to_string(dead_time)+",p:"
            +std::to_string(num_samples_on+dead_time)+")");

    // fill buffer
    // unsigned int num_samples_on = buf_len - 2*dead_time;

    symstreamrcf_set_gain(gen, 0.5f);
    symstreamrcf_write_samples(gen, buf, num_samples_on);
    symstreamrcf_set_gain(gen, 0.0f);
    symstreamrcf_write_samples(gen, buf + num_samples_on, dead_time);

    // mix
    nco_crcf_set_frequency(mixer, 2*M_PI*fc);
    nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
}

// generate a hop of a particular bandwidth and center frequency
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamracf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch)
{
    auto delay = symstreamracf_get_delay(gen);
    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // buf_len is the hop_duration
    if (dwell == 0){
        squelch = 0;
        dwell = buf_len - 2*dead_time;
    }
    if (dwell > buf_len - 2*dead_time){
        dwell = buf_len - 2*dead_time;
    }

    unsigned int num_samples_on = dwell;
    dead_time = (2*dead_time > squelch) ? 2*dead_time : squelch;

    // check values
    if (buf_len < num_samples_on+dead_time)
        throw std::runtime_error("requested hop duration too small (0) (d:"+std::to_string(buf_len)
            +",1:"+std::to_string(num_samples_on)+",0:"+std::to_string(dead_time)+",p:"
            +std::to_string(num_samples_on+dead_time)+")");

    // fill buffer
    // unsigned int num_samples_on = buf_len - 2*dead_time;

    symstreamracf_set_gain(gen, 0.5f);
    symstreamracf_write_samples(gen, buf, num_samples_on);
    symstreamracf_set_gain(gen, 0.0f);
    symstreamracf_write_samples(gen, buf + num_samples_on, dead_time);

    // mix
    nco_crcf_set_frequency(mixer, 2*M_PI*fc);
    nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
}

// generate a hop of a particular bandwidth and center frequency
void generate_hop3(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrncf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch)
{
    auto delay = symstreamrncf_get_delay(gen);
    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // buf_len is the hop_duration
    if (dwell == 0){
        squelch = 0;
        dwell = buf_len - 2*dead_time;
    }
    if (dwell > buf_len - 2*dead_time){
        dwell = buf_len - 2*dead_time;
    }

    unsigned int num_samples_on = dwell;
    dead_time = (2*dead_time > squelch) ? 2*dead_time : squelch;

    // check values
    if (buf_len < num_samples_on+dead_time){
        // Since this is for noise only, we can just jiggle a bit
        num_samples_on = buf_len-dead_time;
        // throw std::runtime_error("requested hop duration too small (0) (d:"+std::to_string(buf_len)
        //     +",1:"+std::to_string(num_samples_on)+",0:"+std::to_string(dead_time)+",p:"
        //     +std::to_string(num_samples_on+dead_time)+")");
    }

    // fill buffer
    // unsigned int num_samples_on = buf_len - 2*dead_time;

    symstreamrncf_set_gain(gen, 0.5f);
    symstreamrncf_write_samples(gen, buf, num_samples_on);
    symstreamrncf_set_gain(gen, 0.0f);
    symstreamrncf_write_samples(gen, buf + num_samples_on, dead_time);

    // mix
    nco_crcf_set_frequency(mixer, 2*M_PI*fc);
    nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
}

// // generate a hop of a particular bandwidth and center frequency
// void generate_hop(float fc, std::complex<float> * buf, unsigned int buf_len,
//         symstreamrfcf gen, nco_crcf mixer)
// {
//     auto delay = symstreamrfcf_get_delay(gen);
//
//     unsigned int dead_time = (unsigned int) (delay + 1.5f);
//
//     // check values
//     if (buf_len < 2*dead_time)
//         throw std::runtime_error("requested hop duration too small");
//
//     // fill buffer
//     unsigned int num_samples_on = buf_len - 2*dead_time;
//
//     symstreamrfcf_set_gain(gen, 0.5f);
//     symstreamrfcf_write_samples(gen, buf, num_samples_on);
//     symstreamrfcf_set_gain(gen, 0.0f);
//     symstreamrfcf_write_samples(gen, buf + num_samples_on, 2*dead_time);
//
//     // mix
//     nco_crcf_set_frequency(mixer, 2*M_PI*fc);
//     nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
// }

// generate a hop of a particular bandwidth and center frequency
void generate_hop2(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrfcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch)
{
    auto delay = symstreamrfcf_get_delay(gen);
    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // buf_len is the hop_duration
    if (dwell == 0){
        squelch = 0;
        dwell = buf_len - 2*dead_time;
    }

    unsigned int num_samples_on = dwell;
    dead_time = (2*dead_time > squelch) ? 2*dead_time : squelch;

    // check values
    if (buf_len < num_samples_on+dead_time)
        // throw std::runtime_error("requested hop duration too small (1) (d:"+std::to_string(buf_len)
        //     +",1:"+std::to_string(num_samples_on)+",0:"+std::to_string(dead_time)+",p:"
        //     +std::to_string(num_samples_on+dead_time)+")");
        num_samples_on = buf_len-dead_time;

    // fill buffer
    // unsigned int num_samples_on = buf_len - 2*dead_time;

    symstreamrfcf_set_gain(gen, 0.5f);
    symstreamrfcf_write_samples(gen, buf, num_samples_on);
    symstreamrfcf_set_gain(gen, 0.0f);
    symstreamrfcf_write_samples(gen, buf + num_samples_on, dead_time);

    // mix
    nco_crcf_set_frequency(mixer, 2*M_PI*fc);
    nco_crcf_mix_block_up(mixer, buf, buf, buf_len);
}

// generate a hop of a particular bandwidth and center frequency
void generate_hop3(float fc, std::complex<float> * buf, unsigned int buf_len,
        symstreamrfcf gen, nco_crcf mixer, unsigned int dwell, unsigned int squelch)
{
    auto delay = symstreamrfcf_get_delay(gen);
    unsigned int dead_time = (unsigned int) (delay + 1.5f);

    // buf_len is the hop_duration
    if (dwell == 0){
        squelch = 0;
        dwell = buf_len - 2*dead_time;
    }

    unsigned int num_samples_on = dwell;
    dead_time = (2*dead_time > squelch) ? 2*dead_time : squelch;

    // check values
    if (buf_len < num_samples_on+dead_time)
    {
        // Since this is for tones only, we can just jiggle a bit
        num_samples_on = buf_len-dead_time;
        // throw std::runtime_error("requested hop duration too small (2) (d:"+std::to_string(buf_len)
        //     +",1:"+std::to_string(num_samples_on)+",0:"+std::to_string(dead_time)+",p:"
        //     +std::to_string(num_samples_on+dead_time)+")");
    }

    // fill buffer
    // unsigned int num_samples_on = buf_len - 2*dead_time;

    // symstreamrfcf_set_gain(gen, 0.5f);
    // symstreamrfcf_write_samples(gen, buf, num_samples_on);
    // symstreamrfcf_set_gain(gen, 0.0f);
    // symstreamrfcf_write_samples(gen, buf + num_samples_on, dead_time);
    for(unsigned int idx = 0; idx < num_samples_on; idx++){
        buf[idx] = 0.5;
    }
    for(unsigned int idx = num_samples_on; idx < buf_len; idx++){
        buf[idx] = 0.0;
    }

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
        bool sweep, float span, int num_channels, unsigned int dwell, unsigned int squelch)
{
    // initialize objects
    unsigned int m = 12;
    symstreamrcf gen = symstreamrcf_create_linear(LIQUID_FIRFILT_ARKAISER, bw, m, 0.25f, ms);
    nco_crcf mixer = nco_crcf_create(LIQUID_VCO);

    //
    std::vector<burst> bursts;

    // std::cout << "Gen Seq Debug:\n";
    // std::cout << "  bw: " << bw << std::endl;
    // std::cout << "  hop_dur: " << hop_dur << std::endl;
    // std::cout << "  dwell: " << (dwell == 0 ? hop_dur : dwell) << std::endl;
    // std::cout << "  squelch: " << (dwell == 0 ? 0 : squelch) << std::endl;
    // std::cout << "  num_hops: " << num_hops << std::endl;
    // std::cout << "  center_freq: " << center_freq << std::endl;
    // std::cout << "  sample_rate: " << sample_rate << std::endl;
    // std::cout << "  span: " << span << std::endl;
    // std::cout << "  num_channels: " << num_channels << std::endl;

    // generate individual hops
    for (auto i=0U; i<num_hops; i++) {
        float fc;
        if(num_channels <= 0){
            fc = sweep ? span*(1-1.2*bw)*get_uniform_fc(i,num_hops) : span*(1-1.2*bw)*get_rand_fc();
        }
        else{
            fc = sweep ? get_channel_fc(i%num_channels,span,num_channels) : get_channel_fc(-1,span,num_channels);
        }
        // generate_hop(fc, buf + i*hop_dur, hop_dur, gen, mixer);
        generate_hop2(fc, buf + i*hop_dur, hop_dur, gen, mixer, dwell, squelch);

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

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq, float sample_rate, fsk_scheme ms,
        bool sweep, float span, int num_channels, unsigned int dwell, unsigned int squelch,
        unsigned int k, double mod_index, unsigned int cpf_type)
{
    uint8_t tweak = (ms == LIQUID_FSK_UNKNOWN);
    // initialize objects
    if (tweak){
        ms = LIQUID_MODEM_FSK2;
        k = 2;
        mod_index = 0.5;
        bw = 0.01;
        cpf_type = 0;
    }
    symstreamrfcf gen = symstreamrfcf_create_fsk(LIQUID_FIRFILT_ARKAISER, k, mod_index, 12, bw, 0.35, cpf_type, ms);
    nco_crcf mixer = nco_crcf_create(LIQUID_VCO);
    if (tweak){
        bw = 0.01;
        ms = LIQUID_FSK_UNKNOWN;
    }

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
        if(tweak){
            generate_hop3(fc, buf + i*hop_dur, hop_dur, gen, mixer, dwell, squelch);
        }
        else{
            // generate_hop(fc, buf + i*hop_dur, hop_dur, gen, mixer);
            generate_hop2(fc, buf + i*hop_dur, hop_dur, gen, mixer, dwell, squelch);
        }

        // append to bursts
        bursts.emplace_back(center_freq + fc*sample_rate, bw*sample_rate,
            (float)(i*hop_dur)/sample_rate, (float)hop_dur/sample_rate, ms);
    }

    // free objects
    symstreamrfcf_destroy(gen);
    nco_crcf_destroy(mixer);

    // return list of bursts
    return bursts;
}

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq, float sample_rate, analog_scheme ms,
        bool sweep, float span, int num_channels, unsigned int dwell, unsigned int squelch,
        double mod_index, double src_freq)
{
    // initialize objects
    unsigned int m = 12;
    symstreamracf gen = symstreamracf_create_analog(LIQUID_FIRFILT_ARKAISER, bw, m, 0.25f, mod_index, src_freq, ms);
    nco_crcf mixer = nco_crcf_create(LIQUID_VCO);

    //
    std::vector<burst> bursts;

    // std::cout << "Gen Seq Debug:\n";
    // std::cout << "  bw: " << bw << std::endl;
    // std::cout << "  hop_dur: " << hop_dur << std::endl;
    // std::cout << "  dwell: " << (dwell == 0 ? hop_dur : dwell) << std::endl;
    // std::cout << "  squelch: " << (dwell == 0 ? 0 : squelch) << std::endl;
    // std::cout << "  num_hops: " << num_hops << std::endl;
    // std::cout << "  center_freq: " << center_freq << std::endl;
    // std::cout << "  sample_rate: " << sample_rate << std::endl;
    // std::cout << "  span: " << span << std::endl;
    // std::cout << "  num_channels: " << num_channels << std::endl;

    // generate individual hops
    for (auto i=0U; i<num_hops; i++) {
        float fc;
        if(num_channels <= 0){
            fc = sweep ? span*(1-1.2*bw)*get_uniform_fc(i,num_hops) : span*(1-1.2*bw)*get_rand_fc();
        }
        else{
            fc = sweep ? get_channel_fc(i%num_channels,span,num_channels) : get_channel_fc(-1,span,num_channels);
        }
        // generate_hop(fc, buf + i*hop_dur, hop_dur, gen, mixer);
        generate_hop2(fc, buf + i*hop_dur, hop_dur, gen, mixer, dwell, squelch);

        // append to labels
        bursts.emplace_back(center_freq + fc*sample_rate, bw*sample_rate,
            (float)(i*hop_dur)/sample_rate, (float)hop_dur/sample_rate, ms);
    }

    // free objects
    symstreamracf_destroy(gen);
    nco_crcf_destroy(mixer);

    // return list of bursts
    return bursts;
}

// generate a sequence of hops
std::vector<burst> generate_sequence(float bw, unsigned int hop_dur, unsigned int num_hops,
        std::complex<float> * buf, float center_freq, float sample_rate, noise_scheme ms,
        bool sweep, float span, int num_channels, unsigned int dwell, unsigned int squelch)
{
    // initialize objects
    unsigned int m = 12;
    symstreamrncf gen = symstreamrncf_create_noise(LIQUID_FIRFILT_ARKAISER, bw, m, 0.25f, ms);
    nco_crcf mixer = nco_crcf_create(LIQUID_VCO);

    //
    std::vector<burst> bursts;

    // std::cout << "Gen Seq Debug:\n";
    // std::cout << "  bw: " << bw << std::endl;
    // std::cout << "  hop_dur: " << hop_dur << std::endl;
    // std::cout << "  dwell: " << (dwell == 0 ? hop_dur : dwell) << std::endl;
    // std::cout << "  squelch: " << (dwell == 0 ? 0 : squelch) << std::endl;
    // std::cout << "  num_hops: " << num_hops << std::endl;
    // std::cout << "  center_freq: " << center_freq << std::endl;
    // std::cout << "  sample_rate: " << sample_rate << std::endl;
    // std::cout << "  span: " << span << std::endl;
    // std::cout << "  num_channels: " << num_channels << std::endl;

    // generate individual hops
    for (auto i=0U; i<num_hops; i++) {
        float fc;
        if(num_channels <= 0){
            fc = sweep ? span*(1-1.2*bw)*get_uniform_fc(i,num_hops) : span*(1-1.2*bw)*get_rand_fc();
        }
        else{
            fc = sweep ? get_channel_fc(i%num_channels,span,num_channels) : get_channel_fc(-1,span,num_channels);
        }
        // generate_hop(fc, buf + i*hop_dur, hop_dur, gen, mixer);
        generate_hop3(fc, buf + i*hop_dur, hop_dur, gen, mixer, dwell, squelch);

        // append to labels
        bursts.emplace_back(center_freq + fc*sample_rate, bw*sample_rate,
            (float)(i*hop_dur)/sample_rate, (float)hop_dur/sample_rate, ms);
    }

    // free objects
    symstreamrncf_destroy(gen);
    nco_crcf_destroy(mixer);

    // return list of bursts
    return bursts;
}

void export_json(labels *reporter, std::vector<burst> bursts,
        double start, double loop_time,
        double center_freq, double sample_rate,
        uint64_t meant_to_send, uint64_t sent)
{
    if(reporter==nullptr) return;
    double fs = (double)meant_to_send/loop_time;
    double end_time = start + (double)sent/fs;
    for (auto burst_info: bursts) {
        double start_time = start + burst_info.t0;
        if((meant_to_send == sent) || (start_time < end_time)){
            if(burst_info.type == 1){
                reporter->set_modulation( burst_info.ms_f);
            }
            if(burst_info.type == 2){
                reporter->set_modulation( burst_info.ms_a);
            }
            if(burst_info.type == 3){
                reporter->set_modulation( burst_info.ms_n);
            }
            else{
                reporter->set_modulation( burst_info.ms );
            }
            reporter->append(
                start_time,
                burst_info.dur,
                burst_info.fc,
                burst_info.bw);
        }
    }
}

