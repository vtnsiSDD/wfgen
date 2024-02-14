// transmit tone signal with offset
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <getopt.h>
#include <iostream>
#include <complex>
#include <csignal>

#include "liquid.h"
#include "fskmodems.hh"
#include "labels.hh"

#include <uhd/usrp/multi_usrp.hpp>

static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "FSKMOD ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}


/////////////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char **argv)
{
    double      uhd_tx_freq = 2.46e9;
    double      uhd_tx_rate = 100e3f;
    double      uhd_tx_gain =   60.0;
    std::string uhd_tx_args{"type=b200"};
    double      grange      =   0.0f;    // software cycle gain range
    double      gcycle      =   2.0f;    // gain cycle duration
    std::string modulation{"fsk4"};
    std::string json("");
    std::string file_dump{""};
    double      bw_f      =   -1;
    double      bw_nr      =   -1;
    double      mod_index     =   1.0f;
    double      symbol_rate =   -1;
    unsigned int cpf_type   =   20;
    unsigned int dry_run    =    0;
    uint8_t     cut_radio   =    0;

    const int max_chrono = 5;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();

    fsk_scheme ms_f = LIQUID_MODEM_FSK4;

    double duration = -1;
    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:M:B:b:x:R:P:d:j:W:C:z:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -M <modulation:%s> ] [ -B <bw_f:%.3f MHz> ]\n", uhd_tx_args.c_str(), modulation.c_str(), bw_f*1.0e-06);
            printf("  [ -b <bw_nr:%.3f NHz> ] [ -x <mod_index:%.3f> ] [ -R <symbol_rate:%.3f kHz> ]\n", bw_nr, mod_index, symbol_rate*1.0e-03);
            printf("  [ -P <cpf_type:%d> ] [ -d <duration:%.3f s> ] [ -j <json:%s> ]\n", cpf_type, duration, json.c_str());
            printf("  [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ] [ -z <dry_run:%u> ]\n", file_dump.c_str(), cut_radio, dry_run);
            printf(" available modulation schemes:\n");
            liquid_print_fsk_modulation_schemes();
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 'M':{
            modulation.assign(optarg);
            ms_f = liquid_getopt_str2fsk(optarg);
            if (ms_f == LIQUID_FSK_UNKNOWN){
                fprintf(stderr,"error: %s, unknown/unsupported modulation scheme '%s'\n", argv[0], optarg);
                return 1;
            }
            break;
        }
        case 'B': bw_f        =  strtod(optarg, &strend); break;
        case 'b': bw_nr       =  strtod(optarg, &strend); break;
        case 'x': mod_index   =  strtod(optarg, &strend); break;
        case 'R': symbol_rate =  strtod(optarg, &strend); break;
        case 'P': cpf_type    =  strtol(optarg, &strend, 10); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'z': dry_run     = strtoul(optarg, &strend, 10); break;
        default: exit(1);
        }
    }

    uint8_t got_BW = bw_f > 0;
    uint8_t got_bw = bw_nr > 0;
    uint8_t got_sr = symbol_rate > 0;
    // std::cout << "debug: (" << (int)got_bw << "," << (int)got_BW << "," << (int)got_sr <<")\n";
    // std::cout << "debug: (" << bw_nr << "," << bw_f << "," << symbol_rate <<")\n";

    if ((got_BW + got_bw + got_sr > 1) || (got_BW + got_bw + got_sr == 0)){
        fprintf(stderr,"Must specify one and only one of {-B, -b, -R}\n");
    }

    if (bw_f > 0){
        bw_nr = bw_f/uhd_tx_rate;
    }
    // std::cout << "debug: (" << bw_nr << "," << bw_f << "," << symbol_rate <<")\n";
    if(bw_nr > 0){
        if(bw_nr >= 1.0){
            fprintf(stderr,"error: %s, must specify bandwidth in range (0,1)\n",argv[0]);
            return 1;
        }
        bw_f = uhd_tx_rate*bw_nr;
    }
    // std::cout << "debug: (" << bw_nr << "," << bw_f << "," << symbol_rate <<")\n";
    if((ms_f > 8) && (ms_f < 17)  && ((cpf_type > 3 || cpf_type < 0) && (cpf_type !=20))){
        fprintf(stderr,"error: %s, invalid cpfsk type, must be in {0,1,2,3}\n",argv[0]);
        return 1;
    }
    else if((ms_f < 9 || ms_f > 16) && (cpf_type != 20)){
        printf("warn: %s, cpfsk type is ignored for this modulation\n",argv[0]);
    }
    if((ms_f < 9) && (mod_index!=1.0f)){
        printf("warn: %s, mod_index ignored for FSK, set to 1.0\n",argv[0]);
    }
    if((ms_f > 16) && (mod_index!=1.0f)){
        printf("warn: %s, mod_index ignored for S/GCPFSK, set to 0.5\n",argv[0]);
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
    unsigned int M = 1<<((((unsigned int)ms_f)-1)%8+1);
    unsigned int k;
    if(symbol_rate > 0){
        if(symbol_rate * M > uhd_tx_rate){
            fprintf(stderr,"error: %s, symbol_rate(%1.3e) and modulation order(%u) exceed sample_rate(%1.3e) settings\n",argv[0],symbol_rate,M,uhd_tx_rate);
            return 1;
        }
        if (ms_f == 25) bw_f = 1.5*symbol_rate; // gmsk
        else if (ms_f > 8) bw_f = symbol_rate * ((M-1)*mod_index + 2); //cpfsk
        else bw_f = symbol_rate*(M+1); //fsk
        bw_nr = bw_f/uhd_tx_rate;
    }

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
        if (symbol_rate * M > uhd_tx_rate){
            symbol_rate = uhd_tx_rate/M;
            printf("warn: %s, derived symbol rate(%1.3e) exceeds limits(%1.3e), adjusting to limit\n",argv[0],symbol_rate,uhd_tx_rate/M);
            if (ms_f == 25) bw_f = 1.5*symbol_rate;
            else if (ms_f > 8) bw_f = symbol_rate * ((M-1)*mod_index + 2);
            else bw_f = symbol_rate*(M+1);
        }
    }
    // std::cout << "debug: (" << bw_nr << "," << bw_f << "," << symbol_rate <<")\n";
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

    printf("Using:\n");
    printf("  modulation:   %s\n",modulation.c_str());
    printf("  order:        %u\n",M);
    printf("  sps:          %u\n",k);
    printf("  baud rate:    %.3f\n",symbol_rate);
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  bw:           %.3f\n",bw_nr);
    printf("  h:            %.3f\n",mod_index);
    printf("  resamp:       %.3f\n",setting_bw/bw_nr);
    printf("  cpfsk_type:   ");
    switch (cpf_type){
        case 0: printf("Square\n"); break;
        case 1: printf("RCos Full\n"); break;
        case 2: printf("RCos Partial\n"); break;
        case 3: printf("GMSK\n"); break;
        default: printf("Ignoring\n"); break;
    }
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    printf("  grange:       %.3f\n",grange);
    printf("  cycle:        %.3f\n",gcycle);
    printf("  delta_f:      %.3f\n",symbol_rate*mod_index);

    if(k > 2048){
        fprintf(stderr,"error: %s, effective symbol rate (%1.3f kHz) is too low\n",argv[0],symbol_rate/1e3);
        return 1;
    }
    if((bw_nr/uhd_tx_rate) > 0.99){
        fprintf(stderr,"error: %s, effective bandwidth (%1.3f kHz) is too high\n",argv[0],bw_nr/1e3);
        return 1;
    }

    if(dry_run == 1){
        return 0;
    }
    chrono_time[1] = get_time();
    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = false;  // never SOB when continuous
    md.end_of_burst   = true;   // 
    md.has_time_spec  = false;  // set to false to send immediately

    uhd::device_addr_t args(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // try to configure hardware
    usrp->set_tx_rate(uhd_tx_rate);
    usrp->set_tx_freq(uhd_tx_freq);
    usrp->set_tx_gain(0);
    usrp->set_tx_bandwidth(bw_f);


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
    unsigned int buf_len = 800;
    std::vector<std::complex<float> > usrp_buffer(buf_len);

    // buffer of buffers? I guess?
    std::vector<std::complex<float>*> bufs(channel_nums.size(), &usrp_buffer.front());

    // signal generator
    // SYMSTREAMRFcf gen = SYMSTREAMRFcf_create_linear(
    //     LIQUID_FIRFILT_ARKAISER, bw_nr, 12, 0.25f, ms_f);
    symstreamrfcf gen = symstreamrfcf_create_fsk(
        LIQUID_FIRFILT_ARKAISER, k, mod_index, 12, bw_nr, 0.35, cpf_type, ms_f);

    printf("Filtering settings of generator:\n");
    printf("  Interp: %u", gen->interp == NULL ? 1 : 2);
    printf("  Arb: %f\n", gen->rate);

    // printf("Debug sanity check M(%u) bps(%u) k(%u) rate(%f)\n",gen->M,gen->bps,gen->k,gen->rate);
    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = false;   // 
    md.has_time_spec  = true;  // set to false to send immediately

    // gain cycle
    uhd_tx_rate = usrp->get_tx_rate(); // get actual rate
    unsigned long int num_samples_cycle = (unsigned long int) (gcycle * uhd_tx_rate);
    unsigned long int num_buffers_cycle = num_samples_cycle / buf_len;
    std::cout << num_samples_cycle << "," << num_buffers_cycle << std::endl;
    unsigned int buffer_counter=0;

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running (hit CTRL-C to stop)" << std::endl;
    std::complex<float> * buf = usrp_buffer.data();
    std::complex<float> sym;
    chrono_time[2] = get_time();
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
    while (continue_running) {
        // set the software gain
        float gain_dB = -grange*(0.5f - 0.5f*cosf(2*M_PI*(float)buffer_counter/(float)num_buffers_cycle));
        float gain = 0.5f*powf(10.0f, gain_dB/20.0f);
        symstreamrfcf_set_gain(gen, gain);
        //printf("%6u : gain=%8.3f\n", buffer_counter, gain_dB);
        buffer_counter = (buffer_counter+1) % num_buffers_cycle;

        // generate samples to buffer
        symstreamrfcf_write_samples(gen, buf, buf_len);

        xfer = 0;
        xfer_len = buf_len;
        while((xfer == 0 && xfer_len > 0) && continue_running){
            // send the result to the USRP
            xfer = tx_stream->send(bufs, xfer_len, md);
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
        if(duration > 0 && get_time() > initial_start+duration) break;
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
    symstreamrfcf_destroy(gen);
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
        // reporter->set_modulation(modulation);
        reporter->set_modulation(modulation);
        reporter->modulation_src = modulation;
        reporter->modality="single_carrier";
        reporter->device_origin=uhd_tx_args;

        reporter->finalize();
        delete reporter;
    }
    return 0;
}

