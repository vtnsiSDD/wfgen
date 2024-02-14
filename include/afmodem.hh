#ifndef ANALOGMODEM_HH
#define ANALOGMODEM_HH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <complex>
#include "liquid.h"
#include "analog.hh"

// report error specifically for invalid object configuration 
// report error
inline void * liquid_error_config_al(const char * _file,
                              int          _line,
                              const char * _format,
                              ...)
{
    int code = LIQUID_EICONFIG;
#if !LIQUID_SUPPRESS_ERROR_OUTPUT
    va_list argptr;
    va_start(argptr, _format);
    fprintf(stderr,"error [%d]: %s\n", code, liquid_error_info((liquid_error_code)code));
    fprintf(stderr,"  %s:%u: ", _file, _line);
    vfprintf(stderr, _format, argptr);
    fprintf(stderr,"\n");
    va_end(argptr);
#endif
#if LIQUID_STRICT_EXIT
    exit(code);
#endif
    return NULL;
}

//
// symbol streaming, as with symstream but arbitrary output rate
//
/* Symbol streaming generator object                                    */
struct symstreamracf_analog_s;
typedef struct symstreamracf_analog_s *symstreamracf;
                                                                        /* Create symstream object with default parameters.                     */
/* This is equivalent to invoking the create_linear() method            */
/* with _ftype=LIQUID_FIRFILT_ARKAISER, _k=2, _m=7, _beta=0.3, and      */
/* with _ms=LIQUID_MODEM_QPSK                                           */
symstreamracf symstreamracf_create(void);

/* Create symstream object with linear modulation                       */
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
                                       int          _ms);

/* Destroy symstream object, freeing all internal memory                */
int symstreamracf_destroy(symstreamracf _q);
/* Print symstream object's parameters                                  */
int symstreamracf_print(symstreamracf _q);
/* Reset symstream internal state                                       */
int symstreamracf_reset(symstreamracf _q);
/* Get internal filter type                                             */
int symstreamracf_get_ftype(symstreamracf _q);
/* Get internal signal bandwidth (symbol rate)                          */
float symstreamracf_get_bw(symstreamracf _q);
/* Get internal filter semi-length                                      */
unsigned int symstreamracf_get_m(symstreamracf _q);
/* Get internal filter sps                                      */
unsigned int symstreamracf_get_k(symstreamracf _q);
/* Get internal filter excess bandwidth factor                          */
float symstreamracf_get_beta(symstreamracf _q);
/* Set internal linear modulation scheme, leaving the filter parameters */
/* (interpolator) unmodified                                            */
int symstreamracf_set_scheme(symstreamracf _q, int _ms);
/* Get internal linear modulation scheme                                */
int symstreamracf_get_scheme(symstreamracf _q);
/* Set internal linear gain (before interpolation)                      */
int symstreamracf_set_gain(symstreamracf _q, float _gain);
/* Get internal linear gain (before interpolation)                      */
float symstreamracf_get_gain(symstreamracf _q);
/* Get delay in samples                                                 */
float symstreamracf_get_delay(symstreamracf _q);
/* Write block of samples to output buffer                              */
/*  _q      : synchronizer object                                       */
/*  _buf    : output buffer [size: _buf_len x 1]                        */
/*  _buf_len: output buffer size                                        */
int symstreamracf_write_samples(symstreamracf  _q, liquid_float_complex *_buf, unsigned int _buf_len);
int symstreamracf_fill_buffer(symstreamracf _q);

typedef enum {
    LIQUID_ANALOG_UNKNOWN=0, // Unknown modulation scheme
    LIQUID_ANALOG_AM_CONSTANT,
    LIQUID_ANALOG_AM_SQUARE,
    LIQUID_ANALOG_AM_TRIANGLE,
    LIQUID_ANALOG_AM_SAWTOOTH,
    LIQUID_ANALOG_AM_SINUSOID,
    LIQUID_ANALOG_AM_WAV_FILE,
    LIQUID_ANALOG_AM_RAND_UNI,
    LIQUID_ANALOG_AM_RAND_GAUSS,
    LIQUID_ANALOG_FM_CONSTANT,
    LIQUID_ANALOG_FM_SQUARE,
    LIQUID_ANALOG_FM_TRIANGLE,
    LIQUID_ANALOG_FM_SAWTOOTH,
    LIQUID_ANALOG_FM_SINUSOID,
    LIQUID_ANALOG_FM_WAV_FILE,
    LIQUID_ANALOG_FM_RAND_UNI,
    LIQUID_ANALOG_FM_RAND_GAUSS,
} analog_scheme;

// structure for holding full modulation type descriptor
struct analog_type_s {
    const char * name;          // short name (e.g. 'bpsk')
    const char * fullname;      // full name (e.g. 'binary phase-shift keying')
    analog_scheme scheme;   // modulation scheme (e.g. LIQUID_ANALOG_FM_SINUSOID)
    unsigned int bps;           // modulation depth (e.g. 1)
};

const struct analog_type_s analog_types[17] = {
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
    {"fm_constant",     "analog_fm_constant",     LIQUID_ANALOG_FM_CONSTANT,    1},
    {"fm_square",       "analog_fm_square",       LIQUID_ANALOG_FM_SQUARE,      1},
    {"fm_triangle",     "analog_fm_triangle",     LIQUID_ANALOG_FM_TRIANGLE,    1},
    {"fm_sawtooth",     "analog_fm_sawtooth",     LIQUID_ANALOG_FM_SAWTOOTH,    1},
    {"fm_sinusoid",     "analog_fm_sinusoid",     LIQUID_ANALOG_FM_SINUSOID,    1},
    {"fm_wav_file",     "analog_fm_wav_file",     LIQUID_ANALOG_FM_WAV_FILE,    1},
    {"fm_rand_uni",     "analog_fm_rand_uni",     LIQUID_ANALOG_FM_RAND_UNI,    1},
    {"fm_rand_gauss",   "analog_fm_rand_gauss",   LIQUID_ANALOG_FM_RAND_GAUSS,  1},
};

inline analog_scheme liquid_getopt_str2analog(const char * _str)
{
    // compare each string to short name
    unsigned int i;
    for (i=0; i<17; i++) {
        if (strcmp(_str,analog_types[i].name)==0)
            return (analog_scheme)i;
    }
    fprintf(stderr,"warning: liquid_getopt_str2analog(), unknown/unsupported analog scheme : %s\n", _str);
    return LIQUID_ANALOG_UNKNOWN;
}

// internal structure
struct symstreamracf_analog_s {
    unsigned int    k;              // samples/symbol
    unsigned int    m;              // filter semi-length
    float           beta;           // filter excess bandwidth
    float           bw;             // bandwidth
    float           mod_idx;
    float           src_freq;
    float           rate;
    int             filter_type;    // filter type (e.g. LIQUID_FIRFILT_ARKAISER)
    int             mod_scheme;     // demodulator
    void*           true_source;
    real_source     wrap_source;
    real_path       path;
    amgen           mod_a;          // modulator
    fmgen           mod_f;          // modulator
    float           gain;           // gain before interpolation
    firinterp_crcf  interp;         // interpolator
    msresamp_crcf   arb_interp;     // arb_interp
    liquid_float_complex *            buf_internal;            // output buffer
    liquid_float_complex *            buf;            // output buffer
    unsigned int    buf_internal_index;      // output buffer sample index
    unsigned int    buf_index;      // output buffer sample index
    unsigned int    buf_size;
};


// Print compact list of existing and available analog modulation schemes
inline int liquid_print_analog_modulation_schemes()
{
    unsigned int i;
    unsigned int len = 10;

    // print all available modem schemes
    printf("          ");
    for (i=1; i<17; i++) {
        printf("%s", analog_types[i].name);

        if (i != 17-1)
            printf(", ");

        len += strlen(analog_types[i].name);
        if (len > 48 && i != 17-1) {
            len = 10;
            printf("\n          ");
        }
    }
    printf("\n");
    return LIQUID_OK;
}


#endif // ANALOGMODEM_HH