#include "../experiments/simple_kernels.h"
#include "../experiments/render_driver.h"
#include <cassert>

int main() {
    Random random;
    random.seed(42, 54);
    const std::uint32_t expected[] = {0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (auto value : expected) assert(random.next() == value);
    std::mt19937 engine(73);
    std::uniform_real_distribution<float> position(-1000, 1000), component(-1, 1);
    int tested = 0;
    for (int count : {0, 1, 7, 8, 9, 15, 16, 17, 35, 36}) {
        SceneData data = cornell_box();
        data.triangles.erase(data.triangles.begin() + count, data.triangles.end());
        ScalarScene scalar(data);
        FlatScene flat(data); flat.prepare();
        RayScene batch(data);
        ScalarScene::Scratch scalar_scratch;
        FlatScene::Scratch flat_scratch(count);
        RayScene::Scratch batch_scratch;
        for (int group = 0; group < 2500; ++group) {
            Ray rays[8]; Hit hits[8];
            for (auto& ray : rays) {
                ray = {{position(engine), position(engine), position(engine)},
                       normalize({component(engine), component(engine), component(engine)})};
            }
            batch.intersect_batch(rays, hits, 8, batch_scratch);
            for (int r = 0; r < 8; ++r) {
                Hit reference, candidate;
                scalar.intersect(rays[r], reference, scalar_scratch);
                flat.intersect(rays[r], candidate, flat_scratch);
                assert(reference.triangle == candidate.triangle && reference.triangle == hits[r].triangle);
                if (reference.triangle >= 0) {
                    assert(reference.t == candidate.t && reference.t == hits[r].t);
                    assert(reference.u == candidate.u && reference.u == hits[r].u);
                    assert(reference.v == candidate.v && reference.v == hits[r].v);
                }
                ++tested;
            }
        }
    }
    std::cout << "PCG published sequence and " << tested << " exact scalar/flat/ray intersections passed\n";
}
