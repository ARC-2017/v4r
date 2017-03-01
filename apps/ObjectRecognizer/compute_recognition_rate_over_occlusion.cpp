#include <v4r/io/filesystem.h>

#include <Eigen/Eigen>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>    // std::next_permutation, std::sort

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/common/centroid.h>

namespace po = boost::program_options;
namespace bf = boost::filesystem;

typedef pcl::PointXYZRGB PointT;


// -g /media/Data/datasets/TUW/annotations/ -r /home/thomas/recognition_results_eval/ -m /media/Data/datasets/TUW/models/

float rotation_error_threshold_deg;
float translation_error_threshold_m;
float occlusion_threshold;

struct Hypothesis
{
    Eigen::Matrix4f pose;
    float occlusion;
};

struct Model
{
    pcl::PointCloud<PointT>::Ptr cloud;
    Eigen::Vector4f centroid;
};

std::map<std::string, std::vector<Hypothesis> > readHypothesesFromFile( const std::string & );

std::map<std::string, std::vector<Hypothesis> >
readHypothesesFromFile( const std::string &filename )
{
    std::map<std::string, std::vector<Hypothesis> > hypotheses;

    std::ifstream anno_f ( filename.c_str() );
    std::string line;
    while (std::getline(anno_f, line))
    {
        std::istringstream iss(line);
        std::string model_name, occlusion_tmp;

        Hypothesis h;
        iss >> model_name >> occlusion_tmp;
        occlusion_tmp = occlusion_tmp.substr( 1, occlusion_tmp.length() - 3 );
        h.occlusion = 1.f-std::stof( occlusion_tmp );

        for(size_t i=0; i<16; i++)
            iss >> h.pose(i / 4, i % 4);

        auto pose_it = hypotheses.find( model_name );
        if( pose_it != hypotheses.end() )
            pose_it->second.push_back( h ) ;
        else
            hypotheses[model_name] = std::vector<Hypothesis>(1, h);
    }

    return hypotheses;
}

int
main (int argc, char ** argv)
{
    std::string out_dir = "/tmp/recognition_rates_over_occlusion/";
    std::string gt_dir, or_dir, models_dir;
    bool use_generated_hypotheses = false;

    rotation_error_threshold_deg = 30.f;
    translation_error_threshold_m = 0.05f;

    std::stringstream description;
    description << "Tool to compute object instance recognition rate." << std::endl <<
                   "==================================================" << std::endl <<
                   "This will generate a text file containing:" << std::endl <<
                   "Column 1: occlusion" << std::endl <<
                   "Column 2: is recognized" << std::endl <<
                   "==================================================" << std::endl <<
                   "** Allowed options";

    po::options_description desc(description.str());
    desc.add_options()
        ("help,h", "produce help message")
        ("groundtruth_dir,g", po::value<std::string>(&gt_dir)->required(), "Root directory containing annotation files (i.e. 4x4 ground-truth pose of each object with filename viewId_ModelId_ModelInstanceCounter.txt")
        ("rec_results_dir,r", po::value<std::string>(&or_dir)->required(), "Root directory containing the recognition results (same format as annotation files).")
        ("out_dir,o", po::value<std::string>(&out_dir)->default_value(out_dir), "Output directory where recognition results will be stored")
        ("trans_thresh", po::value<float>(&translation_error_threshold_m)->default_value(translation_error_threshold_m), "Maximal allowed translational error in metres")
        ("models_dir,m", po::value<std::string>(&models_dir), "Only for visualization. Root directory containing the model files (i.e. filenames 3D_model.pcd).")
        ("rot_thresh", po::value<float>(&rotation_error_threshold_deg)->default_value(rotation_error_threshold_deg), "Maximal allowed rotational error in degrees (NOT IMPLEMENTED)")
        ("use_generated_hypotheses", po::bool_switch(&use_generated_hypotheses), "if true, computes recognition rate for all generated hypotheses instead of verified ones.")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) { std::cout << desc << std::endl; return false; }
    try  {  po::notify(vm); }
    catch( std::exception& e)  { std::cerr << "Error: " << e.what() << std::endl << std::endl << desc << std::endl; return false; }

    bf::path out_path = out_dir;
    out_path /= "results_occlusion.txt";

    if( v4r::io::existsFile( out_path.string() ) )
    {
        std::cerr << out_path.string() << " exists already. Skipping it!" << std::endl;
        return -1;
    }

    v4r::io::createDirForFileIfNotExist( out_path.string() );
    std::ofstream f( out_path.string() );
    std::cout << "Writing results to " << out_path.string() << "..." << std::endl;


    // load models
    std::vector<std::string> model_filenames = v4r::io::getFilesInDirectory( models_dir, "3D_model.pcd", true );
    std::map<std::string, Model> models;
    for(const std::string &model_fn : model_filenames)
    {
        pcl::PointCloud<PointT>::Ptr model_cloud (new pcl::PointCloud<PointT>);
        bf::path model_full_path = models_dir;
        model_full_path /= model_fn;
        pcl::io::loadPCDFile( model_full_path.string(), *model_cloud );

        Model m;
        m.cloud = model_cloud;
        pcl::compute3DCentroid(*m.cloud, m.centroid);

        // model identity is equal folder name -> remove \"/3D_model.pcd\" from filename
        bf::path model_path = model_fn;
        models[ model_path.parent_path().string() ] = m;
    }

    std::vector<std::string> annotation_files = v4r::io::getFilesInDirectory( gt_dir, ".*.anno", true );

    for( const std::string anno_file : annotation_files )
    {
        bf::path gt_path = gt_dir;
        gt_path /= anno_file;

        std::string rec_file = anno_file;
        if( use_generated_hypotheses )
            boost::replace_last( rec_file, ".anno", ".generated_hyps");

        bf::path rec_path = or_dir;
        rec_path /= rec_file;

        std::map<std::string, std::vector<Hypothesis> > gt_hyps = readHypothesesFromFile( gt_path.string() );
        std::map<std::string, std::vector<Hypothesis> > rec_hyps = readHypothesesFromFile( rec_path.string() );

        for(auto const &ent1 : gt_hyps)
        {
            const std::string &model_name_gt = ent1.first;
            const Eigen::Vector4f &centroid_model = models[ model_name_gt ].centroid;
            const std::vector<Hypothesis> &gt_model_hyps = ent1.second;

            for(const Hypothesis &h_gt : gt_model_hyps)
            {
                bool is_recognized = false;

                const Eigen::Matrix4f &gt_pose = h_gt.pose;
                const Eigen::Vector4f centroid_gt = gt_pose * centroid_model;

                float occlusion = h_gt.occlusion;

                const auto it = rec_hyps.find( model_name_gt );
                if (it != rec_hyps.end())
                {
                    const std::vector<Hypothesis> &rec_model_hyps = it->second;
                    for(const Hypothesis &h_rec: rec_model_hyps)
                    {
                        const Eigen::Matrix4f &rec_pose = h_rec.pose;
                        const Eigen::Vector4f centroid_rec = rec_pose * centroid_model;
                        float trans_error = (centroid_rec.head(3)-centroid_gt.head(3)).norm();

                        if( trans_error < translation_error_threshold_m)
                            is_recognized = true;
                    }
                }

                f << occlusion << " " << is_recognized << std::endl;
            }
        }
    }
    f.close();
    std::cout << "Done!" << std::endl;
}
