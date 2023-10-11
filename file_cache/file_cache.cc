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



template<typename preference_type>
struct cache{
    
    
    using offset_type = typename preference_type::offset_type;
    using value_type = typename preference_type::value_type;
    
    constexpr inline static auto kBlockSize =sizeof( value_type )*2; 
    offset_type buffer = 512; // on mingw in my env, over 824 is problematic.
    preference_type * prfx=0;
    
    cache(preference_type *  prf) : prfx(prf){}
    ~cache(){ }
    
    bool exists(value_type input[2]){
        auto btres =kautil::algorithm::btree_search{prfx}.search(static_cast<value_type>(input[0]),false);
        if(btres.direction) return false;
        auto nearest_right = value_type(0);
        auto nearest_right_ptr = &nearest_right;
        prfx->read_value(btres.nearest_pos+sizeof(value_type),&nearest_right_ptr);
        return 2== (((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr)));
    }
    
    
    int merge(value_type input[2]){ 
        value_type diff = 1;
        
        auto btres =kautil::algorithm::btree_search{prfx}.search(static_cast<value_type>(input[0]));
        {// return immediately
            auto nearest_right = value_type(0);
            auto nearest_right_ptr = &nearest_right;
            prfx->read_value(btres.nearest_pos+sizeof(value_type),&nearest_right_ptr);
            if(2==((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr))) return 0;
        }
        
        auto block_size=prfx->block_size();
        auto start_block =btres.nearest_pos; 
        value_type cmp[2];
        value_type new_block[2]={input[0],input[1]};
        auto gap_cnt=-1;{
            auto max_size=prfx->size();
            for(auto i = start_block; i < max_size; i+=block_size ){
                read_block(i,cmp);
                if(!( (input[1] >= cmp[0])
                    + ((cmp[0]-input[1]) <= diff))
                ) break;
                
                new_block[1] =cmp[1];
                ++gap_cnt;
            }
        }
        
        auto rpos = 
             !(btres.neighbor_pos < btres.nearest_pos)*btres.neighbor_pos 
             +(btres.neighbor_pos < btres.nearest_pos)*btres.nearest_pos;
        if(gap_cnt > 0) { /* possible to shrink region*/
            auto end_block = start_block + (block_size * (gap_cnt+1) );
            auto shrink_size = block_size*gap_cnt;
            write_block(start_block,new_block);
            //printf("%ld %ld %d\n",end_block,shrink_size,write_buffer_size); fflush(stdout); 
            kautil::region{prfx}.shrink(end_block,shrink_size,buffer);
        }else if(gap_cnt < 0){ /* block is unique. must extend region for that block. */
            kautil::region{prfx}.claim(rpos,block_size,buffer);
            write_block(rpos,input);
        }else{ /* block is contained by (or has overlap with) the first one. possible to merge them. */
            //printf("write (%ld,b(%lld),e(%lld))\n",start_block,new_block[0],new_block[1]); fflush(stdout);
            write_block(start_block,new_block);
        }
        
        prfx->flush_buffer();
        return 0;
    }
    
    ///@note cache file has ranges, so need to adjust the result of btree saerch for it.
    struct adjust_nearest_result{ value_type value=0;offset_type pos=0;bool is_contained=false; };
    adjust_nearest_result adjust_nearest(value_type input_value,bool direction,bool is_overflow,value_type nearest_value,offset_type nearest_pos){
        auto overflow_upper=false;
        auto overflow_lower=false;
        
        auto v =value_type(0);
        auto vp = &v;
        if(0 == (!is_overflow + !(direction<0)) ){
            v = input_value;
            nearest_pos= 0;
            overflow_lower=true;
        }else prfx->read(nearest_pos-sizeof(value_type),(void**)&vp,sizeof(value_type));
        
        auto v1 =value_type(0);
        auto vp1 = &v1;
        
        if(0 == (!is_overflow + !(direction>0)) ){
            v = input_value;
            nearest_pos= 0;
            overflow_lower=true;
        }else{
            prfx->read(nearest_pos+sizeof(value_type),(void**)&vp1,sizeof(value_type));
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
                  (diff_l < diff_r)*nearest_pos
                +!(diff_l < diff_r)*nearest_pos; 
        
        auto is_contained=bool(!is_overflow*(2==(nearest_value <= input_value)+(input_value <=v1)));
        return {.value=start_v1,.pos=start_pos,.is_contained=is_contained};
    }
    
    
    /**@note  
     * if i is contained by neighbor block, then return neighbor's latter part.  
     * if ovf(overflow) is true, nothing to be done. else the process reading sizeof(value_type) of data is done.  
     * **/

    value_type adjust_with_neighbor(
            value_type i
            ,bool d
            ,bool ovf
            ,value_type neighbor_v
            ,offset_type neighbor_pos 
            ){

        if(ovf) return i;
        auto next_value = value_type(0);
        auto next_pos = neighbor_pos+static_cast<offset_type>(sizeof(value_type)*1);
        auto ptr = &next_value;
        prfx->read(
            next_pos
            ,(void**)&ptr
            ,sizeof(value_type));
        
        auto is_contained = !ovf*bool((2==((neighbor_v<=i) + (i<=next_value))) + (2==((next_value<=i) + (i<=neighbor_v))));
        return is_contained*next_value +!is_contained*i;
    }
    
    
    struct gap_context{ value_type *begin;value_type *end=0; value_type * entity=0; };
    void gap_context_free(gap_context * ctx){ if(ctx)delete ctx->entity; delete ctx; }
    gap_context* gap(value_type input[2]){ 
        
        auto info0 = kautil::algorithm::btree_search{prfx}.search(input[0]);
        auto info1 = kautil::algorithm::btree_search{prfx}.search(input[1]);
        
        auto v0 =adjust_nearest(input[0],info0.direction,info0.overflow,info0.nearest_value,info0.nearest_pos);
        auto v1 =adjust_nearest(input[1],info1.direction,info1.overflow,info1.nearest_value,info1.nearest_pos);
        if(0==( !v0.is_contained + !v1.is_contained + !(v0.pos==v1.pos) ))return 0;
        
        input[0]= adjust_with_neighbor(input[0],info0.direction,info0.overflow,info0.neighbor_value,info0.neighbor_pos);
        input[1]= adjust_with_neighbor(input[1],info1.direction,info1.overflow,info1.neighbor_value,info1.neighbor_pos);
        
        auto entity_bytes = v1.pos -v0.pos+prfx->block_size();
        auto low_pos = (info0.nearest_value<v0.value)*info0.nearest_pos + !(info0.nearest_value<v0.value)*v0.pos; 
        auto high_pos = (info1.nearest_value>v1.value)*info1.nearest_pos + !(info1.nearest_value>v1.value)*v1.pos; 
        
//        {
//            printf("debug info\n");
//            printf("(low ~ high)(%ld ~ %ld)\n",low_pos,high_pos);
//            printf("i0 is contained %d (%lld : %lld ~ %lld)\n",v0.is_contained,input[0],info0.nearest_value,v0.value);
//            printf("i1 is contained %d (%lld : %lld ~ %lld)\n",v1.is_contained,input[1],info1.nearest_value,v1.value);
//            
//            printf("v0 : (t-v,t-p),(%lld : %lld,%ld)\n",input[0],v0.value,v0.pos);
//            printf("v1 : (t-v,t-p),(%lld : %lld,%ld)\n",input[1],v1.value,v1.pos);
//        }


        auto block_size = prfx->block_size();
        auto entity_len  = ((high_pos - low_pos + sizeof(value_type))/sizeof(value_type)) +2;
        auto entity = new value_type[entity_len+1]; /* +1 : for the end value */
        auto arr_len = entity_len-2;
        auto arr = entity+1;
        prfx->read(low_pos,(void**)&arr,arr_len*sizeof(value_type));
        
        // this is intended. i wannaed shift values.
        auto & entity_input = entity[0];
        auto & entity_array_start = entity[1];
        auto & entity_input1 = entity[entity_len-1];
        auto & entity_array_end = entity[entity_len-2];
        
        
        auto res = new gap_context{.entity=entity};
        
        entity_input = (entity_array_start>input[0])*input[0] + !(entity_array_start>input[0])*entity_array_start;
        entity_array_start = !(entity_array_start>input[0])*input[0] + (entity_array_start>input[0])*entity_array_start;
        
        entity_input1 = (entity_array_end>input[1])*entity_array_end+!(entity_array_end>input[1])*input[1];
        entity_array_end = !(entity_array_end>input[1])*entity_array_end+(entity_array_end>input[1])*input[1];
        
        res->begin =(value_type*) ( (entity_array_start>input[0])*uintptr_t(&entity_input) + !(entity_array_start>input[0])*uintptr_t(&entity_array_start) );
        res->end =(value_type*) (
                        (entity_array_end>input[1])*uintptr_t(&entity_array_end) 
                      +!(entity_array_end>input[1])*uintptr_t(&entity_input1));

        {// lower overflow. input[0] is not found.
            auto beg_ovf_cond = (2==(info0.overflow + (info0.direction < 0)));
            res->begin=(value_type*)(beg_ovf_cond*uintptr_t(entity)+!beg_ovf_cond*uintptr_t(res->begin));
            //res->end+=beg_ovf_cond;
        }

        {// upper overflow. input[0] is not found.
            auto end_ovf_cond = (2== (info1.overflow + (info1.direction > 0)));
            auto cur = (value_type*)
                   ((v0.value == *(res->end-1) )*uintptr_t(res->end-1)
                  +!(v0.value == *(res->end-1) )*uintptr_t(res->end));

            *(res->end+1) = end_ovf_cond*v0.value;
            res->end+=end_ovf_cond;
            
            *(res->end+1) = end_ovf_cond*input[1];
            res->end+=end_ovf_cond;
        }
        
        
        res->begin  += v0.is_contained;
        return res;
        
    }
    
    
    bool read_block(offset_type from,value_type continuous[2]){ return prfx->read(from,(void**)&continuous,sizeof(value_type)*2); }
    bool write_block(offset_type from,value_type continuous[2]){ return prfx->write(from,(void**)&continuous,sizeof(value_type)*2); }
    
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
            auto fd = prfx->fd;
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
    
    
    //cache_internal<preference_type> * m = 0;
};

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
        file_16_struct_type::value_type input[2] = {500,900}; // case : shrink  
        //auto pref = file_16_struct_type{.f=fdopen(fd,"r+b")};
        auto pref = file_16_struct_type{.fd=fd};
        //auto pref8 = file_8_struc_type{.fd=fd};
        auto a = cache{&pref/*,&pref8*/};
        auto fw = a.merge(input);
        
        

        {// gap
//            file_16_struct_type::value_type input[2] ={10,90};  
//            file_16_struct_type::value_type input[2] ={10,2000};  
//            file_16_struct_type::value_type input[2] ={890,925}; 
//            file_16_struct_type::value_type input[2] ={911,935}; 
//            file_16_struct_type::value_type input[2] ={920,950}; 
            file_16_struct_type::value_type input[2] ={920,951}; 
            
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
            
            // 2block
//            file_16_struct_type::value_type input[2] ={911,955}; 
//            file_16_struct_type::value_type input[2] ={911,945};  
            
//            file_16_struct_type::value_type input[2] ={911,935}; 
            
//            file_16_struct_type::value_type input[2] ={925,955}; 
//            file_16_struct_type::value_type input[2] ={935,955}; 
//            file_16_struct_type::value_type input[2] ={1980,2000}; 
//            file_16_struct_type::value_type input[2] ={1990,1991};

           // 1990 - 2000 

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
            a.debug_out_file(stdout,fd,0,3000);
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
