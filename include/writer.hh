#ifndef WRITER_HH
#define WRITER_HH

#include <stdio.h>
#include <stdlib.h>
#include "forward.hh"
#include "containers.hh"

#ifdef __cplusplus
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
namespace wfgen{
using namespace containers;
namespace writer{
extern "C" {
#else
#include <pthread.h>
#include <string.h>
#endif

typedef enum write_mode_e{
    WRITER_RAM,
    WRITER_IQ,
    WRITER_SIGMF,
    WRITER_BURST,
    WRITER_OVERWRITE=0x80
} write_mode_t;


#ifndef __cplusplus
#pragma pack(push, 1)
typedef struct thread_state_s{
    uint8_t         state;
    pthread_mutex_t *mutex;
    pthread_cond_t  *cv;
    FILE            *fptr;
    writer          _writer;
} thread_state_t;
typedef struct writer_s{
    uint8_t         kind;
    uint8_t         n_threads;
    char*           filename;
    FILE            *fptr;
    pthread_t       *thread;
    thread_state    _state;
} writer_t;
typedef writer_t * writer;
#pragma pack(pop)
#endif

thread_state thread_state_create_empty();
thread_state thread_state_create();
void thread_state_start(thread_state thread_state);
void thread_state_stop(thread_state thread_state);
void thread_state_join(thread_state thread_state);
uint8_t thread_state_is_joinable(thread_state thread_state);
void thread_state_destroy(thread_state *thread_state);


writer writer_create_empty();
writer writer_create(write_mode_t _mode, const char *_filepath, uint8_t _threaded);
uint64_t writer_store(writer w, container c);
uint64_t writer_store_head(writer w, container c, uint64_t len);
uint64_t writer_store_tail(writer w, container c, uint64_t tail);
uint64_t writer_store_range(writer w, container c, uint64_t skip, uint64_t cut);
void writer_close(writer w);



void writer_start_thread(writer w);
void writer_stop_thread(writer w);
void writer_join_thread(writer w);
void writer_is_joinable(writer w);
void writer_destroy(writer *w);

uint8_t writer_write_burst_info(writer w, cburst b);
uint8_t writer_write_waveform_info(writer w, waveform wf);
uint8_t writer_write_burst_iq(writer w, cburst b, fc32 *iq);
uint8_t writer_write_burst_to_waveform(writer w, cburst b, waveform wf, uint64_t *burst_index);


#ifdef __cplusplus
}
#pragma pack(push, 1)
typedef struct thread_state_s{
    uint8_t                 state;
    std::mutex              *mutex;
    std::condition_variable *cv;
    FILE                    *fptr;
    writer                  _writer;
} thread_state_t;
typedef struct writer_s{
    uint8_t         kind;
    uint8_t         n_threads;
    char*           filename;
    FILE            *fptr;
    std::thread     *thread;
    thread_state    _state;
} writer_t;
typedef writer_t * writer;
#pragma pack(pop)

}}
#endif

#endif // WRITER_HH