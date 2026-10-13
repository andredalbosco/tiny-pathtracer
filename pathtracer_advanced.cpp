#include "advanced_kernels.h"
#include "render_driver.h"

int main(int argc, char** argv) {
    try {
        Options options = parse_options(argc, argv, "culled");
        if (options.kernel == "affine") return render_main<aggressive::AffineScene<SceneData>>(options);
        if (options.kernel == "culled") return render_main<aggressive::CulledScene<SceneData>>(options);
        if (options.kernel == "ray8") return render_main<aggressive::Ray8Scene<SceneData>>(options);
        if (options.kernel == "ray8-affine") return render_main<aggressive::Ray8AffineScene<SceneData>>(options);
        throw std::runtime_error("Kernel must be affine, culled, ray8 or ray8-affine");
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\nUsage: " << argv[0]
                  << " [samples] [width] [output.hdr] [--kernel affine|culled|ray8|ray8-affine]"
                  << " [--threads N] [--runtime std|omp] [--worker-report output.json] [--cpus C0,C1,...] [--batch 1|2|4|8] [--layout aos|soa] [--raw] [--refill]\n";
        return 1;
    }
}
