// transmit tone signal with offset
#include <getopt.h>
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <csignal>
#include <string.h>
#include <stdarg.h>
#include <uhd/usrp/multi_usrp.hpp>

#include "liquid.h"
#include "labels.hh"
#include "afmodem.hh"

static bool continue_running(true);
void signal_interrupt_handler(int) {
    std::cout << "FM ---> ctrl+c received --> exiting\n";
    continue_running = false;
}

double get_time(){
    return std::chrono::system_clock::now().time_since_epoch().count()*double(1e-9);
}

void gen_sin(std::complex<float> *buf, size_t n, double fs, double period, double mod_index, double &phi, double &theta);

int main (int argc, char **argv)
{
    double uhd_tx_freq      = 2.46e9;
    double uhd_tx_rate      =   4.0e6;
    double uhd_tx_gain      =  60.0;
    std::string uhd_tx_args{"type=b200"};
    std::string modulation{"am"};
    std::string json("");
    std::string file_dump{""};
    double      bw_nr       =   -1;
    double      bw_f        =   -1;
    double      grange      =   0.0f;    // software cycle gain range
    double      gcycle      =   2.0f;    // gain cycle duration
    double      period      =   -1;
    double      src_fq      =   -1;
    double      mod_index     =   0.25f;
    unsigned int dry_run    =    0;
    uint8_t     cut_radio   =    0;

    const int max_chrono = 5;
    double chrono_time[max_chrono];
    memset(chrono_time, 0, max_chrono*sizeof(double));
    chrono_time[0] = get_time();

    wav_reader wav = wav_reader_create();
    uint64_t wav_samples = wav_reader_get_len(wav);
    // for(uint index = 0; index < wav->file_count; index++){
    //     print_wav_info(wav->wavs[index], 0, 0, 0);
    // }

    srand(uint64_t(get_time()*1e9));

    double duration = -1.0;
    int dopt;
    char *strend = NULL;
    while ((dopt = getopt(argc,argv,"hf:r:g:a:M:B:b:d:p:x:F:j:W:C:z:")) != EOF) {
        switch (dopt) {
        case 'h':
            printf("Usage of %s [options]\n",argv[0]);
            printf("  [ -f <uhd_tx_freq:%.3f MHz> ] [ -r <uhd_tx_rate:%.3f MHz> ] [ -g <uhd_tx_gain:%.3f dB> ]\n", uhd_tx_freq*1.0e-06, uhd_tx_rate*1.0e-06, uhd_tx_gain);
            printf("  [ -a <uhd_tx_args:%s> ] [ -M <modulation:%s> ] [ -B <bw_f:%.3f MHz> ]\n", uhd_tx_args.c_str(), modulation.c_str(), bw_f*1.0e-06);
            printf("  [ -b <bw_nr:%.3f NHz> ] [ -d <duration:%.3f s> ] [ -p <period:%.3f s> ]\n", bw_nr, duration, period);
            printf("  [ -x <mod_index:%.3f> ] [ -F <src_fq:%.3f MHz> ] [ -j <json:%s> ]\n", mod_index, src_fq*1.0e-06, json.c_str());
            printf("  [ -W <file_dump:%s> ] [ -C <cut_radio:%u> ] [ -z <dry_run:%u> ]\n", file_dump.c_str(), cut_radio, dry_run);
            return 0;
        case 'f': uhd_tx_freq =  strtod(optarg, &strend); break;
        case 'r': uhd_tx_rate =  strtod(optarg, &strend); break;
        case 'g': uhd_tx_gain =  strtod(optarg, &strend); break;
        case 'a': uhd_tx_args   .assign(optarg); break;
        case 'M': modulation    .assign(optarg); break;
        case 'B': bw_f        =  strtod(optarg, &strend); break;
        case 'b': bw_nr       =  strtod(optarg, &strend); break;
        case 'd': duration    =  strtod(optarg, &strend); break;
        case 'p': period      =  strtod(optarg, &strend); break;
        case 'x': mod_index   =  strtod(optarg, &strend); break;
        case 'F': src_fq      =  strtod(optarg, &strend); break;
        case 'j': json          .assign(optarg); break;
        case 'W': file_dump     .assign(optarg); break;
        case 'C': cut_radio   = strtoul(optarg, &strend, 10); break;
        case 'z': dry_run     = strtoul(optarg, &strend, 10); break;
        default: exit(1);
        }
    }

    
    if(period > 0 && src_fq > 0) throw std::runtime_error("Only define one of (period, src_rate)");
    else if(period < 0 && src_fq < 0) src_fq = 0.2;
    else if(period > 0) src_fq = 1/period;
    else period = 1/src_fq;

    if(bw_nr > 0.0){
        bw_f = bw_nr * uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_nr specified directly as: " << bw_nr << " for a bw_f: " << bw_f << "Hz at rate: " << uhd_tx_rate << "Hz\n";
    }
    else if(bw_f > 0.0){
        bw_nr = bw_f / uhd_tx_rate; // specified relative, overwriting other
        std::cout << "bw_f specified directly as: " << bw_f << " Hz at rate: " << uhd_tx_rate << "Hz for a bw_nr: " << bw_nr << std::endl;
    }

    analog_scheme afscheme = liquid_getopt_str2analog(modulation.c_str());
    if(afscheme == LIQUID_ANALOG_UNKNOWN){
        std::cout << (int)afscheme << std::endl;
        printf("Invalid modulation specified: %s\n",modulation.c_str());
        exit(1);
    }
    switch(afscheme){
        case (LIQUID_ANALOG_AM_CONSTANT):{
            modulation = "constant";
            break;
        }
        case (LIQUID_ANALOG_AM_SQUARE):{
            modulation = "square";
            break;
        }
        case (LIQUID_ANALOG_AM_TRIANGLE):{
            modulation = "triangle";
            break;
        }
        case (LIQUID_ANALOG_AM_SAWTOOTH):{
            modulation = "sawtooth";
            break;
        }
        case (LIQUID_ANALOG_AM_SINUSOID):{
            modulation = "sinusoid";
            break;
        }
        case (LIQUID_ANALOG_AM_WAV_FILE):{
            modulation = "wav";
            break;
        }
        case (LIQUID_ANALOG_AM_RAND_UNI):{
            modulation = "rand_uni";
            break;
        }
        case (LIQUID_ANALOG_AM_RAND_GAUSS):{
            modulation = "rand_gauss";
            break;
        }
    }

    std::cout << "result: " << modulation << std::endl;

    printf("Using:\n");
    printf("  modulation:   %s\n",modulation.c_str());
    printf("  freq:         %.3f\n",uhd_tx_freq);
    printf("  rate:         %.3f\n",uhd_tx_rate);
    printf("  bw:           %.3f\n",bw_nr);
    printf("  h:            %.3f\n",mod_index);
    printf("  T:            %.3f\n",period);
    printf("  F:            %.3f\n",src_fq);
    printf("  gain:         %.3f\n",uhd_tx_gain);
    printf("  args:         %s\n",uhd_tx_args.c_str());
    printf("  grange:       %.3f\n",grange);
    printf("  cycle:        %.3f\n",gcycle);

    if((bw_nr/uhd_tx_rate) > 0.99){
        fprintf(stderr,"error: %s, effective bandwidth (%1.3f kHz) is too high\n",argv[0],bw_nr/1e3);
        return 1;
    }

    if(dry_run == 1){
        return 0;
    }


    chrono_time[1] = get_time();

    uhd::device_addr_t args(uhd_tx_args);
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // try to configure hardware
    usrp->set_tx_rate(uhd_tx_rate);
    usrp->set_tx_freq(uhd_tx_freq);
    usrp->set_tx_gain(uhd_tx_gain);
    usrp->set_tx_bandwidth(bw_f);

    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = true;  // never SOB when continuous
    md.end_of_burst   = false;  // 
    md.has_time_spec  = true;  // set to false to send immediately

    // stream
    std::vector<size_t> channel_nums;
    channel_nums.push_back(0);
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = channel_nums;
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

    // vector buffer to send data to USRP
    double time_delay = 0.5;
    uint64_t n(8192);
    uint8_t itemsize = 1;//default is complex float
    if(modulation == message_source_type2str(WAV_FILE)){
        itemsize = wav->itemsize/sizeof(std::complex<float>);//should be 1 right now
        std::cout << "Wav itemsize: " << (int)wav->itemsize;
        std::cout << "Wav channels: " << (int)wav->wavs[0]->num_channels;
        std::cout << "Wav mode: " << (int) wav->read_as;
    }
    std::vector<std::complex<float> > buf(n*itemsize);
    std::vector<std::complex<float>*> bufs(channel_nums.size(), &buf.front());

    std::signal(SIGINT, &signal_interrupt_handler);
    std::cout << "running (hit CTRL-C to stop)" << std::endl;
    double theta  = 0.0f;    // current instantaneous phase
    double phi  = 0.0f;    // current instantaneous phase
    chrono_time[2] = get_time();
    usrp->set_time_now(uhd::time_spec_t(chrono_time[2]),uhd::usrp::multi_usrp::ALL_MBOARDS);
    md.time_spec = uhd::time_spec_t(chrono_time[2]+time_delay);

    labels* reporter;
    double delta = 0;
    if(!json.empty()){
        reporter = new labels(json.c_str(),"TXDL T","TXDL SG1","TXDL S1");
    }
    real_source wrap_source = NULL;
    void* gen_peek = NULL;
    uint64_t wav_offset;
    uint64_t counter = 0;
    if(modulation == analog_source_type2str(CONSTANT)){
        gen_peek = (void*)constant_source_create(src_fq/uhd_tx_rate,20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == analog_source_type2str(SQUARE)){
        gen_peek = (void*)square_source_create(1.0, src_fq/uhd_tx_rate,0,20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == analog_source_type2str(SAWTOOTH)){
        gen_peek = (void*)sawtooth_source_create(1.0, src_fq/uhd_tx_rate,0,20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == analog_source_type2str(TRIANGLE)){
        gen_peek = (void*)triangle_source_create(1.0, src_fq/uhd_tx_rate,0,20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == analog_source_type2str(SINUSOID)){
        gen_peek = (void*)sinusoid_source_create(1.0, src_fq/uhd_tx_rate,0,20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == message_source_type2str(WAV_FILE)){
        float ratio = randf();
        if(ratio < 0){throw std::runtime_error("randf is negative?");}
        wav_offset = (uint64_t)(ratio*(double)wav_samples);
        if(wav_offset >= wav_samples){throw std::runtime_error("WHY?");}
        // wav_offset = 214637408;
        if(wav_reader_set_offset(wav, wav_offset)){throw std::runtime_error("Something is wrong.");}
        // counter = 0;
        printf("Starting file position: %lu/%lu   %lf\n",
            wav_reader_get_offset(wav)-cbufferf_max_size(wav->buffer)/2,
            wav_samples,
            get_time());
        gen_peek = (void*)wav_source_create(MONO, wav, 20000);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,MONO);
    }
    else if(modulation == message_source_type2str(RANDOM_GAUSS)){
        gen_peek = (void*) rand_gauss_source_create(0.5,0.2,8192);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == message_source_type2str(RANDOM_UNI)){
        gen_peek = (void*) rand_uni_source_create(0.1f,0.9f,(uint32_t)8192);
        wrap_source = real_source_create(gen_peek,sizeof(float),1,NONE);
    }
    else if(modulation == digital_source_type2str(BYTES)){
        exit(1);
    }
    else if(modulation == digital_source_type2str(MASK)){
        exit(1);
    }
    else if(modulation == digital_source_type2str(MANCHESTER)){
        exit(1);
    }
    else if(modulation == digital_source_type2str(PPM)){
        exit(1);
    }
    else if(modulation == digital_source_type2str(PDM)){
        exit(1);
    }
    if(!json.empty()){
        reporter->set_modulation("am_"+modulation);
        reporter->eng_bw = bw_f;
    }

    real_path rp = real_path_create(uhd_tx_rate,uhd_tx_rate,0.001*uhd_tx_rate,
                                    .1*uhd_tx_rate,0,1.0,0.0,wrap_source);
    amgen am = amgen_create(1, mod_index, NULL, &rp);
    // fmgen fm = fmgen_create(mod_index,am);

    // // uint64_t samples; // assuming MONO for now
    // float *ptr;
    // std::vector<float> temp_buf(n,0.f);
    // ptr = temp_buf.data();
    uint64_t xfer_counter = 0;
    int checking = 0;
    while (continue_running) {

        // fill buffer
        // samples = wav_reader_data_ptr(wav, ptr);
        // samples = std::min(samples,n);

        // check source
        // real_source_nstep(wrap_source, n, ptr);
        // float min(0),max(0);
        // for(uint32_t idx = 0; idx < n; idx++){
        //     buf[idx] = std::complex<float>(ptr[idx],0.0f);
        //     if(ptr[idx] < min) min = ptr[idx];
        //     if(ptr[idx] > max) max = ptr[idx];
        // }
        // std::cout << "min: " << min << " max: " << max << std::endl;
        // checking += n;

        // check amgen
        amgen_nstep(am, n, buf.data());
        // float min(0),max(0);
        // for(uint32_t idx = 0; idx < n; idx++){
        //     buf[idx] = std::complex<float>(ptr[idx],0.0f);
        //     if(ptr[idx] < min) min = ptr[idx];
        //     if(ptr[idx] > max) max = ptr[idx];
        // }
        // std::cout << "min: " << min << " max: " << max << std::endl;
        // checking += n;

        // FM Gen
        // checking = fmgen_nstep(fm, n, buf.data());
        // float min(0), max(0),phi0,phi1;
        // for(uint32_t idx = 1; idx < n; idx++){
        //     phi0 = std::atan2(buf[idx-1].imag(),buf[idx-1].real());
        //     phi1 = std::atan2(buf[idx].imag(),buf[idx].real());
        //     phi0 = fmod(phi1-phi0,2*M_PI);
        //     if(phi0 < -M_PI) phi0 += 2*M_PI;
        //     if(phi0 > M_PI) phi0 -= 2*M_PI;
        //     if(phi0 < min) min = phi0;
        //     if(phi0 > max) max = phi0;
        // }
        // std::cout << "delta    min: " << min << " max: " << max << std::endl;

        // send the result to the USRP
        tx_stream->send(bufs, buf.size(), md);
        // wav_reader_advance(wav, n);

        counter++;
        xfer_counter+=buf.size();
        // if(real_mesg == WAV_FILE){
        //     if(counter*n*wav->itemsize >= uhd_tx_rate){
        //         printf("Current file position: %lu/%lu (%lu) %lf\n",wav_reader_get_offset(wav),wav_samples,
        //             wav_reader_samples_processed(wav),get_time());
        //         // printf("Current source position: %lu/%lu (%lu) %lf\n",wav_reader_get_offset(wav),wav_samples,
        //         //     wav_reader_samples_processed(wav),get_time());
        //         counter = 0;
        //     }
        // }
        // if(checking > 1*uhd_tx_rate) break;
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
    // while(get_time() < chrono_time[2]+0.5 + xfer_counter/uhd_tx_rate);

    // sleep for a small amount of time to allow USRP buffers to flush
    // usleep(100000);
    uint64_t samples = counter*buf.size();
    while(get_time() < chrono_time[2]+time_delay + samples/uhd_tx_rate){}
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
            double(xfer_counter)/uhd_tx_rate,
            uhd_tx_freq,
            uhd_tx_rate*bw_nr,
            meta);
        reporter->activity_type="lowprob_anomaly";
        reporter->protocol="unknown";
        // reporter->set_modulation(modulation);
        reporter->set_modulation("am_"+modulation);
        reporter->modulation_src = modulation;
        reporter->modality="single_carrier";
        reporter->device_origin=uhd_tx_args;

        reporter->finalize();
        delete reporter;
    }

    return 0;
}




void gen_sin(std::complex<float> *buf, size_t n, double fs, double period, double mod_index, double &phi, double &theta){
    for(size_t idx = 0; idx < n; idx++){
        buf[idx] = std::polar(1.0,theta);
        theta += (2*M_PI*mod_index*sinf(phi));
        phi += (2*M_PI/(period*fs));
    }
    phi = fmod(phi, 2*M_PI);
    phi = phi < -M_PI ? phi+2*M_PI : phi;
    theta = fmod(theta, 2*M_PI);
    theta = theta < -M_PI ? theta+2*M_PI : theta;
    ////// f_max is then 1/period * fs -----> digital fm = f_max/fs = 1/period
    ////// f_delta is then mod_index*f_max ---> digital fd = f_delta/fs = mod_index/period
    /// so f_delta = mod_index/period*fs

    // so doubling the sample rate (fs) should be met with half the value of fm and thus shift that to mod_index
    // and so doubling fs, half mod_index
}