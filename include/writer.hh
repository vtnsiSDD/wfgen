#ifndef WRITER_HH
#define WRITER_HH


#include <fstream>
#include <thread>
#include <mutex>
#include "containers.hh"

#ifdef __cplusplus
namespace wfgen{
namespace writer{
extern "C" {
#endif
enum write_mode_t{
    WRITER_RAM,
    WRITER_IQ,
    WRITER_SIGMF,
    WRITER_BURST
};

typedef struct writer_thread_s{
    uint8_t                 state;
    char                    *filepath;
    uint8_t                 mode;
} writer_thread_t;
typedef writer_thread_t * writer_thread;

writer_thread writer_thread_create_empty();
writer_thread writer_thread_create(uint8_t write_mode, const char* filename);
void writer_thread_start(writer_thread thread_state);
void writer_thread_stop(writer_thread thread_state);
void writer_thread_join(writer_thread thread_state);
uint8_t writer_thread_is_joinable(writer_thread thread_state);
void writer_thread_destroy(writer_thread *thread_state);

typedef struct writer_s{
    uint8_t             kind;
    const char*         filename;
    pthread_t           thread_id;
    writer_thread       thread_state;
} writer_t;
typedef writer_t * writer;

writer writer_create_default();
writer writer_create(write_mode_t _mode, const char *_filepath, void *_settings, uint8_t _threaded);
void writer_start_thread(writer w);
void writer_stop_thread(writer w);
void writer_join_thread(writer w);
void writer_is_joinable(writer w);
void writer_destroy(writer *w);


#ifdef __cplusplus
}

writer_thread writer_thread_create(uint8_t write_mode, std::string filename);
typedef struct thread_state_s{
    uint8_t state;
    std::mutex *mutex;
    std::thread *thread;
    uint8_t writer_mode;
    void *read_ptr;
    void *write_ptr;
} thread_state_t;
typedef thread_state_t * thread_state;
}}
#endif

#endif // WRITER_HH