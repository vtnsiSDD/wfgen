

#include "containers.hh"
#include "writer.hh"

#ifdef __cplusplus
#include <cstring>
namespace wfgen{
using namespace writer;
namespace containers{
extern "C" {
#endif


//////////////////////////////////////////////////////////////////////////
// CONTAINERS
//////////////////////////////////////////////////////////////////////////
uint64_t get_empty_content_size(uint8_t type){
    if (type & 0x40) return sizeof(void*);
    switch (type & 0x3F){
        case 0:             // NULL
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
    uint8_t own = (type&0x80) > 0;
    uint8_t pointer = (type&0x40) > 0;
    // base_size -> value of zero, size should be bytes
    // elif a container -> parse it to figure out unless null, then ...
    // elif pointer -> size is number of items
    uint64_t base_size = get_empty_content_size(type&0x3F);
    uint64_t raw_size;// = size*((base_size) ? base_size : 1);
    if(pointer){
        raw_size = (base_size) ? base_size*size : size;
        c->size = size;
    }
    else{
        raw_size = (base_size) ? base_size : size;
        c->size = raw_size;
    }
    c->type = type;
    if(own){
        /// need to own
        void *adj = realloc(c, raw_size);
        if(adj==NULL){
            c->type &= 0x7F;//// can't own it
            c->ptr = ptr;
            return c;
        }
        else{
            c = (container)adj;
            // there's data to own
            if (ptr != NULL) memcpy(c->ptr,ptr,raw_size);
            else memset(c->ptr, 0, raw_size);
        }
    }
    else if(ptr == NULL && size > 0){
        // create an empty owned buffer
        void *adj = realloc(c, size);
        if(adj == NULL){
            c->ptr = NULL;
            c->size = 0;
        }
        else{
            c = (container)adj;
            c->type |= 0x80;
            memset(c->ptr, 0, size);
        }
    }
    else{
        // i don't want to own what I'm pointing to
        c->ptr = ptr;
    }
    return c;
}

void container_destroy(container *c){
    if(c == NULL) return;
    if((*c) == NULL) return;
    if((*c)->type & 0x80){ // if owner
        if((*c)->ptr != NULL) free((*c)->ptr);
    }
    free(*c);
    *c = NULL;
}

void container_get_info(container c, uint64_t *items, uint64_t *itemsize, size_t *binsize){
    uint64_t base_size = get_empty_content_size(c->type);
    uint8_t ptr = ((c->type&0x40) > 0) ? 1u : 0u;
    if(ptr && base_size){
        *items = c->size;
        *itemsize = base_size;
        *binsize = base_size * c->size;
    }
    else if(ptr){
        /// no bin size readily found
        *itemsize = c->size;
        *items = 1;
        *binsize = c->size;
    }
    else if (base_size){
        /// not flagged as a pointer
        // container might have more allocated than needed
        *items = 1;
        *itemsize = base_size;
        *binsize = base_size;
    }
    else{
        // not a pointer, nor a known bin size
        *itemsize = c->size;
        *binsize = c->size;
        *items = 1;
    }
    ///
}

//////////////////////////////////////////////////////////////////////////
// BURSTS
//////////////////////////////////////////////////////////////////////////
cburst cburst_create_empty(){
    cburst b = (cburst)malloc(sizeof(cburst_t));
    memset(b, 0, sizeof(cburst_t));
    b->size = sizeof(cburst_t) - sizeof(fc32*);
    return b;
}

cburst cburst_create(uint8_t src, double fc, double fs, double bw, double ts, uint64_t max_samples){
    /////
    // if ownership is defined by max_samples > 0
    // if ownership then the array is attached to the struct as a tail
    // if not ownership, the struct is just that with a pointer at the end
    /////
    cburst b;//pointer
    size_t lfcs = sizeof(fc32);
    size_t lfcsp = sizeof(fc32*);
    uint8_t owner = (uint8_t)(max_samples > 0);
    size_t size = sizeof(cburst_t) + (owner ? max_samples*lfcs-lfcsp : 0);
    b = (cburst)malloc(size);
    memset(b, 0, size);
    b->source = src;
    b->incomplete = 0;
    b->carrier = fc;
    b->sample_rate = fs;
    b->relative_bandwidth = bw;
    b->time_offset = ts;
    b->ownership = owner;
    b->max_samples = max_samples;
    b->samples = 0;
    b->size = size;
    b->next = NULL;
    b->previous = NULL;
    return b;
}

void cburst_destroy(cburst *b){
    if (b == NULL) return;
    if (*b == NULL) return;
    if ((*b)->ownership && (*b)->ptr != NULL) free((*b)->ptr); // free buffer if owned
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
    if (max_samples != NULL){
        if (b->ownership){
            void *adj = realloc(b, sizeof(cburst_t)+sizeof(fc32)*(*max_samples)-sizeof(fc32*));
            if(adj != NULL){
                b = (cburst)adj;
                b->max_samples = *max_samples;
            }
        }
        else{// don't own, hopefully user knows what they're doing
            b->max_samples = *max_samples;
        }
    }
}

void cburst_set_pointer(cburst b, uint64_t samples, fc32 *ptr){
    if (b==NULL) return;
    if (b->ownership){//releasing ownership here
        size_t orig_size = b->size;
        b->size = sizeof(cburst_t);
        void *adj = realloc(b, b->size);;
        if(adj == NULL){
            printf("Minimal leak of %lu bytes, maximum leak of %lu bytes",orig_size-b->size, orig_size);
        }
        b = (adj == NULL) ? b : (cburst)adj;
    }
    b->ownership = 0;
    b->samples = samples;
    // don't own, so just adjust
    if (samples > b->max_samples) b->max_samples = samples;
    b->ptr = ptr;
}
void cburst_load_pointer(cburst b, uint64_t samples, fc32 *ptr){
    if (b==NULL) return;
    if (b->ownership){
        if (samples > b->max_samples){
            void *adj = realloc(b, samples*sizeof(fc32) + sizeof(cburst_t) - sizeof(fc32*));
            if (adj == NULL){
                // can't accept all that data
                samples = b->max_samples;
            }
            else{
                b = (cburst)adj;
            }
        }
    }
    b->ownership = 1;
    if(ptr >= b->ptr && ptr < (b->ptr + b->max_samples)){
        // memory is pointing inside owned space
        size_t len = (b->ptr + b->max_samples - ptr);
        memmove(b->ptr, ptr, len*sizeof(fc32));
        b->samples = len;
    }
    else{
        b->samples = samples;
        memcpy(b->ptr, ptr, samples);
    }
}

void cburst_contigous(cburst b, uint64_t samples, fc32 *ptr){
    if (b==NULL) return;
    if (b->ownership){
        // already own some data
        fc32* b_data = (fc32*)&b->ptr;
        if((ptr==NULL)||(b_data == ptr)){
            b->samples = samples;
            return; // pointing to my own data
        }
        if((ptr >= b_data) && (ptr < (b_data + b->max_samples))){
            /// chopping out some of the buffer
            if(ptr+samples > (b_data + b->max_samples)){
                // stretching outside what I should own.. clipping
                samples = (b_data + b->max_samples) - ptr;
            }
            memmove(b_data, ptr, samples*sizeof(fc32));
            b->samples = samples;
            return;
        }
        // ptr isn't in the owned data, so only copy it here then.
        if(samples <= b->max_samples){
            // new data, but enough space available
            memcpy(b_data, ptr, samples*sizeof(fc32));
            b->samples = samples;
        }
        else{
            // need more buffer to hold it all
            void *adj = realloc(b, sizeof(cburst_t)+samples*sizeof(fc32)-sizeof(fc32*));
            if(adj == NULL){
                /// well do what we can since we can't hold it all
                samples = b->max_samples;
            }
            memcpy(&b->ptr, ptr, samples*sizeof(fc32));
            b->samples = samples;
            b->max_samples = samples;
            b->size = sizeof(cburst_t) + samples*sizeof(fc32) - sizeof(fc32*);
        }
    }
    else{
        //// don't own any data at the moment
        if (ptr == NULL){
            // not pointing to anything.... cleanup
            b->samples = 0;
            b->max_samples = 0;
            b->ptr = NULL;
            b->size = sizeof(cburst_t);
            return;
        }
        void *adj = realloc(b, sizeof(cburst_t)+samples*sizeof(fc32)-sizeof(fc32*));
        if(adj == NULL){
            // well.. can't take ownership
            b->samples = 0;
            b->max_samples = 0;
            b->ptr = NULL;
            b->size = sizeof(cburst_t);
            return;
        }
        b = (cburst)adj;
        b->samples = samples;
        b->max_samples = samples;
        fc32 *b_data = (fc32*)&b->ptr;
        memcpy( b_data, ptr, samples*sizeof(fc32) );
        b->size = sizeof(cburst_t) - sizeof(fc32*) + samples*sizeof(fc32);
        b->ownership = 1;
    }
}

//////////////////////////////////////////////////////////////////////////
// WAVEFORMS
//////////////////////////////////////////////////////////////////////////
waveform waveform_create_empty(){
    waveform w = (waveform) malloc(sizeof(waveform_t));
    memset(w, 0, sizeof(waveform_t));
    w->size = sizeof(waveform_t);
    w->burst_array = NULL;
    w->iq_array = NULL;
    return w;
}

waveform waveform_create(uint8_t characterisitics, uint64_t n_bursts, uint64_t max_samples){
    // Going to own it all
    uint64_t wf_size = sizeof(waveform_t);
    uint64_t bh_size = sizeof(cburst_t)*n_bursts;
    uint64_t iq_size = sizeof(fc32)*max_samples*n_bursts;
    /// every burst is made up of cburst_t + nsamps
    uint64_t size = wf_size + bh_size + iq_size;
    // this should allow for the waveform struct to be (wf_h; b_h; b_h; ...; iqdata)
    waveform w = (waveform)malloc(size);
    if(w == NULL) return NULL;
    memset(w, 0, size);
    w->characteristics = characterisitics;
    w->max_burst_length = max_samples;
    w->max_bursts = n_bursts;
    w->bursts = 0;
    w->size = size;
    ////////////////////
    //  w->storage = (cburst)(w+wf_size)
    //  w->storage[idx] ----> cburst_t
    //   == *(w+sizeof(waveform_t)-sizeof(cburst)+idx*sizeof(cburst_t))
    //
    //  w->storage[max_bursts] = (fc32)*(w+wf_size+bh_size) ----> iq[0]
    //
    //
    w->burst_array = (cburst*)(w+wf_size);
    w->iq_array = (fc32*)(w+wf_size+bh_size);
    cburst bptr = (cburst)w->burst_array;
    fc32 *iqptr = w->iq_array;
    for (size_t b_idx = 0; b_idx < n_bursts; b_idx++){
        bptr->ownership = 0;
        bptr->ptr = iqptr;
        bptr->max_samples = max_samples;
        bptr++;
        iqptr += max_samples;
    }
    return w;
}

size_t wf_quick_size(waveform wf, size_t *min, size_t *max){
    if(min != NULL) *min = (size_t)wf;
    if(max != NULL) *max = (size_t)(wf+wf->size);
    size_t len = ((size_t)(wf+wf->size)) - ((size_t)wf);
    return len;
}

uint8_t within(size_t value, size_t min, size_t max){
    return (value >= min) && (value < max);
}

void waveform_destroy(waveform *wf){
    if(wf==NULL) return;
    if(*wf==NULL) return;
    ////// more of a debug thing here
    waveform _wf = *wf;
    size_t w_min, w_max;
    wf_quick_size(_wf, &w_min, &w_max);
    for(size_t idx = 0; idx < _wf->max_bursts; idx++){
        cburst _b = (cburst)&(_wf->burst_array[idx]);
        uint8_t do_something = 0;
        if (_b->ownership){
            /// not a desired setup
            if (within((size_t)_b, w_min, w_max)){
                // this is ok, wf will clear this/// critical fail otherwise
            }
            else do_something |= 0x4;
            if (within((size_t)_b->ptr, w_min, w_max)){
                // this is ok, wf will clear this
            }
            else do_something |= 0x1;
            if (within((size_t)_b->ptr+_b->max_samples, w_min, w_max)){
                // this is ok, wf will clear this
            }
            else do_something |= 0x2;
        }
        if (do_something){
            if (do_something & 0x4){
                printf("..burst is outside of wf memory\n");
            }
            else{
                /////// wf owns the burst
                if ((do_something & 0x3) == 0x3){
                    // iq is out of wf memory space
                    printf("..burst's iq is outside of wf memory\n");
                }
                else{
                    if (do_something & 0x3){
                        printf("..some of the burst's iq is outside of wf memory\n");
                    }
                }
            }
        }
    }
    //////
    free(*wf);
    *wf = NULL;
}


void waveform_add_burst(waveform wf, cburst b){
    if (wf == NULL) return; // not initialized
    if (wf->burst_array == NULL || wf->iq_array == NULL) return; // empty initialized?
    if (b == NULL) return; // not initialzed
    // size_t 
    
}
void waveform_update_burst(waveform wf, uint64_t burst_index,
        uint8_t *src, double *fc, double *fs, double *bw,
        double *ts, uint64_t *max_samples){
    if (wf == NULL) return;
    if (wf->burst_array == NULL) return;
    ////// storage[burst_idx] == wf + sizeof(wf) - sizeof(cburst) + burst_idx*sizeof(cburst_t)
    cburst _b = (cburst)&wf->burst_array[burst_index];
    cburst_update(_b, src, fc, fs, bw, ts, max_samples);
}
void waveform_set_burst_pointer(waveform wf, uint64_t burst_index,
        uint64_t samples, fc32* data){
    if (wf == NULL) return;
    cburst _b = (cburst)&wf->burst_array[burst_index];
    cburst_set_pointer(_b, samples, data);
}
void waveform_set_own_burst(waveform wf, uint64_t burst_index,
        uint64_t samples, fc32* data){
    if (wf == NULL) return;
    cburst _b = (cburst)&wf->burst_array[burst_index];
}
void waveform_compact(waveform *wf){
    if (wf == NULL) return; // a null input
    if (*wf == NULL) return; // pointing to null
    size_t header_size=0, data_size=0;
    size_t total_size=0, current_size=0, index=0;
    waveform _wf = *wf;
    current_size = sizeof(waveform_t) - sizeof(void*); // waveform_t static info
    header_size += current_size;
    total_size += current_size;

    cburst b;
    size_t n_bursts = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = (cburst)_wf->burst_array[index];
        if(b!=NULL){
            if(b->samples > 0){
                //burst is valid
                n_bursts += 1;
                current_size = sizeof(cburst_t); // cburst_t static_info + ptr
                header_size += current_size;
                total_size += current_size;
                current_size = sizeof(fc32)*b->samples; // samples
                data_size += current_size;
                total_size += current_size;
            }
        }
    }
    current_size = n_bursts*sizeof(cburst); // ptr to cburst objects
    total_size += current_size;
    waveform new_wf = (waveform)malloc(total_size);
    uint8_t *compact = (uint8_t*)new_wf;
    current_size = sizeof(waveform_t) - sizeof(void*);
    memcpy( compact, _wf, current_size );
    cburst *burst_ptr = (cburst*)(compact + current_size);
    current_size += n_bursts*sizeof(cburst); // skip burst ptrs
    compact += current_size;
    size_t burst_idx = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = (cburst)_wf->burst_array[index];
        if(b!=NULL){
            if(b->samples > 0){
                current_size = sizeof(cburst_t) - sizeof(fc32*); // cburst_t static_info
                memcpy( compact, b, current_size );
                burst_ptr[burst_idx] = (cburst)compact;
                burst_idx += 1;
                current_size += sizeof(fc32*);
                compact += current_size;
            }
        }
    }
    fc32 *data_ptr = (fc32*)compact;
    burst_idx = 0;
    for(index = 0; index < _wf->max_bursts; index++){
        b = (cburst)_wf->burst_array[index];
        if(b!=NULL){
            if(b->samples > 0){
                burst_ptr[burst_idx]->samples = b->samples;
                burst_ptr[burst_idx]->max_samples = b->samples;
                burst_ptr[burst_idx]->ownership = 1;
                burst_ptr[burst_idx]->ptr = data_ptr;
                burst_idx += 1;
                current_size = sizeof(fc32)*b->samples;
                memcpy( compact, b->ptr, current_size );
                compact += current_size;
                data_ptr = (fc32*)compact;
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
