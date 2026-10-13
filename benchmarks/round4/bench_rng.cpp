#include "simple_kernels.h"
#include "render_driver.h"
#include <chrono>
volatile std::uint64_t sink;
template<int Draws> void short_streams() {
 constexpr int paths=2000000;std::uint64_t sum=0;auto key=Random::seed_key(42);
 auto start=std::chrono::steady_clock::now();
 for(int i=0;i<paths;++i){Random random;random.seed(key,i);for(int j=0;j<Draws;++j)sum+=random.next();}
 sink=sum;double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"{\"rng\":\""<<Random::name()<<"\",\"draws_per_seed\":"<<Draws<<",\"nanoseconds_per_draw\":"<<seconds*1e9/(paths*Draws)<<",\"checksum\":"<<sum<<"}\n";
}
int main(){short_streams<2>();short_streams<16>();short_streams<40>();}
