#ifndef CONTAINERS_HH
#define CONTAINERS_HH

#include "forward.hh"
#include "liquid.h"
// #include <rapidjson/document.h>

#ifdef __cplusplus
namespace wfgen{
namespace containers{
extern "C" {
#endif

typedef enum content_type_e{
    OWNER=0x80,
    POINTER=0x40,
    CONTAINER=0x3F,
    UNSPECIFIED=1,
    UINT8,
    INT8,
    UINT16,
    INT16,
    UINT32,
    INT32,
    UINT64,
    INT64,
    FLOAT32,
    DOUBLE64,
    LDOUBLE128,
    CHAR,
    POINTER_S,
    POINTER32,
    POINTER64,
    CINT8,
    CINT16,
    CINT32,
    CINT64,
    CFLOAT32,
    CDOUBLE64,
    CLDOUBLE128,
    BURST_CT,
    WAVEFORM_CT,
    LIST,
    DICT
} content_t;

uint64_t get_empty_content_size(uint8_t type);

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
typedef struct container_s{
    uint8_t     type;
    uint64_t    size;
    void        *ptr;
} container_t;
typedef container_t * container;
typedef struct cburst_s{
    uint64_t    size;
    uint8_t     source;
    uint8_t     incomplete;
    double      carrier;
    double      sample_rate;
    double      relative_bandwidth;
    double      time_offset;
    uint8_t     ownership;
    uint64_t    max_samples;
    uint64_t    samples;
    cburst      next;
    cburst      previous;
    fc32        *ptr;
} cburst_t;
typedef cburst_t * cburst;
#pragma pack(pop)

container container_create_empty();
#ifdef __cplusplus
container container_create(uint8_t type, uint64_t size, void* ptr=NULL);
void container_get_info(container c, uint64_t *items=NULL, uint64_t *itemsize=NULL, size_t *binsize=NULL);
#else
container container_create(uint8_t type, uint64_t size, void* ptr);
void container_get_info(container c, uint64_t *items, uint64_t *itemsize, size_t *binsize);
#endif
void container_destroy(container* c);

cburst cburst_create_empty();
cburst cburst_create(uint8_t src, double fc, double fs, double bw, double ts, uint64_t max_samples);
void cburst_destroy(cburst* b);
#ifdef __cplusplus
void cburst_update(cburst b,
        uint8_t *src=NULL, double *fc=NULL, double *fs=NULL, double *bw=NULL,
        double *ts=NULL, uint64_t *max_samples=NULL);
#else
void cburst_update(cburst b,
        uint8_t *src, double *fc, double *fs, double *bw,
        double *ts, uint64_t *max_samples);
#endif
void cburst_set_pointer(cburst b, uint64_t samples, fc32* data);
void cburst_load_pointer(cburst b, uint64_t samples, fc32 *ptr);
uint8_t cburst_contiguous(cburst b, uint64_t samples, fc32* data);


//////////////////////////////////////////////////////////////////
// waveform characterisitics
#define WFGEN_STATIC    0   /// continuous waveform
#define WFGEN_BURSTY    1   /// on and off a/periodically
#define WFGEN_HOPPING   2   /// moves around in frequency (not information)
#define WFGEN_CHIRPING  4   /// slides around in continuous frequency (even if digital)
#define WFGEN_WIDE      8   /// uses bandwidth in excess of modulation's required baud rate
#define WFGEN_UNKNOWN   64  /// no idea what this is, but here it is
#define WFGEN_NULL      128 /// nothing is in here
//////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
typedef struct waveform_s{
    uint64_t    size;
    uint8_t     characteristics;
    uint64_t    max_burst_length;
    uint64_t    max_bursts;
    uint64_t    bursts;
    cburst      *burst_array;
    fc32        *iq_array;
} waveform_t;
typedef waveform_t * waveform;

// typedef struct header_ptr_s{
//
// } header_ptr_t;
// typedef header_ptr_t * header_ptr;
//
// typedef struct data_ptr_s{
//
// } data_ptr_t;
// typedef data_ptr_t * data_ptr;
#pragma pack(pop)



waveform waveform_create_empty();
waveform waveform_create(uint8_t characteristics, uint64_t n_bursts, uint64_t max_samples);
void waveform_destroy(waveform *wf);
#ifdef __cplusplus
void waveform_update_burst(waveform wf, uint64_t burst_index,
        uint8_t *src=NULL, double *fc=NULL, double *fs=NULL, double *bw=NULL,
        double *ts=NULL, uint64_t *max_samples=NULL);
#else
void waveform_update_burst(waveform wf, uint64_t burst_index,
        uint8_t *src, double *fc, double *fs, double *bw,
        double *ts, uint64_t *max_samples);
#endif
void waveform_set_burst_pointer(waveform wf, uint64_t burst_index,
        uint64_t samples, fc32* data);
void waveform_set_own_burst(waveform wf, uint64_t burst_index,
        uint64_t samples, fc32* data);
void waveform_compact(waveform *wf);

#ifdef __cplusplus
}}}
#endif

#endif // CONTAINERS_HH