
#include <iostream>
#include "fskmodems.hh"



#define liquid_error_config(format, ...) \
    liquid_error_config_fl(__FILE__, __LINE__, format, ##__VA_ARGS__);

// create symstream object with fsk modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _k              : samples per symbol, _k >= 2^m, _k even
//  _h              : modulation index, _h > 0
//  _m              : filter delay (symbols)
//  _bandwidth      : total signal bandwidth, (0,1)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ftype          : filter type (e.g. LIQUID_CPFSK_SQUARE)
//  _ms             : fsk scheme (e.g. LIQUID_MODEM_FSK4)
symstreamrfcf symstreamrfcf_create(void)
{
    return symstreamrfcf_create_fsk(LIQUID_FIRFILT_ARKAISER,
                                    8,
                                    1.0f,
                                    12,
                                    0.5f,
                                    0.3,
                                    LIQUID_CPFSK_SQUARE,
                                    LIQUID_MODEM_FSK4);
}


// create symstream object with fsk modulation
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _h              : modulation index, _h > 0
//----  _k              : samples per symbol, _k >= 2^m, _k even
//  _m              : filter delay (symbols)
//  _bandwidth      : total signal bandwidth, (0,1.0)
//  _beta           : filter bandwidth parameter, (0,1]
//  _cpftype        : cpfsk filter type (e.g. LIQUID_CPFSK_SQUARE)
//  _ms             : fsk scheme (e.g. LIQUID_MODEM_FSK4)
symstreamrfcf symstreamrfcf_create_fsk( int          _ftype,
                                        unsigned int _k,
                                        float        _h,
                                        unsigned int _m,
                                        float        _bandwidth,
                                        float        _beta,
                                        int          _cpftype,
                                        int          _ms)
{
    float bw_min = 0.001f;
    float bw_max = 0.999f;
    unsigned int _bps = ((((unsigned int)_ms)-1)%8+1);
    // validate input
    if (_bandwidth < bw_min || _bandwidth > bw_max)
        return (symstreamrfcf)liquid_error_config("symstreamrfcf_create(), symbol bandwidth (%g) must be in [%g,%g]", _bandwidth, bw_min, bw_max);
    if (_bps < 1)
        return (symstreamrfcf)liquid_error_config("symstream%s_create(), bits per symbol must be at least 1", "rfcf");
    if (_h <= 0.0f)
        return (symstreamrfcf)liquid_error_config("symstream%s_create(), modulation index must be greater than zero", "rfcf");
    if (_m == 0)
        return (symstreamrfcf)liquid_error_config("symstream%s_create(), filter delay must be greater than zero", "rfcf");
    if (_beta <= 0.0f || _beta > 1.0f)
        return (symstreamrfcf)liquid_error_config("symstream%s_create(), filter excess bandwidth must be in (0,1]", "rfcf");
    if (_ms == LIQUID_FSK_UNKNOWN || _ms >= 33)
        return (symstreamrfcf)liquid_error_config("symstream%s_create(), invalid modulation scheme", "rfcf");

    // allocate memory for main object
    symstreamrfcf q = (symstreamrfcf) malloc( sizeof(struct symstreamrfcf_fsk_s) );

    // unsigned int M = 1<<(((_ms-1)%8)+1);

    if(_ms >= 25) _cpftype = LIQUID_CPFSK_GMSK;
    else if(_ms >= 17) _cpftype = LIQUID_CPFSK_SQUARE;
    // set input parameters
    q->bps         = _bps;
    q->M           = 1<<_bps;
    // mod_idx = freq_dist / symbol_rate
    q->h           = _h;
    // k = samples / symbol ---> 1/k = symbol/sample
    q->k           = _k;
    q->m           = _m;
    q->beta        = _beta;
    // bandwidth = (2*symbol_rate + (M-1)*freq_dist)/sample_rate
    // bandwidth = (2*symbol_rate + (M-1)*mod_idx*symbol_rate)/sample_rate
    // bandwidth = ((2 + (M-1)*mod_idx)*symbol_rate)/sample_rate
    // bandwidth = (2 + (M-1)*mod_idx)*(symbol_rate/sample_rate)
    // bandwidth = (2 + (M-1)*mod_idx)/k
    // bandwidth = (2*symbol_rate + (M-1)*mod_idx*symbol_rate)/sample_rate
    // symbol_rate = (bandwidth*sample_rate)/(2 + (M-1)*mod_idx)
    // fskmod bw = (bandwidth*sample_rate - 2*symbol_rate)/sample_rate/2 < 0.5
    // fskmod bw = (bandwidth - 2*symbol_rate/sample_rate)/2 < 0.5
    // fskmod bw = (bandwidth - 2*symbol_rate/sample_rate)/2 < 0.5
    // fskmod bw = bandwidth/2 - symbol_rate/sample_rate < 0.5
    // fskmod bw = bandwidth/2 - 1/k < 0.5
    // fskmod bw = 2*symbol_rate/sample_rate/2 + (M-1)*mod_idx*symbol_rate/sample_rate/2 - 1/k < 0.5
    // fskmod bw = symbol_rate/sample_rate + (M-1)/2*mod_idx*symbol_rate/sample_rate - 1/k < 0.5
    // fskmod bw = 1/k + (M-1)/2*mod_idx*1/k - 1/k < 0.5
    // fskmod bw = (M-1)*mod_idx/(2*k) < 0.5
    // assume mod_idx = 1
    // fskmod bw = (M-1)/(2*k) < 0.5
    q->bw          = _bandwidth; // = 2*symbol_rate + (M-1)*freq_dist
    q->cpfsk_type  = _cpftype;
    q->filter_type = _ftype;
    q->mod_scheme  = _ms;
    q->gain        = 1.0f;

    float rate;
    float _bw_guess;
    // modulator
    if(_ms == 25){
        // printf("gmskmod(%u,%u,%f)\n",_k,_m,_beta);
        q->mod_g = gmskmod_create(_k, _m, _beta);
        q->mod_c = NULL;
        q->mod_f = NULL;
        _bw_guess = 1.5/(float)_k;
        rate = _bandwidth / _bw_guess;
    }
    else if(_ms > 8){
        // printf("cpfskmod(%u,%f,%u,%u,%f,%u)\n",_bps,_h,_k,_m,_beta,_cpftype);
        q->mod_g = NULL;
        q->mod_c = cpfskmod_create(_bps, _h, _k, _m, _beta, _cpftype);
        q->mod_f = NULL;
        _bw_guess = (2.0f + (q->M-1)*_h)/(float)_k;
        rate = _bandwidth / _bw_guess;
    }
    else{
        // printf("fskmod(%u,%u,0.4)\n", _bps, _k);
        q->mod_g = NULL;
        q->mod_c = NULL;
        //////// fskmod works differently than cpfsk
        //// need to adjust bw to be -2symbol_rate/tx_rate to have a mod_idx of 1
        //// - 2 (symbols/sec) / (samples/sec) = -2(symbols per sample)
        _bw_guess = (q->M - 1.0f)/(float)_k;

        q->mod_f = fskmod_create(_bps, _k, _bw_guess/2);
        // rate = _bandwidth / ((_bw_guess+1.0f/(float)_k)*2);
        // rate = _bandwidth / (_bw_guess*2);
        _bw_guess = (1.0f + q->M)/(float)_k; /// but here we want the full bw again.
        rate = _bandwidth/_bw_guess;
    }
    q->rate = 2.0f * rate;
    printf("ms(%u) bps(%u) k(%u) rate(%f) bw(%f)\n",_ms,_bps,_k,rate,_bw_guess*2.0f);

    // interpolator
    unsigned int int_buf_len;
    float eps = nextafter(0.0f,1.0f);
    if((float)fabs((double)(rate - 1.0f)) < eps*10){
        printf("Symb Interp: NULL k(%u)\n",q->k);
        q->interp = NULL;
        q->rate = 1.0f;
        int_buf_len = _k;
    }
    else{
        printf("Symb Interp (2x): type(%u) k(%u) m(%u) beta(%f) 0\n",q->filter_type,2*q->k,q->m,q->beta);
        q->interp = firinterp_crcf_create_prototype(q->filter_type, 2, q->m, 0.25f, 0);
        int_buf_len = 2*_k;
    }
    printf("Samp Interp: rate(%f) 60.0\n",1/q->rate);
    q->arb_interp = msresamp_crcf_create(1/q->rate, 60.0f);

    // sample buffer
    q->buf_internal = (liquid_float_complex*) malloc(int_buf_len*sizeof(liquid_float_complex));
    // printf("Internal(Symbol) buffer length = %u\n",2);
    unsigned int buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(rate));
    // printf("External(Sample) buffer length = %u\n",buf_len);
    q->buf = (liquid_float_complex*) malloc(buf_len*sizeof(liquid_float_complex));

    // reset and return main object
    symstreamrfcf_reset(q);
    return q;
}

int symstreamrfcf_destroy(symstreamrfcf _q)
{
    if(_q->mod_g != NULL) gmskmod_destroy(_q->mod_g);
    if(_q->mod_c != NULL) cpfskmod_destroy(_q->mod_c);
    if(_q->mod_f != NULL) fskmod_destroy(_q->mod_f);
    if(_q->interp != NULL) firinterp_crcf_destroy(_q->interp);
    if(_q->arb_interp != NULL) msresamp_crcf_destroy(_q->arb_interp);
    free(_q->buf_internal);
    free(_q->buf);
    free(_q);
    return LIQUID_OK;
}

int symstreamrfcf_print(symstreamrfcf _q)
{
    printf("symstream_rfcf:\n");
    return LIQUID_OK;
}

int symstreamrfcf_reset(symstreamrfcf _q)
{
    if(_q->mod_g != NULL) gmskmod_reset(_q->mod_g);
    if(_q->mod_c != NULL) cpfskmod_reset(_q->mod_c);
    if(_q->mod_f != NULL) fskmod_reset(_q->mod_f);
    if(_q->interp != NULL) firinterp_crcf_reset(_q->interp);
    msresamp_crcf_reset(_q->arb_interp);
    _q->buf_internal_index = 0;
    _q->buf_index = 0;
    _q->buf_size = 0;
    return LIQUID_OK;
}

int symstreamrfcf_get_ftype(symstreamrfcf _q)
{
    return _q->filter_type;
}

unsigned int symstreamrfcf_get_k(symstreamrfcf _q)
{
    return _q->k;
}

float symstreamrfcf_get_bw(symstreamrfcf _q)
{
    return _q->bw;
}

unsigned int symstreamrfcf_get_m(symstreamrfcf _q)
{
    return _q->m;
}
float symstreamrfcf_get_beta(symstreamrfcf _q){return _q->beta;}
int symstreamrfcf_get_scheme(symstreamrfcf _q){return _q->mod_scheme;}
int symstreamrfcf_set_gain(symstreamrfcf _q,float _gain){ _q->gain = _gain; return LIQUID_OK;}
float symstreamrfcf_get_gain(symstreamrfcf _q){return _q->gain;}
float symstreamrfcf_get_delay(symstreamrfcf _q){
    float p = _q->k * _q->m;
    float d = msresamp_crcf_get_delay(_q->arb_interp);
    float r = msresamp_crcf_get_rate(_q->arb_interp);
    return (p+d)*r;
}
int symstreamrfcf_set_scheme(symstreamrfcf _q, int _ms){
    if(_q->mod_scheme == 25){
        gmskmod_destroy(_q->mod_g);
        _q->mod_g = NULL;
    }
    else if((_q->mod_scheme > 8 && _q->mod_scheme < 17) || (_q->mod_scheme > 24)){
        cpfskmod_destroy(_q->mod_c);
        _q->mod_c = NULL;
    }
    else{
        fskmod_destroy(_q->mod_f);
        _q->mod_f = NULL;
    }
    if(_ms == 25){
        _q->mod_g = gmskmod_create(_q->k, _q->m, _q->beta);
    }
    else if(_ms > 8){
        _q->mod_c = cpfskmod_create(_q->bps, _q->h, _q->k, _q->m, _q->beta, _q->cpfsk_type);
    }
    else{
        _q->mod_f = fskmod_create(_q->bps, _q->k, _q->bw);
    }
    return LIQUID_OK;
}

unsigned int gen_rand_sym(unsigned int M){
    return rand() % M;
}

unsigned int gmskmod_modulate_rand_sym(symstreamrfcf _q, liquid_float_complex *_y){
    unsigned int s = gen_rand_sym(_q->M);
    gmskmod_modulate(_q->mod_g, s, _y);
    return s;
}
unsigned int cpfskmod_modulate_rand_sym(symstreamrfcf _q, liquid_float_complex *_y){
    unsigned int s = gen_rand_sym(_q->M);
    cpfskmod_modulate(_q->mod_c, s, _y);
    return s;
}
unsigned int fskmod_modulate_rand_sym(symstreamrfcf _q, liquid_float_complex *_y){
    unsigned int s = gen_rand_sym(_q->M);
    fskmod_modulate(_q->mod_f, s, _y);
    return s;
}

int symstreamfcf_fill_buffer(symstreamrfcf _q){
    liquid_float_complex v[_q->k];
    if(_q->mod_g != NULL) gmskmod_modulate_rand_sym(_q, v);
    else if(_q->mod_c != NULL) cpfskmod_modulate_rand_sym(_q, v);
    else fskmod_modulate_rand_sym(_q, v);
    // v *= _q->gain;
    for(unsigned int i = 0; i < _q->k; i++){
        v[i] *= _q->gain;
        if(_q->interp == NULL){
            _q->buf_internal[i] = v[i];
        }
        else{
            firinterp_crcf_execute(_q->interp, v[i], &_q->buf_internal[2*i]);
        }
    }
    return LIQUID_OK;
}
int symstreamfcf_write_samples(symstreamrfcf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    for (i=0; i<_buf_len; i++){
        if(_q->buf_internal_index == 0){
            if(symstreamfcf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamfcf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf_internal[_q->buf_internal_index];
        _q->buf_internal_index = (_q->buf_internal_index + 1) % (_q->k);
    }
    return LIQUID_OK;
}
int symstreamrfcf_fill_buffer(symstreamrfcf _q){
    if (_q->buf_index != _q->buf_size){
        return fprintf(stderr,"systmstreamrfcf_write_samples(), buffer not empty\n");
    }
    _q->buf_size = 0;
    _q->buf_index = 0;

    while(!_q->buf_size){
        liquid_float_complex sample;
        symstreamfcf_write_samples(_q, &sample, 1);

        msresamp_crcf_execute(_q->arb_interp, &sample, 1, _q->buf, &_q->buf_size);
    }

    return LIQUID_OK;
}
int symstreamrfcf_write_samples(symstreamrfcf  _q, liquid_float_complex * _buf, unsigned int _buf_len){
    unsigned int i;
    if(_q->buf_index > _q->buf_size){
        return fprintf(stderr,"symstreamrfcf_write_samples(), called in a bad state\n");
    }
    for (i=0; i<_buf_len; i++){
        if(_q->buf_index == _q->buf_size){
            if(symstreamrfcf_fill_buffer(_q)){
                return fprintf(stderr, "symstreamrfcf_write_samples(), could not fill internal buffer\n");
            }
        }
        _buf[i] = _q->buf[_q->buf_index];
        _q->buf_index++;
    }
    return LIQUID_OK;
}
