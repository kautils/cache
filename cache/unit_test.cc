#ifdef TMAIN_KAUTIL_CACHE_INTERFACE


int tmain_kautil_cache_file_cache_static();
int main(){
    return tmain_kautil_cache_file_cache_static();
}

#include "kautil/cache/cache.hpp"
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

struct file_syscall_16b_pref{
    using value_type = uint64_t;
    using offset_type = long;

    int fd=-1;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~file_syscall_16b_pref(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type)*2; }
    offset_type size(){ fstat(fd,&st);return static_cast<offset_type>(st.st_size); }
    
    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        ::read(fd,*value,sizeof(value_type));
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        return size==::write(fd,*data,size);
    }
    
    // api may make a confusion but correct. this is because to avoid copy of value(object such as bigint) for future.
    bool read(offset_type const& from, void ** data, offset_type size){
        lseek(fd,from,SEEK_SET);
        return size==::read(fd,*data,size);
    }
    
    bool extend(offset_type extend_size){ fstat(fd,&st); return !ftruncate(fd,st.st_size+extend_size);   }
    int shift(offset_type dst,offset_type src,offset_type size){
        if(buffer_size < size){
            if(buffer)free(buffer);
            buffer = (char*) malloc(buffer_size = size);
        }
        lseek(fd,src,SEEK_SET);
        auto read_size = ::read(fd,buffer,size);
        lseek(fd,dst,SEEK_SET);
        ::write(fd,buffer,read_size);
        return 0;
    }
    
    int flush_buffer(){ return 0; }
};


struct file_capi_buffer_16b_pref{
    using value_type = uint64_t;
    using offset_type = long;
    
    FILE* f=0;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~file_capi_buffer_16b_pref(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type)*2; }
    offset_type size(){ fstat(fileno(f),&st);return static_cast<offset_type>(st.st_size); }
    
    bool read_value(offset_type const& offset, value_type ** value){
        fseek(f,offset,SEEK_SET);
        return sizeof(value_type) == fread(*value,1,sizeof(value_type),f);
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        fseek(f,offset,SEEK_SET);
        return size==::fwrite(*data,1,size,f);
    }
    
    // api may make a confusion but correct. this is because to avoid copy of value(object such as bigint) for future.
    bool read(offset_type const& offset, void ** data, offset_type size){
        fseek(f,offset,SEEK_SET);
        return size==::fread(*data,1,size,f);
    }
    
    bool extend(offset_type extend_size){ 
        fstat(fileno(f),&st);
        return !ftruncate(fileno(f),st.st_size+extend_size); 
    }
    
    
    int shift(offset_type dst,offset_type src,offset_type size){
        if(buffer_size < size){
            if(buffer)free(buffer);
            buffer = (char*) malloc(buffer_size = size);
        }
        fseek(f,src,SEEK_SET);
        auto read_size = ::fread(buffer,1,size,f);
        fseek(f,dst,SEEK_SET);
        auto write_size = fwrite(buffer,1,read_size,f);
        return 0;
    }
    
    int flush_buffer(){ return fflush(f); }
    
};

template<typename value_type,typename offset_type>
void debug_out_file(FILE* outto,int fd,offset_type from,offset_type to){
    struct stat st;
    fstat(fd,&st);
    auto cnt = 0;
    lseek(fd,0,SEEK_SET);
    auto start = from;
    auto size = st.st_size;
    value_type block[2];
    for(auto i = 0; i< size; i+=(sizeof(value_type)*2)){
        if(from <= i && i<= to ){
            lseek(fd,i,SEEK_SET);
            ::read(fd,&block, sizeof(value_type) * 2);
            //            read_block(i,block);
            printf("[%lld] %lld %lld\n",i,block[0],block[1]);fflush(outto);
        }
    }
}


#include <string>
#include <fcntl.h>
int mkdir_recurst(char * p);
int tmain_kautil_cache_file_cache_static() {
    
    
    
    auto dir = (char*)"tmain_kautil_cache_file_cache_static";
    auto fn = (char*)".cache";
    
    if(mkdir_recurst(dir)){
        fprintf(stderr,"fail to create %s",dir);
        abort();
    }
    auto path = std::string{dir} + "/" + fn;
    auto f = path.data();
    remove(f);
    
    auto fd = int(-1);
    {
        struct stat st;
        if(stat(f,&st)){
            fd = open(f,O_CREAT|O_BINARY|O_EXCL|O_RDWR,0755);
        }else{
            fd = open(f,O_RDWR|O_BINARY);
        }
    }
    
    
    using file_16_struct_type = file_syscall_16b_pref; 
    
    auto shift = 100;
    auto cnt = 0;
    for(auto i = 0; i < 100 ; ++i){
        auto beg = file_16_struct_type::value_type(cnt*10+shift);
        auto end = beg+10;
        
        auto cur = tell(fd);
        auto block_size = sizeof(file_16_struct_type::value_type);
        write(fd,&beg,block_size);
        lseek(fd,cur+block_size,SEEK_SET);
        write(fd,&end,block_size);
        lseek(fd,cur+(block_size*2),SEEK_SET);
        cnt+=2;
    }
    lseek(fd,0,SEEK_SET);
    
    {
        //file_syscall_16b_pref::value_type input[2] = {155,159}; // case : no extend  
        //file_syscall_16b_pref::value_type input[2] = {155,158}; // case : extend  
//        file_16_struct_type::value_type input[2] = {920,930}; // case : shrink  
        file_16_struct_type::value_type input[2] = {500,900}; // case2 : shrink  
        //auto pref = file_16_struct_type{.f=fdopen(fd,"r+b")};
        auto pref = file_16_struct_type{.fd=fd};
        auto a = kautil::cache{&pref};
        auto fw = a.merge(input);
        

        {// gap
            file_16_struct_type::value_type input[2] ={10,2000};  
//            file_16_struct_type::value_type input[2] ={10,90};  
//            file_16_struct_type::value_type input[2] ={890,925}; 
//            file_16_struct_type::value_type input[2] ={911,935}; 
//            file_16_struct_type::value_type input[2] ={920,950}; 
//            file_16_struct_type::value_type input[2] ={920,951}; 
            
//            file_16_struct_type::value_type input[2] ={925,927}; 
//            file_16_struct_type::value_type input[2] ={916,939}; 
//            file_16_struct_type::value_type input[2] ={911,916};  
//            file_16_struct_type::value_type input[2] ={931,939}; 
//            file_16_struct_type::value_type input[2] ={925,934}; 
//            file_16_struct_type::value_type input[2] ={916,945}; 
//            file_16_struct_type::value_type input[2] ={916,955}; 
//            file_16_struct_type::value_type input[2] ={925,945}; 
//            file_16_struct_type::value_type input[2] ={911,925}; 
//            file_16_struct_type::value_type input[2] ={916,925}; 
            
//            file_16_struct_type::value_type input[2] ={911,955}; 
//            file_16_struct_type::value_type input[2] ={911,945};  
//            file_16_struct_type::value_type input[2] ={925,955}; 
//            file_16_struct_type::value_type input[2] ={935,955}; 
//            file_16_struct_type::value_type input[2] ={1980,2000}; 
//            file_16_struct_type::value_type input[2] ={1990,1991};

            if(auto ctx = a.gap(input)){
                auto cur = ctx->begin;
                auto cnt = 0;
                for(;cur != ctx->end;++cur){
                  printf("%d ,,,%lld\n",cnt++%2,*cur);
                }
                a.gap_context_free(ctx);
            }else{
                printf("there is no gap\n");
            }
        }
        
        {// show result
//            debug_out_file<file_16_struct_type::value_type,file_16_struct_type::offset_type>(stdout,fd,0,3000);
        }
    }
    
    return(0);
}


int mkdir_recurst(char * p){
    auto c = p;
    struct stat st;

    if(0==stat(p,&st)){
        return !S_ISDIR(st.st_mode);
    } 
    auto b = true;
    for(;b;++c){
        if(*c == '\\' || *c=='/'){
            ++c;
            auto evacu = *c;
            *c = 0;
            if(stat(p,&st)){
                if(mkdir(p)) b = false;
            }
            *c = evacu;
        }
        if(0==!b+!*c) continue;
        if(stat(p,&st)){
            if(mkdir(p)) b = false;
        }
        break;
    }
    return !b;
}



#endif