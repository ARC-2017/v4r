//
//  viewer.cpp
//  LyonViewer
//
//  Created by Simon Schreiberhuber on 01.04.15.
//  Copyright (c) 2015 Simon Schreiberhuber. All rights reserved.
//

#include <iostream>
#include <string>
#include <sstream>
#include <limits>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <boost/filesystem.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <v4r/io/filesystem.h>
#include <v4r/rendering/depthmapRenderer.h>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <glog/logging.h>

namespace po = boost::program_options;

int main(int argc, const char * argv[]) {
    std::string input, out_dir;
    bool visualize;
    size_t subdivisions, width, height;
    float radius, fx, fy, cx, cy;

    google::InitGoogleLogging(argv[0]);

    po::options_description desc("Depth-Map Rendering\n======================================\n**Allowed options");
    desc.add_options()
            ("help,h", "produce help message")
            ("input,i", po::value<std::string>(&input)->required(), "input model (.ply)")
            ("output,o", po::value<std::string>(&out_dir)->default_value("/tmp/rendered_pointclouds/"), "output directory to store the point cloud (.pcd) file")
            ("visualize,v", po::value<bool>(&visualize)->default_value(true), "if true, visualizes the rendered depth and color map.")
            ("subdivisions,s", po::value<size_t>(&subdivisions)->default_value(0), "defines the number of subdivsions used for rendering")
            ("radius,r", po::value<float>(&radius)->default_value(3.0, boost::str(boost::format("%.2e") % radius)), "defines the radius used for rendering")
            ("width,w", po::value<size_t>(&width)->default_value(640), "defines the image width")
            ("height,h", po::value<size_t>(&height)->default_value(480), "defines the image height")
            ("fx", po::value<float>(&fx)->default_value(535.4, boost::str(boost::format("%.2e") % fx)), "defines the focal length in x direction used for rendering")
            ("fy", po::value<float>(&fy)->default_value(539.2, boost::str(boost::format("%.2e") % fy)), "defines the focal length in y direction used for rendering")
            ("cx", po::value<float>(&cx)->default_value(320.1, boost::str(boost::format("%.2e") % cx)), "defines the central point of projection in x direction used for rendering")
            ("cy", po::value<float>(&cy)->default_value(247.6, boost::str(boost::format("%.2e") % cy)), "defines the central point of projection in y direction used for rendering")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return false;
    }

    try
    {
        po::notify(vm);
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl << std::endl << desc << std::endl;
        return false;
    }

    v4r::DepthmapRenderer renderer(width, height);
    renderer.setIntrinsics(fx,fy,cx,cy);

    v4r::DepthmapRendererModel model(input);

    //test if the model has colored elements(note! no textures supported yet.... only colored polygons)
    if(model.hasColor()){
        LOG(INFO) << "Model file has color.";
    }

    renderer.setModel(&model);

    std::vector<Eigen::Vector3f> sphere = renderer.createSphere(3, subdivisions);

    LOG(INFO) <<"Rendering file " << input;

    if (!sphere.empty())
        v4r::io::createDirIfNotExist(out_dir);

    for(size_t i=0; i<sphere.size(); i++){
        //get point from list
        Eigen::Vector3f point = sphere[i];
        //get a camera pose looking at the center:
        Eigen::Matrix4f orientation = renderer.getPoseLookingToCenterFrom(point);

        renderer.setCamPose(orientation);
        float visible;
        cv::Mat color;
        cv::Mat depthmap = renderer.renderDepthmap(visible, color);

        if(visualize) {
            LOG(INFO) << visible << "% visible.";
            cv::imshow("depthmap", depthmap*0.25);
            cv::imshow("color", color);
            cv::waitKey();
        }

        //create and save the according pcd files
        std::stringstream ss; ss << out_dir << "/cloud_" << i << ".pcd";
        std::string file = ss.str();
        pcl::io::savePCDFileBinary(file, renderer.renderPointcloudColor(visible));
        LOG(INFO) << "Saved data points to " << file << ".";
    }

    return 0;
}
