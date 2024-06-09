
#include <stdio.h>
#include "containers.hh"

int test_creation_empty(){
    container c = container_create_empty();
    int results = (c == NULL);
    if(results) return results;
    container_destroy(&c);
    return (c != NULL)*2;
}


int main(){
    int res;
    if(res=test_creation_empty()){
        printf("Test Creation Empty -- Failed(%d)\n",res);
    }
    return 0;
}