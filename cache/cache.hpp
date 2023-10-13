// todo : refactor api (e.g : max_size => size, void read => int read)


#include "kautil/region/region.hpp"
#include "kautil/algorithm/btree_search//btree_search.hpp"
#include <stdint.h>



namespace kautil{


template<typename preference_type>
struct cache{
    
    using offset_type = typename preference_type::offset_type;
    using value_type = typename preference_type::value_type;
    
    cache(preference_type *  prf) : pref(prf){}
    ~cache(){ }

    int merge(value_type * begin,value_type * end){ value_type input[2]={*begin,*end};return merge(input); }
    bool exists(value_type * begin,value_type * end){ value_type input[2]={*begin,*end};return exists(input); }
    struct gap_context{ value_type *begin;value_type *end=0; value_type * entity=0; };
    void gap_context_free(gap_context * ctx){ if(ctx)delete ctx->entity; delete ctx; }
    gap_context* gap(value_type * begin,value_type * end){ value_type input[2]={*begin,*end};return gap(input); }
    gap_context* gap(value_type input[2]){ 
        
        auto info0 = kautil::algorithm::btree_search{pref}.search(input[0]);
        auto info1 = kautil::algorithm::btree_search{pref}.search(input[1]);
        
        auto v0 =adjust_nearest(input[0],info0.direction,info0.overflow,info0.nearest_value,info0.nearest_pos);
        auto v1 =adjust_nearest(input[1],info1.direction,info1.overflow,info1.nearest_value,info1.nearest_pos);
        if(0==( !v0.is_contained + !v1.is_contained + !(v0.pos==v1.pos) ))return 0;
        
        input[0]= adjust_with_neighbor(input[0],info0.direction,info0.overflow,info0.neighbor_value,info0.neighbor_pos);
        input[1]= adjust_with_neighbor(input[1],info1.direction,info1.overflow,info1.neighbor_value,info1.neighbor_pos);
        
        auto entity_bytes = v1.pos - v0.pos + pref->block_size();
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


        auto block_size = pref->block_size();
        auto entity_len  = ((high_pos - low_pos + sizeof(value_type))/sizeof(value_type)) +2;
        auto entity = new value_type[entity_len+1]; /* +1 : for the end value */
        auto arr_len = entity_len-2;
        auto arr = entity+1;
        pref->read(low_pos, (void**)&arr, arr_len * sizeof(value_type));
        
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
    
    int merge(value_type input[2]){ 
        value_type diff = 1;
        
        auto btres =kautil::algorithm::btree_search{pref}.search(static_cast<value_type>(input[0]));
        {// return immediately
            auto nearest_right = value_type(0);
            auto nearest_right_ptr = &nearest_right;
            pref->read_value(btres.nearest_pos + sizeof(value_type), &nearest_right_ptr);
            if(2==((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr))) return 0;
        }
        
        auto block_size=pref->block_size();
        auto start_block =btres.nearest_pos; 
        value_type cmp[2];
        value_type new_block[2]={input[0],input[1]};
        auto gap_cnt=-1;{
            auto max_size=pref->size();
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
            kautil::region{pref}.shrink(end_block, shrink_size, buffer_size);
        }else if(gap_cnt < 0){ /* block is unique. must extend region for that block. */
            kautil::region{pref}.claim(rpos, block_size, buffer_size);
            write_block(rpos,input);
        }else{ /* block is contained by (or has overlap with) the first one. possible to merge them. */
            //printf("write (%ld,b(%lld),e(%lld))\n",start_block,new_block[0],new_block[1]); fflush(stdout);
            write_block(start_block,new_block);
        }
        
        pref->flush_buffer();
        return 0;
    }
   
    
    bool exists(value_type input[2]){
        auto btres =kautil::algorithm::btree_search{pref}.search(static_cast<value_type>(input[0]), false);
        if(btres.direction) return false;
        auto nearest_right = value_type(0);
        auto nearest_right_ptr = &nearest_right;
        pref->read_value(btres.nearest_pos + sizeof(value_type), &nearest_right_ptr);
        return 2== (((input[0] == btres.nearest_value) +(input[1] == *nearest_right_ptr)));
    }
    
    
    
private:
    
    
    bool read_block(offset_type from,value_type continuous[2]){ return pref->read(from, (void**)&continuous, sizeof(value_type) * 2); }
    bool write_block(offset_type from,value_type continuous[2]){ return pref->write(from, (void**)&continuous, sizeof(value_type) * 2); }
    
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
        }else pref->read(nearest_pos - sizeof(value_type), (void**)&vp, sizeof(value_type));
        
        auto v1 =value_type(0);
        auto vp1 = &v1;
        
        if(0 == (!is_overflow + !(direction>0)) ){
            v = input_value;
            nearest_pos= 0;
            overflow_lower=true;
        }else{
            pref->read(nearest_pos + sizeof(value_type), (void**)&vp1, sizeof(value_type));
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
        pref->read(
            next_pos
            ,(void**)&ptr
            ,sizeof(value_type));
        
        auto is_contained = !ovf*bool((2==((neighbor_v<=i) + (i<=next_value))) + (2==((next_value<=i) + (i<=neighbor_v))));
        return is_contained*next_value +!is_contained*i;
    }
    
    offset_type buffer_size = 512; // on mingw in my env, over 824 is problematic.
    preference_type * pref=0;

    
    //cache_internal<preference_type> * m = 0;
};


} //namespace kautil{


