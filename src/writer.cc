
#include <iostream>
#include "writer.hh"


#ifdef __cplusplus
namespace wfgen{
namespace writer{
extern "C" {
#endif


void writer_loop_ram(thread_state thread_state)
{
    ////// don't really need to do anything
}

void writer_loop_iq(thread_state thread_state)
{
}

void writer_loop_sigmf(thread_state thread_state)
{
    ////// don't really need to do anything
}

void writer_loop_burst(thread_state thread_state)
{
    ////// don't really need to do anything
}

thread_state thread_state_create_empty(){
    thread_state wt = (thread_state)malloc(sizeof(thread_state_t));
    memset(wt, 0, sizeof(thread_state_t));
    return wt;
}

#ifdef __cplusplus
}
thread_state thread_state_create(){
    return thread_state_create_empty();
}
extern "C" {
#endif

void thread_state_destroy(thread_state *wt){
    if(wt == NULL) return;
    if(*wt == NULL) return;
    if(thread_state_is_joinable(*wt)){
        thread_state_stop(*wt);
        thread_state_join(*wt);
    }
    if((*wt)->state & 0xC0) return; // need to stop this here I think
    free(*wt);
    *wt = NULL;
}
void thread_state_start(thread_state thread_state){

}
void thread_state_stop(thread_state thread_state){

}
void thread_state_join(thread_state thread_state){

}
uint8_t thread_state_is_joinable(thread_state thread_state){
 return 0;
}

#ifdef __cplusplus
}}}
#endif