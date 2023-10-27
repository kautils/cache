#ifdef TMAIN_KAUTIL_CACHE_INTERFACE



#include "kautil/range/exists/exists.hpp"
#include "kautil/range/merge/merge.hpp"
#include "kautil/range/gap/gap.hpp"

template<typename pref_t>
struct cache 
        : private kautil::range::merge<pref_t>
        , private kautil::range::exists<pref_t>
        , private kautil::range::gap<pref_t>
        {
    using value_type = typename pref_t::value_type;
    using offset_type = typename pref_t::offset_type;
    
    cache(pref_t * pref) : pref(pref)
        ,kautil::range::merge<pref_t>(pref) 
        ,kautil::range::exists<pref_t>(pref) 
        ,kautil::range::gap<pref_t>(pref)
    {
        
    }
    ~cache(){}
    
    using kautil::range::merge<pref_t>::set_diff;
    bool exists(value_type from, value_type to){ return kautil::range::exists<pref_t>::exec(from,to); }
    int merge(value_type from, value_type to){ return kautil::range::merge<pref_t>::exec(from,to); }
    kautil::range::gap<pref_t> gap(value_type from, value_type to){
        kautil::range::gap<pref_t>::initialize(from,to);
        return *this;
    }
    
    
    pref_t * pref= 0;
};



#include "cache.hpp"
#include "stdio.h"



#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include "unistd.h"


template<typename premitive>
struct file_syscall_premitive{
    using value_type = premitive;
    using offset_type = long;

    int fd=-1;
    char * buffer = 0;
    offset_type buffer_size = 512;
    
    ~file_syscall_premitive(){ free(buffer); }
    offset_type block_size(){ return sizeof(value_type); }
    offset_type size(){ return lseek(fd,0,SEEK_END)- lseek(fd,0,SEEK_SET); }
    
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
    
    bool extend(offset_type extend_size){ 
        return !ftruncate(fd,size()+extend_size);   
    }
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

using file_syscall_16b_pref= file_syscall_premitive<uint64_t>;
using file_syscall_16b_f_pref= file_syscall_premitive<double>;



int main(){
    
    
    // todo : kautil::range::merge<pref_t>::set_buffer may be curious.
    using value_type = uint64_t;
    using offset_type = long;
    auto f_ranges = fopen("tmain_kautil_range_exsits_interface.cache","w+b");
    auto pref = file_syscall_16b_pref{.fd=fileno(f_ranges),.buffer_size=512};
    
    auto from = value_type(0),to = value_type(0),diff=value_type(0);
    diff=1;
    from = 10;to = 15;
    auto c = cache{&pref};
    c.set_diff(diff);
    
    
    auto lmb_test = [](auto * c,auto from,auto to){
        if(!c->exists(from,to)){
            printf("not exists\n");
            c->merge(from,to);
        }else{
            printf("exists\n");
        }
    };
    
    lmb_test(&c,from,to);
    lmb_test(&c,from,to);
    for(auto const& elem : c.gap(0,20))printf("l,r(%lld,%lld)\n",elem.l,elem.r);
    return 0;
}



#endif


