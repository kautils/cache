


#ifndef _KAUTIL_CACHE_CACHE_H
#define _KAUTIL_CACHE_CACHE_H

#include <stdint.h>

namespace kautil{
namespace cache{

using pos_type = uint64_t;
using size_type = uint64_t;


struct DataInternal;
struct Data{
    
    friend class Cache;
    friend class CacheInternal;

    Data();
    Data(Data const& d) = delete;
    Data & operator=(Data const& d) = delete;
    ~Data();
    pos_type * data();
    size_type size();
    size_type byte_size();
    void move_to(Data * );
    static Data factory_emepty();


private:
    Data(Data && d);
    Data(bool);
    DataInternal * m = nullptr;
};



extern pos_type const& npos;
struct CacheInternal;
struct Cache{

    Cache();
    ~Cache();
    bool count(pos_type const& s , pos_type const& e);
    void add(pos_type const& s , pos_type const& e);
    void arrange_stack();
    bool assign(void * data,size_type const& size);
    void clear();
    Data * stack();
    size_type gap_bytes_max();
    Data search(pos_type const& s, pos_type const& e);
    Data make_gap(Data * cur);
    size_type make_gap(void * dst,pos_type const& s, pos_type const& e);


private:
    CacheInternal * m = nullptr;
};


} //namespace cache{
} //namespace kautil{










#endif