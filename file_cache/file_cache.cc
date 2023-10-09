// todo : refactor api (e.g : max_size => size, void read => int read)


#include <stdio.h>


#include <sys/stat.h>
#include <fcntl.h>
#include "kautil/region/region.hpp"
#include "kautil/algorithm/btree_search/btree_search.hpp"

#include <string>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
int mkdir_recurst(char * p);


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
    
    bool write_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        return sizeof(value_type)==::write(fd,*value,sizeof(value_type));
    }
    
    bool write(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        // todo : size check
        return sizeof(value_type)==::write(fd,*data,size);
    }
    
    // api may make a confusion but correct. this is because to avoid copy of value(object such as bigint) for future.
    bool read(offset_type const& offset, void ** data, offset_type size){
        lseek(fd,offset,SEEK_SET);
        // todo : size check
        return sizeof(value_type)==::read(fd,*data,size);
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
};


struct file_capi {
    using value_type = uint64_t;
    using offset_type = long;
    
    FILE* f=0;
    char * buffer = 0;
    offset_type buffer_size = 0;
    struct stat st;
    
    ~file_capi(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type)*2; }
    offset_type size(){ fstat(fileno(f),&st);return static_cast<offset_type>(st.st_size); }
    
    bool read_value(offset_type const& offset, value_type ** value){
        fseek(f,offset,SEEK_SET);
        return sizeof(value_type) == fread(*value,1,sizeof(value_type),f);
    }
    
    bool write_value(offset_type const& offset, value_type ** value){
        fseek(f,offset,SEEK_SET);
        return sizeof(value_type)==::fwrite(*value,1,sizeof(value_type),f);
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
        fflush(f);
        fseek(f,dst,SEEK_SET);
        auto write_size = fwrite(buffer,1,read_size,f);
        fflush(f);
        return 0;
    }
    

    
    
    
};

template <typename prefix>
struct rw{
    
    using offset_type = typename prefix::offset_type;
    using value_type = typename prefix::value_type;
    
    rw(prefix * prfx) : m(prfx){}
    bool write(offset_type const& offset,void ** data,offset_type size){ return m->write(offset,data,size); }
    bool read(offset_type const& offset,void ** data,offset_type size){ return m->read(offset,data,size); }
    bool write_value(offset_type const& offset, value_type ** value){ return m->write_value(offset,value); }
    void read_value(offset_type const& offset, value_type ** value){ m->read_value(offset,value); }
    
    prefix * m=0;
};







template<typename btree_preference>
struct cache_internal{
    using offset_type = typename btree_preference::offset_type;
    using value_type = typename btree_preference::value_type;
    
    cache_internal(btree_preference* prfx) : prfx(prfx){}
    btree_preference * prfx=0;
};


template<typename btree_preference>
struct cache{
    
    using offset_type = typename btree_preference::offset_type;
    using value_type = typename btree_preference::value_type;
    
    cache(btree_preference *  prf) : m(new cache_internal{prf}){}
    ~cache(){ delete m; }
    struct find_result{ bool exact=false;offset_type position=0;int direction=0; };
    
    
    
    find_result merge(value_type input[2]){ 
        value_type diff = 1;
        
        auto res =find_result{};
        auto btres =kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(input[0])/*, &res.position, &res.direction*/); 
        auto block_size=m->prfx->block_size();
        auto max_size=m->prfx->size();
        auto start_block =btres.pos; 
        value_type cmp[2];
        value_type new_block[2]={input[0],input[1]};
        
        
        auto gap_cnt=-1;
        auto end_block = start_block; 
        for(auto i = start_block; i < max_size; i+=block_size ){
            read_block(i,cmp);
            auto cond = bool((input[1] >= cmp[0])+((cmp[0]-input[1]) <= diff));
            if(/*(start_block<i) && */!cond) break;
            new_block[1] =cmp[1];
            ++gap_cnt;
        }
        
        auto write_buffer_size=512;
        auto left_pos = 
                     (btres.neighbor_pos < btres.pos)*btres.neighbor_pos 
                   +!(btres.neighbor_pos < btres.pos)*btres.pos;
        auto rpos = 
                     !(btres.neighbor_pos < btres.pos)*btres.neighbor_pos 
                     +(btres.neighbor_pos < btres.pos)*btres.pos;
        if(gap_cnt > 0) { // possible to shrink region
            auto end_block = start_block + (block_size * (gap_cnt+1) );
            auto shrink_size = block_size*gap_cnt;
            write_block(start_block,new_block);
            //printf("%ld %ld %d\n",end_block,shrink_size,write_buffer_size); fflush(stdout); 
            kautil::region{m->prfx}.shrink(end_block,shrink_size,write_buffer_size);
        }else if(gap_cnt < 0){ // block is unique. must extend region for that block. 
            kautil::region{m->prfx}.claim(rpos,block_size,write_buffer_size);
            write_block(rpos,input);
        }else{ // block is contained by (or has overlap with) the first one. possible to merge them. 
            //printf("write (%ld,b(%lld),e(%lld))\n",start_block,new_block[0],new_block[1]); fflush(stdout);
            write_block(start_block,new_block);
        }
        
        fflush(m->prfx->f);
        return res;
    }
    
    find_result find(uint64_t from,uint64_t to){ 
        auto res =find_result{};
        auto btres =kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(from)/*, &res.position, &res.direction*/); 
        return res;
    }
    
    
    
    offset_type buffer = 4096;
    constexpr inline static auto kBlockSize =sizeof( value_type )*2; 
    
    
    bool read_block(offset_type from,value_type continuous[2]){
      return rw<btree_preference>(m->prfx).read(from,(void**)&continuous,sizeof(value_type)*2);
    }
    bool write_block(offset_type from,value_type continuous[2]){
       return rw<btree_preference>(m->prfx).write(from,(void**)&continuous,sizeof(value_type)*2);
    }
    
    
    void add_block(offset_type from,value_type continuous[2]){
        auto reg = kautil::region<btree_preference>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_preference>(m->prfx).write(from,(void**)&continuous,sizeof(value_type)*2);
    }
    
    void add(offset_type from,value_type * fw,value_type * bw){
        auto reg = kautil::region<btree_preference>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_preference>(m->prfx).write_value(from,&fw);
        rw<btree_preference>(m->prfx).write_value(from+sizeof(value_type),&bw);
        
    }
    
    void debug_out_file(FILE* outto,int fd,offset_type from,offset_type to){
        {
            struct stat st;
            fstat(fd,&st);
            auto cnt = 0;
            lseek(fd,0,SEEK_SET);
            auto start = from;
            auto end = from+kBlockSize;
            auto size = st.st_size;
            value_type block[2];
            for(auto i = 0; i< size; i+=(sizeof(value_type)*2)){
                if(from <= i && i<= to ){
                    read_block(i,block);
                    printf("[%lld] %lld %lld\n",i,block[0],block[1]);fflush(outto);
                }
            }
        }
        
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
    
    cache_internal<btree_preference> * m = 0;
};


#include <stdlib.h>
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
    
    
    using file_struct_type = file_capi; 
    
    auto cnt = 0;
    for(auto i = 0; i < 100 ; ++i){
        auto beg = file_struct_type::value_type(cnt*10);
        auto end = beg+10;
        
        auto cur = tell(fd);
        auto block_size = sizeof(file_struct_type::value_type);
        write(fd,&beg,block_size);
        lseek(fd,cur+block_size,SEEK_SET);
        write(fd,&end,block_size);
        lseek(fd,cur+(block_size*2),SEEK_SET);
        cnt+=2;
    }
    lseek(fd,0,SEEK_SET);
    
    {
        // 140 150 155 158 160 170
        //file_syscall_16b_pref::value_type input[2] = {155,159}; // case : no extend  
        //file_syscall_16b_pref::value_type input[2] = {155,158}; // case : extend  
        file_struct_type::value_type input[2] = {0,900}; // case : shrink  
        auto pref = file_struct_type{.f=fdopen(fd,"r+b")};
        //        auto pref = file_struct_type{.fd=fd};
        auto a = cache{&pref};
        auto fw = a.merge(input);
        a.debug_out_file(stdout,fd,0,2000);
        
        return 0;
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
