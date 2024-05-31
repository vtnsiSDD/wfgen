#ifndef FORWARD_HH
#define FORWARD_HH

#ifdef __cplusplus
namespace wfgen{
namespace containers{
extern "C" {
#endif
struct container_s;
struct cburst_s;
struct waveform_s;
typedef struct container_s container_t;
typedef struct cburst_s cburst_t;
typedef struct waveform_s waveform_t;
typedef container_t * container;
typedef cburst_t * cburst;
typedef waveform_t * waveform;
#ifdef __cplusplus
}}
#endif

#ifdef __cplusplus
namespace writer{
extern "C" {
#endif
struct thread_state_s;
struct writer_s;
typedef struct thread_state_s thread_state_t;
typedef struct writer_s writer_t;
typedef thread_state_t * thread_state;
typedef writer_t * writer;
#ifdef __cplusplus
}}}
#endif

#endif // FORWARD_HH