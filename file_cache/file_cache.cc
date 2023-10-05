
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
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~file_syscall_16b_pref(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type)*2; }
    
    // todo: bad. max_size == size
    offset_type max_size(){ fstat(fd,&st);return static_cast<uint64_t>(st.st_size); }
    offset_type size(){ fstat(fd,&st);return static_cast<offset_type>(st.st_size); }
    
    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        read(fd,*value,sizeof(value_type));
    }
    
    bool write_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        return sizeof(value_type)==::write(fd,*value,sizeof(value_type));
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        // todo : size check
        return sizeof(value_type)==::write(fd,*data,size);
    }
    
    
    void extend(offset_type extend_size){ fstat(fd,&st);ftruncate(fd,st.st_size+extend_size); }
    int shift(offset_type dst,offset_type src,offset_type size){
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
};


template <typename prefix>
struct rw{
    
    using offset_type = typename prefix::offset_type;
    using value_type = typename prefix::value_type;
    
    rw(prefix * prfx) : m(prfx){}
    bool write(offset_type const& offset,void ** data,offset_type size){ 
        return m->write(offset,data,size); 
    }
    bool write_value(offset_type const& offset, value_type ** value){ return m->write_value(offset,value); }
    void read_value(offset_type const& offset, value_type ** value){ m->read_value(offset,value); }
    
    prefix * m=0;
};




#include <string>



template<typename btree_prefix>
struct cache_internal{
    using offset_type = typename btree_prefix::offset_type;
    using value_type = typename btree_prefix::value_type;
    
    cache_internal(btree_prefix* prfx) : prfx(prfx){}
    btree_prefix * prfx=0;
};


template<typename btree_prefix>
struct cache{
    
    using offset_type = typename btree_prefix::offset_type;
    using value_type = typename btree_prefix::value_type;
    
    cache(btree_prefix *  prf) : m(new cache_internal{prf}){}
    ~cache(){ delete m; }
    
    struct find_result{ bool found=false;offset_type position=0;int direction=0; };
    find_result find(uint64_t search_v){ 
        auto res =find_result{};
        res.found=kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(search_v),&res.position,&res.direction); 
        return res;
    }
    
    offset_type buffer = 4096;
    constexpr inline static auto kBlockSize =sizeof( value_type )*2; 
    
    void add_block(offset_type from,value_type continuous[2]){
        auto reg = region<btree_prefix>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_prefix>(m->prfx).write(from,(void**)&continuous,sizeof(value_type)*2);
    }
    
    void add(offset_type from,value_type * fw,value_type * bw){
        auto reg = region<btree_prefix>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_prefix>(m->prfx).write_value(from,&fw);
        rw<btree_prefix>(m->prfx).write_value(from+sizeof(value_type),&bw);
        
    }
    
    void debug_out_block(FILE* outto,offset_type from){
        {
            auto fd = m->prfx->fd;
            struct stat st;
            fstat(fd,&st);
            
            auto c = '0';
//            for(auto i = 0; i < st.st_size; ++i ){
//                lseek(fd,i+from,SEEK_SET);
//                write(fd,&c,sizeof(char));
//            }
            
            auto cnt = 0;
            lseek(fd,0,SEEK_SET);
            auto start = from;
            auto end = from+kBlockSize;
            for(auto i = 0;i < st.st_size;++i){
                if((2== ( (start<=i) + (i<end))) ){
                    lseek(fd,i,SEEK_SET);
                    read(fd,&c,sizeof(c));
                    if(0==(cnt)%10) fprintf(outto,"\n");
                    fprintf(outto,"%08x ",c);
                }
                ++cnt;
            }
        }
        
    }
    
    
    cache_internal<btree_prefix/*,region_prefix*/> * m = 0;
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
//    auto region_prfx=syscall_pref{.fd=fd};
    auto a = cache{&pref/*,&region_prfx*/};
    
    
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
        auto from = fw.position+fw.direction*sizeof(decltype(a)::value_type)*2;
//        a.add(from,&input[0],&input[1]);
        a.add_block(from,input);
        a.debug_out_block(stderr,from);
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
