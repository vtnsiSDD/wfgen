
#include <stdio.h>
#include "containers.hh"

#ifdef __cplusplus
extern "C" {
namespace wfgen{
namespace containers{
#endif
int test_creation_cycle_empty(){
    container c = container_create_empty();
    int results = (c == NULL);
    if(results) return results;
    container_destroy(&c);
    return (c != NULL)*2;
}

int test_null_creation(){
    for (uint8_t content_type=1; content_type < 29; content_type++){
        container c = container_create(content_type,0,NULL);
        if(c == NULL) return content_type;
        container_destroy(&c);
        if(c!=NULL) return -1;
    }
    return 0;
}

int test_null_ptr_creation(){
    for (uint8_t content_type=1; content_type < 29; content_type++){
        container c = container_create(POINTER | content_type,0,NULL);
        if(c == NULL) return content_type;
        container_destroy(&c);
        if(c!=NULL) return -1;
    }
    return 0;
}

int test_small_creation(){
    for (uint8_t content_type=1; content_type < 29; content_type++){
        container c = container_create(content_type,4096,NULL);
        if(c == NULL) return content_type;
        container_destroy(&c);
        if(c!=NULL) return -1;
    }
    return 0;
}

int test_large_creation(){
    for (uint8_t content_type=1; content_type < 29; content_type++){
        container c = container_create(content_type,4U*1024U*1024U*1024U,NULL);
        if(c == NULL) return content_type;
        container_destroy(&c);
        if(c!=NULL) return -1;
    }
    return 0;
}

int test_item_owner_creation(){
    uint64_t data_point = 1337;
    container c = container_create(OWNER | UINT64,0,&data_point);
    if(c == NULL) return 3;
    // printf("size(%lu), data: %lu, container: %lu, <addr>container %p, <addr>c->ptr %p\n",c->size,data_point,*(uint64_t*)(c->ptr), c, c->ptr);
    int res = *(uint64_t *)(c->ptr) != data_point;
    container_destroy(&c);
    if(c!=NULL) return -1-res;
    return res;
}

int test_pointer_owner_creation(){
    uint64_t data_point = 1337;
    void *ptr = &data_point;
    container c = container_create(OWNER | POINTER | UINT64, 1, ptr);
    if(c == NULL) return 3;
    // printf("size(%lu), data: %lu, container: %lu, <addr>container %p, <addr>c->ptr %p\n",c->size,data_point,*(uint64_t*)(c->ptr), c, c->ptr);
    int res = *(uint64_t *)(c->ptr) != data_point;
    container_destroy(&c);
    if(c!=NULL) return -1-res;
    return res;
}

int test_pointer_owner_wrong_size(){
    uint64_t data_point = 1337;
    void *ptr = &data_point;
    container c = container_create(OWNER | POINTER | UINT64, 0, ptr);
    if(c == NULL) return 3;
    // printf("size(%lu), data: %lu, container: %lu, <addr>container %p, <addr>c->ptr %p\n",c->size,data_point,*(uint64_t*)(c->ptr), c, c->ptr);
    int res = *(uint64_t *)(c->ptr) == data_point;
    container_destroy(&c);
    if(c!=NULL) return -1-res;
    return res;
}

int test_pointer_info_checker(){
    uint8_t data_point[4] = {1, 3, 3, 7};
    container c = container_create(POINTER | UINT8, 4, data_point);
    if(c == NULL) return 3;
    uint64_t items = 0, itemsize = 0, binsize = 0;
    container_get_info(c, &items, &itemsize, &binsize);
    // printf("expect items: %lu, itemsize: %lu, binsize: %lu\n",4lu,sizeof(uint8_t),sizeof(container_t));
    // printf("   got items: %lu, itemsize: %lu, binsize: %lu\n",items,itemsize,binsize);
    int res = (items != 4u);
    res += (itemsize != 1u);
    res += (binsize != sizeof(container_t));
    container_destroy(&c);
    if(c!=NULL) return 1+res;
    return res;
}

int test_pointer_info_checker_owner(){
    uint8_t data_point[4] = {1, 3, 3, 7};
    container c = container_create(OWNER | POINTER | UINT8, 4, data_point);
    if(c == NULL) return 3;
    uint64_t items = 0, itemsize = 0, binsize = 0;
    container_get_info(c, &items, &itemsize, &binsize);
    // printf("expect items: %lu, itemsize: %lu, binsize: %lu\n",4lu,sizeof(uint8_t),sizeof(container_t)+4*sizeof(uint8_t));
    // printf("   got items: %lu, itemsize: %lu, binsize: %lu\n",items,itemsize,binsize);
    int res = (items != 4);
    res += (itemsize != 1);
    res += (binsize != sizeof(container_t) + 4);
    container_destroy(&c);
    if(c!=NULL) return 1+res;
    return res;
}

int test_info_unspecified_owner(){
    uint16_t data_point[4] = {1, 3, 3, 7};
    container c = container_create(OWNER | UNSPECIFIED, 4*sizeof(uint16_t), data_point);
    if(c == NULL) return 3;
    uint64_t items = 0, itemsize = 0, binsize = 0;
    container_get_info(c, &items, &itemsize, &binsize);
    printf("expect items: %lu, itemsize: %lu, binsize: %lu\n",1lu,4*sizeof(uint16_t),sizeof(container_t)+4*sizeof(uint16_t));
    printf("   got items: %lu, itemsize: %lu, binsize: %lu\n",items,itemsize,binsize);
    int res = (items != 1);
    res += (itemsize != 8);
    res += (binsize != sizeof(container_t) + 8);
    container_destroy(&c);
    if(c!=NULL) return 1+res;
    return res;
}

int test_info_unspecified_pointer_owner(){
    uint16_t data_point[4] = {1, 3, 3, 7};
    container c = container_create(OWNER | POINTER | UNSPECIFIED, 4*sizeof(uint16_t), data_point);
    if(c == NULL) return 3;
    uint64_t items = 0, itemsize = 0, binsize = 0;
    container_get_info(c, &items, &itemsize, &binsize);
    printf("expect items: %lu, itemsize: %lu, binsize: %lu\n",1lu,4*sizeof(uint16_t),sizeof(container_t)+4*sizeof(uint16_t));
    printf("   got items: %lu, itemsize: %lu, binsize: %lu\n",items,itemsize,binsize);
    int res = (items != 1);
    res += (itemsize != 8);
    res += (binsize != sizeof(container_t) + 8);
    container_destroy(&c);
    if(c!=NULL) return 1+res;
    return res;
}

int main(){
    int res=0;
    if((res+=test_creation_cycle_empty())){
        printf("Test Creation Cycle Empty -- Failed(%d)\n",res);
    }
    else{
        printf("Test Creation Cycle Empty -- Passed\n");
    }
    if((res+=test_null_creation())){
        printf("Test Null Creation -- Failed(%d)\n",res);
    }
    else{
        printf("Test Null Creation -- Passed\n");
    }
    if((res+=test_small_creation())){
        printf("Test Small Creation -- Failed(%d)\n",res);
    }
    else{
        printf("Test Small Creation -- Passed\n");
    }
    if((res+=test_large_creation())){
        printf("Test Large Creation -- Failed(%d)\n",res);
    }
    else{
        printf("Test Large Creation -- Passed\n");
    }
    if((res+=test_item_owner_creation())){
        printf("Test Item Owner Creation -- Failed(%d)\n",res);
    }
    else{
        printf("Test Item Owner Creation -- Passed\n");
    }
    if((res+=test_pointer_owner_creation())){
        printf("Test Pointer Owner Creation -- Failed(%d)\n",res);
    }
    else{
        printf("Test Pointer Owner Creation -- Passed\n");
    }
    if((res+=test_pointer_owner_wrong_size())){
        printf("Test Pointer Owner Wrong Size -- Failed(%d)\n",res);
    }
    else{
        printf("Test Pointer Owner Wrong Size -- Passed\n");
    }
    if((res+=test_pointer_info_checker())){
        printf("Test Get Info -- Failed(%d)\n",res);
    }
    else{
        printf("Test Get Info -- Passed\n");
    }
    if((res+=test_pointer_info_checker_owner())){
        printf("Test Get Info -- Failed(%d)\n",res);
    }
    else{
        printf("Test Get Info -- Passed\n");
    }
    if((res+=test_info_unspecified_owner())){
        printf("Test Get Info Undefined -- Failed(%d)\n",res);
    }
    else{
        printf("Test Get Info Undefined -- Passed\n");
    }
    if((res+=test_info_unspecified_pointer_owner())){
        printf("Test Get Info Undefined Pointer -- Failed(%d)\n",res);
    }
    else{
        printf("Test Get Info Undefined Pointer -- Passed\n");
    }
    return res;
}

#ifdef __cplusplus
}}}
#endif