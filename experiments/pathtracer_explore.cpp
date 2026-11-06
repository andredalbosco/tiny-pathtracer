#include "experimental_kernels.h"
#include "render_driver.h"

int main(int argc, char** argv) {
    try {
        Options options = parse_options(argc, argv, "rays");
        if (options.kernel == "rays") return render_main<RayScene>(options);
        if (options.kernel == "numerator") return render_main<NumeratorScene>(options);
        if (options.kernel == "plane") return render_main<PlaneScene>(options);
        throw std::runtime_error("Kernel must be rays, numerator or plane");
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
