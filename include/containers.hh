#ifndef CONTAINERS_HH
#define CONTAINERS_HH

#include <complex>
#include <rapidjson/document.h>
#include "liquid.h"

#ifdef __cplusplus
namespace wfgen{
namespace containers{
extern "C" {
#endif

typedef enum burst_src_e{
    EMPTY=255,
    TONE=0,
    AM,
    FM,
    FSK,
    LDAPM,
    MULTITONE,
    MULTICARRIER,
} burst_src_t;

#pragma pack(push, 1)
typedef struct cburst_s{
    uint64_t    size;
    uint8_t     source;
    double      carrier;
    double      sample_rate;
    double      relative_bandwidth;
    double      time_offset;
    uint8_t     ownership;
    uint64_t    max_samples;
    uint64_t    samples;
    liquid_float_complex *ptr;
} cburst_t;
typedef cburst_t * cburst;
#pragma pack(pop)

cburst cburst_create_empty();
cburst cburst_create(uint8_t src, double fc, double fs, double bw, double ts, uint64_t max_samples);
void cburst_destroy(cburst* b);
void cburst_update(cburst b,
        uint8_t *src=NULL, double *fc=NULL, double *fs=NULL, double *bw=NULL,
        double *ts=NULL, uint64_t *max_samples=NULL);
void cburst_set_pointer(cburst b, uint64_t samples, liquid_float_complex* data);
void cburst_own_cburst(cburst b, uint64_t samples, liquid_float_complex* data);

#define WFGEN_STATIC    0
#define WFGEN_BURSTY    1
#define WFGEN_HOPPING   2
#define WFGEN_CHIRPING  4
#define WFGEN_WIDE      8
#define WFGEN_UNKNOWN   64
#define WFGEN_NULL      128

#pragma pack(push, 1)
typedef struct waveform_s{
    uint64_t    size;
    uint8_t     characteristics;
    uint64_t    max_bursts;
    uint64_t    bursts;
    cburst      *burst_p;
} waveform_t;
typedef waveform_t * waveform;

typedef struct header_ptr_s{

} header_ptr_t;
typedef header_ptr_t * header_ptr;

typedef struct data_ptr_s{

} data_ptr_t;
typedef data_ptr_t * data_ptr;
#pragma pack(pop)



waveform waveform_create_empty();
waveform waveform_create(uint8_t characteristics,uint64_t n_bursts);
void waveform_destroy(waveform *wf);
void waveform_update_burst(waveform wf, uint64_t burst_index,
        uint8_t *src=NULL, double *fc=NULL, double *fs=NULL, double *bw=NULL,
        double *ts=NULL, uint64_t *max_samples=NULL);
void waveform_set_burst_pointer(waveform wf, uint64_t burst_index,
        uint64_t samples, liquid_float_complex* data);
void waveform_set_own_burst(waveform wf, uint64_t burst_index,
        uint64_t samples, liquid_float_complex* data);
void waveform_compact(waveform *wf);

#ifdef __cplusplus
}}}
#endif

#endif // CONTAINERS_HH