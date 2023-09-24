#include "./cache.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdio.h>


namespace kautil{
namespace cache{

struct range{ pos_type s;pos_type e; } __attribute__ ((aligned (8), packed));
struct DataInternal{ std::vector<range> v; };
using itr_type = std::vector<range>::iterator;


Data::Data(bool){}
Data Data::factory_emepty(){ return Data(false); }
void Data::move_to(Data * move_to){
    if(move_to->m) delete move_to->m;
    move_to->m = m;
    m = nullptr;
}


Data::Data() : m(new DataInternal){}
Data::~Data(){ if(m) delete m; }
Data::Data(Data && d){ d.m = m;m = nullptr; }

size_type Data::byte_size(){ return m->v.size() * 2 * sizeof(pos_type); }
pos_type * Data::data(){
    if(!m) m = new DataInternal{};
    return (pos_type *) m->v.data();
}
size_type Data::size(){
    if(!m) m = new DataInternal{};
    return m->v.size() * 2;
}

pos_type const& npos = -1;
struct CacheInternal{
    friend struct Cache;
    Data data;
    std::vector<range> & get_vector(){
        if(!data.m) data.m = new DataInternal();
        return data.m->v;
    }
    std::pair<range*,range*> a(bool & cached,bool & sf,bool & ef,const pos_type &s, const pos_type &e);
};
Cache::Cache() : m(new CacheInternal){}
Cache::~Cache(){ if(m) delete m; }

void Cache::clear() { m->data.m->v.clear(); }
bool Cache::assign(void * data, const size_type &size) {
    auto const& len = size/sizeof(range);
    if(len%2) return false;
    if(data){
        m->get_vector().assign((range*)data,(range*)(reinterpret_cast<uintptr_t>(data)+size));
    }else{
        m->get_vector().resize(0);
    }
    return true;
}


void Cache::add(pos_type const& s , pos_type const& e){
    m->get_vector().push_back(range{.s=s,.e=e});
}


Data * Cache::stack(){ return &m->data; }
void Cache::arrange_stack(){
    auto & c = m->get_vector();
    std::sort(c.begin(),c.end(),[](auto const& l , auto const& r ){ return l.s < r.s; });
    bool ocr;
    do{
        ocr = false;
        for(auto itr = c.begin(); itr+1 != c.end(); ++ itr){

            if(itr->s == npos) continue;
            auto & f = *itr;
            auto & l = *(itr+1);

            // f.e and l.s :  n and n+1 => merge
            if( f.e + 1 < l.s ) continue;
            ocr = true;
            l.s = f.s;
            l.e = (f.e > l.e) ? f.e : l.e;
            f.s = -1;
        }
    }while(ocr);

    std::sort(c.begin(),c.end(),[](auto const& l , auto const& r ){ return l.s < r.s; });
    auto at = std::find_if(c.begin(),c.end(),[](auto const& v){ return v.s == npos; });
    if(at != c.end()) c.resize(at - c.begin());
}


bool Cache::count(const pos_type &s, const pos_type &e) {
    for(auto const& v : m->get_vector())
        if( (v.s <= s && s <= v.e) && (v.s <= e && e <= v.e)) return true;
    return false;
}



Data Cache::search(pos_type const& s, pos_type const& e){
    Data res;
    auto & c = m->get_vector();
    auto sitr = c.end();
    for(auto itr = c.begin(); itr != c.end(); ++itr )if( s < itr->s ||  (itr->s <= s && s <= itr->e) ) { sitr=itr; break;}

    auto eitr = c.end();
    for(auto itr = c.end()-1; ; --itr){
        if( itr->e < e || (itr->s <= e && e <= itr->e)  ) { eitr=itr; break; }
        if(itr == c.begin()) break;
    }

    if((sitr != c.end() && eitr != c.end())){
        auto & vec = res.m->v;
        vec =  std::vector<range>{sitr,eitr+1};
        if(s > vec.front().s)vec.front().s = s;
        if(e < vec.back().e) vec.back().e = e;
    }
    return res;
}

Data Cache::make_gap(Data * cur){
    Data res;
    auto & current = cur->m->v;
    { // get blanck
        if(current.size() > 1){
            auto & v = res.m->v;
            auto s = (pos_type*)current.data() + 1;
            v.assign((range*) s, (range*)s + (current.size() - 1));
        }
    }
    return res;
}


size_type Cache::gap_bytes_max(){ return (m->get_vector().size() + 2) * sizeof(pos_type); }
//void Cache::gap_bytes_max(size_type * res){ *res = gap_bytes_max(); }


std::pair<range*,range*> CacheInternal::a(bool & cached,bool & sf,bool & ef, const pos_type &s, const pos_type &e) {
    auto & c = get_vector();
    auto res = std::pair<range*,range*>{nullptr,nullptr};
    if(c.empty()) return res;
    auto & beg = res.first;
    auto & end = res.second;

    for(auto itr = c.begin(); itr != c.end() ;++itr){
        if(!beg){
            if(s < itr->s) beg = itr.base();
            if(itr->s <= s && s <= itr->e){
                if(itr->s <= e && e <= itr->e){ cached = true; return res; }
                beg=itr.base(); sf=true; break;
            }
        }
    }

    for(auto itr = c.end()-1;;--itr){
        if(!end){
            if(itr->e < e) end = itr.base();
            else if(itr->s <= e && e <= itr->e){ end=itr.base(); ef=true;  break; }
        }
        if(itr == c.begin()) break;
    }
    return {beg,end};
}


size_type Cache::make_gap(void * dst,pos_type const& s, pos_type const& e){
    if(m->get_vector().empty()){
        static pos_type size_2 = sizeof(pos_type)*2;
        auto arr = (pos_type*) dst;
        arr[0] = s; arr[1] = e;
        return size_2;
    }
    /*
    possible to make a bit fater
     do count and remain iterator .... x
     do search from x
    */

    auto cached=false, sf = false,ef = false;
    auto [beg,end] = m->a(cached,sf,ef,s,e);
    if(cached)return 0;

    if(!beg && end) beg = end;
    else if(beg && !end) end = beg;

    if(beg != end){ /* case : cache has multiple block  */
        auto arr = (pos_type*) dst;
        auto last_elem = (end == m->get_vector().end().base()) ?  end-1 : end ; /* end may indecate end of vector not last value */
        if( (e < beg->s) || (last_elem->e < s) ){
            *arr = s;++arr;
            *arr = e;++arr;
        }else{
            if( s < beg->s){ *arr =s;++arr;*arr =beg->s-1;++arr; } /*outside : former side*/
            { // middle
                auto const& bytesize = (last_elem-beg /* +1-1 */)*sizeof(range);
                auto const& len = bytesize/sizeof(pos_type);
                std::memcpy( arr,((pos_type*)beg + 1) ,bytesize);
                for(auto ii = 0 ; ii < len ; ii+=2){ arr[ii]+=1;arr[ii+1]-=1; }
                arr+=len;
            }
            if( last_elem->e < e){ *arr = last_elem->e+1; ++arr;*arr =e;++arr; } /* outside : latter side*/
        }
        return (arr - (pos_type*)dst) * sizeof(pos_type);
    }else{  /* case : beg == end   */
        // (1)
        auto arr = (pos_type*) dst;
        auto cur = int8_t(0);
        if(e < beg->s || beg->e < s){
            arr[cur++] = s;
            arr[cur++] = e;
        }else{
            if(s < beg->s){
                arr[cur++] = s;
                arr[cur++] = beg->s-1;
            }
            if(beg->e < e){
                arr[cur++] = beg->e+1;
                arr[cur++] = e;
            }
        }
        return sizeof(pos_type)*cur;
    }

}


} //namespace cache{
} //namespace kautil{

