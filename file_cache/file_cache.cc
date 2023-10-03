
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <fcntl.h>
#include "libgen.h"

int mkdir_recurst(char * p);

template <typename value_type>
struct element{
    value_type beg;
    value_type end;
};

template <typename value_type>
struct file_cache_internal{
    ~file_cache_internal(){
        close(fd);
    }
    
    std::string uri;
    int fd=-1;
    long pos=0;
    struct stat st;
    value_type diff = 1;
};


template <typename value_type>
struct file_cache{
    
    file_cache() : m(new file_cache_internal<value_type>()){}
    ~file_cache() { delete m; }
    int set_uri(const char * p,int mode = 0644){
//        struct stat st;
        if(stat(p,&m->st)){
            m->uri=p;
            auto buf_dir = std::string{p}; 
            if(0==mkdir_recurst(dirname(buf_dir.data()))){
                return !((m->fd = open(p,O_CREAT|O_EXCL|O_RDWR,mode))>0);
            }
        }else return !((m->fd = open(p,O_RDWR,mode)) > 0);
       return 1; 
    };
    
    
    int set_diff(value_type * diff){ m->diff = *diff; return 0; }
    
    
    
    /// @return 0,1,2 
    /// -1 : do not overlap(__x < __y)  
    /// 0 : has overlap (4 pattern but treat as 1)  
    /// 1 : do not overlap(__y < __x)  
    int cmp(value_type * __xb,value_type * __xe,value_type * __yb,value_type * __ye){
        if(*__xe < *__yb) return -1;
        if(*__ye < *__xb) return 1;
        return 0;
    }
    
    
    value_type abs(value_type const& v){ return ((v < 0)*-v) + (!(v < 0)*v); }
    
    
    int find(long * pos,value_type * yb,value_type * ye){
        fstat(m->fd,&m->st);
        auto seekto = (m->st.st_size / (sizeof(value_type)*2)) / 2 * sizeof(value_type)*2;
        printf("%lld\n",seekto);
        fflush(stdout);
        
        lseek(m->fd,seekto,SEEK_SET);
        
        
        value_type xb,xe;
        read(m->fd,&xb,sizeof(value_type));
        read(m->fd,&xe,sizeof(value_type));
        
        constexpr auto kReadBufferMax = 1024;
        switch(cmp(&xb,&xe,yb,ye)){
            case 0:{
//                // possible to merge
//                auto min = (m->diff < abs(xb - *yb))* xb + !(m->diff < abs(xb - *yb))* *yb;
//                auto max = (m->diff > abs(-xe+ *ye))* xe + !(m->diff > abs(-xe + *ye))* *ye;
//                lseek(m->fd,seekto+sizeof(value_type)*2,SEEK_SET);
//                value_type betw_b,betw_e;
//                auto write_head = seekto;
//                
//                for(auto b = 1;b!=0;){
//                    b = read(m->fd,&betw_b,sizeof(value_type));
//                    b += read(m->fd,&betw_e,sizeof(value_type));
//                    if(!b){ // there is no latter part
//                        lseek(m->fd,write_head,SEEK_SET);
//                        b = ::write(m->fd,&min,sizeof(value_type));
//                        b += ::write(m->fd,&max,sizeof(value_type));
//                        break;
//                    }else{ // there is latter part. iterate searching for mergiable block
//                        printf("w(%lld) r(%lld)",betw_b,betw_e); fflush(stdout);
//                        if(!(max - betw_e> m->diff)){ 
//                            printf("all is mergiable."); fflush(stdout);
//                            auto read_head = long(-1);
//                            if(!(max-betw_b< m->diff )){ // possible to merge 
//                                {
//                                    read_head=tell(m->fd)+(sizeof(value_type)*2);
//                                    lseek(m->fd,seekto,SEEK_SET);
//                                    b = ::write(m->fd,&min,sizeof(value_type));
//                                    b += ::write(m->fd,&betw_e,sizeof(value_type));
//                                    write_head = tell(m->fd);
//                                }
//                                
//                                char buffer[kReadBufferMax];
//                                auto readsize=0;
//                                while(true){
//                                    printf("w(%ld) r(%ld)",write_head,read_head); fflush(stdout);
//                                    {
//                                        lseek(m->fd,read_head,SEEK_SET);
//                                        readsize=read(m->fd,buffer,sizeof(buffer));
//                                        read_head = tell(m->fd);
//                                    }
//                                    if(readsize == 1024){
//                                        lseek(m->fd,write_head,SEEK_SET);
//                                        ::write(m->fd,buffer,readsize);
//                                    }else{
//                                        break;
//                                    }
//                                }
//                                // write cur to end from here 
//                            }else{
//                                printf("last one is not mergiable. be not implemented yet"); fflush(stdout);
//                                abort();
//                                int jjj = 0;
//                                
//                                // can not merge
//                            }
//                            break;
//                        }
//                    }
//                }
                break;
            }
            case -1:{
                append(yb,ye);
                break;
            } 
            case 1: {
                
                
                printf("%lld %lld\n", *yb,*ye);
                printf("%lld", seekto);
                fflush(stdout);
                
//                auto read_head = seekto;
//                auto write_head = read_head+sizeof(value_type)*2; 
//                lseek(m->fd,read_head,SEEK_SET);
//                constexpr auto buffer_len = 1024; 
//                char buffer[buffer_len];
//                auto read_bytes=read(m->fd,buffer,buffer_len);
//                lseek(m->fd,write_head,SEEK_SET);
//                write(m->fd,buffer,read_bytes);
                
//                
//                append(yb,ye);
                
                
                
                // can not merge
                break;
            }
            break;
        }
        
        printf("beg(%lld) end(%lld)\n",xb,xe);
        
        return 0;
    }
    
    
    int append(value_type * beg,value_type * end){
        auto cur = lseek(m->fd,0,SEEK_END);
        auto b = ::write(m->fd,beg,sizeof(value_type));
        lseek(m->fd,cur+sizeof(value_type),SEEK_SET); // about particular value, data of 9 bytes are written for some reason.     
        b += ::write(m->fd,end,sizeof(value_type));
        return b>0;
    }
    
    
    int add(value_type * beg,value_type * end){
        fstat(m->fd,&m->st);
        if(0==m->st.st_size) this->append(beg, end);
        else{
            if(0!=find(&m->pos,beg,end)) this->append(beg, end);
        }
        return 0;
    }
    
    
    void dbg(){
        auto cur = lseek(m->fd,0,SEEK_SET);
        value_type x,y;
        auto cnt = 0;
        for(;;){
            auto b = read(m->fd,&x,sizeof(value_type));
            b += read(m->fd,&y,sizeof(value_type));
            if(!b) break;
            printf("%d %lld %lld\n",cnt++,x,y); fflush(stdout);
        }
        
        
    }
    
    
    file_cache_internal<value_type> *m=0;
    
};




int tmain_kautil_cache_file_cache_static() {

    {
        auto f = "./tmain_kautil_cache_file_cache_static";
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
        
        using value_type = uint64_t;
        for(auto i = 0; i < 100 ; ++i){
            auto beg = value_type(i*10);
            auto end = beg+10;
            auto cur = tell(fd);
            write(fd,&beg,sizeof(value_type));
            lseek(fd,cur+sizeof(value_type),SEEK_SET);
            write(fd,&end,sizeof(value_type));
            lseek(fd,cur+(sizeof(value_type)*2),SEEK_SET);
        }
        lseek(fd,0,SEEK_SET);
        
        struct stat st;
        fstat(fd,&st);
        
        auto res = 0;
        auto pos = 0l;
        auto max_size = st.st_size; 
        auto min_size = st.st_size;
        auto want=-10;
        
        auto is_continue = true;
        while(is_continue){
            
            auto mid = (max_size-pos) /  2 + pos;
            auto adj = mid % (sizeof(value_type)*2);

            min_size = (mid< min_size)*mid + !(mid< min_size)*min_size; 
            
            auto lb = static_cast<long>(mid - adj - (sizeof(value_type)*2));
            auto rb = static_cast<long>(mid - adj );
            
            lb *= !(lb < 0);
            rb *= !(rb > st.st_size);
            
            
            value_type lv;
            lseek(fd,lb,SEEK_SET);
            read(fd,&lv,sizeof(value_type));
            
            res = /*((want == lv) * 0) +*/ ((want < lv)*-1) + ((want > lv) *1);  
            pos=static_cast<long>(lb-(sizeof(value_type)*2)*bool(res));
            
            value_type rv(0);
            if(res==1){
                lseek(fd,rb,SEEK_SET);
                read(fd,&rv,sizeof(value_type));
                res = /*((want == rv) * 0) +*/ ((want > rv) *1);  
                pos=static_cast<long>(rb+(sizeof(value_type)*2*bool(res)));
            }
            
            
            max_size = (res < 0)*long(lb) + !(res < 0)*max_size;
            pos=!(res==-1)*pos;
           
            printf("%d %ld [%ld %ld] (%ld %ld)\n",res,pos,lb,rb,lv,rv); fflush(stdout);
            
            is_continue= !( (res==0) + (mid==0) + (mid>=st.st_size) + (lb<=0)); 
        }
        
        
        
        int jjj = 0;
        
        
    }
    
    return 0;
    
    
    
    //    printf("%f",std::numeric_limits<float>::min());
//    exit(0);
    
    
    auto f = "./test/cache/uint64.cache";
    remove(f);
    auto diff = uint64_t(1);
    auto cache = file_cache<uint64_t>{};
    auto b = cache.set_uri(f);
    cache.set_diff(&diff);
    printf("%d\n",b);
    {
        uint64_t beg = 30;
        uint64_t end = 40;
        cache.add(&beg,&end);
    }
    
    {
        uint64_t beg = 100;
        uint64_t end = 200;
        cache.add(&beg,&end);
    }
    cache.dbg();

    printf("++++++++++++++++++++\n");fflush(stdout);
    {
        uint64_t beg = 10;
        uint64_t end = 20;
        cache.add(&beg,&end);
    }
    cache.dbg();
    
    return 0;
    
    
    
    exit(0);
    
    // expect {10,20} {100 200} 
    exit(0);
    

    {
        uint64_t beg = 150;
        uint64_t end = 300;
        cache.add(&beg,&end);
    }
    cache.dbg();
//    cache.dbg();
    
    
    
    return 0;
}


int mkdir_recurst(char * p){
    auto c = p;
    struct stat st;
    if(!stat(p,&st)) return 0;
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
