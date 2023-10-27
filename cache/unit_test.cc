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
    using value_type = pref_t::value_type;
    using offset_type = pref_t::offset_type;
    
    cache(pref_t * pref) : pref(pref)
        ,kautil::range::merge<pref_t>(pref) 
        ,kautil::range::exists<pref_t>(pref) 
        ,kautil::range::gap<pref_t>(pref) 
    {
        
    }
    ~cache(){}
    
    bool exists(value_type from, value_type to){
        return kautil::range::exists<pref_t>::exec(from,to);
    }
    
    
    
    pref_t * pref= 0;
};



#include "cache.hpp"
#include "stdio.h"





int main(){
    
    
    
    
    
    printf("++++\n");
    return 0;
}



#endif


