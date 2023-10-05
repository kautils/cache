
#include <sys/stat.h>
#include <fcntl.h>
#include "kautil/region/region.hpp"
#include "kautil/algorithm/btree_search/btree_search.hpp"

int mkdir_recurst(char * p);


#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

struct file_syscall_16b_pref{
    using value_type = uint64_t;
    using offset_type = long;

    int fd=-1;
    
    offset_type block_size(){ return sizeof(value_type)*2; }
    offset_type max_size(){
        struct stat st;
        fstat(fd,&st);
        return static_cast<uint64_t>(st.st_size);
    }

    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        read(fd,*value,sizeof(value_type));
    }
};


struct syscall_pref{
    using offset_type = long;
    int fd = -1;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~syscall_pref(){ free(buffer); }
    void extend(offset_type extend_size){ 
        fstat(fd,&st);
        ftruncate(fd,st.st_size+extend_size);
    }
    int write(offset_type dst,offset_type src,offset_type size){
        if(buffer_size < size){
            if(buffer)free(buffer);
            buffer = (char*) malloc(buffer_size = size);
        }
        lseek(fd,src,SEEK_SET);
        ::read(fd,buffer,size);
        lseek(fd,dst,SEEK_SET);
        ::write(fd,buffer,size);
        return 0;
    }
    
    offset_type size(){ 
        fstat(fd,&st);
        return static_cast<offset_type>(st.st_size); 
    }
};




#include <string>



template<typename btree_prefix,typename region_prefix>
struct cache_internal{
    using offset_type = typename btree_prefix::offset_type;
    using value_type = typename btree_prefix::value_type;
    
    cache_internal(btree_prefix* prfx,region_prefix * region_prfx) : prfx(prfx),region_prfx(region_prfx){}
    btree_prefix * prfx=0;
    region_prefix * region_prfx=0;
};


template<typename btree_prefix,typename region_prefix>
struct cache{
    
    using offset_type = typename btree_prefix::offset_type;
    using value_type = typename btree_prefix::value_type;
    static_assert(sizeof(typename btree_prefix::offset_type) == sizeof(typename region_prefix::offset_type));
    
    cache(btree_prefix *  prf,region_prefix * region_prfx) : m(new cache_internal{prf,region_prfx}){}
    ~cache(){ delete m; }
    
    struct find_result{ bool found=false;offset_type position=0;int direction=0; };
    find_result find(uint64_t search_v){ 
        auto res =find_result{};
        res.found=kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(search_v),&res.position,&res.direction); 
        return res;
    }
    
    void add(offset_type from){
//        syscall_pref prf{.fd=fd};
        auto extend_size =sizeof( value_type )*2;
        auto buffer = 4096;
        auto reg = region<region_prefix>{&m->region_prfx};
        reg.claim(from,extend_size,buffer);
//        lseek(fd,from,SEEK_SET);
//        write(fd,&input,extend_size);
        
        
    }
    
    cache_internal<btree_prefix,region_prefix> * m = 0;
};



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
            fd = open(f,O_CREAT|O_EXCL|O_RDWR,0755);
        }else{
            fd = open(f,O_RDWR);
        }
    }
    
    auto cnt = 0;
    for(auto i = 0; i < 100 ; ++i){
        auto beg = file_syscall_16b_pref::value_type(cnt*10);
        auto end = beg+10;
        cnt+=2;
        
        auto cur = tell(fd);
        auto block_size = sizeof(file_syscall_16b_pref::value_type);
        write(fd,&beg,block_size);
        lseek(fd,cur+block_size,SEEK_SET);
        write(fd,&end,block_size);
        lseek(fd,cur+block_size*2,SEEK_SET);
    }
    lseek(fd,0,SEEK_SET);
    
    
    file_syscall_16b_pref::value_type input[2] = {11,15};
    auto pref = file_syscall_16b_pref{.fd=fd};
    auto region_prfx=syscall_pref{.fd=fd};
    auto a = cache{&pref,&region_prfx};
    
    
    auto fw = a.find(input[0]);
    auto bw = a.find(input[1]);
    if(fw.found + bw.found){ // there is overlap
        printf("there is overlap");
        bool is_partial = false;
        if(is_partial){
            // merge
        }else{
            // nothing
        }
         //printf("found. pos %ld direction %d\n",a.pos(),a.direction());
    }else{// there is not overlap
        // add
        printf("there is not overlap");
//        {
//            struct stat st;
//            auto c = '0';
//            fstat(fd,&st);
//            auto cnt = 0;
//            lseek(fd,0,SEEK_SET);
//            auto start = from;
//            auto end = from+extend_size;
//            for(auto i = 0;i < st.st_size;++i){
//                if((2== ( (start<=i) + (i<end))) ){
//                    lseek(fd,i,SEEK_SET);
//                    read(fd,&c,sizeof(c));
//                    if(0==(cnt)%10) printf("\n");
//                    printf("%08x ",c);
//                }
//                ++cnt;
//            }
//        }
    }
    
    return 0;
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
