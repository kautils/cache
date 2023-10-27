#ifndef FLOW_CACHE2_CACHE_CACHE_HPP
#define FLOW_CACHE2_CACHE_CACHE_HPP

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
    
    cache(pref_t * pref, value_type diff,offset_type buffer_size = 512) : pref(pref)
        ,kautil::range::merge<pref_t>(pref) 
        ,kautil::range::exists<pref_t>(pref) 
        ,kautil::range::gap<pref_t>(pref){
        kautil::range::merge<pref_t>::set_diff(diff);
        kautil::range::merge<pref_t>::set_buffer_size(buffer_size);
    }
    ~cache(){}
    
    bool exists(value_type from, value_type to){ return kautil::range::exists<pref_t>::exec(from,to); }
    int merge(value_type from, value_type to){ return kautil::range::merge<pref_t>::exec(from,to); }
    kautil::range::gap<pref_t> gap(value_type from, value_type to){
        kautil::range::gap<pref_t>::initialize(from,to);
        return dynamic_cast<kautil::range::gap<pref_t>&>(*this);
    }
    pref_t * pref= 0;
};



#endif