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
namespace wfgen{
using namespace containers;
namespace writer{
extern "C" {
#else
#include <pthread.h>
#endif
enum write_mode_t{
    WRITER_RAM,
    WRITER_IQ,
    WRITER_SIGMF,
    WRITER_BURST
};

struct thread_state_s;
typedef struct thread_state_s thread_state_t;
typedef thread_state_t * thread_state;
struct writer_s;
typedef struct writer_s writer_t;
typedef writer_t * writer;

#ifndef __cplusplus
typedef struct thread_state_s{
    uint8_t         state;
    pthread_mutex_t *mutex;
    pthread_cond_t  *cv;
    writer          _writer;
} thread_state_t;
#endif

thread_state thread_state_create_empty();
thread_state thread_state_create(uint8_t write_mode, const char* filename);
void thread_state_start(thread_state thread_state);
void thread_state_stop(thread_state thread_state);
void thread_state_join(thread_state thread_state);
uint8_t thread_state_is_joinable(thread_state thread_state);
void thread_state_destroy(thread_state *thread_state);

typedef struct writer_s{
    uint8_t         kind;
    char*           filename;
#ifndef __cplusplus
    pthread_t       *thread;
#else
    std::thread     *thread;
#endif
    thread_state    thread_state;
} writer_t;
typedef writer_t * writer;

writer writer_create_default();
writer writer_create(write_mode_t _mode, const char *_filepath, uint8_t _threaded);
void writer_start_thread(writer w);
void writer_stop_thread(writer w);
void writer_join_thread(writer w);
void writer_is_joinable(writer w);
void writer_destroy(writer *w);

uint8_t writer_write_burst_info(writer w, cburst b);
uint8_t writer_write_waveform_info(writer w, waveform wf);
uint8_t writer_write_burst_iq(writer w, cburst b, liquid_float_complex *iq);
uint8_t writer_write_burst_to_waveform(writer w, cburst b, waveform wf, uint64_t *burst_index);


#ifdef __cplusplus
}
typedef struct thread_state_s{
    uint8_t     state;
    std::mutex  *mutex;
    writer      _writer;
} thread_state_t;

thread_state thread_state_create(uint8_t write_mode, std::string filename);
}}
#endif

#endif // WRITER_HH