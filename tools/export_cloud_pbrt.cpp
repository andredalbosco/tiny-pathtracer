// Export independent PBRT-v4 scenes; reuse only scene settings and sky equations.
#define CLOUD_ATMOSPHERE_TEST
#include "../experiments/cloud_viewer.cpp"
#include <nanovdb/util/IO.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc,char** argv) try {
    namespace fs=std::filesystem;
    fs::path asset=fs::absolute(argc>1 ? argv[1] : "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb");
    fs::path out=fs::absolute(argc>2 ? argv[2] : "build/cloud-pbrt");
    int width=argc>3 ? std::stoi(argv[3]) : 256, spp=argc>4 ? std::stoi(argv[4]) : 1024;
    if (argc>5 || width<16 || width>4096 || width%4 || spp<1 || spp>1000000)
        throw std::runtime_error("Usage: export_cloud_pbrt [cloud.nvdb] [directory] [width divisible by 4] [spp]");
    if (fs::file_size(asset)>100000000) throw std::runtime_error("Cloud exceeds 100 MB");
    auto handle=nanovdb::io::readGrid(asset.string(),std::string("density"));
    const auto* grid=handle.grid<float>();
    if (!grid || !grid->isFogVolume() || !grid->activeVoxelCount() || handle.size()>100000000)
        throw std::runtime_error("Expected a float density fog grid under 100 MB");
    auto bounds=grid->worldBBox();
    auto extent=bounds.max()-bounds.min(), center=(bounds.max()+bounds.min())*0.5;
    double scale=std::max({extent[0],extent[1],extent[2]})*0.5;
    if (!(scale>0)) throw std::runtime_error("Invalid cloud bounds");
    double h[]={extent[0]/(2*scale),extent[1]/(2*scale),extent[2]/(2*scale)};
    fs::create_directories(out);
    std::ofstream manifest(out/"scenes.json");
    manifest << std::setprecision(10) << "{\n  \"asset\": " << std::quoted(asset.string())
             << ",\n  \"asset_bytes\": " << fs::file_size(asset)
             << ",\n  \"width\": " << width << ", \"height\": " << width*3/4
             << ", \"spp\": " << spp << ",\n  \"scenes\": [\n";
    const char* names[]={"day","warm","cool","day-fuller"};
    for (int scene=0;scene<4;++scene) {
        auto p=cloud::preset(scene==3 ? 0 : scene);
        if (scene==3) { p.albedo=0.995f; p.extinction=8; }
        const std::string name=names[scene];
        float3 camera=p.camera(), sun=p.sun(), position{0,air::Radius+p.altitude,0};
        float3 sunlight=air::Solar*air::sunTransmittance(position,sun,p.haze,32);
        // PBRT's lat-long convention has +Z up. Rotate -90 degrees about X
        // in the scene to obtain our +Y-up world. Exclude the solar disc:
        // the separate distant light accounts for direct solar irradiance.
        constexpr int skyWidth=512,skyHeight=256;
        std::vector<float3> sky(skyWidth*skyHeight);
        #pragma omp parallel for schedule(dynamic,1)
        for (int y=0;y<skyHeight;++y) for (int x=0;x<skyWidth;++x) {
            float theta=air::Pi*(y+0.5f)/skyHeight, phi=2*air::Pi*(x+0.5f)/skyWidth;
            float3 direction{sinf(theta)*cosf(phi),cosf(theta),-sinf(theta)*sinf(phi)};
            sky[y*skyWidth+x]=air::trace(position,direction,1e20f,sun,p.haze,64,32,false).radiance;
        }
        std::ofstream pfm(out/(name+"-sky-latlong.pfm"),std::ios::binary);
        pfm << "PF\n" << skyWidth << ' ' << skyHeight << "\n-1.0\n";
        for (int y=skyHeight-1;y>=0;--y)
            pfm.write(reinterpret_cast<const char*>(sky.data()+y*skyWidth),skyWidth*sizeof(float3));
        if (!pfm) throw std::runtime_error("Cannot write sky map");

        std::ofstream f(out/(name+".pbrt"));
        f << std::setprecision(10)
          << "# Generated from cloud::preset in cloud_viewer.cpp. One unit = 1 km.\n"
          << "# Sky/sun evaluated at cloud center; no participating exterior air.\n"
          << "# Neutral cloud; HG g=.35; albedo=" << p.albedo << "; extinction=" << p.extinction << ".\n"
          << "ColorSpace \"srgb\"\nFilm \"rgb\" \"integer xresolution\" [" << width
          << "] \"integer yresolution\" [" << width*3/4 << "]\n"
          << "    \"string filename\" [" << std::quoted((out/(name+"-pbrt.exr")).string()) << "]\n"
          << "    \"string sensor\" [\"cie1931\"] \"float whitebalance\" [0]\n"
          << "    \"float maxcomponentvalue\" [1e30]\n"
          << "PixelFilter \"box\" \"float xradius\" [.5] \"float yradius\" [.5]\n"
          << "Sampler \"sobol\" \"integer pixelsamples\" [" << spp << "]\n"
          << "Integrator \"volpath\" \"integer maxdepth\" [128]\n"
          // PBRT's camera is left handed. Flip camera X to match our basis.
          << "Scale -1 1 1\nLookAt " << camera.x << ' ' << camera.y << ' ' << camera.z
          << "  0 0 0  0 1 0\nCamera \"perspective\" \"float fov\" [45]\nWorldBegin\n"
          << "AttributeBegin\nRotate -90 1 0 0\nLightSource \"infinite\" \"string filename\" ["
          << std::quoted((out/(name+"-sky.exr")).string()) << "]\nAttributeEnd\n"
          << "LightSource \"distant\" \"point3 from\" [" << sun.x << ' ' << sun.y << ' ' << sun.z
          << "] \"point3 to\" [0 0 0] \"rgb L\" [" << sunlight.x << ' ' << sunlight.y << ' ' << sunlight.z << "]\n"
          << "AttributeBegin\nScale " << 1/scale << ' ' << 1/scale << ' ' << 1/scale
          << "\nTranslate " << -center[0] << ' ' << -center[1] << ' ' << -center[2]
          << "\nMakeNamedMedium \"cloud\" \"string type\" [\"nanovdb\"]\n"
          << "    \"string filename\" [" << std::quoted(asset.string()) << "] \"string gridname\" [\"density\"]\n"
          << "    \"rgb sigma_a\" [" << p.extinction*(1-p.albedo) << ' ' << p.extinction*(1-p.albedo) << ' ' << p.extinction*(1-p.albedo) << "]\n"
          << "    \"rgb sigma_s\" [" << p.extinction*p.albedo << ' ' << p.extinction*p.albedo << ' ' << p.extinction*p.albedo << "]\n"
          << "    \"float g\" [.35]\nAttributeEnd\n"
          << "AttributeBegin\nMaterial \"interface\"\nMediumInterface \"cloud\" \"\"\n"
          << "Shape \"trianglemesh\" \"point3 P\" [\n";
        for (int i=0;i<8;++i) {
            int x=(i==1 || i==2 || i==5 || i==6) ? 1 : -1;
            int y=(i==2 || i==3 || i==6 || i==7) ? 1 : -1;
            int z=i>=4 ? 1 : -1;
            f << x*h[0] << ' ' << y*h[1] << ' ' << z*h[2] << '\n';
        }
        f << "] \"integer indices\" [0 2 1 0 3 2 4 5 6 4 6 7 0 4 7 0 7 3 1 2 6 1 6 5 0 1 5 0 5 4 3 7 6 3 6 2]\nAttributeEnd\n";
        if (!f) throw std::runtime_error("Cannot write PBRT scene");
        manifest << (scene ? ",\n" : "") << "    {\"name\": " << std::quoted(name)
                 << ", \"preset\": " << std::quoted(scene==3 ? "day" : names[scene])
                 << ", \"exposure\": " << p.exposure << ", \"density\": " << p.extinction
                 << ", \"albedo\": " << p.albedo << ", \"g\": 0.35, \"maxdepth\": 128"
                 << ", \"camera\": [" << camera.x << ',' << camera.y << ',' << camera.z
                 << "], \"sun\": [" << sun.x << ',' << sun.y << ',' << sun.z
                 << "], \"sun_rgb\": [" << sunlight.x << ',' << sunlight.y << ',' << sunlight.z << "]}";
        std::cout << "Exported " << name << " (albedo " << p.albedo << ", density " << p.extinction << ")\n";
    }
    manifest << "\n  ]\n}\n";
    if (!manifest) throw std::runtime_error("Cannot write manifest");
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
