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





struct file_syscall_8b_pref{
    using value_type = uint64_t;
    using offset_type = long;

    int fd=-1;
    offset_type block_size(){ return sizeof(value_type); }
    offset_type size(){
        struct stat st;
        fstat(fd,&st);
        return static_cast<uint64_t>(st.st_size);
    }

    void read_value(offset_type const& offset, value_type ** value){
        lseek(fd,offset,SEEK_SET);
        read(fd,*value,sizeof(value_type));
    }
};


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
    
    //int fileno(){ return fd; }
    
    
};


struct file_capi{
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
        fseek(f,dst,SEEK_SET);
        auto write_size = fwrite(buffer,1,read_size,f);
        return 0;
    }
    
    int flush_buffer(){ return fflush(f); }
    //int fileno(){ return ::fileno(f); }
    
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
    
    bool exists(value_type input[2]){
        auto btres =kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(input[0]),false);
        if(btres.direction) return false;
        auto nearest_right = value_type(0);
        auto nearest_right_ptr = &nearest_right;
        m->prfx->read_value(btres.nearest_pos+sizeof(value_type),&nearest_right_ptr);
        return 2== (((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr)));
    }
    
    
    int merge(value_type input[2]){ 
        value_type diff = 1;
        
        auto btres =kautil::algorithm::btree_search{m->prfx}.search(static_cast<value_type>(input[0]));
        {// return immediately
            auto nearest_right = value_type(0);
            auto nearest_right_ptr = &nearest_right;
            m->prfx->read_value(btres.nearest_pos+sizeof(value_type),&nearest_right_ptr);
            if(2==((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr))) return 0;
        }
        
        auto block_size=m->prfx->block_size();
        auto start_block =btres.nearest_pos; 
        value_type cmp[2];
        value_type new_block[2]={input[0],input[1]};
        auto gap_cnt=-1;{
            auto max_size=m->prfx->size();
            for(auto i = start_block; i < max_size; i+=block_size ){
                read_block(i,cmp);
                if(!( (input[1] >= cmp[0])
                    + ((cmp[0]-input[1]) <= diff))
                ) break;
                
                new_block[1] =cmp[1];
                ++gap_cnt;
            }
        }
        
        auto write_buffer_size=512;
        auto rpos = 
             !(btres.neighbor_pos < btres.nearest_pos)*btres.neighbor_pos 
             +(btres.neighbor_pos < btres.nearest_pos)*btres.nearest_pos;
        if(gap_cnt > 0) { /* possible to shrink region*/
            auto end_block = start_block + (block_size * (gap_cnt+1) );
            auto shrink_size = block_size*gap_cnt;
            write_block(start_block,new_block);
            //printf("%ld %ld %d\n",end_block,shrink_size,write_buffer_size); fflush(stdout); 
            kautil::region{m->prfx}.shrink(end_block,shrink_size,write_buffer_size);
        }else if(gap_cnt < 0){ /* block is unique. must extend region for that block. */
            kautil::region{m->prfx}.claim(rpos,block_size,write_buffer_size);
            write_block(rpos,input);
        }else{ /* block is contained by (or has overlap with) the first one. possible to merge them. */
            //printf("write (%ld,b(%lld),e(%lld))\n",start_block,new_block[0],new_block[1]); fflush(stdout);
            write_block(start_block,new_block);
        }
        
        m->prfx->flush_buffer();
        return 0;
    }
    

    value_type adjust_nearest_pos(value_type nearest_pos,value_type input){
        if(!nearest_pos)return nearest_pos;
        auto info_bw = value_type(0);
        auto info_bw_ptr = &info_bw;
        m->prfx->read(nearest_pos-sizeof(value_type),(void**)&info_bw_ptr,sizeof(value_type));
        return nearest_pos+ (info_bw >= input)*-m->prfx->block_size();
    }
    
    value_type left_value(int direction,value_type nearest_pos,value_type input){
        if(!nearest_pos)return nearest_pos;
        if(direction >0) return nearest_pos;
        auto info_bw = value_type(0);
        auto info_bw_ptr = &info_bw;
        return nearest_pos-m->prfx->block_size();
    }

    
    
    struct true_nearest_result{
        value_type value=0;
        offset_type pos=0;
    };
//    true_nearest_result true_nearest(value_type input_value,kautil::algorithm::btree_search_result<btree_preference> & ){
    true_nearest_result true_nearest(value_type input_value,value_type nearest_value,offset_type nearest_pos){
        auto overflow_upper=false;
        auto overflow_lower=false;
        
        auto v =value_type(0);
        auto vpos =nearest_pos;
        auto vp = &v;
        if(vpos>sizeof(value_type)){
            m->prfx->read(vpos-sizeof(value_type),(void**)&vp,sizeof(value_type));
        }else{
            v = 0;
            vpos= 0;
            overflow_lower=true;
        }
        
        auto v1 =value_type(0);
        auto vpos1 = nearest_pos;
        auto vp1 = &v1;
        if(vpos1+sizeof(value_type) < m->prfx->size()){
            m->prfx->read(vpos1+sizeof(value_type),(void**)&vp1,sizeof(value_type));
        }else{
            v1 = 0;
            vpos1=m->prfx->size();
            overflow_upper=true;
        }
        
        auto diff_l = 
                  (v > input_value)*((v - input_value)) 
                +!(v > input_value)*((input_value-v)); 
        
        auto diff_r = 
                  (v1 > input_value)*((v1 - input_value)) 
                +!(v1 > input_value)*((input_value-v1)); 
        
        auto start_v1 = 
                  (diff_l < diff_r)*v
                +!(diff_l < diff_r)*v1; 
        
        auto start_pos = 
                  (diff_l < diff_r)*vpos
                +!(diff_l < diff_r)*vpos1; 
        
        
        
        
        return {.value=start_v1,.pos=start_pos};
    }
    
    bool inside_range(value_type cmp, value_type pole0, value_type pole1){ return (2==((pole0 <= cmp) + (cmp <= pole1))) +(2==((pole0 <= cmp) + (cmp <= pole1))); }
    
    struct gap_context{ value_type * gap = 0;uint64_t gap_len = 0; value_type * entity=0; };
    void gap_context_free(gap_context * ctx){ delete ctx->entity; delete ctx; }
    
    gap_context* gap(value_type input[2]){ 
        
        //file_syscall_8b_pref pref{.fd=m->prfx->fd};
        
        typename kautil::algorithm::btree_search<btree_preference>::btree_search_result a;
        auto info0 = kautil::algorithm::btree_search{m->prfx}.search(input[0]);
        auto info1 = kautil::algorithm::btree_search{m->prfx}.search(input[1]);
        
        auto overflow_upper=false;
        auto overflow_lower=false;
        
        auto v0 =true_nearest(input[0],info0.nearest_value,info0.nearest_pos);
        auto v0_is_contained = inside_range(input[0],info0.nearest_value,info0.neighbor_value);
        
        auto v1 =true_nearest(input[1],info1.nearest_value,info1.nearest_pos);
        auto v1_is_contained = inside_range(input[1],info1.nearest_value,info1.neighbor_value);
        
        if(v0_is_contained && v1_is_contained && v0.pos==v1.pos){
            return 0;
        }
        
        auto entity_bytes = v1.pos -v0.pos+m->prfx->block_size();
        //auto entity_len = entity_bytes/ sizeof(value_type)+2;
        
        auto low_pos = (info0.nearest_value<v0.value)*info0.nearest_pos + !(info0.nearest_value<v0.value)*v0.pos; 
        auto high_pos = (info1.nearest_value>v1.value)*info1.nearest_pos + !(info1.nearest_value>v1.value)*v1.pos; 
        
        
        {
            printf("debug info\n");
            printf("(low ~ high)(%ld ~ %ld)\n",low_pos,high_pos);
            printf("i0 is contained %d (%lld : %lld ~ %lld)\n",v0_is_contained,input[0],info0.nearest_value,v0.value);
            printf("i1 is contained %d (%lld : %lld ~ %lld)\n",v1_is_contained,input[1],info1.nearest_value,v1.value);
            
            printf("v0 : (t-v,t-p),(%lld : %lld,%ld)\n",input[0],v0.value,v0.pos);
            printf("v1 : (t-v,t-p),(%lld : %lld,%ld)\n",input[1],v1.value,v1.pos);
        }

        // todo overflow : low_pos+sizeof(value_type) >= size()? 
        auto block_size = m->prfx->block_size();
        auto entity_len  = (high_pos - low_pos + sizeof(value_type))/sizeof(value_type)+2;
        auto entity = new value_type[entity_len]; 

        auto arr_len = entity_len-2;
        auto arr = entity+1;
        m->prfx->read(low_pos,(void**)&arr,arr_len*sizeof(value_type));
        
        
        auto beg = (value_type *)0;
        auto end = (value_type *)0;
        
        entity[0] = (entity[1]>input[0])*input[0] + !(entity[1]>input[0])*entity[1];
        entity[1] = !(entity[1]>input[0])*input[0] + (entity[1]>input[0])*entity[1];
        
        
        entity[entity_len-1] = (entity[entity_len-2]>input[1])*entity[entity_len-2]+!(entity[entity_len-2]>input[1])*input[1];
        entity[entity_len-2] = !(entity[entity_len-2]>input[1])*entity[entity_len-2]+(entity[entity_len-2]>input[1])*input[1];
        
        beg =(value_type*) ( (entity[1]>input[0])*uintptr_t(&entity[0]) + !(entity[1]>input[0])*uintptr_t(&entity[1]) );
        end =(value_type*) (
                        (entity[entity_len-2]>input[1])*uintptr_t(&entity[entity_len-2]) 
                      +!(entity[entity_len-2]>input[1])*uintptr_t(&entity[entity_len-1]));
        
        
        if(v0_is_contained)++beg;
        if(v1_is_contained)--end;
        // addr of beg / end
        
        printf("++++++++++++++++++++++++++++\n");
        auto cur = beg;
        for(;cur != end;++cur){
            printf("%lld \n",*cur);
        }
        printf("++++++++++++++++++++++++++++\n");
        
//        
//        for(auto i = 0 ; i < 5; ++i){
//            printf("[%d] %lld \n",i,entity[i]);
//        }
//        
        
        
        exit(0);
        
        
        // 0 , entity_len-1
        // 1 , entity_len-1
        // 0 , arr_len -1
        // 1 , arr_len -1
        
        entity[0]            = v0_is_contained*-1 + !v0_is_contained*input[0];
        //entity[entity_len-1] = v1_is_contained*-1 + !v1_is_contained*input[1];
        for(auto i = 0 ; i < entity_len; i+=2)printf("[%d] %lld %lld\n",i,entity[i],entity[i+1]);
        
        
        arr += (!v0_is_contained)*-1 + v0_is_contained*1;
        arr_len += !v0_is_contained*2+v0_is_contained*-2 + !v1_is_contained*2+v1_is_contained*-2;
        arr_len = (arr_len/2) * 2;
        for(auto i = 0 ; i < arr_len; i+=2)printf("[%d] %lld %lld\n",i,arr[i],arr[i+1]);
//        exit(0);
        
        arr[arr_len-1] = v1_is_contained*-1 + !v1_is_contained*input[1];
        
//        for(auto i = 0 ; i < entity_len; i+=2)printf("[%d] %lld %lld\n",i,entity[i],entity[i+1]);
        for(auto i = 0 ; i < arr_len; i+=2)printf("[%d] %lld %lld\n",i,arr[i],arr[i+1]);
        
        
        
        //auto test_v1 =true_nearest(input[1],info1.nearest_value,info1.nearest_pos);
        //auto test = inside_range(input[1],info1.nearest_value,info1.neighbor_value);
//        printf("%lld %lld\n",v0,p0);
//        printf("%lld %lld\n",v1,p1);
        
        
        exit(0);
    }
//    gap_context* gap(value_type input[2]){ 
//        
//        //file_syscall_8b_pref pref{.fd=m->prfx->fd};
//        
//        typename kautil::algorithm::btree_search<btree_preference>::btree_search_result a;
//        auto info0 = kautil::algorithm::btree_search{m->prfx}.search(input[0]);
//        auto info1 = kautil::algorithm::btree_search{m->prfx}.search(input[1]);
//        
//        auto overflow_upper=false;
//        auto overflow_lower=false;
//        
//        auto v0 =true_nearest(input[0],info0.nearest_value,info0.nearest_pos);
//        auto v0_is_contained = inside_range(input[0],info0.nearest_value,info0.neighbor_value);
//        
//        auto v1 =true_nearest(input[1],info1.nearest_value,info1.nearest_pos);
//        auto v1_is_contained = inside_range(input[1],info1.nearest_value,info1.neighbor_value);
//        
//        auto adjust_size = !v0_is_contained+!v1_is_contained;
//        auto ptr = (value_type*)0;
//        auto ptr_bytes = (v1.pos-v0.pos);
//        if(auto ptr_len = ptr_bytes/sizeof(value_type)+adjust_size){
////        auto ptr_bytes = (v1.pos-v0.pos)+m->prfx->block_size();
////        if(auto ptr_len = ptr_bytes/sizeof(value_type)){
//            
//            if(ptr_len==1){
//                ptr_len = 2; // belongs to same block and v0 or v1 is not contained  
//            }
//            
//            ptr = new value_type[ptr_len];
//            {// case dose not contained
//                ptr[0]=input[0];
//                ptr[ptr_len-1]=(info1.nearest_value > input[1])*input[1] + !(info1.nearest_value > input[1])*info1.nearest_value;
//            }
//            auto arr= ptr+!(v0_is_contained);
//            m->prfx->read(v0.pos,(void**)&arr,ptr_bytes);
//            
//            
//            for(auto i = 0 ; i < ptr_len; i+=2)printf("[%d] %lld %lld\n",i,ptr[i],ptr[i+1]);
//            
//            arr =(value_type*)(
//                      (v0_is_contained*uintptr_t(arr+1))
//                    + (!v0_is_contained*uintptr_t(ptr)));
//            
//            printf("%llx %llx %llx\n",uintptr_t(ptr),uintptr_t(arr),!v0_is_contained*uintptr_t(ptr));
//            fflush(stdout);
//            auto arr_len= 
//                      v0_is_contained*(ptr_len-!v1_is_contained)
//                    +!v0_is_contained*(ptr_len);
//            
//            
//            
//            printf("+++++++++++++++++++++++++++++++\n");
//            for(auto i = 0 ; i < ptr_len; i+=2)printf("%lld %lld\n",ptr[i],ptr[i+1]);
//            printf("+++++++++++++++++++++++++++++++\n");
//            for(auto i = 0 ; i < arr_len; i+=2){
//                printf("%lld %lld\n", arr[i], arr[i + 1]);
//                fflush(stdout);
//            }
//            delete ptr;
//        }else{
//            //printf("v1 is contained %d\n",v1_is_contained);
//            printf("there is no gap.\n");
//        }
//
//        {
//            printf("debug info\n");
//            printf("i0 is contained %d (%lld : %lld ~ %lld)\n",v0_is_contained,input[0],info0.nearest_value,v0.value);
//            printf("i1 is contained %d (%lld : %lld ~ %lld)\n",v1_is_contained,input[1],info1.nearest_value,v1.value);
//            
//            printf("v0 : (t-v,t-p),(%lld : %lld,%ld)\n",input[0],v0.value,v0.pos);
//            printf("v1 : (t-v,t-p),(%lld : %lld,%ld)\n",input[1],v1.value,v1.pos);
//        }
//        
//        
//        //auto test_v1 =true_nearest(input[1],info1.nearest_value,info1.nearest_pos);
//        //auto test = inside_range(input[1],info1.nearest_value,info1.neighbor_value);
////        printf("%lld %lld\n",v0,p0);
////        printf("%lld %lld\n",v1,p1);
//        
//        
//        exit(0);
//    }
    
    
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
    
    
    //using file_16_struct_type = file_capi; 
    using file_16_struct_type = file_syscall_16b_pref; 
    using file_8_struc_type = file_syscall_8b_pref; 
    
    auto cnt = 0;
    for(auto i = 0; i < 100 ; ++i){
        auto beg = file_16_struct_type::value_type(cnt*10);
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
        file_16_struct_type::value_type input[2] = {0,900}; // case : shrink  
        //auto pref = file_16_struct_type{.f=fdopen(fd,"r+b")};
        auto pref = file_16_struct_type{.fd=fd};
        //auto pref8 = file_8_struc_type{.fd=fd};
        auto a = cache{&pref/*,&pref8*/};
        auto fw = a.merge(input);
        
        

        {// gap
//            file_16_struct_type::value_type input[2] ={890,925}; 
//            file_16_struct_type::value_type input[2] ={911,935}; 
//            file_16_struct_type::value_type input[2] ={920,950}; 
//            file_16_struct_type::value_type input[2] ={920,951};
            
//            file_16_struct_type::value_type input[2] ={925,927}; 
//            file_16_struct_type::value_type input[2] ={916,939}; 
//            file_16_struct_type::value_type input[2] ={911,955}; 
//            file_16_struct_type::value_type input[2] ={911,916};  
//            file_16_struct_type::value_type input[2] ={931,939}; 
            file_16_struct_type::value_type input[2] ={925,934}; 
//            file_16_struct_type::value_type input[2] ={916,945}; 
//            file_16_struct_type::value_type input[2] ={916,955}; 
            //file_16_struct_type::value_type input[2] ={925,945}; 
//            file_16_struct_type::value_type input[2] ={911,925}; //*
//            file_16_struct_type::value_type input[2] ={916,925}; // *
            
            // 2block
//            file_16_struct_type::value_type input[2] ={911,955}; 
//            file_16_struct_type::value_type input[2] ={911,945}; // * 
            
            //file_16_struct_type::value_type input[2] ={911,935}; 
            
//            file_16_struct_type::value_type input[2] ={925,955}; 
//            file_16_struct_type::value_type input[2] ={935,955}; 
//            file_16_struct_type::value_type input[2] ={945,955}; 
//            
            if(auto ctx = a.gap(input)){
                for(auto i = 0; i < ctx->gap_len; i+=2){
                    printf("%lld %lld\n",ctx->gap[i],ctx->gap[i+1]);
                }
                a.gap_context_free(ctx);
            }else{
                printf("there is no gap\n");
            }
        }
        
        {// show result
            a.debug_out_file(stdout,fd,0,2000);
        }
        
        exit(0);
        
        
        


        {
            file_16_struct_type::value_type ij [2]={0,10}; // case : shrink  
            auto res = a.exists(ij);
            printf("%d\n",a.exists(ij));
            for(int i = 0; i < 1000; i+=10){
                for(int j = 0; j < 1000; j+=10){
                    ij[0] = i;
                    ij[1] = j;
                    auto res = a.exists(ij);
                    if(res){
                        printf("%d [%lld,%lld]\n",a.exists(ij),ij[0],ij[1]);fflush(stdout);
                    }
                }
            }
        }

        {// show result
            a.debug_out_file(stdout,fd,0,2000);
        }
        
        
        
        
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
