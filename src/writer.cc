
#include "writer.hh"

#ifdef __cplusplus
#include <cstring>
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

thread_state thread_state_create(){
    return thread_state_create_empty();
}

#ifdef __cplusplus
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


writer writer_create_empty(){
    writer w = (writer)malloc(sizeof(writer_t));
    memset(w, 0, sizeof(writer_t));
    return w;
}
writer writer_create(write_mode_t _mode, const char* _filepath, uint8_t _threaded){
    writer w = writer_create_empty();
    w->kind = _mode;
    uint32_t flen = strlen(_filepath);
    if (flen > FILENAME_MAX){
        printf("Invalid filepath provided.\n");
        exit(1);
    }
    w->filename = (char*)malloc(flen+1);
    memcpy(w->filename,_filepath,flen+1);
    w->n_threads = _threaded;
    w->fptr = fopen(w->filename,"wb");
    return w;
}
void writer_destroy(writer *w){
    if (w == NULL) return;
    if (*w == NULL) return;
    for (uint8_t tidx = 0; tidx < (*w)->n_threads; tidx++){
        thread_state_stop(&(*w)->_state[tidx]);
    }
    for (uint8_t tidx = 0; tidx < (*w)->n_threads; tidx++){
        thread_state_join(&(*w)->_state[tidx]);
    }
    writer_close(*w);
    free(*w);
}
uint64_t writer_store(writer w, container c){
    if(w->n_threads){}
    else{
        //block until written
        fwrite(c->ptr, get_empty_content_size(c->type), c->size, w->fptr);
    }
    return 0;
}
uint64_t writer_store_head(writer w, container c, uint64_t head){
    if(w->fptr == NULL) return 0;
    size_t tru = fwrite(c->ptr, get_empty_content_size(c->type), head, w->fptr);
    return tru;
}
uint64_t writer_store_tail(writer w, container c, uint64_t tail){
    if(w->fptr == NULL) return 0;
    uint8_t *qptr = (uint8_t*)c->ptr;
    size_t itemsize = get_empty_content_size(c->type);
    size_t bytes = tail*itemsize;
    size_t offset = c->size*itemsize-bytes;
    size_t tru = fwrite(&qptr[offset], itemsize, tail, w->fptr);
    return tru;
}
uint64_t writer_store_range(writer w, container c, uint64_t skip, uint64_t cut){
    if(w->fptr == NULL) return 0;
    uint8_t *qptr = (uint8_t*)c->ptr;
    size_t itemsize = get_empty_content_size(c->type);
    size_t offset = skip*itemsize;
    size_t items = (cut-skip);
    size_t tru = fwrite(&qptr[offset], itemsize, items, w->fptr);
    return cut-skip;
}
void writer_close(writer w){
    if(w->fptr == NULL) return;
    fclose(w->fptr);
    w->fptr = NULL;
}






#ifdef __cplusplus
}}}
#endif