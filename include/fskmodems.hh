#ifndef FSKMODEMS_HH
#define FSKMODEMS_HH

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
inline void * liquid_error_config_fl(const char * _file,
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
struct symstreamrfcf_fsk_s;
typedef struct symstreamrfcf_fsk_s *symstreamrfcf;
                                                                        /* Create symstream object with default parameters.                     */
/* This is equivalent to invoking the create_linear() method            */
/* with _ftype=LIQUID_FIRFILT_ARKAISER, _k=2, _m=7, _beta=0.3, and      */
/* with _ms=LIQUID_MODEM_QPSK                                           */
symstreamrfcf symstreamrfcf_create(void);

/* Create symstream object with linear modulation                       */
//  _ftype          : filter type (e.g. LIQUID_FIRFILT_ARKAISER)
//  _k              : samples per symbol, _k >= 2^m, _k even
//  _h              : modulation index, _h > 0
//  _m              : filter delay (symbols)
//  _bandwidth      : total signal bandwidth, (0,0.5)
//  _beta           : filter bandwidth parameter, (0,1]
//  _ftype          : filter type (e.g. LIQUID_CPFSK_SQUARE)
//  _ms             : fsk scheme (e.g. LIQUID_MODEM_FSK4)
symstreamrfcf symstreamrfcf_create_fsk(int          _ftype,
                                       unsigned int _k,
                                       float        _h,
                                       unsigned int _m,
                                       float        _bandwidth,
                                       float        _beta,
                                       int          _cpftype,
                                       int          _ms);

/* Destroy symstream object, freeing all internal memory                */
int symstreamrfcf_destroy(symstreamrfcf _q);
/* Print symstream object's parameters                                  */
int symstreamrfcf_print(symstreamrfcf _q);
/* Reset symstream internal state                                       */
int symstreamrfcf_reset(symstreamrfcf _q);
/* Get internal filter type                                             */
int symstreamrfcf_get_ftype(symstreamrfcf _q);
/* Get internal signal bandwidth (symbol rate)                          */
float symstreamrfcf_get_bw(symstreamrfcf _q);
/* Get internal filter semi-length                                      */
unsigned int symstreamrfcf_get_m(symstreamrfcf _q);
/* Get internal filter sps                                      */
unsigned int symstreamrfcf_get_k(symstreamrfcf _q);
/* Get internal filter excess bandwidth factor                          */
float symstreamrfcf_get_beta(symstreamrfcf _q);
/* Set internal linear modulation scheme, leaving the filter parameters */
/* (interpolator) unmodified                                            */
int symstreamrfcf_set_scheme(symstreamrfcf _q, int _ms);
/* Get internal linear modulation scheme                                */
int symstreamrfcf_get_scheme(symstreamrfcf _q);
/* Set internal linear gain (before interpolation)                      */
int symstreamrfcf_set_gain(symstreamrfcf _q, float _gain);
/* Get internal linear gain (before interpolation)                      */
float symstreamrfcf_get_gain(symstreamrfcf _q);
/* Get delay in samples                                                 */
float symstreamrfcf_get_delay(symstreamrfcf _q);
/* Write block of samples to output buffer                              */
/*  _q      : synchronizer object                                       */
/*  _buf    : output buffer [size: _buf_len x 1]                        */
/*  _buf_len: output buffer size                                        */
int symstreamrfcf_write_samples(symstreamrfcf  _q, liquid_float_complex *_buf, unsigned int _buf_len);
int symstreamrfcf_fill_buffer(symstreamrfcf _q);

typedef enum {
    LIQUID_FSK_UNKNOWN=0, // Unknown modulation scheme

    // Frequency-shift keying (FSK)
    LIQUID_MODEM_FSK2,      LIQUID_MODEM_FSK4,
    LIQUID_MODEM_FSK8,      LIQUID_MODEM_FSK16,
    LIQUID_MODEM_FSK32,     LIQUID_MODEM_FSK64,
    LIQUID_MODEM_FSK128,    LIQUID_MODEM_FSK256,

    // Frequency-shift keying (CPFSK)
    LIQUID_MODEM_CPFSK2,      LIQUID_MODEM_CPFSK4,
    LIQUID_MODEM_CPFSK8,      LIQUID_MODEM_CPFSK16,
    LIQUID_MODEM_CPFSK32,     LIQUID_MODEM_CPFSK64,
    LIQUID_MODEM_CPFSK128,    LIQUID_MODEM_CPFSK256,

    // Minimum-shift keying (MSK)
    LIQUID_MODEM_MSK2,      LIQUID_MODEM_MSK4,
    LIQUID_MODEM_MSK8,      LIQUID_MODEM_MSK16,
    LIQUID_MODEM_MSK32,     LIQUID_MODEM_MSK64,
    LIQUID_MODEM_MSK128,    LIQUID_MODEM_MSK256,

    // Gaussian Minimum-shift keying (GMSK)
    LIQUID_MODEM_GMSK2,      LIQUID_MODEM_GMSK4,
    LIQUID_MODEM_GMSK8,      LIQUID_MODEM_GMSK16,
    LIQUID_MODEM_GMSK32,     LIQUID_MODEM_GMSK64,
    LIQUID_MODEM_GMSK128,    LIQUID_MODEM_GMSK256
} fsk_scheme;

// structure for holding full modulation type descriptor
struct fsk_type_s {
    const char * name;          // short name (e.g. 'bpsk')
    const char * fullname;      // full name (e.g. 'binary phase-shift keying')
    fsk_scheme scheme;   // modulation scheme (e.g. LIQUID_MODEM_BPSK)
    unsigned int bps;           // modulation depth (e.g. 1)
};

#define FSK_TYPE_COUNT 33
extern const struct fsk_type_s fsk_types[];

inline fsk_scheme liquid_getopt_str2fsk(const char * _str)
{
    // compare each string to short name
    unsigned int i;
    for (i=0; i<FSK_TYPE_COUNT; i++) {
        if (strcmp(_str,fsk_types[i].name)==0)
            return (fsk_scheme)i;
    }
    fprintf(stderr,"warning: liquid_getopt_str2fsk(), unknown/unsupported fsk scheme : %s\n", _str);
    return LIQUID_FSK_UNKNOWN;
}

// internal structure
struct symstreamrfcf_fsk_s {
    unsigned int    bps;            // bits per symbol
    unsigned int    M;              // Modualtion order
    float           h;              // modulation index
    unsigned int    k;              // samples/symbol
    unsigned int    m;              // filter semi-length
    float           beta;           // filter excess bandwidth
    float           bw;             // bandwidth
    float           rate;
    int             cpfsk_type;     // smoother type (e.g. LIQUID_CPFSK_SQUARE)
    int             filter_type;    // filter type (e.g. LIQUID_FIRFILT_ARKAISER)
    int             mod_scheme;     // demodulator
    gmskmod         mod_g;          // modulator
    fskmod          mod_f;          // modulator
    cpfskmod        mod_c;          // modulator
    float           gain;           // gain before interpolation
    firinterp_crcf  interp;         // interpolator
    msresamp_crcf   arb_interp;     // arb_interp
    liquid_float_complex *            buf_internal;            // output buffer
    liquid_float_complex *            buf;            // output buffer
    unsigned int    buf_internal_index;      // output buffer sample index
    unsigned int    buf_index;      // output buffer sample index
    unsigned int    buf_size;
};


// Print compact list of existing and available fsk modulation schemes
inline int liquid_print_fsk_modulation_schemes()
{
    unsigned int i;
    unsigned int len = 10;

    // print all available modem schemes
    printf("          ");
    for (i=1; i<FSK_TYPE_COUNT; i++) {
        printf("%s", fsk_types[i].name);

        if (i != FSK_TYPE_COUNT-1)
            printf(", ");

        len += strlen(fsk_types[i].name);
        if (len > 48 && i != FSK_TYPE_COUNT-1) {
            len = 10;
            printf("\n          ");
        }
    }
    printf("\n");
    return LIQUID_OK;
}


#endif // FSKMODEMS_HH