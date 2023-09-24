
#ifdef TMAIN_KAUTIL_CACHE_STATIC

#include <iostream>
#include "./cache.h"
#include <cassert>
#include <algorithm>

int main(){

    auto lmb = [](auto * d){
        for(auto i = 0; i < d->size() - 1; i+=2){
            std::cout << d->data()[i] << " " << d->data()[i+1]  << std::endl;
        }
    };


    {
        kautil::cache::CacheFile x{};
        x.set_file("test.cache");
        x.add(123,1);
        x.arrange_stack();
    }
    
    
    exit(0);
    
    
//    for(;;)
    {
        kautil::cache::Cache x{};
        x.add(6,15);
        x.add(16,19);
        x.add(20,21);
        x.add(30,35);
        x.add(50,79);
        x.add(150,200);
        x.arrange_stack();
        auto c = x.stack();
        auto r = x.search(5,80);
        auto g = x.make_gap(&r);


        std::cout << "cached" << std::endl;
        lmb(c);

        std::cout << "specified range" << std::endl;
        lmb(&r);

        std::cout << "specified gap" << std::endl;
        lmb(&g);

        kautil::cache::pos_type buf[]={6,15,30,35};
        kautil::cache::pos_type buf2[]={100,200};

        assert(x.assign(nullptr,0));
        for(auto i = 0; i < sizeof(buf)/sizeof(kautil::cache::pos_type); i+=2) x.add(buf[i],buf[i+1]);
        x.add(100,200);

        auto ccc = x.stack();
        lmb(ccc);


        kautil::cache::Data dst = kautil::cache::Data::factory_emepty();
        x.stack()->move_to(&dst);
        lmb(&dst);
    }

    return 0;
}

#endif

#ifdef TMAIN_KAUTIL_CACHE_STATIC_MONKEY_TEST

#include "./cache.h"
#include <iostream>
#include <random>



int main(){


    // monkey test
    using kautil::cache::pos_type;
    using kautil::cache::size_type;
    auto rd = std::random_device{}; //Get a random seed from the OS entropy device, or whatever
    auto lim = (pos_type) std::numeric_limits<uint16_t>::max();
    auto distr = std::uniform_int_distribution<uint16_t>{};
    auto gen64 = std::mt19937_64(rd());

    for(;;){
        kautil::cache::Cache x{};
        auto times = 0;
        for(;;){
            ++times;
            auto s = distr(gen64);
            auto e = distr(gen64);
            if(s > e) std::swap(s,e);

            for(auto i = 0; i < 2; ++i){
                auto data = std::vector<uint8_t>{};
                if(!x.count(s,e)){
                    x.add(s,e);
                    x.arrange_stack();
                }else{
                    data.resize((x.stack()->size()+2)*sizeof(pos_type));
                    data.resize(x.make_gap(data.data(),s,e));
                }
            }// for : 2

            auto ss = x.stack()->data()[0];
            auto ee = x.stack()->data()[1];
            if(ss == 0 && ee == lim){
                std::cout << "success : " << lim << std::endl;
                std::cout << times << std::endl;
                break;
            }
        } // for ;;
    }


    
}



#endif