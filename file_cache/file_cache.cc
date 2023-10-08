#include <stdio.h>


#include <sys/stat.h>
#include <fcntl.h>
#include "kautil/region/region.hpp"
#include "kautil/algorithm/btree_search/btree_search.hpp"

int mkdir_recurst(char * p);


#include <string>
#include <stdint.h>
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
        auto block=m->prfx->block_size();
        auto max_size=m->prfx->size();
        auto start_block =btres.pos; 
        value_type cmp[2];
        value_type new_block[2]={input[0],input[1]};
        
        
        auto block_cnt=-1;
        auto end_block = start_block; 
        for(auto i = start_block; i < max_size; i+=block ){
            read_block(i,cmp);
            printf("input[1] < cmp[0] : %lld %lld\n",input[1],cmp[0]); fflush(stdout);
            auto cond = bool((input[1] >= cmp[0])+((input[1] - cmp[0]) < diff));
            if((start_block<i) && !cond) break;
            new_block[1] =cmp[1];
            ++block_cnt;
        }
        printf("cnt(%d) %ld %lld\n",block_cnt,btres.pos,new_block[1]); fflush(stdout);
        if(block_cnt){
            printf("need to change the size of range\n"); fflush(stdout);
        }else{
            printf("need not to change the size of range\n"); fflush(stdout);
            printf("write (%ld,b(%lld),e(%lld))\n",start_block,new_block[0],new_block[1]); fflush(stdout);
            write_block(start_block,new_block);
            //printf("cnt(%d) %ld %ld",block_cnt,btres.pos,end_pos); fflush(stdout);
        }
        
        
//        btres.pos;
//        btres.direction;


        return res;
    }
    
    find_result find(uint64_t from,uint64_t to){ 
        auto res =find_result{};
        auto btres =kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(from)/*, &res.position, &res.direction*/); 
        return res;
    }
    
    
    
    offset_type buffer = 4096;
    constexpr inline static auto kBlockSize =sizeof( value_type )*2; 
    
    
    void read_block(offset_type from,value_type continuous[2]){
        rw<btree_preference>(m->prfx).read(from,(void**)&continuous,sizeof(value_type)*2);
    }
    void write_block(offset_type from,value_type continuous[2]){
        rw<btree_preference>(m->prfx).write(from,(void**)&continuous,sizeof(value_type)*2);
    }
    
    
    void add_block(offset_type from,value_type continuous[2]){
        auto reg = region<btree_preference>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_preference>(m->prfx).write(from,(void**)&continuous,sizeof(value_type)*2);
    }
    
    void add(offset_type from,value_type * fw,value_type * bw){
        auto reg = region<btree_preference>{m->prfx};
        reg.claim(from,kBlockSize,buffer);
        rw<btree_preference>(m->prfx).write_value(from,&fw);
        rw<btree_preference>(m->prfx).write_value(from+sizeof(value_type),&bw);
        
    }
    
    void debug_out_file(FILE* outto,offset_type from,offset_type to){
        {
            auto fd = m->prfx->fd;
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
                    printf("%lld %lld\n",block[0],block[1]);fflush(outto);
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
        
        auto cur = tell(fd);
        auto block_size = sizeof(file_syscall_16b_pref::value_type);
        write(fd,&beg,block_size);
        lseek(fd,cur+block_size,SEEK_SET);
        write(fd,&end,block_size);
        lseek(fd,cur+(block_size*2),SEEK_SET);
        cnt+=2;
    }
    lseek(fd,0,SEEK_SET);

    
    

    {
        file_syscall_16b_pref::value_type input[2] = {155,160}; 
        auto pref = file_syscall_16b_pref{.fd=fd};
        auto a = cache{&pref};
        a.debug_out_file(stdout,112,144);
        auto fw = a.merge(input);
        a.debug_out_file(stdout,112,144);
        
        
        
        return 0;
    }
    
    
    
    
    file_syscall_16b_pref::value_type input[2] = {150,155};
    auto pref = file_syscall_16b_pref{.fd=fd};
    auto a = cache{&pref};
    auto fw = a.merge(input);
    exit(0);
    
    auto lmb_have_oevrlap = [](auto f, auto b){ return f[1] > b[0]; };

    
    if(!(pref.size()==fw.position) /*+ !fw.direction>0*/) {
        //case : want exists(includes partially overlapped block) 
        printf("position(%ld) direction(%d)\n",fw.position,fw.direction); fflush(stdout);
        
        auto bytes = pref.size();
        auto block_bytes = pref.block_size();
        auto value = decltype(pref)::value_type{};
        decltype(pref)::value_type block[2]={0,0};
    
        auto beg = fw.position+block_bytes;
        auto begv = decltype(pref)::value_type(0);{// dose want value have overlap with first block?
            decltype(pref)::value_type found_block[2]={0,0};
            auto ptr_found_block = &found_block;
            pref.read(fw.position,(void**) &ptr_found_block,sizeof(found_block));
            auto has_overlap = lmb_have_oevrlap(found_block,input);
            begv = has_overlap*found_block[0] + !has_overlap*input[0]; 
        }
        auto end = beg;
        auto endv= decltype(pref)::value_type(0);
        for(; end < bytes; end+=block_bytes){
            auto vptr = &value;
            auto arr = &block; // == block itself  == void * , i want &&block. &block is insufficient
            pref.read(end,(void**) &arr,sizeof(block));
            //printf("i(%lld / %lld) %lld / %lld \n",found_block[0],found_block[1],block[0],block[1]); fflush(stdout);
            printf("input[1] <= block[0] (%lld <= %lld)\n",input[1],block[0]);
            if(!lmb_have_oevrlap(input,block)){
                endv = block[1];
                break;
            }
        }
        printf("pos(%ld %ld), value(%lld,%lld) \n",beg,end,begv,endv);
    } else{
        int jjj = 0;
    }
    
    
    return(0);
    
//    if(fw.exact + bw.exact){ // there is overlap
//        printf("there is overlap");
//        auto from = fw.position+fw.direction*sizeof(decltype(a)::value_type)*2;
//        
//        file_syscall_16b_pref::value_type r[2];
//        a.read_block(from,r);
//        printf("----%lld %lld\n",r[0],r[1]); fflush(stdout);
//        
//        
//        bool is_partial = false;
//        if(is_partial){
//            
//            
//            // merge
//        }else{
//            // nothing
//        }
//         //printf("found. pos %ld direction %d\n",a.pos(),a.direction());
//    }else{// there is not overlap
//        // add
//        printf("there is not overlap");
//        auto from = fw.position+fw.direction*sizeof(decltype(a)::value_type)*2;
////        a.add(from,&input[0],&input[1]);
//        a.add_block(from,input);
//        a.debug_out_block(stderr,from);
//    }
    
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
