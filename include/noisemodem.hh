#ifndef NOISEMODEM_HH
#define NOISEMODEM_HH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#ifdef __cplusplus
#include <complex>
#else
#include <complex.h>
#endif
#include "liquid.h"

// report error specifically for invalid object configuration 
// report error
static inline void * liquid_error_config_nl(const char * _file,
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
struct symstreamrncf_noise_s;
typedef struct symstreamrncf_noise_s *symstreamrncf;
                                                                        /* Create symstream object with default parameters.                     */
/* This is equivalent to invoking the create_linear() method            */
/* with _ftype=LIQUID_FIRFILT_ARKAISER, _k=2, _m=7, _beta=0.3, and      */
/* with _ms=LIQUID_MODEM_QPSK                                           */
symstreamrncf symstreamrncf_create(void);

/* Create symstream object with linear modulation                       */
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _bandwidth      : total signal bandwidth, (0,0.5)
//  _m              : filter delay (symbols)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ms             : noise scheme (e.g. LIQUID_NOISE_AWGN)
symstreamrncf symstreamrncf_create_noise(int          _ftype,
                                       float        _bandwidth,
                                       unsigned int _m,
                                       float        _beta,
                                       int          _ms);

/* Destroy symstream object, freeing all internal memory                */
int symstreamrncf_destroy(symstreamrncf _q);
/* Print symstream object's parameters                                  */
int symstreamrncf_print(symstreamrncf _q);
/* Reset symstream internal state                                       */
int symstreamrncf_reset(symstreamrncf _q);
/* Get internal filter type                                             */
int symstreamrncf_get_ftype(symstreamrncf _q);
/* Get internal signal bandwidth (symbol rate)                          */
float symstreamrncf_get_bw(symstreamrncf _q);
/* Get internal filter semi-length                                      */
unsigned int symstreamrncf_get_m(symstreamrncf _q);
/* Get internal filter sps                                      */
unsigned int symstreamrncf_get_k(symstreamrncf _q);
/* Get internal filter excess bandwidth factor                          */
float symstreamrncf_get_beta(symstreamrncf _q);
/* Set internal linear modulation scheme, leaving the filter parameters */
/* (interpolator) unmodified                                            */
int symstreamrncf_set_scheme(symstreamrncf _q, int _ms);
/* Get internal linear modulation scheme                                */
int symstreamrncf_get_scheme(symstreamrncf _q);
/* Set internal linear gain (before interpolation)                      */
int symstreamrncf_set_gain(symstreamrncf _q, float _gain);
/* Get internal linear gain (before interpolation)                      */
float symstreamrncf_get_gain(symstreamrncf _q);
/* Get delay in samples                                                 */
float symstreamrncf_get_delay(symstreamrncf _q);
/* Write block of samples to output buffer                              */
/*  _q      : synchronizer object                                       */
/*  _buf    : output buffer [size: _buf_len x 1]                        */
/*  _buf_len: output buffer size                                        */
int symstreamrncf_write_samples(symstreamrncf  _q, liquid_float_complex *_buf, unsigned int _buf_len);
int symstreamrncf_fill_buffer(symstreamrncf _q);

typedef enum {
    LIQUID_NOISE_UNKNOWN=0, // Unknown modulation scheme
    LIQUID_NOISE_AWGN,
} noise_scheme;

// structure for holding full modulation type descriptor
struct noise_type_s {
    const char * name;          // short name (e.g. 'bpsk')
    const char * fullname;      // full name (e.g. 'binary phase-shift keying')
    noise_scheme scheme;   // modulation scheme (e.g. LIQUID_MODEM_BPSK)
    unsigned int bps;           // modulation depth (e.g. 1)
};

#define noise_type_count 3
extern const struct noise_type_s noise_types[];

inline noise_scheme liquid_getopt_str2noise(const char * _str)
{
    // compare each string to short name
    unsigned int i;
    for (i=0; i<noise_type_count; i++) {
        if (strcmp(_str,noise_types[i].name)==0)
            return noise_types[i].scheme;
    }
    fprintf(stderr,"warning: liquid_getopt_str2noise(), unknown/unsupported noise scheme : %s\n", _str);
    return LIQUID_NOISE_UNKNOWN;
}

typedef struct noisemod_s * noisemod;

// create noisemod object (frequency modulator)
//  _k          :   samples/symbol, _k >= 2^_m
//  _bandwidth  :   total signal bandwidth, (0,0.5)
noisemod noisemod_create(
                     unsigned int _k,
                     float        _bandwidth);

// Copy object recursively including all internal objects and state
noisemod noisemod_copy(noisemod _q);

// destroy noisemod object
int noisemod_destroy(noisemod _q);

// print noisemod object internals
int noisemod_print(noisemod _q);

// reset state
int noisemod_reset(noisemod _q);

// modulate sample
//  _q      :   frequency modulator object
//  _s      :   input symbol
//  _y      :   output sample array, [size: _k x 1]
int noisemod_modulate(noisemod                 _q,
                    liquid_float_complex * _y);



// internal structure
struct symstreamrncf_noise_s {
    unsigned int    k;              // samples/symbol
    unsigned int    m;              // filter semi-length
    float           beta;           // filter excess bandwidth
    float           bw;             // bandwidth
    float           rate;
    int             filter_type;    // filter type (e.g. LIQUID_FIRFILT_ARKAISER)
    int             mod_scheme;     // demodulator
    noisemod        mod;            // modulator
    float           gain;           // gain before interpolation
    firinterp_crcf  interp;         // interpolator
    msresamp_crcf   arb_interp;     // arb_interp
    liquid_float_complex *            buf_internal;            // output buffer
    liquid_float_complex *            buf;            // output buffer
    unsigned int    buf_internal_index;      // output buffer sample index
    unsigned int    buf_index;      // output buffer sample index
    unsigned int    buf_size;
};


// Print compact list of existing and available noise modulation schemes
inline int liquid_print_noise_modulation_schemes()
{
    unsigned int i;
    unsigned int len = 10;

    // print all available modem schemes
    printf("          ");
    for (i=1; i<noise_type_count; i++) {
        printf("%s", noise_types[i].name);

        if (i != noise_type_count-1)
            printf(", ");

        len += strlen(noise_types[i].name);
        if (len > 48 && i != noise_type_count-1) {
            len = 10;
            printf("\n          ");
        }
    }
    printf("\n");
    return LIQUID_OK;
}


#endif // NOISEMODEM_HH