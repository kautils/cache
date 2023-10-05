
#include <sys/stat.h>
#include <fcntl.h>
#include "kautil/region/region.hpp"
#include "kautil/algorithm/btree_search/btree_search.hpp"

int mkdir_recurst(char * p);


#include <stdint.h>
#include <stdio.h>

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


#include <string>

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
    for(auto i = 0; i < 100 ; ++i){
        auto beg = file_syscall_16b_pref::value_type(i*10);
        auto end = beg+10;
        auto cur = tell(fd);
        auto block_size = sizeof(file_syscall_16b_pref::value_type);
        write(fd,&beg,block_size);
        lseek(fd,cur+block_size,SEEK_SET);
        write(fd,&end,block_size);
        lseek(fd,cur+block_size*2,SEEK_SET);
    }
    lseek(fd,0,SEEK_SET);
    
    
    
    auto search_v=file_syscall_16b_pref::value_type(550);
    auto pref = file_syscall_16b_pref{.fd=fd};
    auto bt = kautil::algorithm::btree_search{&pref};
    auto pos = file_syscall_16b_pref::offset_type(0);
    auto direction = int(0);
    auto found = bt.search(static_cast<file_syscall_16b_pref::value_type>(search_v),&pos,&direction);
    printf("%s. pos is %ld. direction is %d\n",found?"found": "not found",pos,direction);

    
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
