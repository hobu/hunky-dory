#include <entwine/tree/tiler.hpp>
#include <pdal/ChipperFilter.hpp>

#include "chip.hpp"
#include "cpd.hpp"
#include "icp.hpp"

namespace hunky_dory {

int chip(const hunky_dory::DocoptMap& args) {
    std::string source_path = args.at("<source>").asString();
    std::string target_path = args.at("<target>").asString();
    std::ofstream outfile(args.at("<outfile>").asString());
    outfile.precision(2);
    outfile << std::fixed;
    double sigma2 = std::stod(args.at("--sigma2").asString());
    long capacity = args.at("--capacity").asLong();

    if (args.at("--no-entwine").asBool()) {
        std::cout << "Using PDAL's chipper\n";

        pdal::StageFactory factory(false);

        pdal::Stage* source_reader =
            hunky_dory::infer_and_create_reader(factory, source_path);
        pdal::Stage* target_reader =
            hunky_dory::infer_and_create_reader(factory, target_path);

        pdal::Options chipper_options;
        chipper_options.add("capacity", capacity);
        pdal::ChipperFilter chipper;
        chipper.setInput(*source_reader);
        chipper.setOptions(chipper_options);

        pdal::PointTable source_table;
        chipper.prepare(source_table);

        std::cout << "Chipping..." << std::flush;
        pdal::PointViewSet viewset = chipper.execute(source_table);
        std::cout << "done, with " << viewset.size() << " chips\n";

        for (auto source_view : viewset) {
            hunky_dory::Matrix source =
                hunky_dory::point_view_to_matrix(source_view);
            pdal::BOX2D bounds;
            source_view->calculateBounds(bounds);

            std::cout << "Cropping target data..." << std::flush;
            hunky_dory::CroppedFile target_file(target_path, bounds);
            std::cout << "done\n";
            hunky_dory::Matrix target = target_file.matrix;

            Result result;
            if (args.at("cpd").asBool()) {
                result = cpd(source, target, args);
            } else if (args.at("icp").asBool()) {
                result = icp(source, target, args);
            } else {
                std::cerr << "Unsupported method." << std::endl;
                return 1;
            }
            std::cout << "Runtime: " << result.runtime << "s\nAverage motion:\n"
                      << result.dx << ", " << result.dy << ", " << result.dz
                      << "\n";
            outfile << bounds.minx << " " << bounds.maxx << " " << bounds.miny
                    << " " << bounds.maxy << " " << result.dx << " "
                    << result.dy << " " << result.dz << "\n";
            outfile << std::flush;
        }
    } else {
        std::cout << "Using entwine indices\n";
        entwine::arbiter::Arbiter a;
        const entwine::arbiter::Endpoint source(a.getEndpoint(source_path));
        entwine::Tiler t(source, 6, 1000, &ENTWINE_XYZ_SCHEMA);
        auto handler([&](pdal::PointView& view, entwine::Bounds bbox) {
            std::cout << "Number of points: " << view.size() << "\n";
            return true;
        });

        t.go(handler);
    }
    outfile.close();
    return 0;
}
}
