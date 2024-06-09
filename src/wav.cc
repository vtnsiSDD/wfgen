
#include "wav.hh"
#include <glob.h>
#ifdef __cplusplus
#include <iostream>
#include <cstdlib>
#include <cassert>
#else
#include <stdlib.h>
#include <assert.h>
#endif
/////WAV
int compare_check(void* buff1, void* buff2, int length){
    uint8_t *b1,*b2;
    b1 = (uint8_t*)buff1;
    b2 = (uint8_t*)buff2;
    for(int i = 0; i < length; i++){
        if(b1[i] != b2[i]){
            return 1;
        }
    }
    return 0;
}
#ifdef __cplusplus
int64_t find_flag(wav_obj wav, uint32_t offset, std::string flag){
    char* searcher = (char*)flag.data();
    int length = flag.length();
    size_t searching = offset;
    uint8_t *data_ptr = (uint8_t*)wav->data_ptr;
    while(searching < wav->filesize+length-1){
        if(!compare_check((void*)&data_ptr[searching], (void*)searcher, length)){
            // found it
            return searching;
        }
        searching++;
    }
    return -1;//didn't find it
}

wav_obj_t* open_wav(std::string fp){
    wav_obj out = (wav_obj_t*)malloc(sizeof(wav_obj_s));
    if (!out){
        return NULL;//couldn't make object
    }
    memset(out, 0, sizeof(*out));
    out->err_state = 0;
    out->filepath = (char*)malloc(fp.size()+1);
    if (!out->filepath){
        out->err_state = 0xff;//couldn't malloc
        return out;
    }
    memcpy( out->filepath, fp.data(), fp.size() );
    out->filepath[fp.size()] = 0;
    out->filehandle = (void*)fopen(out->filepath, "r");
    if(!out->filehandle){
        out->err_state = 1;//couldn't open
        return out;
    }
    fseek((FILE*)out->filehandle, 0L, SEEK_END);
    out->filesize = ftell((FILE*)out->filehandle);
    fseek((FILE*)out->filehandle, 0L, SEEK_SET);
    out->data_ptr = mmap(0, out->filesize, PROT_READ, MAP_SHARED, fileno((FILE*)out->filehandle), 0);
    if((uint8_t*)out->data_ptr == (uint8_t*)MAP_FAILED){
        std::cout << fileno((FILE*)out->filehandle)
            << " " << 0 << " " << out->filesize << std::endl;
        out->err_state = 2;// couldn't mmap
        return out;
    }

    uint8_t *ptr8;
    ptr8 = (uint8_t*)out->data_ptr;
    std::string header("RIFF"),type("WAVE");
    if(compare_check((void*)ptr8, (void*)header.data(), header.size())
            || compare_check((void*)&ptr8[8], (void*)type.data(), type.size())){
        std::cout << (char)ptr8[0]
                  << (char)ptr8[1]
                  << (char)ptr8[2]
                  << (char)ptr8[3]
                  << (char)ptr8[8]
                  << (char)ptr8[9]
                  << (char)ptr8[10]
                  << (char)ptr8[11]
                  << std::endl;

        out->err_state = 3;// not a wav file
        return out;
    }

    int64_t fmt_offset = 0;
    fmt_offset = find_flag(out, 12, "fmt ");
    out->data_offset = find_flag(out, 12, "data");
    if(fmt_offset == -1 || out->data_offset == -1){
        out->err_state = 4;// couldn't find reqired flags
        return out;
    }

    if(*((uint32_t*)(&ptr8[fmt_offset+4])) != 16){
        out->err_state = 5; // something is wrong with the fmt section
        return out;
    }
    out->audio_fmt = *((uint16_t*)(&ptr8[fmt_offset+8]));
    out->num_channels = *((uint16_t*)(&ptr8[fmt_offset+10]));
    out->sample_rate = *((uint32_t*)(&ptr8[fmt_offset+12]));
    out->byte_rate = *((uint32_t*)(&ptr8[fmt_offset+16]));
    out->block_align = *((uint16_t*)(&ptr8[fmt_offset+20]));
    out->bit_depth = *((uint16_t*)(&ptr8[fmt_offset+22]));

    if(out->audio_fmt != 1){
        out->err_state = 0xF0;//don't know how to handle this format
        return out;
    }

    out->data_len = (size_t) *((uint32_t*)(&ptr8[out->data_offset+4]));
    out->data_type = uint8_t(int((float)(out->bit_depth)/8.0f));
    out->data_offset += 8;

    return out;
}
#else
int64_t find_flag(wav_obj wav, uint32_t offset, const char* flag){
    char* searcher = (char*)flag;
    int length = strlen(flag);
    size_t searching = offset;
    uint8_t *data_ptr = (uint8_t*)wav->data_ptr;
    while(searching < wav->filesize+length-1){
        if(!compare_check((void*)&data_ptr[searching], (void*)searcher, length)){
            // found it
            return searching;
        }
        searching++;
    }
    return -1;//didn't find it
}

wav_obj_t* open_wav(char* fp){
    wav_obj out = (wav_obj_t*)malloc(sizeof(struct wav_obj_s));
    if (!out){
        return NULL;//couldn't make object
    }
    memset(out, 0, sizeof(*out));
    out->err_state = 0;
    out->filepath = (char*)malloc(strlen(fp)+1);
    if (!out->filepath){
        out->err_state = 0xff;//couldn't malloc
        return out;
    }
    memcpy( out->filepath, fp, strlen(fp) );
    out->filepath[strlen(fp)] = 0;
    out->filehandle = (void*)fopen(out->filepath, "r");
    if(!out->filehandle){
        out->err_state = 1;//couldn't open
        return out;
    }
    fseek((FILE*)out->filehandle, 0L, SEEK_END);
    out->filesize = ftell((FILE*)out->filehandle);
    fseek((FILE*)out->filehandle, 0L, SEEK_SET);
    out->data_ptr = mmap(0, out->filesize, PROT_READ, MAP_SHARED, fileno((FILE*)out->filehandle), 0);
    if((uint8_t*)out->data_ptr == (uint8_t*)MAP_FAILED){
        out->err_state = 2;// couldn't mmap
        return out;
    }

    uint8_t *ptr8;
    ptr8 = (uint8_t*)out->data_ptr;
    const char * header = "RIFF";
    const char * type = "WAVE";
    if(compare_check((void*)ptr8, (void*)header, strlen(header))
            || compare_check((void*)&ptr8[8], (void*)type, strlen(type))){
        out->err_state = 3;// not a wav file
        return out;
    }

    int64_t fmt_offset = 0;
    fmt_offset = find_flag(out, 12, "fmt ");
    out->data_offset = find_flag(out, 12, "data");
    if(fmt_offset == -1 || out->data_offset == -1){
        out->err_state = 4;// couldn't find reqired flags
        return out;
    }

    if(*((uint32_t*)(&ptr8[fmt_offset+4])) != 16){
        out->err_state = 5; // something is wrong with the fmt section
        return out;
    }
    out->audio_fmt = *((uint16_t*)(&ptr8[fmt_offset+8]));
    out->num_channels = *((uint16_t*)(&ptr8[fmt_offset+10]));
    out->sample_rate = *((uint32_t*)(&ptr8[fmt_offset+12]));
    out->byte_rate = *((uint32_t*)(&ptr8[fmt_offset+16]));
    out->block_align = *((uint16_t*)(&ptr8[fmt_offset+20]));
    out->bit_depth = *((uint16_t*)(&ptr8[fmt_offset+22]));

    if(out->audio_fmt != 1){
        out->err_state = 0xF0;//don't know how to handle this format
        return out;
    }

    out->data_len = (size_t) *((uint32_t*)(&ptr8[out->data_offset+4]));
    // out->data_type = uint8_t(int((float)(out->bit_depth)/8.0f));
    out->data_type = (uint8_t)((float)(out->bit_depth))/8.0f;
    out->data_offset += 8;

    return out;
}
#endif

int close_wav(wav_obj* wav_ptr){
    if (wav_ptr == NULL) return 1;
    if (*wav_ptr == NULL) return 1;
    wav_obj wav = *wav_ptr;
    if(wav->filehandle != NULL){
        fclose((FILE*)wav->filehandle);
        wav->filehandle = NULL;
    }
    if(wav->data_ptr != NULL){
        munmap(wav->data_ptr,wav->filesize);
    }
    return 0;
}

void destroy_wav_obj(wav_obj* wav_ptr){
    if(close_wav(wav_ptr)) return;
    wav_obj wav = *wav_ptr;
    if(wav->filepath != NULL){
        free(wav->filepath);
        wav->filepath = NULL;
    }
    free(*wav_ptr);
    wav_ptr = NULL;
}

void print_wav_info(wav_obj wav, uint8_t mode, uint32_t offset, int32_t length){
    #ifdef __cplusplus
    if (mode == 0){
        std::cout << wav->filepath;
        std::cout << "\n  err_state:      " << (int)wav->err_state;
        std::cout << "\n  filesize:       " << wav->filesize;
        std::cout << "\n  audio_fmt:      " << (int)wav->audio_fmt;
        std::cout << "\n  num_channels:   " << (int)wav->num_channels;
        std::cout << "\n  sample_rate:    " << (int)wav->sample_rate;
        std::cout << "\n  byte_rate:      " << (int)wav->byte_rate;
        std::cout << "\n  block_align:    " << (int)wav->block_align;
        std::cout << "\n  bit_depth:      " << (int)wav->bit_depth;
        std::cout << "\n  data_type:      " << (int)wav->data_type;
        std::cout << "\n  data_offset:    " << wav->data_offset;
        std::cout << "\n  data_len:       " << wav->data_len;
    }
    else{
        std::cout << &wav->filepath[strlen(wav->filepath)-20];
        std::cout << " " << (int)wav->err_state;
        std::cout << " " << wav->filesize;
        std::cout << " " << (int)wav->audio_fmt;
        std::cout << " " << (int)wav->num_channels;
        std::cout << " " << (int)wav->sample_rate;
        std::cout << " " << (int)wav->byte_rate;
        std::cout << " " << (int)wav->block_align;
        std::cout << " " << (int)wav->bit_depth;
        std::cout << " " << (int)wav->data_type;
        std::cout << " " << wav->data_offset;
        std::cout << " " << wav->data_len;
    }
    std::cout << std::endl;
    if(mode == 2){
        std::vector<float> data(2*length,0.);
        int count = index_wav(wav, offset, length, &data[0]);
        size_t len = (count > 0) ? count : 0;
        for(size_t idx = 0; idx < len; idx++){
            std::cout << " " << data[2*idx];
        }
        std::cout << std::endl;
        for(size_t idx = 0; idx < len; idx++){
            std::cout << " " << data[2*idx+1];
        }
        std::cout << std::endl;
    }
    #endif
}

int index_wav(wav_obj wav, uint32_t start_idx, uint32_t stereo_samples, float* out_ptr){
    if (wav == NULL) return -1;
    if (out_ptr == NULL) return -2;
    uint8_t bytes_per_sample = wav->block_align;
    float scaling = 512;
    float shift = -1.0;
    if (wav->data_type == 2){
        scaling = 32768.0;
        shift = 0.0;
    }
    else if (wav->data_type == 4){
        scaling = 2147483648.0;
        shift = 0.0;
    }
    else if (wav->data_type != 1){
        return -3;
    }
    scaling = 1.0/scaling;
    size_t len = wav->data_len/bytes_per_sample;
    size_t offset = 0;
    uint32_t xfer = 0;
    float samp[2];
    uint8_t *data_ptr = &((uint8_t*)wav->data_ptr)[wav->data_offset];
    while(xfer < stereo_samples){
        samp[0] = 0.;
        samp[1] = 0.;
        if(start_idx+xfer >= len){
            if (xfer >= len) break;
            start_idx = 0;
            offset = 0;
        }
        switch(wav->data_type){
            case 1:
                samp[0] = scaling*(float)(data_ptr[start_idx*bytes_per_sample + offset++])+shift;
                if(wav->num_channels > 1){
                    samp[1] = scaling*(float)(data_ptr[start_idx*bytes_per_sample + offset++])+shift;
                }
                else{
                    //assume it's mono and just dumbly make it stereo
                    samp[0] /= 2.0f;
                    samp[1] = samp[0];
                }
                break;
            case 2:
                samp[0] = scaling*(float)(*((int16_t*)&data_ptr[start_idx*bytes_per_sample + offset]));
                offset += 2;
                if(wav->num_channels > 1){
                    samp[1] = scaling*(float)(*((int16_t*)&data_ptr[start_idx*bytes_per_sample + offset]));
                    offset += 2;
                }
                else{
                    //assume it's mono and just dumbly make it stereo
                    samp[0] /= 2.0f;
                    samp[1] = samp[0];
                }
                break;
            case 4:
                samp[0] = scaling*(float)(*((int32_t*)&data_ptr[start_idx*bytes_per_sample + offset]));
                offset += 4;
                if(wav->num_channels > 1){
                    samp[1] = scaling*(float)(*((int32_t*)&data_ptr[start_idx*bytes_per_sample + offset]));
                    offset += 4;
                }
                else{
                    //assume it's mono and just dumbly make it stereo
                    samp[0] /= 2.0f;
                    samp[1] = samp[0];
                }
                break;
            default:
                return -3;
                break;
        }
        out_ptr[2*xfer+0] = samp[0];
        out_ptr[2*xfer+1] = samp[1];
        xfer++;
    }
    return xfer;
}

void load_files(char **filepaths, int file_count, wav_reader wav){
    if(wav == NULL) return;
    if(wav->wavs != NULL){
        for(uint8_t idx = 0; idx < wav->file_count; idx++)
            destroy_wav_obj(&wav->wavs[idx]);
        free(wav->wavs);
    }
    wav->file_count = file_count;
    wav->wavs = (wav_obj*)malloc(file_count*sizeof(wav_obj));
    for(int idx = 0; idx < file_count; idx++){
        wav->wavs[idx] = open_wav(filepaths[idx]);
    }
}

#ifdef __cplusplus
int find_default_valid(char **&filepaths){
#else
int find_default_valid(char **filepaths){
#endif
    if(filepaths != NULL) return -1;
    int valid_files = 0;
    #ifdef __cplusplus
    char* default_path = std::getenv("WFGEN_AUDIO_FOLDER");
    std::vector<std::string> good_files(0);
    if(default_path != NULL){
        glob_t globbuf;
        std::string pattern = std::string(default_path)+"/combined_audio*.wav";
        if(0==glob(pattern.c_str(), 0, NULL, &globbuf)){
            valid_files = globbuf.gl_pathc;
            if(valid_files == 0){
                globfree(&globbuf);
                pattern = std::string(default_path)+"/*.wav";
                if(0==glob(pattern.c_str(), 0, NULL, &globbuf)){
                    valid_files = globbuf.gl_pathc;
                }
            }
        }
        if(valid_files > 0){
            wav_obj wav;
            for(uint8_t idx = 0; idx < globbuf.gl_pathc; idx++){
                wav = open_wav(globbuf.gl_pathv[idx]);
                if(wav == NULL){
                    valid_files--;
                }
                else{
                    if (wav->err_state != 0){
                        valid_files--;
                    }
                    good_files.emplace_back(globbuf.gl_pathv[idx]);
                }
                close_wav(&wav);
            }
        }
        globfree(&globbuf);
    }
    if(valid_files){
        filepaths = (char**) malloc(valid_files*sizeof(char*));
        for(uint32_t idx = 0; idx < good_files.size(); idx++){
            filepaths[idx] = (char*)malloc(good_files[idx].size()+1);
            memcpy(filepaths[idx], good_files[idx].data(), good_files[idx].size());
            filepaths[idx][good_files[idx].size()] = 0;
        }
    }
    #endif
    return valid_files;
}

#ifdef __cplusplus
wav_reader wav_reader_create(){
#else
wav_reader wav_reader_create_default(){
#endif
    wav_reader wav = (wav_reader)malloc(sizeof(wav_reader_t));
    wav->buffer = NULL;
    wav->file_count = 0;
    wav->file_idx = 0;
    wav->itemsize = 1;
    wav->offsets = NULL;
    wav->prefetch = 0;
    wav->read_as = ALL;//assume by default for now
    wav->sample_length = 0;
    wav->processed = 0;
    wav->wavs = NULL;
    char** filepaths = NULL;
    int file_count = find_default_valid(filepaths);
    load_files(filepaths,file_count,wav);
    wav->prefetch = 20000;
    wav->file_idx = 0;
    wav->itemsize = wav_reader_sample_bytes(wav);
    wav->sample_length = 0;
    size_t byte_length = 0;
    size_t bytes_per_sample = 1;
    size_t sample_len = 0;
    for(uint8_t idx = 0; idx < wav->file_count; idx++){
        byte_length = wav->wavs[idx]->data_len;
        bytes_per_sample = wav->wavs[idx]->block_align;
        sample_len = byte_length/bytes_per_sample;
        wav->sample_length += sample_len;
    }
    wav->offsets = (uint32_t*) malloc(file_count*sizeof(uint32_t));
    memset(wav->offsets, 0, file_count*sizeof(uint32_t));
    wav->buffer = cbufferf_create(wav->prefetch);
    wav_reader_fill_buffer(wav);
    return wav;
}

wav_reader wav_reader_create(wav_mode_t mode, uint8_t file_count, char **filepaths, uint32_t prefetch){
    if (!(mode == MONO || mode == STEREO)){
        return NULL;
    }
    wav_reader wav = (wav_reader)malloc(sizeof(wav_reader_t));
    wav->buffer = NULL;
    wav->file_count = 0;
    wav->file_idx = 0;
    wav->itemsize = 1;
    wav->offsets = NULL;
    wav->prefetch = 0;
    wav->read_as = ALL;//assume by default for now
    wav->sample_length = 0;
    wav->processed = 0;
    wav->wavs = NULL;
    load_files(filepaths,file_count,wav);
    wav->prefetch = prefetch;
    wav->file_idx = 0;
    wav->itemsize = wav_reader_sample_bytes(wav);
    wav->sample_length = 0;
    size_t byte_length = 0;
    size_t bytes_per_sample = 1;
    size_t sample_len = 0;
    for(uint8_t idx = 0; idx < wav->file_count; idx++){
        byte_length = wav->wavs[idx]->data_len;
        bytes_per_sample = wav->wavs[idx]->block_align;
        sample_len = byte_length/bytes_per_sample;
        wav->sample_length += sample_len;
    }
    wav->offsets = (uint32_t*) malloc(file_count*sizeof(uint32_t));
    memset(wav->offsets, 0, file_count*sizeof(uint32_t));
    wav->buffer = cbufferf_create(wav->prefetch*(wav->itemsize/sizeof(float)));
    wav_reader_fill_buffer(wav);
    return wav;
}

void wav_reader_destroy(wav_reader *wav){
    if (wav == NULL) return;
    if (*wav == NULL) return;
    if ((*wav)->wavs != NULL){
    for(int idx = 0; idx < (*wav)->file_count; idx++){
            destroy_wav_obj(&((*wav)->wavs[idx]));
        }
        free((*wav)->wavs);
    }
    if ((*wav)->offsets != NULL) free((*wav)->offsets);
    cbufferf_destroy((*wav)->buffer);
    free((*wav));
    *wav = NULL;
}

uint8_t wav_reader_sample_bytes(wav_reader wav){
    // how many bytes is a sample in the reader's internal buffer
    if(wav == NULL) return 0;
    if(wav->wavs == NULL) return 0;
    uint8_t reader_sample_bytes = sizeof(float);/// going to produce floats
    if(wav->read_as == ALL) reader_sample_bytes *= 2;
    return reader_sample_bytes;
}

uint64_t wav_reader_fill_buffer(wav_reader wav){
    // fills the reader buffer (float*) with {prefetch} samples
    if(wav->read_as != ALL){
        #ifdef __cplusplus
        throw std::runtime_error("wav_reader is in an unexpected read_as mode.");
        #else
        return 0;
        #endif
    }
    uint64_t loaded = 0;
    uint64_t floats_available = cbufferf_space_available(wav->buffer);//if read_as is ALL, then itemsize = 2*float
    uint8_t scaler = wav->itemsize/sizeof(float);// sooo this should be 2 if read_as==ALL
    uint64_t samples_available = floats_available/scaler;
    float sample[2];
    // int count = 0;
    uint64_t file_limit = wav->wavs[wav->file_idx]->data_len/wav->wavs[wav->file_idx]->block_align;
    while(loaded < samples_available){
        // index_wav produces stereo output no matter what format the file is
        index_wav(wav->wavs[wav->file_idx], wav->offsets[wav->file_idx],1,sample);

        cbufferf_push(wav->buffer,sample[0]);
        cbufferf_push(wav->buffer,sample[1]);
        wav->offsets[wav->file_idx]++;  // one more sample read
        loaded++;                       // one more sample loaded
        if(wav->offsets[wav->file_idx] == file_limit){
            wav->file_idx++;
            if(wav->file_idx == wav->file_count) wav->file_idx = 0;
            wav->offsets[wav->file_idx] = 0;
            wav->itemsize = wav_reader_sample_bytes(wav); // at the moment this shouldn't change for any wav_file
        }
    }
    return loaded;
}

#ifdef __cplusplus
uint64_t wav_reader_data_ptr(wav_reader wav, float* &ptr){
#else
uint64_t wav_reader_data_ptr(wav_reader wav, float* ptr){
#endif
    // set the ptr to where to read from, and return number of samples in buffer
    uint64_t max_len = cbufferf_max_read(wav->buffer); // floats
    uint32_t max_read = 0;
    uint8_t scaler = wav->itemsize/sizeof(float);
    cbufferf_read(wav->buffer, max_len, &ptr, &max_read); // floats
    return max_read/scaler; // samples
}

uint64_t wav_reader_advance(wav_reader wav, uint64_t samples){
    // set the ptr to where to read from, and return number of samples in buffer
    uint8_t scaler = wav->itemsize/sizeof(float);
    if(cbufferf_release(wav->buffer,samples*scaler)){
        return 0; // something went wrong, likeyl samples*scaler > buffer_max_size
    }
    wav->processed += samples;
    wav_reader_fill_buffer(wav);
    return samples;
}

int wav_reader_set_offset(wav_reader wav, uint64_t offset){
    //set the offset in terms of samples held by the reader
    if(offset > wav->sample_length) return 1;
    // printf("Requesting an offset of %lu with a maximum limit of %lu\t%lu/%lu\n",offset,wav->sample_length,offset,wav->sample_length);
    memset(wav->offsets, 0, sizeof(uint32_t)*wav->file_count);
    uint64_t filesize = 0;
    // uint64_t decr = 0;
    uint8_t fidx = 0;
    uint8_t scaler = wav->itemsize / sizeof(float); // should allways be two for now
    assert(scaler == 2);
    size_t byte_len;
    size_t bytes_per_sample;
    size_t sample_len;
    while( offset > 0 ){
        //// (A) number of data floats in the file : wav->wavs[fidx]->data_len
        //// (B) bytes per sample in the data : wav->wavs[fidx]->block_align
        //// (C) number of floats in a sample : 2
        //// (D) number of samples in the file: A/B
        byte_len = wav->wavs[fidx]->data_len;
        bytes_per_sample = wav->wavs[fidx]->block_align;
        sample_len = byte_len/bytes_per_sample;
        filesize = sample_len;// number of samples
        if(offset > filesize){
            offset -= filesize;
            // store in terms of float items
            wav->offsets[fidx] = filesize;// num_samps
            fidx++;
        }
        else{
            wav->offsets[fidx] = offset;
            offset = 0;
        }
    }
    wav->file_idx = fidx;
    cbufferf_reset(wav->buffer); // clear out any old cached buffer since it's out of date now
    wav_reader_fill_buffer(wav); // now fill it back up.
    return 0;
}

uint64_t wav_reader_get_len(wav_reader wav){
    // get the number of samples in the reader
    return wav->sample_length;
}

uint64_t wav_reader_get_offset(wav_reader wav){
    // get offset in number of floats
    uint64_t offset = 0;
    // uint8_t scaler = wav->itemsize / sizeof(float);
    for(uint8_t idx = 0; idx <= wav->file_idx; idx++){
        // offset += (wav->offsets[idx] * scaler);
        offset += wav->offsets[idx];
    }
    return offset;
}

uint64_t wav_reader_samples_processed(wav_reader wav){
    // This is the number of samples extracted by the reader into it's own buffer
    uint8_t scaler = wav->itemsize / sizeof(float);
    return wav->processed + cbufferf_max_size(wav->buffer)/scaler;
}
///////WAV END
