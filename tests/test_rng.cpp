#include "simple_kernels.h"
#include "render_driver.h"
#include <cassert>

int main() {
    static_assert(sizeof(Random)==16);
#ifdef RNG_XOROSHIRO128PP
    const std::uint32_t known[]={0,0x000260c0,0x180acc04,0x9e226d35,0x849bc9ac,0x31c5870f,0x17790d7c,0x94fc9bb1};
#elif defined(RNG_XOROSHIRO128SS)
    const std::uint32_t known[]={0,0x00000016,0x86b5b3ad,0x800044a4,0x73fe9d66,0xd9d20b3a,0x7635a9c6,0xe62f03ff};
#else
    // Official PCG minimal example initstate42/initseq54.
    const std::uint32_t known[]={0xa15c02b7,0x7b47f409,0xba1d3330,0x83d2f293,0xbfa4784b,0xcbed606e};
#endif
    Random r;
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
    r.state=1;r.increment=2;
#else
    r.seed(42,54);
#endif
    for(auto expected:known) assert(r.next()==expected);
    r.seed(Random::seed_key(42),54);
    std::array<int,256> bins{};double sum=0,squares=0,product=0,previous=0;
    constexpr int n=1000000;
    for(int i=0;i<n;++i) {
        float u=r.uniform();assert(u>=0 && u<1);
        ++bins[int(u*256)];sum+=u;squares+=u*u;if(i)product+=previous*u;previous=u;
    }
    double mean=sum/n,variance=squares/n-mean*mean,correlation=(product/(n-1)-mean*mean)/variance;
    double chi2=0;for(int count:bins)chi2+=(count-n/256.0)*(count-n/256.0)/(n/256.0);
    assert(std::abs(mean-.5)<.002 && std::abs(variance-1.0/12)<.001 && std::abs(correlation)<.008);
    assert(chi2>100 && chi2<450);
    double cross=0,first_sum=0,second_sum=0;
    for(int i=0;i<200000;++i) {
        Random a,b;a.seed(Random::seed_key(101),i);b.seed(Random::seed_key(102),i-1);
        assert(a.state!=b.state || a.increment!=b.increment);
        float x=a.uniform(),y=b.uniform();first_sum+=x;second_sum+=y;cross+=x*y;
    }
    double stream_corr=(cross/200000-(first_sum/200000)*(second_sum/200000))*12;
    assert(std::abs(stream_corr)<.02);
    std::cout << "{\"rng\":\""<<Random::name()<<"\",\"known_vectors_pass\":true,\"mean\":"<<mean
              <<",\"variance\":"<<variance<<",\"lag1_correlation\":"<<correlation
              <<",\"bins_chi2\":"<<chi2<<",\"neighbor_seed_stream_correlation\":"<<stream_corr<<"}\n";
}
