#include "simple_kernels.h"
#include "render_driver.h"
#include "quality_sampling.h"

// Keep the quality experiment small: one ordinary C++ intersection kernel.
int main(int argc, char** argv) {
    try {
        std::string sampling="nee";
        std::vector<char*> arguments{argv[0]};
        for (int i=1; i<argc; ++i) {
            if (std::string(argv[i]) == "--sampling") {
                if (++i == argc) throw std::runtime_error("Missing sampling mode");
                sampling=argv[i];
            } else arguments.push_back(argv[i]);
        }
        Options options=parse_options(int(arguments.size()),arguments.data(),"rays");
        if (options.kernel != "rays") throw std::runtime_error("Quality modes use --kernel rays");
        if (sampling == "uniform") return render_main<RayScene>(options);
        if (sampling == "cosine") return render_main<QualityScene<Sampling::cosine>>(options);
        if (sampling == "nee") return render_main<QualityScene<Sampling::nee>>(options);
        if (sampling == "mis") return render_main<QualityScene<Sampling::mis>>(options);
        throw std::runtime_error("Sampling must be uniform, cosine, nee or mis");
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
