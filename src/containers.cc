

#include "containers.hh"
#include "writer.hh"

#ifdef __cplusplus
namespace wfgen{
using namespace writer;
namespace containers{
extern "C" {
#endif

uint64_t get_empty_content_size(uint8_t type){
    if (type & 0x40) return sizeof(void*);
    switch (type & 0x3F){
        case NULL:          // NULL
        case UNSPECIFIED:   // UNSPECIFIED
            return 0;
            break;
        case UINT8:         // uint8_t
        case INT8:          // int8_t
        case CHAR:          // char
            return 1;
            break;
        case UINT16:        // uint16_t
        case INT16:         // int16_t
        case CINT8:         // complex int8
            return 2;
            break;
        case UINT32:        // uint32_t
        case INT32:         // int32_t
        case FLOAT32:       // float32_t
        case POINTER32:     // pointer32
        case CINT16:        // complex int16
            return 4;
            break;
        case UINT64:        // uint64_t
        case INT64:         // int64_t
        case DOUBLE64:      // double64_t
        case POINTER64:     // pointer64
        case CINT32:        // complex int32
        case CFLOAT32:      // complex float32
            return 8;
            break;
        case LDOUBLE128:    // ldouble128_t
        case CINT64:        // complex int64
        case CDOUBLE64:     // complex double64
            return 16;
            break;
        case POINTER_S:       // pointer
            return sizeof(char *);
            break;
        case CLDOUBLE128:   // complex ldouble128
            return 32;
            break;
        case 24:    // BURST_CT
            return sizeof(cburst_t);
            break;
        case 25:    // WAVEFORM_CT
            return sizeof(waveform_t);
            break;
        case 26:    // WRITER_CT
            return sizeof(writer_t);
            break;
        case 27:    // LIST
        case 28:    // DICT
        case 63:    // CONTAINER
            return sizeof(container_t);
            break;
        case 29:    //
        case 30:    //
        case 31:    //
        case 32:    //
        case 33:    //
        case 34:    //
        case 35:    //
        case 36:    //
        case 37:    //
        case 38:    //
        case 39:    //
        case 40:    //
        case 41:    //
        case 42:    //
        case 43:    //
        case 44:    //
        case 45:    //
        case 46:    //
        case 47:    //
        case 48:    //
        case 49:    //
        case 50:    //
        case 51:    //
        case 52:    //
        case 53:    //
        case 54:    //
        case 55:    //
        case 56:    //
        case 57:    //
        case 58:    //
        case 59:    //
        case 60:    //
        case 61:    //
        case 62:    //
            break;
        default:
            break;
    }
    return 0;
}

container container_create_empty(){
    container c = (container) malloc(sizeof(container_t));
    memset(c, 0, sizeof(container_t));
    return c;
}

container container_create(uint8_t type, uint64_t size, void* ptr){
    container c = container_create_empty();
    c->type = type;
    c->size = size;
    if(ptr == NULL && size > 0){
        c->type |= 0x80;
        ptr = malloc(size);
    }
    return c;
}

void container_destroy(container *c){
    if(c == NULL) return;
    if((*c) == NULL) return;
    if((*c)->type & 0x80){
        if((*c)->ptr != NULL){
            free((*c)->ptr);
        }
    }
    free(*c);
    *c = NULL;
}

cburst cburst_create_empty(){
    cburst b = (cburst)malloc(sizeof(cburst_t));
    memset(b, 0, sizeof(cburst_t));
    b->size = sizeof(cburst_t) - sizeof(liquid_float_complex*);
    return b;
}

cburst cburst_create(uint8_t src, double fc, double fs, double bw, double ts, uint64_t max_samples){
    cburst b = cburst_create_empty();
    b->source = src;
    b->carrier = fc;
    b->sample_rate = fs;
    b->relative_bandwidth = bw;
    b->time_offset = ts;
    b->ownership = (uint8_t) max_samples > 0;
    b->max_samples = max_samples;
    if (b->ownership){
        b->ptr = (liquid_float_complex*) malloc(max_samples*sizeof(liquid_float_complex));
    }
    b->samples = 0;
    b->size = sizeof(cburst_t) - sizeof(liquid_float_complex*)
                + (b->ownership ? max_samples*sizeof(liquid_float_complex) : 0);
    return b;
}

void cburst_destroy(cburst *b){
    if (b == NULL) return;
    if (*b == NULL) return;
    if ((*b)->ownership){
        if ((*b)->ptr != NULL)
            free((*b)->ptr);
    }
    free(*b);
    *b = NULL;
}

void cburst_update(cburst b, uint8_t *src, double *fc, double *fs, double *bw,
        double *ts, uint64_t *max_samples){
    if (b==NULL) return;
    if (src != NULL) b->source = *src;
    if (fc != NULL) b->carrier = *fc;
    if (fs != NULL) b->sample_rate = *fs;
    if (bw != NULL) b->relative_bandwidth = *bw;
    if (ts != NULL) b->time_offset = *ts;
    if (max_samples != NULL) b->max_samples = *max_samples;
}

void cburst_set_pointer(cburst b, uint64_t samples, liquid_float_complex *ptr){
    if (b==NULL) return;
    if (b->ownership){
        b->size = sizeof(cburst_t) - sizeof(liquid_float_complex*);
        free(b->ptr);
    }
    b->ownership = 0;
    b->samples = samples;
    if (samples > b->max_samples) b->max_samples = samples;
    b->ptr = ptr;
}

void cburst_own_cburst(cburst b, uint64_t samples, liquid_float_complex *ptr){
    if (b==NULL) return;
    if (b->ownership){
        if(b->ptr == ptr) return;
        free(b->ptr);
    }
    b->ownership = 0;
    if (ptr == NULL){
        b->samples = 0;
        b->max_samples = 0;
        b->ptr = NULL;
        return;
    }
    b->samples = samples;
    b->max_samples = samples;
    b->ptr = (liquid_float_complex*) malloc(samples*sizeof(liquid_float_complex));
    memcpy( b->ptr, ptr, samples*sizeof(liquid_float_complex) );
    b->size = sizeof(cburst_t) - sizeof(liquid_float_complex*) + samples*sizeof(liquid_float_complex);
}

waveform waveform_create_empty(){
    waveform w = (waveform) malloc(sizeof(waveform_t));
    memset(w, 0, sizeof(waveform_t));
    w->size = sizeof(waveform_t) - sizeof(cburst*);
    return w;
}

waveform waveform_create(uint8_t characterisitics, uint64_t n_bursts){
    waveform w = waveform_create_empty();
    w->characteristics = characterisitics;
    w->max_bursts = n_bursts;
    w->bursts = 0;
    w->burst_p = (cburst*)malloc(sizeof(cburst)*n_bursts);
    memset(w->burst_p, 0, n_bursts*sizeof(cburst));
    w->size = sizeof(waveform_t) - sizeof(cburst*) + sizeof(cburst)*n_bursts;
    return w;
}

void waveform_destroy(waveform *wf){
    if(wf==NULL) return;
    if(*wf==NULL) return;
    if((*wf)->burst_p != NULL){
        for(size_t idx = 0; idx < (*wf)->max_bursts; idx++){
            cburst_destroy(&((*wf)->burst_p[idx]));
        }
        free((*wf)->burst_p);
    }
    free(*wf);
    *wf = NULL;
}


void waveform_update_burst(waveform wf, uint64_t burst_index,
        uint8_t *src, double *fc, double *fs, double *bw,
        double *ts, uint64_t *max_samples){
    if (wf == NULL) return;
    if (wf->burst_p == NULL) return;
    cburst_update(wf->burst_p[burst_index],
        src, fc, fs, bw, ts, max_samples);
}
void waveform_set_burst_pointer(waveform wf, uint64_t burst_index,
        uint64_t samples, liquid_float_complex* data){
    if (wf == NULL) return;
    if (wf->burst_p == NULL) return;
    cburst_set_pointer(wf->burst_p[burst_index], samples, data);
}
void waveform_set_own_burst(waveform wf, uint64_t burst_index,
        uint64_t samples, liquid_float_complex* data){
    if (wf == NULL) return;
    if (wf->burst_p == NULL) return;
    cburst_own_cburst(wf->burst_p[burst_index], samples, data);
}
void waveform_compact(waveform *wf){
    if (wf == NULL) return; // a null input
    if (*wf == NULL) return; // pointing to null
    if ((*wf)->burst_p == NULL) return; // wf has no burst ptr
    size_t header_size(0), data_size(0);
    size_t total_size(0), current_size(0), index(0);
    waveform _wf = *wf;
    current_size = sizeof(waveform_t) - sizeof(cburst*); // waveform_t static info
    header_size += current_size;
    total_size += current_size;

    cburst b;
    size_t n_bursts = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = _wf->burst_p[index];
        if(b!=NULL){
            if(b->samples > 0){
                //burst is valid
                n_bursts += 1;
                current_size = sizeof(cburst_t); // cburst_t static_info + ptr
                header_size += current_size;
                total_size += current_size;
                current_size = sizeof(liquid_float_complex)*b->samples; // samples
                data_size += current_size;
                total_size += current_size;
            }
        }
    }
    current_size = n_bursts*sizeof(cburst); // ptr to cburst objects
    total_size += current_size;
    waveform new_wf = (waveform)malloc(total_size);
    uint8_t *compact = (uint8_t*)new_wf;
    current_size = sizeof(waveform_t) - sizeof(cburst*);
    memcpy( compact, _wf, current_size );
    cburst *burst_ptr = (cburst*)(compact + current_size);
    current_size += n_bursts*sizeof(cburst); // skip burst ptrs
    compact += current_size;
    size_t burst_idx = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = _wf->burst_p[index];
        if(b!=NULL){
            if(b->samples > 0){
                current_size = sizeof(cburst_t) - sizeof(liquid_float_complex*); // cburst_t static_info
                memcpy( compact, b, current_size );
                burst_ptr[burst_idx] = (cburst)compact;
                burst_idx += 1;
                current_size += sizeof(liquid_float_complex*);
                compact += current_size;
            }
        }
    }
    liquid_float_complex *data_ptr = (liquid_float_complex*)compact;
    burst_idx = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = _wf->burst_p[index];
        if(b!=NULL){
            if(b->samples > 0){
                burst_ptr[burst_idx]->samples = b->samples;
                burst_ptr[burst_idx]->max_samples = b->samples;
                burst_ptr[burst_idx]->ownership = 1;
                burst_ptr[burst_idx]->ptr = data_ptr;
                burst_idx += 1;
                current_size = sizeof(liquid_float_complex)*b->samples;
                memcpy( compact, b->ptr, current_size );
                compact += current_size;
                data_ptr = (liquid_float_complex*)compact;
            }
        }
    }

    waveform *temp = wf;
    wf = &new_wf;
    waveform_destroy(temp);
}

#ifdef __cplusplus
}}}
#endif