
#include <iostream>
#include "writer.hh"


#ifdef __cplusplus
namespace wfgen{
namespace writer{

void writer_loop_ram(writer_thread thread_state)
{
    ////// don't really need to do anything
}

void writer_loop_iq(writer_thread thread_state)
{
    std::string filename = std::string(thread_state->filepath);
    std::ofstream file_handle(filename, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    while (thread_state->state & 0x80){
        ///// some buffer object to the file_handle is done here ---> only looking at IQ
        if(thread_state->state & 0x40){

        }
    }
}

void writer_loop_sigmf(writer_thread thread_state)
{
    ////// don't really need to do anything
}

void writer_loop_burst(writer_thread thread_state)
{
    ////// don't really need to do anything
}

extern "C" {

#endif



















writer_thread writer_thread_create_empty(){
    writer_thread wt = (writer_thread)malloc(sizeof(writer_thread_t));
    memset(wt, 0, sizeof(writer_thread_t));
    return wt;
}

#ifdef __cplusplus
}
writer_thread writer_thread_create(uint8_t mode, std::string filename){
    writer_thread wt = writer_thread_create_empty();
    wt->state = 0;
    wt->filepath = (char*)malloc(filename.size()+1);
    memset((char*)wt->filepath, 0, filename.size()+1);
    memcpy((char*)wt->filepath, filename.c_str(), filename.size());
    wt->mode = mode;
    return wt;
}
extern "C" {
#endif

void writer_thread_destroy(writer_thread *wt){
    if(wt == NULL) return;
    if(*wt == NULL) return;
    if(writer_thread_is_joinable(*wt)){
        writer_thread_stop(*wt);
        writer_thread_join(*wt);
    }
    if((*wt)->state & 0xC0) return; // need to stop this here I think
    if((*wt)->filepath != NULL) free((*wt)->filepath);
    free(*wt);
    *wt = NULL;
}
void writer_thread_start(writer_thread thread_state){

}
void writer_thread_stop(writer_thread thread_state){

}
void writer_thread_join(writer_thread thread_state){

}
uint8_t writer_thread_is_joinable(writer_thread thread_state){
 return 0;
}

#ifdef __cplusplus
}}}
#endif