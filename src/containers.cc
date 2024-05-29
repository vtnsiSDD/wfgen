

#include "containers.hh"

#ifdef __cplusplus
namespace wfgen{
namespace containers{
extern "C" {
#endif

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