
#include <iostream>
#include "noisemodem.hh"


#define liquid_error_config(format, ...) \
    liquid_error_config_nl(__FILE__, __LINE__, format, ##__VA_ARGS__);

// noisemod
struct noisemod_s {
    // common
    unsigned int m;             // bits per symbol
    unsigned int k;             // samples per symbol
    float        bandwidth;     // filter bandwidth parameter

    unsigned int M;             // constellation size
    float        M2;            // (M-1)/2
    nco_crcf     oscillator;    // nco
};

// create noisemod object (frequency modulator)
//  _m          :   bits per symbol, _m > 0
//  _k          :   samples/symbol, _k >= 2^_m
//  _bandwidth  :   total signal bandwidth, (0,0.5)
noisemod noisemod_create(
                     unsigned int _k,
                     float        _bandwidth)
{
    // validate input
    if (_k < 2 || _k > 2048)
        return (noisemod)liquid_error_config("noisemod_create(), samples/symbol must be in [2^_m, 2048]");
    if (_bandwidth <= 0.0f || _bandwidth >= 1.0f)
        return (noisemod)liquid_error_config("noisemod_create(), bandwidth must be in (0,1.0)");

    // create main object memory
    noisemod q = (noisemod) malloc(sizeof(struct noisemod_s));

    // set basic internal properties
    q->k         = _k;              // samples per symbol
    q->bandwidth = _bandwidth;      // signal bandwidth

    q->oscillator = nco_crcf_create(LIQUID_VCO);

    // reset modem object
    noisemod_reset(q);

    // return main object
    return q;
}

// copy object
noisemod noisemod_copy(noisemod q_orig)
{
    // validate input
    if (q_orig == NULL)
        return (noisemod)liquid_error_config("noisemod_copy(), object cannot be NULL");

    // create object and copy base parameters
    noisemod q_copy = (noisemod) malloc(sizeof(struct noisemod_s));
    memmove(q_copy, q_orig, sizeof(struct noisemod_s));

    // copy oscillator object
    q_copy->oscillator = nco_crcf_copy(q_orig->oscillator);

    // return new object
    return q_copy;
}

// destroy noisemod object
int noisemod_destroy(noisemod _q)
{
    // destroy oscillator object
    nco_crcf_destroy(_q->oscillator);

    // free main object memory
    free(_q);
    return LIQUID_OK;
}

// print noisemod object internals
int noisemod_print(noisemod _q)
{
    printf("noisemod : noise modulator\n");
    printf("    samples/symbol  :   %u\n", _q->k);
    printf("    bandwidth       :   %8.5f\n", _q->bandwidth);
    return LIQUID_OK;
}

// reset state
int noisemod_reset(noisemod _q)
{
    // reset internal objects
    nco_crcf_reset(_q->oscillator);
    return LIQUID_OK;
}

// modulate sample
//  _q      :   frequency modulator object
//  _y      :   output sample
int noisemod_modulate(noisemod          _q,
                    liquid_float_complex * _y)
{
    // cawgn(_y, 1.0);
    crandnf(_y);
    *_y *= .707106781186547f;
    return LIQUID_OK;
}


// create symstream object with noise modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _k              : samples per symbol, _k >= 2^m, _k even
//  _m              : filter delay (symbols)
//  _bandwidth      : total signal bandwidth, (0,1)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ms             : noise scheme (e.g. LIQUID_NOISE_AWGN)
symstreamrncf symstreamrncf_create(void)
{
    return symstreamrncf_create_noise(LIQUID_FIRFILT_ARKAISER,
                                    0.5f,
                                    7,
                                    0.3,
                                    LIQUID_NOISE_AWGN);
}


// create symstream object with noise modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _bandwidth      : total signal bandwidth, (0,0.5)
//  _m              : filter delay (symbols)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ms             : noise scheme (e.g. LIQUID_NOISE_AWGN)
symstreamrncf symstreamrncf_create_noise(int          _ftype,
                                        float        _bandwidth,
                                        unsigned int _m,
                                        float        _beta,
                                        int          _ms)
{
    float bw_min = 0.001f;
    float bw_max = 0.999f;
    // validate input
    if (_bandwidth < bw_min || _bandwidth > bw_max)
        return (symstreamrncf)liquid_error_config("symstreamrncf_create(), symbol bandwidth (%g) must be in [%g,%g]", _bandwidth, bw_min, bw_max);
    if (_m == 0)
        return (symstreamrncf)liquid_error_config("symstream%s_create(), filter delay must be greater than zero", "rncf");
    if (_beta <= 0.0f || _beta > 1.0f)
        return (symstreamrncf)liquid_error_config("symstream%s_create(), filter excess bandwidth must be in (0,1]", "rncf");
    if (_ms == LIQUID_NOISE_UNKNOWN || _ms >= 2)
        return (symstreamrncf)liquid_error_config("symstream%s_create(), invalid modulation scheme", "rncf");

    // allocate memory for main object
    symstreamrncf q = (symstreamrncf) malloc( sizeof(struct symstreamrncf_noise_s) );


    ////// pseudo symstream

    // set input parameters
    q->filter_type = _ftype;
    q->m           = _m;
    q->beta        = _beta;
    q->bw          = _bandwidth;
    q->mod_scheme  = _ms;
    q->gain        = 1.0f;
    q->k           = 2;

    // modulator
    q->mod = noisemod_create(2, _bandwidth);

    // interpolator
    // printf("Symb Interp: type(%u) k(%u) m(%u) beta(%f) 0\n",q->filter_type,q->k,q->m,q->beta);
    q->interp = firinterp_crcf_create_prototype(q->filter_type, q->k, q->m, q->beta, 0);
    // q->interp = NULL;

    // sample buffer
    q->buf_internal = (liquid_float_complex*) malloc(q->k*sizeof(liquid_float_complex));


    q->rate = 0.5f / _bandwidth;
    // printf("ms(%u) bps(%u) k(%u) rate(%f) bw(%f)\n",_ms,_bps,_k,rate,_bw_guess*2.0f);

    // printf("Samp Interp: rate(%f) 60.0\n",rate);
    q->arb_interp = msresamp_crcf_create(q->rate, 60.0f);

    // printf("Internal(Symbol) buffer length = %u\n",2);
    unsigned int buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(q->rate));
    // printf("External(Sample) buffer length = %u\n",buf_len);
    q->buf = (liquid_float_complex*) malloc(buf_len*sizeof(liquid_float_complex));

    // reset and return main object
    symstreamrncf_reset(q);
    return q;
}

int symstreamrncf_destroy(symstreamrncf _q)
{
    //symstream
    if(_q->mod != NULL) noisemod_destroy(_q->mod);
    if(_q->interp != NULL) firinterp_crcf_destroy(_q->interp);
    free(_q->buf_internal);
    //symstreamr
    if(_q->arb_interp != NULL) msresamp_crcf_destroy(_q->arb_interp);
    free(_q->buf);
    free(_q);
    return LIQUID_OK;
}

int symstreamrncf_print(symstreamrncf _q)
{
    printf("symstream_rncf:\n");
    return LIQUID_OK;
}

int symstreamrncf_reset(symstreamrncf _q)
{
    //symstream
    noisemod_reset(_q->mod);
    if(_q->interp != NULL) firinterp_crcf_reset(_q->interp);
    _q->buf_internal_index = 0;
    //symstreamr
    msresamp_crcf_reset(_q->arb_interp);
    _q->buf_index = 0;
    _q->buf_size = 0;
    return LIQUID_OK;
}

int symstreamrncf_get_ftype(symstreamrncf _q)
{
    return _q->filter_type;
}

unsigned int symstreamrncf_get_k(symstreamrncf _q)
{
    return _q->k;
}

float symstreamrncf_get_bw(symstreamrncf _q)
{
    return _q->bw;
}

unsigned int symstreamrncf_get_m(symstreamrncf _q)
{
    return _q->m;
}
float symstreamrncf_get_beta(symstreamrncf _q){return _q->beta;}
int symstreamrncf_get_scheme(symstreamrncf _q){return _q->mod_scheme;}
int symstreamrncf_set_gain(symstreamrncf _q,float _gain){ _q->gain = _gain; return LIQUID_OK;}
float symstreamrncf_get_gain(symstreamrncf _q){return _q->gain;}
float symstreamrncf_get_delay(symstreamrncf _q){
    float p = _q->k;
    float d = msresamp_crcf_get_delay(_q->arb_interp);
    float r = msresamp_crcf_get_rate(_q->arb_interp);
    return (p+d)*r;
}
int symstreamrncf_set_scheme(symstreamrncf _q, int _ms){
    _q->mod = noisemod_create(_q->m, _q->beta);
    return LIQUID_OK;
}

unsigned int noisemod_modulate_rand_sym(symstreamrncf _q, liquid_float_complex *_y){
    // void cawgn(liquid_float_complex *_x, float _nstd);
    noisemod_modulate(_q->mod,_y);
    return 0;
}

int symstreamncf_fill_buffer(symstreamrncf _q){
    liquid_float_complex v;
    noisemod_modulate_rand_sym(_q, &v);
    v *= _q->gain;
    firinterp_crcf_execute(_q->interp, v, _q->buf_internal);
    return LIQUID_OK;
}
int symstreamncf_write_samples(symstreamrncf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    for (i=0; i<_buf_len; i++){
        if(_q->buf_internal_index == 0){
            if(symstreamncf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamncf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf_internal[_q->buf_internal_index];
        _q->buf_internal_index = (_q->buf_internal_index + 1) % (_q->k);
    }
    return LIQUID_OK;
}
int symstreamrncf_fill_buffer(symstreamrncf _q){
    if (_q->buf_index != _q->buf_size){
        return fprintf(stderr,"symstreamrncf_write_samples(), buffer not empty\n");
    }
    _q->buf_size = 0;
    _q->buf_index = 0;

    while(!_q->buf_size){
        liquid_float_complex sample;
        symstreamncf_write_samples(_q, &sample, 1);

        msresamp_crcf_execute(_q->arb_interp, &sample, 1, _q->buf, &_q->buf_size);
    }

    return LIQUID_OK;
}
int symstreamrncf_write_samples(symstreamrncf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    if(_q->buf_index > _q->buf_size){
        return fprintf(stderr,"symstreamrncf_write_samples(), called in a bad state\n");
    }
    for (i=0; i<_buf_len; i++){
        if(_q->buf_index == _q->buf_size){
            if(symstreamrncf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamrncf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf[_q->buf_index];
        _q->buf_index++;
    }
    return LIQUID_OK;
}
