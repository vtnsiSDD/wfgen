
#include "analog.hh"
#ifdef __cplusplus
#include <cstdlib>
#include <iostream>


//***************************************** WAV FILE HANDLING
//***********************************************************

//***************************************** ANALOG

void print_vec(uint32_t n, float* vec, std::string name){
    std::cout << name << " = [";
    if(n == 0){
        std::cout << "]\n";
    }
    else{
        std::cout << vec[0];
        for(uint32_t idx = 1; idx < n; idx++){
            std::cout << ", " << vec[idx];
        }
        std::cout << "]\n";
    }
}
void print_vec(uint32_t n, double* vec, std::string name){
    std::cout << name << " = [";
    if(n == 0){
        std::cout << "]\n";
    }
    else{
        std::cout << vec[0];
        for(uint32_t idx = 1; idx < n; idx++){
            std::cout << ", " << vec[idx];
        }
        std::cout << "]\n";
    }
}
#else
#include <stdlib.h>
#include <math.h>
#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b;})
typedef unsigned int uint;
#endif

a_src_t str2analog_source_type_(char* type){
    uint8_t str_len = strlen(type);
    a_src_t atype = INVALID_ANALOG;
    if(str_len < 2) return atype;
    switch (type[0]){
        case 'c':
        case 'C':{
            atype = CONSTANT;
            break;
        }
        case 's':
        case 'S':{
            switch (type[1]){
                case 'a':
                case 'A':{
                    atype = SAWTOOTH;
                    break;
                }
                case 'i':
                case 'I':{
                    atype = SINUSOID;
                    break;
                }
                case 'q':
                case 'Q':{
                    atype = SQUARE;
                    break;
                }
                default: break;
            }
            break;
        }
        case 't':
        case 'T':{
            atype = TRIANGLE;
            break;
        }
        default: break;
    }
    return atype;
}

d_src_t str2digital_source_type_(char* type){
    uint8_t str_len = strlen(type);
    d_src_t dtype = INVALID_DIGITAL;
    if(str_len < 3) return dtype;
    switch (type[0]){
        case 'b':
        case 'B':{
            dtype = BYTES;
            break;
        }
        case 'm':
        case 'M':{
            switch (type[2]){
                case 'n':
                case 'N':{
                    dtype = MANCHESTER;
                    break;
                }
                case 's':
                case 'S':{
                    dtype = MASK;
                    break;
                }
                default: break;
            }
            break;
        }
        case 'p':
        case 'P':{
            switch (type[1]){
                case 'd':
                case 'D':{
                    dtype = PDM;
                    break;
                }
                case 'p':
                case 'P':{
                    dtype = PPM;
                    break;
                }
                default: break;
            }
            break;
        }
        default: break;
    }
    return dtype;

}

m_src_t str2message_source_type_(char* type){
    uint8_t str_len = strlen(type);
    m_src_t ctype = INVALID_MESSAGE;
    if(str_len < 8) return ctype;
    switch (type[0]){
        case 'r':
        case 'R':{
            switch (type[8]){//random_x
                case 'a':
                case 'A':{
                    ctype = RANDOM_GAUSS;
                    break;
                }
                case 'n':
                case 'N':{
                    ctype = RANDOM_UNI;
                    break;
                }
                default: break;
            }
            if(ctype == INVALID_MESSAGE){
                switch (type[5]){//rand_x
                    case 'g':
                    case 'G':{
                        ctype = RANDOM_GAUSS;
                        break;
                    }
                    case 'u':
                    case 'U':{
                        ctype = RANDOM_UNI;
                        break;
                    }
                    default: break;
                }
            }
            break;
        }
        case 'w':
        case 'W':{
            ctype = WAV_FILE;
            break;
        }
        default: break;
    }
    return ctype;
}
#ifdef __cplusplus
std::string analog_source_type2str(a_src_t type){
    if(type == CONSTANT){
        return "constant";
    }
    if(type == SQUARE){
        return "square";
    }
    if(type == SAWTOOTH){
        return "sawtooth";
    }
    if(type == TRIANGLE){
        return "triangle";
    }
    if(type == SINUSOID){
        return "sinusoid";
    }
    return "invalid";
}
std::string digital_source_type2str(d_src_t type){
    if(type == BYTES){
        return "bytes";
    }
    if(type == MASK){
        return "mask";
    }
    if(type == MANCHESTER){
        return "manchester";
    }
    if(type == PPM){
        return "ppm";
    }
    if(type == PDM){
        return "pdm";
    }
    return "invalid";
}
std::string message_source_type2str(m_src_t type){
    if(type == RANDOM_GAUSS){
        return "rand_gauss";
    }
    if(type == RANDOM_UNI){
        return "rand_uni";
    }
    if(type == WAV_FILE){
        return "wav";
    }
    return "invalid";
}
#endif
double bound(double v){
    return v-floor(v);
}

double make_square(double offset){ // 0 0.5 = 0; (0,0.5) = 1, (0.5,1.0) = -1
    // offset -> [-inf, inf]
    // theta -> offset%1.0 -> [0,1)
    //lower -> (0.0,0.5) = 1.0
    //upper -> (0.5,1.0) = -1.0
    double theta = bound(offset);
    double lower = ((theta < 0.5) && (theta > 0.0))*1.0;
    double upper = ((theta < 1.0) && (theta > 0.5))*-1.0;
    return lower+upper;
}

double make_triangle(double offset){
    // offset -> [-inf, inf]
    // a -> (offset-0.25)%1.0 -> [-0.25,0.75) -> [0.0,1.0)
    // b -> (2*offset) -> [0,2)%1.0 -> [0,1)
    //       (   a,   b)   (as,bs)
    //       (0.75,0.00) = (-1, 0)  -> 0.00 ->  0
    //       (0.00,0.50) = ( 0, 0)  -> 0.25 ->  1
    //       (0.25,0.00) = ( 1, 0)  -> 0.50 ->  0
    //       (0.50,0.50) = ( 0, 0)  -> 0.75 -> -1
    //         offset
    // 1s -> (0.00,0.25) = (-1, 1)
    // 2s -> (0.25,0.50) = ( 1,-1)
    // 3s -> (0.50,0.75) = ( 1, 1)
    // 4s -> (0.75,1.00) = (-1,-1)
    float a = bound(offset-0.25);
    float b = bound(2*offset);
    int8_t b_s = round(make_square(b));
    int8_t a_s = round(make_square(a));
    if(b_s == 0){
        if(a_s != 0) return 0.0;
        else{
            return 2.0*(a < 0.5)-1.0;
        }
    }
    float slope = -4*a_s;
    //desired (0, 2, 2, -4)
    float intercept = 2*(a_s==1) - 4*(a_s==-1)*(a<0.75);

    return (double)(slope*bound(offset)+intercept);
}
#ifdef __cplusplus
void real_source_print(real_source src, uint8_t indent){
    char* indent_buf = (char*)malloc(indent);
    memset(indent_buf,' ',indent);
    std::string ind(indent_buf,indent);
    free(indent_buf);
    std::cout << ind << "domain   : " << ((int)src->adm_type ? "DIGITAL" : "ANALOG") << std::endl;
    std::cout << ind << "variable : " << ((int)src->sd_type ? "DYNAMIC" : "STATIC") << std::endl;
    if((int)src->adm_type){//DIGITAL
        if (src->src_type == BYTES)
            std::cout << ind << "source   : bytes" << std::endl;
        else if (src->src_type == MASK)
            std::cout << ind << "source   : mask" << std::endl;
        else if (src->src_type == MANCHESTER)
            std::cout << ind << "source   : manchester" << std::endl;
        else if (src->src_type == PPM)
            std::cout << ind << "source   : ppm" << std::endl;
        else if (src->src_type == PDM)
            std::cout << ind << "source   : pdm" << std::endl;
        else
            std::cout << ind << "source   : ????" << std::endl;
    }
    else{
        if (src->src_type == CONSTANT)
            std::cout << ind << "source   : constant" << std::endl;
        else if (src->src_type == SQUARE)
            std::cout << ind << "source   : square" << std::endl;
        else if (src->src_type == SAWTOOTH)
            std::cout << ind << "source   : sawtooth" << std::endl;
        else if (src->src_type == TRIANGLE)
            std::cout << ind << "source   : triangle" << std::endl;
        else if (src->src_type == SINUSOID)
            std::cout << ind << "source   : sinusoid" << std::endl;
        else
            std::cout << ind << "source   : ????" << std::endl;
    }
    std::cout << ind << "itemsize : " << (int)src->itemsize << std::endl;
    std::cout << ind << "channels : " << (int)src->channels << std::endl;
    std::cout << ind << "special  : " << (int)src->special << std::endl;
    std::cout << ind << "source   : " << (uint64_t)src->source << std::endl;
}
void real_path_print(real_path path, uint8_t indent){
    char* indent_buf = (char*)malloc(indent);
    memset(indent_buf,' ',indent);
    std::string ind(indent_buf,indent);
    free(indent_buf);
    std::cout << ind << "input_fs   : " << path->input_fs << std::endl;
    std::cout << ind << "output_fs  : " << path->output_fs << std::endl;
    std::cout << ind << "bandwidth  : " << path->bandwidth << std::endl;
    std::cout << ind << "max_freq   : " << path->max_freq << std::endl;
    std::cout << ind << "baud_rate  : " << path->baud_rate << std::endl;
    std::cout << ind << "path_gain  : " << path->path_gain << std::endl;
    std::cout << ind << "fc_offset  : " << path->fc_offset << std::endl;
    std::cout << ind << "buffer_len : " << cbufferf_max_size(path->buffer) << std::endl;
    real_source_print(path->source, indent+2);
    std::cout << ind << "modulator  : " << (uint64_t)path->modulator << std::endl;
    std::cout << ind << "resampler  : " << (uint64_t)path->resampler << std::endl;
}
#endif
//------------------------------------------------
#ifdef __cplusplus
constant_source constant_source_create(){
#else
constant_source constant_source_create_default(){
#endif
    constant_source src = (constant_source)malloc(sizeof(struct constant_source_s));
    memset(src, 0, sizeof(struct constant_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = CONSTANT;
    src->_amp  = 1.0;
    src->buffer = cbufferf_create(1024);
    return src;
}
constant_source constant_source_create(float amp, uint32_t buffer_len){
    constant_source src = (constant_source)malloc(sizeof(struct constant_source_s));
    memset(src, 0, sizeof(struct constant_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = CONSTANT;
    src->_amp  = amp;
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void constant_source_destroy(constant_source *src){
    if((*src) == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    src = NULL;
}
void constant_source_fill_buffer(constant_source src){
    if(src == NULL) return;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint item = 0; item < to_gen; item++){
        cbufferf_push(src->buffer,src->_amp);
    }
}
void constant_source_reset(constant_source src){
    if(src == NULL) return;
    cbufferf_reset(src->buffer);
}
int constant_source_release(constant_source src, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    uint32_t available = cbufferf_size(src->buffer);
    #ifdef __cplusplus
    len = std::min(len,available);
    #else
    len = min(len,available);
    #endif
    if(actual_len != NULL){
        *actual_len = len;
    }
    return cbufferf_release(src->buffer, len);
}
int constant_source_step(constant_source src, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    if(src->buffer == NULL) return 2;
    if(cbufferf_max_size(src->buffer)==0) *out = src->_amp;
    else{
        constant_source_get_stream(src,out,1);
    }
    return 0;
}
int constant_source_nstep(constant_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    if(src->buffer == NULL) return 2;
    if(cbufferf_max_size(src->buffer)==0){
        for(uint32_t idx = 0; idx < n; idx++){
            out[idx] = src->_amp;
        }
    }
    else{
        constant_source_get_stream(src,out,n);
    }
    return 0;
}
int constant_source_set(constant_source src, double *amp){
    if(src == NULL) return 1;
    if(amp != NULL) src->_amp = *amp;
    return 0;
}
int constant_source_get(constant_source src, double *amp){
    if(src == NULL) return 1;
    if(amp != NULL) *amp = src->_amp;
    return 0;
}
int constant_source_get_stream(constant_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    uint32_t moved = 0;
    uint32_t moving;
    float *ptr;
    while (moved < len){
        cbufferf_read(src->buffer, len-moved, &ptr, &moving);
        memcpy(buf, ptr, moving*sizeof(float));
        cbufferf_release(src->buffer, moving);
        moved += moving;
        constant_source_fill_buffer(src);
    }
    return 0;
}
#ifdef __cplusplus
int constant_source_get_stream_ptr(constant_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int constant_source_get_stream_ptr(constant_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
square_source square_source_create(){
#else
square_source square_source_create_default(){
#endif
    square_source src = (square_source)malloc(sizeof(struct square_source_s));
    memset(src, 0, sizeof(struct square_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SQUARE;
    src->_amp  = 1.0;
    src->_freq = 0.1;
    src->_offset = 0.0;
    src->_incr = src->_freq;
    src->buffer = cbufferf_create(1024);
    return src;
}
square_source square_source_create(float amp, double freq, double period_offset, uint32_t buffer_len){
    square_source src = (square_source)malloc(sizeof(struct square_source_s));
    memset(src, 0, sizeof(struct square_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SQUARE;
    src->_amp  = amp;
    src->_freq = freq;
    src->_offset = period_offset;
    src->_incr = src->_freq;
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void square_source_destroy(square_source *src){
    if((*src) == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    src = NULL;
}
void square_source_fill_buffer(square_source src){
    if(src == NULL) return;
    float value;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint item = 0; item < to_gen; item++){
        square_source_step(src,&value);
        cbufferf_push(src->buffer,value);
    }
}
void square_source_reset(square_source src, double *offset){
    if(src == NULL) return;
    src->_offset = (offset==NULL) ? 0.0 : *offset;
    cbufferf_reset(src->buffer);
}
void square_source_center(square_source src){
    if(src == NULL) return;
    src->_offset = bound(src->_offset);
}
int square_source_release(square_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    uint32_t available = cbufferf_size(src->buffer);
    #ifdef __cplusplus
    len = std::min(len,available);
    #else
    len = min(len,available);
    #endif
    if(actual_len != NULL){
        *actual_len = len;
    }
    return cbufferf_release(src->buffer, len);
}
int square_source_incr(square_source src, double* delta, float* out){
    if(src == NULL) return 1;
    if(out != NULL) *out = src->_amp*make_square(src->_offset);
    src->_offset += ((delta == NULL) ? src->_incr : *delta);
    return 0;
}
int square_source_nincr(square_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return 1;
    if(n == 0) return 0;
    double offset = src->_offset;
    if(out == NULL){
        for(uint idx = 1; idx < n; idx++){
            offset += ((delta == NULL) ? src->_incr : delta[idx]);
        }
    }
    else{
        for(uint idx = 1; idx < n; idx++){
            // out[idx] = src->_amp*(2*make_square(src->_offset + offset)-1);
            out[idx] = src->_amp*make_square(offset);
            offset += ((delta == NULL) ? src->_incr : delta[idx]);
        }
    }
    src->_offset = offset;
    square_source_center(src);
    return 0;
}
int square_source_step(square_source src, float *out){
    if(src == NULL) return 1;
    return square_source_incr(src,(double*)NULL,out);
}
int square_source_nstep(square_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    return square_source_nincr(src,n,(double*)NULL,out);
}
int square_source_set(square_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) src->_amp = *amp;
    if(freq != NULL){
        src->_freq = *freq;
        src->_incr = 1/src->_freq;
    }
    return 0;
}
int square_source_get(square_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) *amp = src->_amp;
    if(freq != NULL) *freq = src->_freq;
    return 0;
}
int square_source_get_stream(square_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    uint32_t moved = 0;
    uint32_t moving;
    float *ptr;
    while (moved < len){
        cbufferf_read(src->buffer, len-moved, &ptr, &moving);
        memcpy(buf, ptr, moving*sizeof(float));
        cbufferf_release(src->buffer, moving);
        moved += moving;
        square_source_fill_buffer(src);
    }
    return 0;
}
#ifdef __cplusplus
int square_source_get_stream_ptr(square_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int square_source_get_stream_ptr(square_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
sawtooth_source sawtooth_source_create(){
#else
sawtooth_source sawtooth_source_create_default(){
#endif
    sawtooth_source src = (sawtooth_source)malloc(sizeof(struct sawtooth_source_s));
    memset(src, 0, sizeof(struct sawtooth_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SAWTOOTH;
    src->_amp  = 2.0;
    src->_freq = 0.1;
    src->_offset = 0.0;
    src->_incr = src->_freq*2;
    src->buffer = cbufferf_create(1024);
    return src;
}
sawtooth_source sawtooth_source_create(float amp, double freq, double period_offset, uint32_t buffer_len){
    sawtooth_source src = (sawtooth_source)malloc(sizeof(struct sawtooth_source_s));
    memset(src, 0, sizeof(struct sawtooth_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SAWTOOTH;
    src->_amp  = 2*amp;
    src->_freq = freq;
    src->_offset = period_offset;
    src->_incr = src->_freq*2;
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void sawtooth_source_destroy(sawtooth_source *src){
    if((*src) == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    src = NULL;
}
void sawtooth_source_fill_buffer(sawtooth_source src){
    if(src == NULL) return;
    float value;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint item = 0; item < to_gen; item++){
        sawtooth_source_step(src,&value);
        cbufferf_push(src->buffer,value);
    }
}
void sawtooth_source_reset(sawtooth_source src, double *offset){
    if(src == NULL) return;
    src->_offset = (offset==NULL) ? 0.0 : *offset;
    cbufferf_reset(src->buffer);
}
void sawtooth_source_center(sawtooth_source src){
    if(src == NULL) return;
    src->_offset = bound(src->_offset);
}
int sawtooth_source_release(sawtooth_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    uint32_t available = cbufferf_size(src->buffer);
    #ifdef __cplusplus
    len = std::min(len,available);
    #else
    len = min(len,available);
    #endif
    if(actual_len != NULL){
        *actual_len = len;
    }
    return cbufferf_release(src->buffer, len);
}
int sawtooth_source_incr(sawtooth_source src, double* delta, float* out){
    if(src == NULL) return 1;
    if(out != NULL) *out = src->_amp*(make_triangle(bound(src->_offset)/4.0)-0.5);
    src->_offset += ((delta == NULL) ? src->_incr : *delta);
    return 0;
}
int sawtooth_source_nincr(sawtooth_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return 1;
    double offset = src->_offset;
    if(out == NULL){
        for(uint idx = 0; idx < n; idx++){
            offset += (delta == NULL) ? src->_incr : delta[idx];
        }
    }
    else{
        for(uint idx = 0; idx < n; idx++){
            out[idx] = src->_amp*(make_triangle(bound(offset)/4.0)-0.5);
            offset += ((delta == NULL) ? src->_incr : delta[idx]);
        }
    }
    src->_offset = offset;
    sawtooth_source_center(src);
    return 0;
}
int sawtooth_source_step(sawtooth_source src, float *out){
    if(src == NULL) return 1;
    return sawtooth_source_incr(src,(double*)NULL,out);
}
int sawtooth_source_nstep(sawtooth_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    return sawtooth_source_nincr(src,n,(double*)NULL,out);
}
int sawtooth_source_set(sawtooth_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) src->_amp = (*amp)*2;
    if(freq != NULL){
        src->_freq = *freq;
        src->_incr = 1/src->_freq;
    }
    return 0;
}
int sawtooth_source_get(sawtooth_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) *amp = (src->_amp/2);
    if(freq != NULL) *freq = src->_freq;
    return 0;
}
int sawtooth_source_get_stream(sawtooth_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    uint32_t moved = 0;
    uint32_t moving;
    float *ptr;
    while (moved < len){
        cbufferf_read(src->buffer, len-moved, &ptr, &moving);
        memcpy(buf, ptr, moving*sizeof(float));
        cbufferf_release(src->buffer, moving);
        moved += moving;
        sawtooth_source_fill_buffer(src);
    }
    return 0;
}
#ifdef __cplusplus
int sawtooth_source_get_stream_ptr(sawtooth_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int sawtooth_source_get_stream_ptr(sawtooth_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
triangle_source triangle_source_create(){
#else
triangle_source triangle_source_create_default(){
#endif
    triangle_source src = (triangle_source)malloc(sizeof(struct triangle_source_s));
    memset(src, 0, sizeof(struct triangle_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = TRIANGLE;
    src->_amp  = 1.0;
    src->_freq = 0.1;
    src->_offset = 0.0;
    src->_incr = src->_freq;
    src->buffer = cbufferf_create(1024);
    return src;
}
triangle_source triangle_source_create(float amp, double freq, double period_offset, uint32_t buffer_len){
    triangle_source src = (triangle_source)malloc(sizeof(struct triangle_source_s));
    memset(src, 0, sizeof(struct triangle_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = TRIANGLE;
    src->_amp  = amp;
    src->_freq = freq;
    src->_offset = period_offset;
    src->_incr = src->_freq;
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void triangle_source_destroy(triangle_source *src){
    if((*src) == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    src = NULL;
}
void triangle_source_fill_buffer(triangle_source src){
    if(src == NULL) return;
    float value;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint item = 0; item < to_gen; item++){
        triangle_source_step(src,&value);
        cbufferf_push(src->buffer,value);
    }
}
void triangle_source_reset(triangle_source src, double *offset){
    if(src == NULL) return;
    src->_offset = (offset==NULL) ? 0.0 : *offset;
    cbufferf_reset(src->buffer);
}
void triangle_source_center(triangle_source src){
    if(src == NULL) return;
    src->_offset = bound(src->_offset);
}
int triangle_source_release(triangle_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    uint32_t available = cbufferf_size(src->buffer);
    #ifdef __cplusplus
    len = std::min(len,available);
    #else
    len = min(len,available);
    #endif
    if(actual_len != NULL){
        *actual_len = len;
    }
    return cbufferf_release(src->buffer, len);
}
int triangle_source_incr(triangle_source src, double* delta, float* out){
    if(src == NULL) return 1;
    if(out != NULL) *out = src->_amp*make_triangle(src->_offset);
    src->_offset += ((delta == NULL) ? src->_incr : *delta);
    return 0;
}
int triangle_source_nincr(triangle_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return 1;
    double offset = src->_offset;
    if(out == NULL){
        for(uint idx = 0; idx < n; idx++){
            offset += (delta == NULL) ? src->_incr : delta[idx];
        }
    }
    else{
        for(uint idx = 0; idx < n; idx++){
            out[idx] = src->_amp*make_triangle(offset);
            offset += ((delta == NULL) ? src->_incr : delta[idx]);
        }
    }
    src->_offset = offset;
    triangle_source_center(src);
    return 0;
}
int triangle_source_step(triangle_source src, float *out){
    if(src == NULL) return 1;
    return triangle_source_incr(src,(double*)NULL,out);
}
int triangle_source_nstep(triangle_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    return triangle_source_nincr(src,n,(double*)NULL,out);
}
int triangle_source_set(triangle_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) src->_amp = *amp;
    if(freq != NULL){
        src->_freq = *freq;
        src->_incr = 1/src->_freq;
    }
    return 0;
}
int triangle_source_get(triangle_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) *amp = src->_amp;
    if(freq != NULL) *freq = src->_freq;
    return 0;
}
int triangle_source_get_stream(triangle_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    uint32_t moved = 0;
    uint32_t moving;
    float *ptr;
    while (moved < len){
        cbufferf_read(src->buffer, len-moved, &ptr, &moving);
        memcpy(buf, ptr, moving*sizeof(float));
        cbufferf_release(src->buffer, moving);
        moved += moving;
        triangle_source_fill_buffer(src);
    }
    return 0;
}
#ifdef __cplusplus
int triangle_source_get_stream_ptr(triangle_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int triangle_source_get_stream_ptr(triangle_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
sinusoid_source sinusoid_source_create(){
#else
sinusoid_source sinusoid_source_create_default(){
#endif
    sinusoid_source src = (sinusoid_source)malloc(sizeof(struct sinusoid_source_s));
    memset(src, 0, sizeof(struct sinusoid_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SINUSOID;
    src->_2pi = 2*M_PI;
    src->_amp  = 1.0;
    src->_freq = 0.1;
    src->_phi  = 0.0;
    src->_incr = src->_2pi*src->_freq;
    src->buffer = cbufferf_create(1024);
    return src;
}
sinusoid_source sinusoid_source_create(float amp, double freq, double phi, uint32_t buffer_len){
    sinusoid_source src = (sinusoid_source)malloc(sizeof(struct sinusoid_source_s));
    memset(src, 0, sizeof(struct sinusoid_source_s));
    if(src == NULL) return src;
    src->_adm = ANALOG;
    src->_sd = STATIC;
    src->_src = SINUSOID;
    src->_2pi = 2*M_PI;
    src->_amp  = amp;
    src->_freq = freq;
    src->_phi  = phi;
    src->_incr = src->_2pi*src->_freq;
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void sinusoid_source_destroy(sinusoid_source *src){
    if((*src) == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    src = NULL;
}
void sinusoid_source_fill_buffer(sinusoid_source src){
    if(src == NULL) return;
    float value;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint item = 0; item < to_gen; item++){
        sinusoid_source_step(src,&value);
        cbufferf_push(src->buffer,value);
    }
}
void sinusoid_source_reset(sinusoid_source src, double *phi){
    if(src == NULL) return;
    src->_phi = (phi==NULL) ? 0.0 : *phi;
    cbufferf_reset(src->buffer);
}
void sinusoid_source_center(sinusoid_source src){
    if(src == NULL) return;
    src->_phi = fmod(src->_phi, src->_2pi);
    if(src->_phi < -M_PI) src->_phi += src->_2pi;
    if(src->_phi >= M_PI) src->_phi -= src->_2pi;
}
int sinusoid_source_release(sinusoid_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    uint32_t available = cbufferf_size(src->buffer);
    #ifdef __cplusplus
    len = std::min(len,available);
    #else
    len = min(len,available);
    #endif
    if(actual_len != NULL){
        *actual_len = len;
    }
    return cbufferf_release(src->buffer, len);
}
int sinusoid_source_incr(sinusoid_source src, double* delta, float* out){
    if(src == NULL) return 1;
    if(out != NULL) *out = src->_amp*sinf(src->_phi);
    src->_phi += ((delta == NULL) ? src->_incr : (*delta));
    return 0;
}
int sinusoid_source_nincr(sinusoid_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return 1;
    double phi = src->_phi;
    if(out == NULL){
        for(uint idx = 0; idx < n; idx++){
            phi += (delta == NULL) ? src->_incr : delta[idx];
        }
    }
    else{
        for(uint idx = 0; idx < n; idx++){
            out[idx] = src->_amp*sinf(phi);
            phi += ((delta == NULL) ? src->_incr : delta[idx]);
        }
    }
    src->_phi = phi;
    sinusoid_source_center(src);
    return 0;
}
int sinusoid_source_step(sinusoid_source src, float *out){
    if(src == NULL) return 1;
    return sinusoid_source_incr(src,(double*)NULL,out);
}
int sinusoid_source_nstep(sinusoid_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    return sinusoid_source_nincr(src,n,(double*)NULL,out);
}
int sinusoid_source_set(sinusoid_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) src->_amp = *amp;
    if(freq != NULL){
        src->_freq = *freq;
        src->_incr = src->_2pi*src->_freq;
    }
    return 0;
}
int sinusoid_source_get(sinusoid_source src, double *amp, double *freq){
    if(src == NULL) return 1;
    if(amp != NULL) *amp = src->_amp;
    if(freq != NULL) *freq = src->_freq;
    return 0;
}
int sinusoid_source_get_stream(sinusoid_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    uint32_t moved = 0;
    uint32_t moving;
    float *ptr;
    while (moved < len){
        cbufferf_read(src->buffer, len-moved, &ptr, &moving);
        memcpy(buf, ptr, moving*sizeof(float));
        cbufferf_release(src->buffer, moving);
        moved += moving;
        sinusoid_source_fill_buffer(src);
    }
    return 0;
}
#ifdef __cplusplus
int sinusoid_source_get_stream_ptr(sinusoid_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int sinusoid_source_get_stream_ptr(sinusoid_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
rand_gauss_source rand_gauss_source_create(){
#else
rand_gauss_source rand_gauss_source_create_default(){
#endif
    rand_gauss_source src = (rand_gauss_source)malloc(sizeof(struct rand_gauss_source_s));
    memset(src, 0, sizeof(struct rand_gauss_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = RANDOM_GAUSS;
    src->_mean = 0.;
    src->_stdd = 0.7071067811865476;
    #ifdef __cplusplus
    std::random_device rd;
    src->_rgen = new std::mt19937_64(rd());
    src->_dist = new std::normal_distribution<float>(src->_mean, src->_stdd);
    // #else
    #endif
    // src->_gen = make_guass_gen(src->_rgen,src->_dist);
    src->buffer = cbufferf_create(1024);
    return src;
}
rand_gauss_source rand_gauss_source_create(float mean, float stdd, uint32_t buffer_len){
    rand_gauss_source src = (rand_gauss_source) malloc(sizeof(struct rand_gauss_source_s));
    memset(src, 0, sizeof(struct rand_gauss_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = RANDOM_GAUSS;
    src->_mean = mean;
    src->_stdd = stdd;
    #ifdef __cplusplus
    std::random_device rd;
    src->_rgen = new std::mt19937_64(rd());
    src->_dist = new std::normal_distribution<float>(src->_mean, src->_stdd);
    #else
    src->_rgen = NULL;
    src->_dist = NULL;
    #endif
    // src->_gen = make_guass_gen(src->_rgen,src->_dist);
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void rand_gauss_source_destroy(rand_gauss_source *src){
    if(src == NULL) return;
    if(*src == NULL) { src = NULL; return; }
    #ifdef __cplusplus
    if((*src)->_rgen != NULL){ delete (*src)->_rgen; (*src)->_rgen = NULL; }
    if((*src)->_dist != NULL){ delete (*src)->_dist; (*src)->_dist = NULL; }
    #else
    if((*src)->_rgen != NULL){ free((*src)->_rgen); (*src)->_rgen = NULL; }
    if((*src)->_dist != NULL){ free((*src)->_dist); (*src)->_dist = NULL; }
    #endif
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    *src = NULL;
}
void rand_gauss_source_fill_buffer(rand_gauss_source src){
    if(src == NULL) return;
    uint to_gen = cbufferf_space_available(src->buffer);
    for(uint idx = 0; idx < to_gen; idx++){
        // cbufferf_push(src->buffer,src->_gen());
        // cbufferf_push(src->buffer,0.0f);
        #ifdef __cplusplus
        cbufferf_push(src->buffer,(*src->_dist)(*src->_rgen));
        #else
        cbufferf_push(src->buffer,0);
        #endif
    }
}
void rand_gauss_source_reset(rand_gauss_source src){
    if(src == NULL) return;
    cbufferf_reset(src->buffer);
}
int rand_gauss_source_release(rand_gauss_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    #ifdef __cplusplus
    len = std::min(len,cbufferf_size(src->buffer));
    #else
    len = min(len,cbufferf_size(src->buffer));
    #endif
    if(actual_len != NULL) *actual_len = len;
    return cbufferf_release(src->buffer, len);
}
int rand_gauss_source_step(rand_gauss_source src, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    if(cbufferf_size(src->buffer) == 0) rand_gauss_source_fill_buffer(src);
    return cbufferf_pop(src->buffer, out); 
}
int rand_gauss_source_nstep(rand_gauss_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    uint32_t loaded = 0, loading = 0;
    float *ptr;
    while(loaded < n){
        #ifdef __cplusplus
        cbufferf_read(src->buffer, std::min(n,n-loaded), &ptr, &loading);
        #else
        cbufferf_read(src->buffer, min(n,n-loaded), &ptr, &loading);
        #endif
        memcpy(&out[loaded], ptr, loading*sizeof(float));
        cbufferf_release(src->buffer, loading);
        rand_gauss_source_fill_buffer(src);
        loaded += loading;
    }
    return 0;
}
int rand_gauss_source_set(rand_gauss_source src, double *mean, double *stdd){
    if(src == NULL) return 1;
    if((mean == NULL) && (stdd == NULL)) return 0;
    if(mean != NULL) src->_mean = *mean;
    if(stdd != NULL) src->_stdd = *stdd;
    #ifdef __cpluscplus
    delete src->_dist;
    src->_dist = new std::normal_distribution<float>(src->_mean, src->_stdd);
    #else
    src->_dist = NULL;
    #endif
    // src->_gen = make_guass_gen(src->_rgen,src->_dist);
    return 0;
}
int rand_gauss_source_get(rand_gauss_source src, double *mean, double *stdd){
    if(src == NULL) return 1;
    if(mean != NULL) *mean = src->_mean;
    if(stdd != NULL) *stdd = src->_stdd;
    return 0;
}
int rand_gauss_source_get_stream(rand_gauss_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    return rand_gauss_source_nstep(src, len, buf);
}
#ifdef __cplusplus
int rand_gauss_source_get_stream_ptr(rand_gauss_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int rand_gauss_source_get_stream_ptr(rand_gauss_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
rand_uni_source rand_uni_source_create(){
#else
rand_uni_source rand_uni_source_create_default(){
#endif
    rand_uni_source src = (rand_uni_source)malloc(sizeof(struct rand_uni_source_s));
    memset(src, 0, sizeof(struct rand_uni_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = RANDOM_UNI;
    src->_mini = -1.;
    src->_maxi = 1.;
    #ifdef __cplusplus
    std::random_device rd;
    src->_rgen = new std::mt19937_64(rd());
    src->_dist = new std::uniform_real_distribution<float>(-1,1);
    #else
    src->_rgen = NULL;
    src->_dist = NULL;
    #endif
    // src->_gen = make_uni_gen(src->_rgen,src->_dist);
    src->buffer = cbufferf_create(1024);
    return src;
}
rand_uni_source rand_uni_source_create(float mini, float maxi, uint32_t buffer_len){
    rand_uni_source src = (rand_uni_source) malloc(sizeof(struct rand_uni_source_s));
    memset(src, 0, sizeof(struct rand_uni_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = RANDOM_UNI;
    src->_mini = mini;
    src->_maxi = maxi;
    #ifdef __cplusplus
    std::random_device rd;
    src->_rgen = new std::mt19937_64(rd());
    src->_dist = new std::uniform_real_distribution<float>(src->_mini, src->_maxi);
    #else
    src->_rgen = NULL;
    src->_dist = NULL;
    #endif
    // src->_gen = make_uni_gen(src->_rgen,src->_dist);
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void rand_uni_source_destroy(rand_uni_source *src){
    if(src == NULL) return;
    if(*src == NULL) { src = NULL; return; }
    #ifdef __cplusplus
    if((*src)->_rgen != NULL){ delete (*src)->_rgen; (*src)->_rgen = NULL; }
    if((*src)->_dist != NULL){ delete (*src)->_dist; (*src)->_dist = NULL; }
    #else
    if((*src)->_rgen != NULL){ free((*src)->_rgen); (*src)->_rgen = NULL; }
    if((*src)->_dist != NULL){ free((*src)->_dist); (*src)->_dist = NULL; }
    #endif
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    *src = NULL;
}
void rand_uni_source_fill_buffer(rand_uni_source src){
    if(src == NULL) return;
    uint to_gen = cbufferf_space_available(src->buffer);
    // std::mt19937_64 gen = *src->_rgen;
    // std::uniform_real_distribution<float> dist = *src->_dist;
    for(uint idx = 0; idx < to_gen; idx++){
        // cbufferf_push(src->buffer,src->_gen());
        // cbufferf_push(src->buffer,0.0f);
        #ifdef __cplusplus
        cbufferf_push(src->buffer,(*src->_dist)(*src->_rgen));
        #else
        cbufferf_push(src->buffer,0);
        #endif
    }
}
void rand_uni_source_reset(rand_uni_source src){
    if(src == NULL) return;
    cbufferf_reset(src->buffer);
}
int rand_uni_source_release(rand_uni_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    #ifdef __cplusplus
    len = std::min(len,cbufferf_size(src->buffer));
    #else
    len = min(len,cbufferf_size(src->buffer));
    #endif
    if(actual_len != NULL) *actual_len = len;
    return cbufferf_release(src->buffer, len);
}
int rand_uni_source_step(rand_uni_source src, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    if(cbufferf_size(src->buffer) == 0) rand_uni_source_fill_buffer(src);
    return cbufferf_pop(src->buffer, out);
}
int rand_uni_source_nstep(rand_uni_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    uint32_t loaded = 0, loading = 0;
    float *ptr;
    while(loaded < n){
        #ifdef __cplusplus
        cbufferf_read(src->buffer, std::min(n,n-loaded), &ptr, &loading);
        #else
        cbufferf_read(src->buffer, min(n,n-loaded), &ptr, &loading);
        #endif
        memcpy(&out[loaded], ptr, loading*sizeof(float));
        cbufferf_release(src->buffer, loading);
        rand_uni_source_fill_buffer(src);
        loaded += loading;
    }
    return 0;
}
int rand_uni_source_set(rand_uni_source src, double *mini, double *maxi){
    if(src == NULL) return 1;
    if((mini == NULL) && (maxi == NULL)) return 0;
    if(mini != NULL) src->_mini = *mini;
    if(maxi != NULL) src->_maxi = *maxi;
    #ifdef __cplusplus
    delete src->_dist;
    src->_dist = new std::uniform_real_distribution<float>(src->_mini, src->_maxi);
    #else
    src->_dist = NULL;
    #endif
    // src->_gen = make_uni_gen(src->_rgen,src->_dist);
    return 0;
}
int rand_uni_source_get(rand_uni_source src, double *mini, double *maxi){
    if(src == NULL) return 1;
    if(mini != NULL) *mini = src->_mini;
    if(maxi != NULL) *maxi = src->_maxi;
    return 0;
}
int rand_uni_source_get_stream(rand_uni_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    return rand_uni_source_nstep(src, len, buf);
}
#ifdef __cplusplus
int rand_uni_source_get_stream_ptr(rand_uni_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int rand_uni_source_get_stream_ptr(rand_uni_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
wav_source wav_source_create(){
#else
wav_source wav_source_create_default(){
#endif
    wav_source src = (wav_source)malloc(sizeof(struct wav_source_s));
    memset(src, 0, sizeof(struct wav_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = WAV_FILE;
    src->_special = MONO;
    #ifdef __cplusplus
    src->wav_r = wav_reader_create();//WAV READER SHOULD HAVE SPECIAL MODE == ALL ||| so two floats per sample
    #else
    src->wav_r = wav_reader_create_default();//WAV READER SHOULD HAVE SPECIAL MODE == ALL ||| so two floats per sample
    #endif
    // src->_gen = make_mono(src->wav_r);
    src->buffer = cbufferf_create(1024);
    return src;
}

float wav_source_get_sample(wav_source src){
    float value[2];
    if(cbufferf_max_read(src->wav_r->buffer) == 0) wav_reader_fill_buffer(src->wav_r);
    cbufferf_pop(src->wav_r->buffer, &value[0]);// releases 1 float
    cbufferf_pop(src->wav_r->buffer, &value[1]);// releases 1 float
    if(src->_special == LEFT) return value[0];
    else if(src->_special == RIGHT) return value[1];
    else if(src->_special == STEREO) return (value[0]-value[1])/2.0;
    else return (value[0]+value[1])/2.0;
}
size_t wav_source_get_nsample(wav_source src){
    float *reader_ptr = NULL;
    size_t avail = wav_reader_data_ptr(src->wav_r, reader_ptr);// number of available samples
    // reader_ptr now points to valid audio samples (L,R)->(float,float)
    ///// goal is to load n samples into the wav_source->buffer

    #ifdef __cplusplus
    avail = std::min(avail, (size_t)cbufferf_space_available(src->buffer));// only digest the smaller number of samples
    #else
    avail = min(avail, (size_t)cbufferf_space_available(src->buffer));
    #endif

    float value; // just assuming only 1 float now
    for(size_t idx = 0; idx < avail; idx++){
        if(src->_special == LEFT) value = reader_ptr[2*idx+0];
        else if(src->_special == RIGHT) value = reader_ptr[2*idx+1];
        else if(src->_special == STEREO) value = (reader_ptr[2*idx+0]-reader_ptr[2*idx+1])/2.0;
        else value = (reader_ptr[2*idx+0]+reader_ptr[2*idx+1])/2.0;
        cbufferf_push(src->buffer, value);
    }
    wav_reader_advance(src->wav_r, avail);
    return avail;
}
wav_source wav_source_create(wav_mode_t mode, wav_reader reader, uint32_t buffer_len){
    wav_source src = (wav_source) malloc(sizeof(struct wav_source_s));
    memset(src, 0, sizeof(struct wav_source_s));
    if(src == NULL) return src;
    src->_adm = MESSAGE;
    src->_sd = STATIC;
    src->_src = WAV_FILE;
    src->_special = mode;
    src->wav_r = reader;
    // if(mode == LEFT) src->_gen = make_left(src->wav_r);            // assumes the source is in stereo format...
    // else if(mode == RIGHT) src->_gen = make_right(src->wav_r);     // assumes the source is in stereo format...
    // else if(mode == MONO) src->_gen = make_mono(src->wav_r);       // assumes the source is in stereo format...
    // else if(mode == STEREO) src->_gen = make_stereo(src->wav_r);   // assumes the source is in stereo format...
    // else src->_gen = make_mono(src->wav_r);                        // assumes the source is in stereo format...
    if(buffer_len == 0){
        src->buffer = cbufferf_create_max(0,1);
    }
    else{
        src->buffer = cbufferf_create(buffer_len);
    }
    return src;
}
void wav_source_destroy(wav_source *src){
    if(src == NULL) return;
    if(*src == NULL) { src = NULL; return; }
    wav_reader_destroy(&((*src)->wav_r));
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
    *src = NULL;
}
void wav_source_fill_buffer(wav_source src){
    if(src == NULL) return;
    uint to_gen = cbufferf_space_available(src->buffer);
    size_t gend;
    while( to_gen > 0){
        gend = wav_source_get_nsample(src);//number of samples loaded into wav_source
        to_gen -= gend;//number to still load
    }
}
void wav_source_reset(wav_source src){
    if(src == NULL) return;
    cbufferf_reset(src->buffer);
}
int p(wav_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return 1;
    #ifdef __cplusplus
    len = std::min(len,cbufferf_size(src->buffer));
    #else
    len = min(len,cbufferf_size(src->buffer));
    #endif
    if(actual_len != NULL) *actual_len = len;
    return cbufferf_release(src->buffer, len);
}
int wav_source_step(wav_source src, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    if(cbufferf_size(src->buffer) == 0) wav_source_fill_buffer(src);
    return cbufferf_pop(src->buffer, out);
}
int wav_source_nstep(wav_source src, uint32_t n, float *out){
    if(src == NULL) return 1;
    if(out == NULL) return 0;
    uint32_t loaded = 0, loading = 0;
    float *ptr;
    while(loaded < n){
        cbufferf_read(src->buffer, n, &ptr, &loading);
        memcpy(out, ptr, loading*sizeof(float));
        cbufferf_release(src->buffer, loading);
        wav_source_fill_buffer(src);
        loaded += loading;
    }
    return 0;
}
int wav_source_set(wav_source src, wav_reader reader){
    if(src == NULL) return 1;
    if(reader != NULL){
        cbufferf_reset(src->buffer);
        wav_reader_destroy(&(src->wav_r));
        src->wav_r = reader;
        wav_source_fill_buffer(src);
    }
    return 0;
}
int wav_source_get(wav_source src, wav_reader reader){
    if(src == NULL) return 1;
    reader = src->wav_r;
    return 0;
}
int wav_source_get_stream(wav_source src, float *buf, uint32_t len){
    if(src == NULL) return 1;
    return wav_source_nstep(src, len, buf);
}
#ifdef __cplusplus
int wav_source_get_stream_ptr(wav_source src, float *ptr, uint32_t len, uint32_t &actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, &actual_len);
}
#else
int wav_source_get_stream_ptr(wav_source src, float *ptr, uint32_t len, uint32_t *actual_len){
    if(src == NULL) return 1;
    return cbufferf_read(src->buffer, len, &ptr, actual_len);
}
#endif

//------------------------------------------------
#ifdef __cplusplus
bytes_source bytes_source_create(){
#else
bytes_source bytes_source_create_default(){
#endif
    bytes_source src = (bytes_source) malloc(sizeof(struct bytes_source_s));
    memset(src, 0, sizeof(struct bytes_source_s));
    if(src == NULL) return NULL;
    src->_adm = DIGITAL;
    src->_sd = STATIC;
    src->_src = BYTES;
    src->buffer = cbufferf_create(1024);
    return src;
}
void bytes_source_destroy(bytes_source *src){
    if(src == NULL) return;
    if(*src == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
}
void bytes_source_fill_buffer(bytes_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void bytes_source_reset(bytes_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void bytes_source_center(bytes_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
int bytes_source_release(bytes_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_incr(bytes_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_nincr(bytes_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_step(bytes_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_nstep(bytes_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_set(bytes_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_get(bytes_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int bytes_source_get_stream(bytes_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
int bytes_source_get_stream_ptr(bytes_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int bytes_source_get_stream_ptr(bytes_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
mask_source mask_source_create(){
#else
mask_source mask_source_create_default(){
#endif
    mask_source src = (mask_source) malloc(sizeof(struct mask_source_s));
    memset(src, 0, sizeof(struct mask_source_s));
    if(src == NULL) return NULL;
    src->_adm = DIGITAL;
    src->_sd = STATIC;
    src->_src = MASK;
    src->buffer = cbufferf_create(1024);
    return src;
}
void mask_source_destroy(mask_source *src){
    if(src == NULL) return;
    if(*src == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
}
void mask_source_fill_buffer(mask_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void mask_source_reset(mask_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void mask_source_center(mask_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
int mask_source_release(mask_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_incr(mask_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_nincr(mask_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_step(mask_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_nstep(mask_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_set(mask_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_get(mask_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int mask_source_get_stream(mask_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
int mask_source_get_stream_ptr(mask_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int mask_source_get_stream_ptr(mask_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
manchester_source manchester_source_create(){
#else
manchester_source manchester_source_create_default(){
#endif
    manchester_source src = (manchester_source) malloc(sizeof(struct manchester_source_s));
    memset(src, 0, sizeof(struct manchester_source_s));
    if(src == NULL) return NULL;
    src->_adm = DIGITAL;
    src->_sd = STATIC;
    src->_src = MANCHESTER;
    src->buffer = cbufferf_create(1024);
    return src;
}
void manchester_source_destroy(manchester_source *src){
    if(src == NULL) return;
    if(*src == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
}
void manchester_source_fill_buffer(manchester_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void manchester_source_reset(manchester_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void manchester_source_center(manchester_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
int manchester_source_release(manchester_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_incr(manchester_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_nincr(manchester_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_step(manchester_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_nstep(manchester_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_set(manchester_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_get(manchester_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int manchester_source_get_stream(manchester_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
int manchester_source_get_stream_ptr(manchester_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int manchester_source_get_stream_ptr(manchester_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
ppm_source ppm_source_create(){
#else
ppm_source ppm_source_create_default(){
#endif
    ppm_source src = (ppm_source) malloc(sizeof(struct ppm_source_s));
    memset(src, 0, sizeof(struct ppm_source_s));
    if(src == NULL) return NULL;
    src->_adm = DIGITAL;
    src->_sd = STATIC;
    src->_src = PPM;
    src->buffer = cbufferf_create(1024);
    return src;
}
void ppm_source_destroy(ppm_source *src){
    if(src == NULL) return;
    if(*src == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
}
void ppm_source_fill_buffer(ppm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void ppm_source_reset(ppm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void ppm_source_center(ppm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
int ppm_source_release(ppm_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_incr(ppm_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_nincr(ppm_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_step(ppm_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_nstep(ppm_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_set(ppm_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_get(ppm_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int ppm_source_get_stream(ppm_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
int ppm_source_get_stream_ptr(ppm_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int ppm_source_get_stream_ptr(ppm_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
pdm_source pdm_source_create(){
#else
pdm_source pdm_source_create_default(){
#endif
    pdm_source src = (pdm_source) malloc(sizeof(struct pdm_source_s));
    memset(src, 0, sizeof(struct pdm_source_s));
    if(src == NULL) return NULL;
    src->_adm = DIGITAL;
    src->_sd = STATIC;
    src->_src = PDM;
    src->buffer = cbufferf_create(1024);
    return src;
}
void pdm_source_destroy(pdm_source *src){
    if(src == NULL) return;
    if(*src == NULL) return;
    if((*src)->buffer != NULL) cbufferf_destroy((*src)->buffer);
    free((*src));
}
void pdm_source_fill_buffer(pdm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void pdm_source_reset(pdm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
void pdm_source_center(pdm_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
}
int pdm_source_release(pdm_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_incr(pdm_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_nincr(pdm_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_step(pdm_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_nstep(pdm_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_set(pdm_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_get(pdm_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
int pdm_source_get_stream(pdm_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}
#ifdef __cplusplus
int pdm_source_get_stream_ptr(pdm_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int pdm_source_get_stream_ptr(pdm_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    return -3;
}

//------------------------------------------------
#ifdef __cplusplus
real_source real_source_create(){
#else
real_source real_source_create_default(){
#endif
    real_source src = (real_source)malloc(sizeof(struct real_source_s));
    memset(src, 0, sizeof(struct real_source_s));
    if (src == NULL) return src;
    src->adm_type = ANALOG;
    src->sd_type = STATIC;
    src->src_type = SINUSOID;
    src->itemsize = sizeof(float);
    src->channels = 1;
    src->special = NONE;
    #ifdef __cplusplus
    src->source = (void*)sinusoid_source_create();
    #else
    src->source = (void*)sinusoid_source_create_default();
    #endif
    return src;
}
real_source real_source_create(real_path_t mode, real_param_t vari, uint8_t src_type,
                                uint8_t itemsize, uint8_t channels, wav_mode_t special){
    real_source src = (real_source)malloc(sizeof(struct real_source_s));
    memset(src, 0, sizeof(struct real_source_s));
    if (src == NULL) return src;
    src->adm_type = mode;
    src->sd_type = vari;
    src->src_type = src_type;
    src->itemsize = itemsize;
    src->channels = channels;
    src->special = special;
    if(mode == ANALOG){
        if(src_type == CONSTANT)
            #ifdef __cplusplus
            src->source = (void*) constant_source_create();
            #else
            src->source = (void*) constant_source_create_default();
            #endif
        else if(src_type == SQUARE)
            #ifdef __cplusplus
            src->source = (void*) square_source_create();
            #else
            src->source = (void*) square_source_create_default();
            #endif
        else if(src_type == SAWTOOTH)
            #ifdef __cplusplus
            src->source = (void*) sawtooth_source_create();
            #else
            src->source = (void*) sawtooth_source_create_default();
            #endif
        else if(src_type == TRIANGLE)
            #ifdef __cplusplus
            src->source = (void*) triangle_source_create();
            #else
            src->source = (void*) triangle_source_create_default();
            #endif
        else if(src_type == SINUSOID)
            #ifdef __cplusplus
            src->source = (void*) sinusoid_source_create();
            #else
            src->source = (void*) sinusoid_source_create_default();
            #endif
    }
    else if(src->adm_type == MESSAGE){
        if(src_type == RANDOM_GAUSS){
            #ifdef __cplusplus
            src->source = (void*) rand_gauss_source_create();
            #else
            src->source = (void*) rand_gauss_source_create_default();
            #endif
        }
        else if(src_type == RANDOM_UNI){
            #ifdef __cplusplus
            src->source = (void*) rand_uni_source_create();
            #else
            src->source = (void*) rand_uni_source_create_default();
            #endif
        }
        else if(src_type == WAV_FILE){
            #ifdef __cplusplus
            src->source = (void*) wav_source_create();
            #else
            src->source = (void*) wav_source_create_default();
            #endif
        }
    }
    else{
        if(src_type == BYTES)
            #ifdef __cplusplus
            src->source = (void*) bytes_source_create();
            #else
            src->source = (void*) bytes_source_create_default();
            #endif
        else if(src_type == MASK)
            #ifdef __cplusplus
            src->source = (void*) mask_source_create();
            #else
            src->source = (void*) mask_source_create_default();
            #endif
        else if(src_type == MANCHESTER)
            #ifdef __cplusplus
            src->source = (void*) manchester_source_create();
            #else
            src->source = (void*) manchester_source_create_default();
            #endif
        else if(src_type == PPM)
            #ifdef __cplusplus
            src->source = (void*) ppm_source_create();
            #else
            src->source = (void*) ppm_source_create_default();
            #endif
        else if(src_type == PDM)
            #ifdef __cplusplus
            src->source = (void*) pdm_source_create();
            #else
            src->source = (void*) pdm_source_create_default();
            #endif
    }
    return src;
}
#ifdef __cplusplus
real_source real_source_create(void* source, uint8_t itemsize, uint8_t channels, wav_mode_t special){
#else
real_source real_source_create_from_source(void* source, uint8_t itemsize, uint8_t channels, wav_mode_t special){
#endif
    real_source src = (real_source)malloc(sizeof(struct real_source_s));
    memset(src, 0, sizeof(struct real_source_s));
    if (src == NULL) return src;
    src->adm_type = (real_path_t)((uint8_t *)source)[0];
    src->sd_type = ((uint8_t *)source)[1];
    src->src_type = ((uint8_t *)source)[2];
    src->itemsize = itemsize;
    src->channels = channels;
    src->special = special;
    src->source = source;
    return src;
}
void real_source_destroy(real_source *src){
    if (src == NULL) return;
    if (*src == NULL) return;
    if ((*src)->source != NULL){
        if ((*src)->adm_type == ANALOG){
            if ((*src)->src_type == CONSTANT){
                constant_source ptr = (constant_source)(*src)->source;
                constant_source_destroy(&ptr);
            }
            if ((*src)->src_type == SQUARE){
                square_source ptr = (square_source)(*src)->source;
                square_source_destroy(&ptr);
            }
            if ((*src)->src_type == SAWTOOTH){
                sawtooth_source ptr = (sawtooth_source)(*src)->source;
                sawtooth_source_destroy(&ptr);
            }
            if ((*src)->src_type == TRIANGLE){
                triangle_source ptr = (triangle_source)(*src)->source;
                triangle_source_destroy(&ptr);
            }
            if ((*src)->src_type == SINUSOID){
                sinusoid_source ptr = (sinusoid_source)(*src)->source;
                sinusoid_source_destroy(&ptr);
            }
        }
        else if((*src)->adm_type == MESSAGE){
            if((*src)->src_type == RANDOM_GAUSS){
                rand_gauss_source ptr = (rand_gauss_source)(*src)->source;
                rand_gauss_source_destroy(&ptr);
            }
            else if((*src)->src_type == RANDOM_UNI){
                rand_uni_source ptr = (rand_uni_source)(*src)->source;
                rand_uni_source_destroy(&ptr);
            }
            else if((*src)->src_type == WAV_FILE){
                wav_source ptr = (wav_source)(*src)->source;
                wav_source_destroy(&ptr);
            }
        }
        else{
            if((*src)->src_type == BYTES){
                bytes_source ptr = (bytes_source)(*src)->source;
                bytes_source_destroy(&ptr);
            }
            if((*src)->src_type == MASK){
                mask_source ptr = (mask_source)(*src)->source;
                mask_source_destroy(&ptr);
            }
            if((*src)->src_type == MANCHESTER){
                manchester_source ptr = (manchester_source)(*src)->source;
                manchester_source_destroy(&ptr);
            }
            if((*src)->src_type == PPM){
                ppm_source ptr = (ppm_source)(*src)->source;
                ppm_source_destroy(&ptr);
            }
            if((*src)->src_type == PDM){
                pdm_source ptr = (pdm_source)(*src)->source;
                pdm_source_destroy(&ptr);
            }
        }
    }
    free((*src));
}
void real_source_fill_buffer(real_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            constant_source_fill_buffer((constant_source)src->source);
        else if(src->src_type == SQUARE)
            square_source_fill_buffer((square_source)src->source);
        else if(src->src_type == SAWTOOTH)
            sawtooth_source_fill_buffer((sawtooth_source)src->source);
        else if(src->src_type == TRIANGLE)
            triangle_source_fill_buffer((triangle_source)src->source);
        else if(src->src_type == SINUSOID)
            sinusoid_source_fill_buffer((sinusoid_source)src->source);
    }
    else if(src->adm_type == MESSAGE){
        if(src->src_type == RANDOM_GAUSS)
            rand_gauss_source_fill_buffer((rand_gauss_source)src->source);
        if(src->src_type == RANDOM_UNI)
            rand_uni_source_fill_buffer((rand_uni_source)src->source);
        if(src->src_type == WAV_FILE)
            wav_source_fill_buffer((wav_source)src->source);
    }
    else{
        if(src->src_type == BYTES)
            bytes_source_fill_buffer((bytes_source)src->source);
        else if(src->src_type == MASK)
            mask_source_fill_buffer((mask_source)src->source);
        else if(src->src_type == MANCHESTER)
            manchester_source_fill_buffer((manchester_source)src->source);
        else if(src->src_type == PPM)
            ppm_source_fill_buffer((ppm_source)src->source);
        else if(src->src_type == PDM)
            pdm_source_fill_buffer((pdm_source)src->source);
    }
}
void real_source_reset(real_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            constant_source_reset((constant_source)src->source);
        else if(src->src_type == SQUARE)
            square_source_reset((square_source)src->source,NULL);
        else if(src->src_type == SAWTOOTH)
            sawtooth_source_reset((sawtooth_source)src->source,NULL);
        else if(src->src_type == TRIANGLE)
            triangle_source_reset((triangle_source)src->source,NULL);
        else if(src->src_type == SINUSOID)
            sinusoid_source_reset((sinusoid_source)src->source,NULL);
    }
    else if(src->adm_type == MESSAGE){
    }
    else{
        if(src->src_type == BYTES)
            bytes_source_reset((bytes_source)src->source);
        else if(src->src_type == MASK)
            mask_source_reset((mask_source)src->source);
        else if(src->src_type == MANCHESTER)
            manchester_source_reset((manchester_source)src->source);
        else if(src->src_type == PPM)
            ppm_source_reset((ppm_source)src->source);
        else if(src->src_type == PDM)
            pdm_source_reset((pdm_source)src->source);
    }
}
void real_source_center(real_source src){
    if(src == NULL) return;
    if(src->source == NULL) return;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT) return;
            // constant_source_center((constant_source)src->source);
        else if(src->src_type == SQUARE)
            square_source_center((square_source)src->source);
        else if(src->src_type == SAWTOOTH)
            sawtooth_source_center((sawtooth_source)src->source);
        else if(src->src_type == TRIANGLE)
            triangle_source_center((triangle_source)src->source);
        else if(src->src_type == SINUSOID)
            sinusoid_source_center((sinusoid_source)src->source);
    }
    else if(src->adm_type == MESSAGE){
    }
    else{
        if(src->src_type == BYTES)
            bytes_source_center((bytes_source)src->source);
        else if(src->src_type == MASK)
            mask_source_center((mask_source)src->source);
        else if(src->src_type == MANCHESTER)
            manchester_source_center((manchester_source)src->source);
        else if(src->src_type == PPM)
            ppm_source_center((ppm_source)src->source);
        else if(src->src_type == PDM)
            pdm_source_center((pdm_source)src->source);
    }
}
int real_source_release(real_source src, uint32_t len, uint32_t* actual_len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_release((constant_source)src->source, len, actual_len);
        else if(src->src_type == SQUARE)
            return square_source_release((square_source)src->source, len, actual_len);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_release((sawtooth_source)src->source, len, actual_len);
        else if(src->src_type == TRIANGLE)
            return triangle_source_release((triangle_source)src->source, len, actual_len);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_release((sinusoid_source)src->source, len, actual_len);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_release((bytes_source)src->source, len, actual_len);
        else if(src->src_type == MASK)
            return mask_source_release((mask_source)src->source, len, actual_len);
        else if(src->src_type == MANCHESTER)
            return manchester_source_release((manchester_source)src->source, len, actual_len);
        else if(src->src_type == PPM)
            return ppm_source_release((ppm_source)src->source, len, actual_len);
        else if(src->src_type == PDM)
            return pdm_source_release((pdm_source)src->source, len, actual_len);
    }
    return -3;
}
int real_source_incr(real_source src, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_step((constant_source)src->source, out);
        else if(src->src_type == SQUARE)
            return square_source_incr((square_source)src->source, delta, out);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_incr((sawtooth_source)src->source, delta, out);
        else if(src->src_type == TRIANGLE)
            return triangle_source_incr((triangle_source)src->source, delta, out);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_incr((sinusoid_source)src->source, delta, out);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_incr((bytes_source)src->source, delta, out);
        else if(src->src_type == MASK)
            return mask_source_incr((mask_source)src->source, delta, out);
        else if(src->src_type == MANCHESTER)
            return manchester_source_incr((manchester_source)src->source, delta, out);
        else if(src->src_type == PPM)
            return ppm_source_incr((ppm_source)src->source, delta, out);
        else if(src->src_type == PDM)
            return pdm_source_incr((pdm_source)src->source, delta, out);
    }
    return -3;
}
int real_source_nincr(real_source src, uint32_t n, double* delta, float* out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_nstep((constant_source)src->source, n, out);
        else if(src->src_type == SQUARE)
            return square_source_nincr((square_source)src->source, n, delta, out);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_nincr((sawtooth_source)src->source, n, delta, out);
        else if(src->src_type == TRIANGLE)
            return triangle_source_nincr((triangle_source)src->source, n, delta, out);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_nincr((sinusoid_source)src->source, n, delta, out);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_nincr((bytes_source)src->source, n, delta, out);
        else if(src->src_type == MASK)
            return mask_source_nincr((mask_source)src->source, n, delta, out);
        else if(src->src_type == MANCHESTER)
            return manchester_source_nincr((manchester_source)src->source, n, delta, out);
        else if(src->src_type == PPM)
            return ppm_source_nincr((ppm_source)src->source, n, delta, out);
        else if(src->src_type == PDM)
            return pdm_source_nincr((pdm_source)src->source, n, delta, out);
    }
    return -3;
}
int real_source_step(real_source src, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_step((constant_source)src->source, out);
        else if(src->src_type == SQUARE)
            return square_source_step((square_source)src->source, out);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_step((sawtooth_source)src->source, out);
        else if(src->src_type == TRIANGLE)
            return triangle_source_step((triangle_source)src->source, out);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_step((sinusoid_source)src->source, out);
    }
    else if(src->adm_type == MESSAGE){
        if(src->src_type == RANDOM_GAUSS)
            return rand_gauss_source_step((rand_gauss_source)src->source, out);
        else if(src->src_type == RANDOM_UNI)
            return rand_uni_source_step((rand_uni_source)src->source, out);
        else if(src->src_type == WAV_FILE)
            return wav_source_step((wav_source)src->source, out);
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_step((bytes_source)src->source, out);
        else if(src->src_type == MASK)
            return mask_source_step((mask_source)src->source, out);
        else if(src->src_type == MANCHESTER)
            return manchester_source_step((manchester_source)src->source, out);
        else if(src->src_type == PPM)
            return ppm_source_step((ppm_source)src->source, out);
        else if(src->src_type == PDM)
            return pdm_source_step((pdm_source)src->source, out);
    }
    return -3;
}
int real_source_nstep(real_source src, uint32_t n, float *out){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_nstep((constant_source)src->source, n, out);
        else if(src->src_type == SQUARE){
            // printf("Hmm, got here at least\n");
            return square_source_nstep((square_source)src->source, n, out);
        }
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_nstep((sawtooth_source)src->source, n, out);
        else if(src->src_type == TRIANGLE)
            return triangle_source_nstep((triangle_source)src->source, n, out);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_nstep((sinusoid_source)src->source, n, out);
    }
    else if(src->adm_type == MESSAGE){
        if(src->src_type == RANDOM_GAUSS)
            return rand_gauss_source_nstep((rand_gauss_source)src->source, n, out);
        if(src->src_type == RANDOM_UNI)
            return rand_uni_source_nstep((rand_uni_source)src->source, n, out);
        if(src->src_type == WAV_FILE)
            return wav_source_nstep((wav_source)src->source, n, out);
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_nstep((bytes_source)src->source, n, out);
        else if(src->src_type == MASK)
            return mask_source_nstep((mask_source)src->source, n, out);
        else if(src->src_type == MANCHESTER)
            return manchester_source_nstep((manchester_source)src->source, n, out);
        else if(src->src_type == PPM)
            return ppm_source_nstep((ppm_source)src->source, n, out);
        else if(src->src_type == PDM)
            return pdm_source_nstep((pdm_source)src->source, n, out);
    }
    return -3;
}
int real_source_set(real_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_set((constant_source)src->source, p1);
        else if(src->src_type == SQUARE)
            return square_source_set((square_source)src->source, p1, p2);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_set((sawtooth_source)src->source, p1, p2);
        else if(src->src_type == TRIANGLE)
            return triangle_source_set((triangle_source)src->source, p1, p2);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_set((sinusoid_source)src->source, p1, p2);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_set((bytes_source)src->source, p1, p2);
        else if(src->src_type == MASK)
            return mask_source_set((mask_source)src->source, p1, p2);
        else if(src->src_type == MANCHESTER)
            return manchester_source_set((manchester_source)src->source, p1, p2);
        else if(src->src_type == PPM)
            return ppm_source_set((ppm_source)src->source, p1, p2);
        else if(src->src_type == PDM)
            return pdm_source_set((pdm_source)src->source, p1, p2);
    }
    return -3;
}
int real_source_get(real_source src, double *p1, double *p2){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_get((constant_source)src->source, p1);
        else if(src->src_type == SQUARE)
            return square_source_get((square_source)src->source, p1, p2);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_get((sawtooth_source)src->source, p1, p2);
        else if(src->src_type == TRIANGLE)
            return triangle_source_get((triangle_source)src->source, p1, p2);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_get((sinusoid_source)src->source, p1, p2);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_get((bytes_source)src->source, p1, p2);
        else if(src->src_type == MASK)
            return mask_source_get((mask_source)src->source, p1, p2);
        else if(src->src_type == MANCHESTER)
            return manchester_source_get((manchester_source)src->source, p1, p2);
        else if(src->src_type == PPM)
            return ppm_source_get((ppm_source)src->source, p1, p2);
        else if(src->src_type == PDM)
            return pdm_source_get((pdm_source)src->source, p1, p2);
    }
    return -3;
}
int real_source_get_stream(real_source src, float *buf, uint32_t len){
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            return constant_source_get_stream((constant_source)src->source, buf, len);
        else if(src->src_type == SQUARE)
            return square_source_get_stream((square_source)src->source, buf, len);
        else if(src->src_type == SAWTOOTH)
            return sawtooth_source_get_stream((sawtooth_source)src->source, buf, len);
        else if(src->src_type == TRIANGLE)
            return triangle_source_get_stream((triangle_source)src->source, buf, len);
        else if(src->src_type == SINUSOID)
            return sinusoid_source_get_stream((sinusoid_source)src->source, buf, len);
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            return bytes_source_get_stream((bytes_source)src->source, buf, len);
        else if(src->src_type == MASK)
            return mask_source_get_stream((mask_source)src->source, buf, len);
        else if(src->src_type == MANCHESTER)
            return manchester_source_get_stream((manchester_source)src->source, buf, len);
        else if(src->src_type == PPM)
            return ppm_source_get_stream((ppm_source)src->source, buf, len);
        else if(src->src_type == PDM)
            return pdm_source_get_stream((pdm_source)src->source, buf, len);
    }
    return -3;
}
#ifdef __cplusplus
int real_source_get_stream_ptr(real_source src, float *ptr, uint32_t len, uint32_t &actual_len){
#else
int real_source_get_stream_ptr(real_source src, float *ptr, uint32_t len, uint32_t *actual_len){
#endif
    if(src == NULL) return -1;
    if(src->source == NULL) return -2;
    if(src->adm_type == ANALOG){
        if(src->src_type == CONSTANT)
            #ifdef __cplusplus
            return constant_source_get_stream_ptr((constant_source)src->source, ptr, len, actual_len);
            #else
            return constant_source_get_stream_ptr((constant_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == SQUARE)
            #ifdef __cplusplus
            return square_source_get_stream_ptr((square_source)src->source, ptr, len, actual_len);
            #else
            return square_source_get_stream_ptr((square_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == SAWTOOTH)
            #ifdef __cplusplus
            return sawtooth_source_get_stream_ptr((sawtooth_source)src->source, ptr, len, actual_len);
            #else
            return sawtooth_source_get_stream_ptr((sawtooth_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == TRIANGLE)
            #ifdef __cplusplus
            return triangle_source_get_stream_ptr((triangle_source)src->source, ptr, len, actual_len);
            #else
            return triangle_source_get_stream_ptr((triangle_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == SINUSOID)
            #ifdef __cplusplus
            return sinusoid_source_get_stream_ptr((sinusoid_source)src->source, ptr, len, actual_len);
            #else
            return sinusoid_source_get_stream_ptr((sinusoid_source)src->source, ptr, len, actual_len);
            #endif
    }
    else if(src->adm_type == MESSAGE){
        return -4;
    }
    else{
        if(src->src_type == BYTES)
            #ifdef __cplusplus
            return bytes_source_get_stream_ptr((bytes_source)src->source, ptr, len, actual_len);
            #else
            return bytes_source_get_stream_ptr((bytes_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == MASK)
            #ifdef __cplusplus
            return mask_source_get_stream_ptr((mask_source)src->source, ptr, len, actual_len);
            #else
            return mask_source_get_stream_ptr((mask_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == MANCHESTER)
            #ifdef __cplusplus
            return manchester_source_get_stream_ptr((manchester_source)src->source, ptr, len, actual_len);
            #else
            return manchester_source_get_stream_ptr((manchester_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == PPM)
            #ifdef __cplusplus
            return ppm_source_get_stream_ptr((ppm_source)src->source, ptr, len, actual_len);
            #else
            return ppm_source_get_stream_ptr((ppm_source)src->source, ptr, len, actual_len);
            #endif
        else if(src->src_type == PDM)
            #ifdef __cplusplus
            return pdm_source_get_stream_ptr((pdm_source)src->source, ptr, len, actual_len);
            #else
            return pdm_source_get_stream_ptr((pdm_source)src->source, ptr, len, actual_len);
            #endif
    }
    return -3;
}

//------------------------------------------------

#ifdef __cplusplus
ammod ammod_create(){//default don't mod
#else
ammod ammod_create_default(){//default don't mod
#endif
    ammod mod = (ammod) malloc(sizeof(struct ammod_s));
    memset(mod, 0, sizeof(struct ammod_s));
    if(mod == NULL) return mod;
    mod->bias_src = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->bias_src->source));
    mod->bias_src->source = (void*)constant_source_create(0.0,0);
    mod->_mod_idx = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->_mod_idx->source));
    mod->_mod_idx->source = (void*)constant_source_create(1.0,0);
    mod->freq_src = real_source_create(ANALOG, STATIC, SINUSOID, sizeof(float), 1, NONE);
    sinusoid_source ptr = (sinusoid_source)mod->freq_src->source;
    ptr->_freq = 0;
    return mod;
}
ammod ammod_create(double mod_idx, double bias, double freq){
    ammod mod = (ammod) malloc(sizeof(struct ammod_s));
    memset(mod, 0, sizeof(struct ammod_s));
    if(mod == NULL) return mod;
    mod->bias_src = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->bias_src->source));
    mod->bias_src->source = (void*)constant_source_create(bias,0);
    mod->_mod_idx = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->_mod_idx->source));
    mod->_mod_idx->source = (void*)constant_source_create(mod_idx,0);
    mod->freq_src = real_source_create(ANALOG, STATIC, SINUSOID, sizeof(float), 1, NONE);
    sinusoid_source ptr = (sinusoid_source)mod->freq_src->source;
    ptr->_freq = freq;
    return mod;
}
void ammod_destroy(ammod *src){
    if (src == NULL) return;
    if (*src == NULL) return;
    if((*src)->bias_src != NULL){
        real_source ptr = (real_source)(*src)->bias_src;
        real_source_destroy(&ptr);
    }
    if((*src)->freq_src != NULL){
        real_source ptr = (real_source)(*src)->freq_src;
        real_source_destroy(&ptr);
    }
    free((*src));
}
int ammod_step(ammod mod, float *out){
    if (mod == NULL) return 1;
    float mod_idx;
    float bias;
    float freq;
    real_source_step(mod->_mod_idx, &mod_idx);
    real_source_step(mod->bias_src, &bias);
    real_source_step(mod->freq_src, &freq);
    if(out != NULL){
        *out = (mod_idx*(*out)+bias)*freq;
    }
    return 0;
}
int ammod_nstep(ammod mod, uint32_t n, float *out){
    if (mod == NULL) return 1;
    float *mod_idx;
    float *bias;
    float *freq;
    if(out != NULL){
        mod_idx = (float*)malloc(n*sizeof(float));
        bias = (float*)malloc(n*sizeof(float));
        freq = (float*)malloc(n*sizeof(float));
        memset(mod_idx, 0, n*sizeof(float));
        memset(bias, 0, n*sizeof(float));
        memset(freq, 0, n*sizeof(float));
    }
    real_source_nstep(mod->_mod_idx, n, mod_idx);
    real_source_nstep(mod->bias_src, n, bias);
    real_source_nstep(mod->freq_src, n, freq);
    if(out != NULL){
        for(uint32_t idx = 0; idx < n; idx++){
            out[idx] = (mod_idx[idx]*out[idx]+bias[idx])*freq[idx];
        }
        free(mod_idx);
        free(bias);
        free(freq);
    }
    return 0;
}

//------------------------------------------------

#ifdef __cplusplus
real_path real_path_create(){
#else
real_path real_path_create_default(){
    #endif
    real_path path = (real_path)malloc(sizeof(struct real_path_s));
    memset(path, 0, sizeof(struct real_path_s));
    if(path == NULL) return path;
    path->input_fs = 100e3;
    path->output_fs = 1e6;
    path->bandwidth = 0.5;
    path->max_freq = 0.25;
    path->baud_rate = 50e3;
    path->path_gain = 1.0;
    path->fc_offset = 0.0;
    path->buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(path->output_fs/path->input_fs));
    path->buffer = cbufferf_create(path->buf_len);
    #ifdef __cplusplus
    path->source = real_source_create();
    #else
    path->source = real_source_create_default();
    #endif
    path->modulator = NULL;
    path->resampler = msresamp_rrrf_create(path->output_fs/path->input_fs, 60.0f);
    #ifdef __cplusplus
    std::cout << "real_path using resampler\n";
    #else
    printf("real_path using resampler\n");
    #endif
    return path;
}
real_path real_path_create(float input_fs, float output_fs, float bandwidth, float max_freq,
                            float baud_rate, float gain, float fc_offset,
                            real_source source, ammod modulator, msresamp_rrrf resampler){
    real_path path = (real_path)malloc(sizeof(struct real_path_s));
    memset(path, 0, sizeof(struct real_path_s));
    if(path == NULL) return path;
    if(source == NULL){
        free(path);
        path = NULL;
        return path;
    }
    path->input_fs = input_fs;
    path->output_fs = output_fs;
    path->bandwidth = bandwidth;
    path->max_freq = max_freq;
    path->baud_rate = baud_rate;
    path->path_gain = gain;
    path->fc_offset = fc_offset;
    path->source = source;
    path->modulator = modulator;
    if (modulator == NULL && (gain != 1.0 || fc_offset != 0.0)){
        path->modulator = ammod_create(gain,0.0,fc_offset);
    }
    if (resampler == NULL){
        if (path->output_fs != path->input_fs){
            path->resampler = msresamp_rrrf_create(path->output_fs/path->input_fs, 60.0f);
            path->buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(path->output_fs/path->input_fs));
            path->buffer = cbufferf_create(path->buf_len);
            #ifdef __cplusplus
            std::cout << "real_path using resampler\n";
            #else
            printf("real_path using resampler\n");
            #endif
        }
        else{
            path->buf_len = 0;
            path->buffer = cbufferf_create_max(0,1);
        }
    }
    else{
        #ifdef __cplusplus
        std::cout << "real_path using resampler\n";
        #else
        printf("real_path using resampler\n");
        #endif
        path->buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(msresamp_rrrf_get_rate(resampler)));
        path->buffer = cbufferf_create(path->buf_len);
        path->resampler = resampler;
    }
    return path;
}

void real_path_destroy(real_path *path){
    if (path == NULL) return;
    if (*path == NULL) return;
    if((*path)->buffer != NULL){
        cbufferf ptr = (*path)->buffer;
        cbufferf_destroy(ptr);
    }
    if((*path)->source != NULL){
        real_source ptr = (*path)->source;
        real_source_destroy(&ptr);
    }
    if((*path)->modulator != NULL){
        ammod ptr = (*path)->modulator;
        ammod_destroy(&ptr);
    }
    if((*path)->resampler != NULL){
        msresamp_rrrf ptr = (*path)->resampler;
        msresamp_rrrf_destroy(ptr);
    }
    free((*path));
    (*path) = NULL;
}

int real_path_step(real_path path, float *out){
    if (path == NULL) return 1;
    int rc = 0;
    // float max_v0(0),max_v1(0);
    rc = real_source_step(path->source, out); // if out null handled in there
    // if(out != NULL){
    //     max_v0 = std::abs(*out);
    // }
    if( path->modulator != NULL ){
        ammod_step(path->modulator, out); // if out null handled in there (out + bias)*freq
        // if(out != NULL){
        //     max_v1 = std::abs(*out);
        // }
    }
    // if(out != NULL){
    //     // std::cout << "out? " << *out << std::endl;
    //     if(max_v0 > 1.0) std::cout << "maxv0 exceeds 1.0: " << max_v0 << std::endl; 
    //     if(max_v1 > 1.0) std::cout << "maxv1 exceeds 1.0: " << max_v1 << std::endl; 
    // }
    return rc;
}
int real_path_nstep(real_path path, uint32_t n, float *out){
    if (path == NULL) return 1;
    // float max_v0(0),max_v1(0);
    // float *tweaks = (float*) malloc(n*sizeof(float));
    // memset(tweaks, 0, n*sizeof(float));
    int rc = 0;
    rc = real_source_nstep(path->source, n, out);
    // if(out != NULL){
    //     for(size_t idx = 0; idx < n; idx++){
    //         max_v0 = (std::abs(out[idx]) > max_v0) ? std::abs(out[idx]) : max_v0;
    //     }
    // }
    ammod_nstep(path->modulator, n, out); // ammod will handle if modulator is null
    // if(out != NULL){
    //     for(size_t idx = 0; idx < n; idx++){
    //         max_v1 = (std::abs(out[idx]) > max_v1) ? std::abs(out[idx]) : max_v1;
    //     }
    //     if(max_v0 > 1.0) std::cout << "maxv0 exceeds 1.0: " << max_v0 << std::endl; 
    //     if(max_v1 > 1.0) std::cout << "maxv1 exceeds 1.0: " << max_v1 << std::endl; 
    // }
    return rc;
}

//------------------------------------------------

#ifdef __cplusplus
amgen amgen_create(){
#else
amgen amgen_create_default(){
    #endif
    amgen gen = (amgen)malloc(sizeof(struct amgen_s));
    memset(gen, 0, sizeof(struct amgen_s));
    if(gen == NULL) return gen;
    gen->num_paths = 1;
    gen->mod_idx = real_source_create(ANALOG,STATIC,CONSTANT,sizeof(float),1,NONE);
    constant_source ptr = (constant_source)gen->mod_idx->source;
    ptr->_amp = 0.7;
    gen->path_gains = (double*)malloc(sizeof(double));
    memset(gen->path_gains, 0, sizeof(double));
    gen->path_gains[0] = 1.0;
    gen->paths = (real_path*)malloc(sizeof(real_path));
    memset(gen->paths, 0, sizeof(real_path));
    #ifdef __cplusplus
    gen->paths[0] = real_path_create();
    #else
    gen->paths[0] = real_path_create_default();
    #endif
    return gen;
}
amgen amgen_create(uint8_t num_paths, float mod_idx, double* gains, real_path *paths){
    if(paths == NULL) return NULL;
    amgen gen = (amgen)malloc(sizeof(struct amgen_s));
    memset(gen, 0, sizeof(struct amgen_s));
    if(gen == NULL) return gen;
    gen->num_paths = num_paths;
    gen->paths = (real_path*)malloc(num_paths*sizeof(real_path));
    gen->mod_idx = real_source_create(ANALOG,STATIC,CONSTANT,sizeof(float),1,NONE);
    double h = mod_idx;
    real_source_set(gen->mod_idx,&h,NULL);

    if(gains == NULL){
        gen->path_gains = (double*)malloc(num_paths*sizeof(double));
        for(uint8_t idx = 0; idx < num_paths; idx++){
            gen->path_gains[idx] = 1.0;
        }
    }
    memmove(gen->paths, paths, num_paths*sizeof(real_path));
    return gen;
}
void amgen_destroy(amgen *gen){
    if(gen == NULL) return;
    if(*gen == NULL) return;
    if((*gen)->paths != NULL){
        real_path rp_ptr;
        for(uint idx = 0; idx < (*gen)->num_paths; idx++){
            rp_ptr = (*gen)->paths[idx];
            real_path_destroy(&rp_ptr);
        }
        free((*gen)->paths);
    }
    if((*gen)->path_gains != NULL) free((*gen)->path_gains);
    free((*gen));
}
int amgen_step(amgen gen, float *out){
    if (gen == NULL) return 1;
    if(out != NULL){
        *out = 0;
        float dump = 0;
        for(uint8_t idx = 0; idx < gen->num_paths; idx++){
            real_path_step(gen->paths[idx], &dump);
            (*out) += dump;
        }
        // if(std::abs(*out) > 1.0){ std::cout << "amgen_step value exceeds 1.0 limit: " << *out << " with n("<<(int)gen->num_paths<<") paths\n"; exit(1);}
        // else{ std::cout << "amgen_step value below 1.0 limit: " << *out << std::endl;}
    }
    else{
        for(uint8_t idx = 0; idx < gen->num_paths; idx++){
            real_path_step(gen->paths[idx], NULL);
        }
    }
    return 0;
}
#ifdef __cplusplus
int amgen_nstep(amgen gen, uint32_t n, float *out){
#else
int amgen_nstepf(amgen gen, uint32_t n, float *out){
#endif
    if (gen == NULL) return 1;
    if (gen->num_paths == 0) return 2;
    if(out != NULL){
        float *dump = (float*)malloc(n*sizeof(float));
        memset(dump, 0, n*sizeof(float));
        memset(out, 0, n*sizeof(float));
        real_path_nstep(gen->paths[0], n, out);
        for(uint8_t idx = 1; idx < gen->num_paths; idx++){
            real_path_nstep(gen->paths[idx], n, dump);
            for(uint32_t samp = 0; samp < n; samp++){
                out[samp] += dump[samp];
            }
        }
        free(dump);
    }
    else{
        for(uint8_t idx = 0; idx < gen->num_paths; idx++){
            real_path_nstep(gen->paths[idx], n, NULL);
        }
    }
    return 0;
}
int amgen_nstep(amgen gen, uint32_t n, liquid_float_complex *out){
    if (gen == NULL) return 1;
    if (gen->num_paths == 0) return 2;
    if(out != NULL){
        float *base = (float*)malloc(n*sizeof(float));
        float *dump = (float*)malloc(n*sizeof(float));
        memset(base, 0, n*sizeof(float));
        memset(dump, 0, n*sizeof(float));
        real_path_nstep(gen->paths[0], n, base);
        for(uint8_t idx = 1; idx < gen->num_paths; idx++){
            real_path_nstep(gen->paths[idx], n, dump);
            for(uint32_t samp = 0; samp < n; samp++){
                base[samp] += dump[samp];
            }
        }
        for(uint32_t samp = 0; samp < n; samp++){
            #ifdef __cplusplus
            out[samp] = std::complex<float>(base[samp],0.f);
            #else
            out[samp] = base[samp] + 0.f*I;
            #endif
        }
    }
    else{
        for(uint8_t idx = 0; idx < gen->num_paths; idx++){
            real_path_nstep(gen->paths[idx], n, NULL);
        }
    }
    return 0;
}

//------------------------------------------------

#ifdef __cplusplus
fmmod fmmod_create(){
#else
fmmod fmmod_create_default(){
    #endif
    fmmod mod = (fmmod) malloc(sizeof(struct fmmod_s));
    memset(mod, 0, sizeof(struct fmmod_s));
    if(mod == NULL) return mod;
    #ifdef __cplusplus
    mod->tone = real_source_create();// sinusoid amp=1, fc= 0.1
    #else
    mod->tone = real_source_create_default();// sinusoid amp=1, fc= 0.1
    #endif
    mod->_mod_idx = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->_mod_idx->source));
    mod->_mod_idx->source = (void*)constant_source_create(0.25,0);
    return mod;
}
fmmod fmmod_create(float mod_idx){
    fmmod mod = (fmmod) malloc(sizeof(struct fmmod_s));
    memset(mod, 0, sizeof(struct fmmod_s));
    if(mod == NULL) return mod;
    #ifdef __cplusplus
    mod->tone = real_source_create();// sinusoid amp=1, fc= 0.1
    #else
    mod->tone = real_source_create_default();// sinusoid amp=1, fc= 0.1
    #endif
    mod->_mod_idx = real_source_create(ANALOG, STATIC, CONSTANT, sizeof(float), 1, NONE);
    constant_source_destroy((constant_source*)&(mod->_mod_idx->source));
    mod->_mod_idx->source = (void*)constant_source_create(mod_idx,0);
    return mod;
}
void fmmod_destroy(fmmod *mod){
    if(mod == NULL) return;
    if(*mod == NULL) return;
    if((*mod)->_mod_idx != NULL){
        real_source ptr = (real_source)(*mod)->_mod_idx;
        real_source_destroy(&ptr);
    }
    if((*mod)->tone != NULL){
        real_source ptr = (real_source)(*mod)->tone;
        real_source_destroy(&ptr);
    }
    free((*mod));
}
int fmmod_step(fmmod mod, float mesg, liquid_float_complex *out){
    if(mod == NULL) return 1;
    if(mod->tone == NULL) return 2;
    if(mod->tone->source == NULL) return 3;
    sinusoid_source ptr = (sinusoid_source)mod->tone->source;
    if(out != NULL){
        #ifdef __cplusplus
        *out = std::polar(1.0,ptr->_phi);
        #else
        *out = cexp(ptr->_phi*I);
        #endif
    }
    float mod_idx;
    real_source_step(mod->_mod_idx, &mod_idx);
    double s = 2*M_PI*mod_idx*mesg;
    sinusoid_source_incr(ptr, &s, NULL);
    sinusoid_source_center(ptr);
    return 0;
}
int fmmod_nstep(fmmod mod, uint32_t n, float *mesg, liquid_float_complex *out){
    if(mod == NULL) return 1;
    if(mod->tone == NULL) return 2;
    if(mod->tone->source == NULL) return 3;
    sinusoid_source ptr = (sinusoid_source)mod->tone->source;
    double *s = (double*) malloc(n*sizeof(double));
    float *h = (float*) malloc(n*sizeof(float));
    memset(s, 0, n*sizeof(double));
    memset(h, 0, n*sizeof(float));
    real_source_nstep(mod->_mod_idx, n, h);
    for(uint32_t idx = 0; idx < n; idx++){
        s[idx] = 2*M_PI*h[idx]*mesg[idx];
    }
    if(out != NULL){
        for(uint32_t idx = 0; idx < n; idx++){
            #ifdef __cplusplus
            out[idx] = std::polar(1.0,ptr->_phi);
            #else
            out[idx] = cexp(ptr->_phi*I);
            #endif
            sinusoid_source_incr(ptr, &s[idx], NULL);
            sinusoid_source_center(ptr);
        }
    }
    else{
        // don't care about the output, so
        sinusoid_source_nincr(ptr, n, s, NULL);//calls center
    }
    free(s);
    free(h);
    return 0;
}


//------------------------------------------------

#ifdef __cplusplus
fmgen fmgen_create(){
#else
fmgen fmgen_create_default(){
    #endif
    fmgen gen = (fmgen) malloc(sizeof(struct fmgen_s));
    memset(gen, 0, sizeof(struct fmgen_s));
    if(gen == NULL) return gen;
    #ifdef __cplusplus
    gen->source = amgen_create();
    #else
    gen->source = amgen_create_default();
    #endif
    if(gen->source == NULL){
        free(gen);
        gen = NULL;
        return gen;
    }
    #ifdef __cplusplus
    gen->mod = fmmod_create();
    #else
    gen->mod = fmmod_create_default();
    #endif
    if(gen->mod == NULL){
        amgen_destroy(&(gen->source));
        free(gen);
        gen = NULL;
    }
    gen->buf_len = 0;
    gen->buffer = NULL;
    gen->resampler = NULL;
    return gen;
}
fmgen fmgen_create(float mod_idx, amgen source, msresamp_rrrf resampler){
    if(source == NULL) return NULL;
    fmgen gen = (fmgen) malloc(sizeof(struct fmgen_s));
    memset(gen, 0, sizeof(struct fmgen_s));
    if(gen == NULL) return gen;
    gen->source = source;
    gen->mod = fmmod_create(mod_idx);
    gen->buf_len = 0;
    gen->buffer = NULL;
    gen->resampler = resampler;
    if(resampler != NULL){
        #ifdef __cpluscplus
        std::cout << "fmgen is using resampler\n";
        #else
        printf("fmgen is using resampler\n");
        #endif
        gen->buf_len = 1 << liquid_nextpow2((unsigned int)ceilf(msresamp_rrrf_get_rate(resampler)));
        gen->buffer = cbufferf_create(gen->buf_len);
    }
    return gen;
}
void fmgen_destroy(fmgen *gen){
    if(gen == NULL) return;
    if(*gen == NULL) return;
    if((*gen)->source != NULL) amgen_destroy(&((*gen)->source));
    if((*gen)->mod != NULL) fmmod_destroy(&((*gen)->mod));
    if((*gen)->buffer != NULL) cbufferf_destroy((*gen)->buffer);
    if((*gen)->resampler != NULL) msresamp_rrrf_destroy((*gen)->resampler);
    free((*gen));
    *gen = NULL;
}
int fmgen_step(fmgen gen, liquid_float_complex *out){
    if(gen == NULL) return 1;
    if(gen->source == NULL) return 2;
    if(gen->mod == NULL) return 3;
    float mesg = 0.;
    if(gen->resampler != NULL){
        #ifdef __cplusplus
        std::cout << "fmgen_step WANTS TO USE resampler but is NULL\n";
        #else
        printf("fmgen_step WANTS TO USE resampler but is NULL\n");
        #endif
        return 4; // need to actually use this --> needed if the mod_idx can be >~ 0.5;
    }
    else{
        amgen_step(gen->source, &mesg);
        #ifdef __cplusplus
        if(std::abs(mesg) > 1.0) std::cout << "fmgen_step value exceeds 1.0 limit from amgen: " << mesg << " with n("<<(int)gen->source->num_paths<<") paths\n";
        #endif
        fmmod_step(gen->mod, mesg, out);
    }
    return 0;
}
int fmgen_nstep(fmgen gen, uint32_t n, liquid_float_complex *out){
    if(gen == NULL) return 1;
    if(gen->source == NULL) return 2;
    if(gen->mod == NULL) return 3;
    float *mesg = (float*)malloc(n*sizeof(float));
    memset(mesg, 0, n*sizeof(float));
    if(gen->resampler != NULL){
        #ifdef __cplusplus
        std::cout << "fmgen_nstep WANTS TO USE resampler but is NULL\n";
        #else
        printf("fmgen_nstep WANTS TO USE resampler but is NULL\n");
        #endif
        return 4; // need to actually use this --> needed if the mod_idx can be >~ 0.85;
    }
    else{
        #ifdef __cplusplus
        amgen_nstep(gen->source, n, mesg);
        #else
        amgen_nstepf(gen->source, n, mesg);
        #endif
        fmmod_nstep(gen->mod, n, mesg, out);
    }
    free(mesg);
    return 0;
}










//************************************************

