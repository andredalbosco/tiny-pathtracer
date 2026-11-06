#include "simple_kernels.h"
#include "render_driver.h"

int main(int argc, char** argv) {
    try {
        Options options = parse_options(argc, argv, "flat");
        if (options.kernel == "scalar") return render_main<ScalarScene>(options);
        if (options.kernel == "flat") return render_main<FlatScene>(options);
        if (options.kernel == "rays") return render_main<RayScene>(options);
        throw std::runtime_error("Kernel must be scalar, flat or rays");
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\nUsage: " << argv[0]
                  << " [samples] [width] [output.hdr] [--kernel scalar|flat|rays]"
                  << " [--threads N] [--runtime std|omp] [--worker-report output.json] [--cpus C0,C1,...] [--batch 1|2|4|8] [--layout aos|soa] [--raw] [--refill]\n";
        return 1;
    }
}
