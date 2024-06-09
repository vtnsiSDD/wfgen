
#ifdef __cplusplus
#include <iostream>
#endif
#include "afmodem.hh"

const struct analog_type_s analog_types[ANALOG_TYPE_COUNT] = {
    // name      fullname                         scheme          bps

    // unknown
    {"unknown",         "unknown_analog",         LIQUID_ANALOG_UNKNOWN,        0},

    // ANALOG
    {"am_constant",     "analog_am_constant",     LIQUID_ANALOG_AM_CONSTANT,    1},
    {"am_square",       "analog_am_square",       LIQUID_ANALOG_AM_SQUARE,      1},
    {"am_triangle",     "analog_am_triangle",     LIQUID_ANALOG_AM_TRIANGLE,    1},
    {"am_sawtooth",     "analog_am_sawtooth",     LIQUID_ANALOG_AM_SAWTOOTH,    1},
    {"am_sinusoid",     "analog_am_sinusoid",     LIQUID_ANALOG_AM_SINUSOID,    1},
    {"am_wav_file",     "analog_am_wav_file",     LIQUID_ANALOG_AM_WAV_FILE,    1},
    {"am_rand_uni",     "analog_am_rand_uni",     LIQUID_ANALOG_AM_RAND_UNI,    1},
    {"am_rand_gauss",   "analog_am_rand_gauss",   LIQUID_ANALOG_AM_RAND_GAUSS,  1},
    {"am_ppm",          "analog_am_ppm",          LIQUID_ANALOG_AM_PPM,         1},
    {"am_pwm",          "analog_am_pwm",          LIQUID_ANALOG_AM_PWM,         1},
    {"fm_constant",     "analog_fm_constant",     LIQUID_ANALOG_FM_CONSTANT,    1},
    {"fm_square",       "analog_fm_square",       LIQUID_ANALOG_FM_SQUARE,      1},
    {"fm_triangle",     "analog_fm_triangle",     LIQUID_ANALOG_FM_TRIANGLE,    1},
    {"fm_sawtooth",     "analog_fm_sawtooth",     LIQUID_ANALOG_FM_SAWTOOTH,    1},
    {"fm_sinusoid",     "analog_fm_sinusoid",     LIQUID_ANALOG_FM_SINUSOID,    1},
    {"fm_wav_file",     "analog_fm_wav_file",     LIQUID_ANALOG_FM_WAV_FILE,    1},
    {"fm_rand_uni",     "analog_fm_rand_uni",     LIQUID_ANALOG_FM_RAND_UNI,    1},
    {"fm_rand_gauss",   "analog_fm_rand_gauss",   LIQUID_ANALOG_FM_RAND_GAUSS,  1},
    {"fm_chirp",        "analog_fm_chirp",        LIQUID_ANALOG_FM_CHIRP,       1},
    {"fm_chirp_nonlin", "analog_fm_chirp_nonlin", LIQUID_ANALOG_FM_CHIRP_NONLIN,1},
};

#define liquid_error_config(format, ...) \
    liquid_error_config_al(__FILE__, __LINE__, format, ##__VA_ARGS__);

// create symstream object with analog modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _k              : samples per symbol, _k >= 2^m, _k even
//  _m              : filter delay (symbols)
//  _bandwidth      : total signal bandwidth, (0,1)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ms             : analog scheme (e.g. LIQUID_ANALOG_FM_SINUSOID)
symstreamracf symstreamracf_create(void)
{
    return symstreamracf_create_analog(LIQUID_FIRFILT_ARKAISER,
                                    0.5f,
                                    7,
                                    0.3,
                                    0.25,
                                    0.2,
                                    LIQUID_ANALOG_FM_SINUSOID);
}


// create symstream object with analog modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _bandwidth      : total signal bandwidth, (0,0.5)
//  _m              : filter delay (symbols)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ms             : analog scheme (e.g. LIQUID_ANALOG_FM_SINUSOID)
symstreamracf symstreamracf_create_analog(int          _ftype,
                                        float        _bandwidth,
                                        unsigned int _m,
                                        float        _beta,
                                        float        _mod_idx,
                                        float        _src_freq,
                                        int          _ms)
{
    float bw_min = 0.001f;
    float bw_max = 0.999f;
    // validate input
    if (_bandwidth < bw_min || _bandwidth > bw_max)
        return (symstreamracf)liquid_error_config("symstreamracf_create(), symbol bandwidth (%g) must be in [%g,%g]", _bandwidth, bw_min, bw_max);
    if (_m == 0)
        return (symstreamracf)liquid_error_config("symstream%s_create(), filter delay must be greater than zero", "racf");
    if (_beta <= 0.0f || _beta > 1.0f)
        return (symstreamracf)liquid_error_config("symstream%s_create(), filter excess bandwidth must be in (0,1]", "racf");
    if (_ms == LIQUID_ANALOG_UNKNOWN || _ms >= 17)
        return (symstreamracf)liquid_error_config("symstream%s_create(), invalid modulation scheme", "racf");
    if (_mod_idx <= 0.0){
        if(_ms > 9) _mod_idx = 0.25; // fm
        else _mod_idx = 0.75; // am
    }
    if (_src_freq <= 0.0){
        if(_ms > 9) _src_freq = 0.001;
        else _src_freq = 0.2; // am
    }

    // allocate memory for main object
    symstreamracf q = (symstreamracf) malloc( sizeof(struct symstreamracf_analog_s) );


    ////// pseudo symstream

    // set input parameters
    q->filter_type = _ftype;
    q->m           = _m;
    q->beta        = _beta;
    q->bw          = _bandwidth;
    q->mod_idx     = _mod_idx;
    q->src_freq    = _src_freq;
    q->mod_scheme  = _ms;
    q->gain        = 1.0f;
    q->k           = 2;//spas

    symstreamracf_set_scheme(q, _ms);

    // interpolator
    // printf("Symb Interp: type(%u) k(%u) m(%u) beta(%f) 0\n",q->filter_type,q->k,q->m,q->beta);
    q->interp = firinterp_crcf_create_prototype(q->filter_type, q->k, q->m, q->beta, 0);
    // q->interp = NULL;

    // sample buffer
    q->buf_internal = (liquid_float_complex*) malloc(q->k*sizeof(liquid_float_complex));


    q->rate = 0.5f / _bandwidth; // interp by 2 / target_bandwidth
    // printf("ms(%u) bps(%u) k(%u) rate(%f) bw(%f)\n",_ms,_bps,_k,rate,_bw_guess*2.0f);

    // printf("Samp Interp: rate(%f) 60.0\n",rate);
    q->arb_interp = msresamp_crcf_create(q->rate, 60.0f);

    // printf("Internal(Symbol) buffer length = %u\n",2);
    unsigned int buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(q->rate));
    // printf("External(Sample) buffer length = %u\n",buf_len);
    q->buf = (liquid_float_complex*) malloc(buf_len*sizeof(liquid_float_complex));

    // reset and return main object
    symstreamracf_reset(q);
    return q;
}

int symstreamracf_destroy(symstreamracf _q)
{
    //symstream
    if(_q->mod_f != NULL) fmgen_destroy(&_q->mod_f);
    else amgen_destroy(&_q->mod_a);
    if(_q->interp != NULL) firinterp_crcf_destroy(_q->interp);
    free(_q->buf_internal);
    //symstreamr
    if(_q->arb_interp != NULL) msresamp_crcf_destroy(_q->arb_interp);
    free(_q->buf);
    free(_q);
    return LIQUID_OK;
}

int symstreamracf_print(symstreamracf _q)
{
    printf("symstream_racf:\n");
    return LIQUID_OK;
}

int symstreamracf_reset(symstreamracf _q)
{
    //symstream
    // fmgen_reset(_q->mod_f);
    // amgen_reset(_q->mod_a);
    if(_q->interp != NULL) firinterp_crcf_reset(_q->interp);
    _q->buf_internal_index = 0;
    //symstreamr
    msresamp_crcf_reset(_q->arb_interp);
    _q->buf_index = 0;
    _q->buf_size = 0;
    return LIQUID_OK;
}

int symstreamracf_get_ftype(symstreamracf _q)
{
    return _q->filter_type;
}

unsigned int symstreamracf_get_k(symstreamracf _q)
{
    return _q->k;
}

float symstreamracf_get_bw(symstreamracf _q)
{
    return _q->bw;
}

unsigned int symstreamracf_get_m(symstreamracf _q)
{
    return _q->m;
}
float symstreamracf_get_beta(symstreamracf _q){return _q->beta;}
int symstreamracf_get_scheme(symstreamracf _q){return _q->mod_scheme;}
int symstreamracf_set_gain(symstreamracf _q,float _gain){ _q->gain = _gain; return LIQUID_OK;}
float symstreamracf_get_gain(symstreamracf _q){return _q->gain;}
float symstreamracf_get_delay(symstreamracf _q){
    float p = _q->k;
    float d = msresamp_crcf_get_delay(_q->arb_interp);
    float r = msresamp_crcf_get_rate(_q->arb_interp);
    return (p+d)*r;
}
int symstreamracf_set_scheme(symstreamracf _q, int _ms){
    if(_q->mod_f != NULL) fmgen_destroy(&_q->mod_f);
    else    amgen_destroy(&_q->mod_a);
    wav_mode_t special = NONE;
    // source setup
    switch(_ms){
        case 1: case 9:{
            _q->true_source = (void*) constant_source_create(_q->src_freq,1024);
            break;
        }
        case 2: case 10:{
            _q->true_source = (void*) square_source_create(1.0,_q->src_freq,0,1024);
            break;
        }
        case 3: case 11:{
            _q->true_source = (void*) triangle_source_create(1.0,_q->src_freq,0,1024);
            break;
        }
        case 4: case 12:{
            _q->true_source = (void*) sawtooth_source_create(1.0,_q->src_freq,0,1024);
            break;
        }
        case 5: case 13:{
            _q->true_source = (void*) sinusoid_source_create(1.0,_q->src_freq,0,1024);
            break;
        }
        case 6: case 14:{
            #ifdef __cplusplus
            _q->true_source = (void*) wav_source_create();
            #else
            _q->true_source = (void*) wav_source_create_default();
            #endif
            special = MONO;
            break;
        }
        case 7: case 15:{
            #ifdef __cplusplus
            _q->true_source = (void*) rand_uni_source_create();
            #else
            _q->true_source = (void*) rand_uni_source_create_default();
            #endif
            break;
        }
        case 8: case 16:{
            #ifdef __cplusplus
            _q->true_source = (void*) rand_gauss_source_create();
            #else
            _q->true_source = (void*) rand_gauss_source_create_default();
            #endif
            break;
        }
        default: break;
    }
    #ifdef __cplusplus
    _q->wrap_source = real_source_create(_q->true_source, sizeof(float), 1, special);
    #else
    _q->wrap_source = real_source_create_from_source(_q->true_source, sizeof(float), 1, special);
    #endif
    _q->path = real_path_create(1.0,1.0,1.0,0.5,0.0,1.0,0.0,_q->wrap_source,NULL,NULL);
    // modulator
    if(_ms < 9){
        _q->mod_a = amgen_create(1, _q->mod_idx, NULL, &_q->path);
        _q->mod_f = NULL;
    }
    else{
        _q->mod_a = amgen_create(1, 1.0, NULL, &_q->path);
        _q->mod_f = fmgen_create(_q->mod_idx, _q->mod_a, NULL);
    }

    return LIQUID_OK;
}

/////////// 'internal buffer'
int symstreamacf_fill_buffer(symstreamracf _q){
    liquid_float_complex v;
    float f;
    if(_q->mod_f != NULL) fmgen_step(_q->mod_f, &v);
    else{
        amgen_step(_q->mod_a, &f);
        #ifdef __cplusplus
        v = liquid_float_complex(f,0.f);
        #else
        v = f + 0.f*I;
        #endif
    }
    v *= _q->gain;
    firinterp_crcf_execute(_q->interp, v, _q->buf_internal);
    return LIQUID_OK;
}
int symstreamacf_write_samples(symstreamracf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    for (i=0; i<_buf_len; i++){
        if(_q->buf_internal_index == 0){
            if(symstreamacf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamacf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf_internal[_q->buf_internal_index];
        _q->buf_internal_index = (_q->buf_internal_index + 1) % (_q->k);
    }
    return LIQUID_OK;
}
///////////
int symstreamracf_fill_buffer(symstreamracf _q){
    if (_q->buf_index != _q->buf_size){
        return fprintf(stderr,"symstreamracf_write_samples(), buffer not empty\n");
    }
    _q->buf_size = 0;
    _q->buf_index = 0;

    while(!_q->buf_size){
        liquid_float_complex sample;
        symstreamacf_write_samples(_q, &sample, 1);

        msresamp_crcf_execute(_q->arb_interp, &sample, 1, _q->buf, &_q->buf_size);
    }

    return LIQUID_OK;
}
int symstreamracf_write_samples(symstreamracf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    if(_q->buf_index > _q->buf_size){
        return fprintf(stderr,"symstreamracf_write_samples(), called in a bad state\n");
    }
    for (i=0; i<_buf_len; i++){
        if(_q->buf_index == _q->buf_size){
            if(symstreamracf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamracf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf[_q->buf_index];
        _q->buf_index++;
    }
    return LIQUID_OK;
}
