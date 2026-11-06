#pragma once

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Include after the renderer's Vector3f, Triangle and Scene definitions.
// Position-only OBJ loading; flat diffuse triangles, as in the original renderer.
// Preserve the OBJ coordinates so the scene controls camera and placement.
inline void add_mesh(Scene& scene, const char* filename, Vector3f color) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error(std::string("Cannot read ") + filename);
    std::vector<Vector3f> vertices;
    std::vector<int> faces;
    std::string line, kind, token;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        std::istringstream row(line);
        row >> kind;
        if (kind == "v") {
            Vector3f p;
            if (!(row >> p.x() >> p.y() >> p.z()) || !std::isfinite(p.x())
                || !std::isfinite(p.y()) || !std::isfinite(p.z()))
                throw std::runtime_error("Invalid OBJ vertex at line " + std::to_string(line_number));
            vertices.push_back(p);
        } else if (kind == "f") {
            std::vector<int> polygon;
            while (row >> token && token[0] != '#') {
                std::string position = token.substr(0, token.find('/'));
                size_t used;
                int id = std::stoi(position, &used);
                if (used != position.size() || id == 0)
                    throw std::runtime_error("Invalid OBJ index at line " + std::to_string(line_number));
                id = id > 0 ? id - 1 : int(vertices.size()) + id;
                if (id < 0 || id >= int(vertices.size()))
                    throw std::runtime_error("OBJ index out of range at line " + std::to_string(line_number));
                polygon.push_back(id);
            }
            if (polygon.size() < 3) throw std::runtime_error("OBJ face needs three vertices");
            for (size_t i = 1; i + 1 < polygon.size(); ++i)
                faces.insert(faces.end(), {polygon[0], polygon[i], polygon[i+1]});
        }
        kind.clear();
    }
    if (faces.empty()) throw std::runtime_error("OBJ has no faces");
    for (size_t i = 0; i < faces.size(); i += 3) {
        Vector3f p = vertices[faces[i]], a = vertices[faces[i+1]], b = vertices[faces[i+2]];
        Vector3f n = (a - p).cross(b - p);
        if (n.squaredNorm() > 0) scene.triangles.emplace_back(p, a, b, color);
    }
}
