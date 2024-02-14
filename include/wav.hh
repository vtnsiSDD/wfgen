
#ifndef WAV_HH
#define WAV_HH

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <vector>
#include "liquid.h"

typedef enum{
    NONE=-1,
    LEFT,
    RIGHT,
    MONO,
    STEREO,
    ALL,
    RDS
} wav_mode_t;

typedef struct wav_obj_s{
    uint8_t err_state;
    char* filepath;
    void* filehandle;
    size_t filesize;
    uint16_t audio_fmt;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint32_t block_align;
    uint32_t bit_depth;
    uint8_t data_type;//itemsize
    int64_t data_offset;
    size_t data_len;
    void* data_ptr;
} wav_obj_t;
typedef wav_obj_t* wav_obj;


typedef struct wav_reader_s{
    uint8_t file_count;
    uint8_t file_idx;
    uint8_t itemsize;
    uint64_t sample_length;
    uint64_t processed;
    uint32_t prefetch;
    wav_mode_t read_as;
    wav_obj *wavs;
    uint32_t *offsets;
    cbufferf buffer;
} wav_reader_t;
typedef wav_reader_t* wav_reader;



wav_obj open_wav(std::string fp);
int close_wav(wav_obj* wav_ptr);
void destroy_wav_obj(wav_obj* wav_ptr);
void print_wav_info(wav_obj wav, uint8_t mode, uint32_t offset=0, int32_t length=25);
int index_wav(wav_obj wav, uint32_t start_idx, uint32_t stereo_samples, float* out_ptr);

wav_reader wav_reader_create();
wav_reader wav_reader_create(wav_mode_t mode, uint8_t file_count, char **filepaths, uint32_t prefetch=20000);
void wav_reader_destroy(wav_reader *wav);
uint8_t wav_reader_sample_bytes(wav_reader wav);
uint64_t wav_reader_fill_buffer(wav_reader wav);
uint64_t wav_reader_data_ptr(wav_reader wav, float* &ptr);
uint64_t wav_reader_advance(wav_reader wav, uint64_t samples);
int wav_reader_set_offset(wav_reader wav, uint64_t offset);
uint64_t wav_reader_get_len(wav_reader wav);
uint64_t wav_reader_get_offset(wav_reader wav);
uint64_t wav_reader_samples_processed(wav_reader wav);

#endif // WAV_HH
