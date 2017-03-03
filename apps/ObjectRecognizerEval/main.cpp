
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <glog/logging.h>

#include <v4r/apps/ObjectRecognizer.h>
#include "compute_recognition_rate.h"
#include "boost_xml_editor.h"

#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <v4r/io/filesystem.h>

namespace po = boost::program_options;

namespace bf=boost::filesystem;

int
main (int argc, char ** argv)
{
    typedef pcl::PointXYZRGB PT;

    std::string test_dir;
    std::string out_dir = "/tmp/object_recognition_results/";
    std::string multipipeline_xml_config_fn = "cfg/multipipeline_config.xml";
    std::string debug_dir = "";
    std::string gt_dir;

    po::options_description desc("Single-View Object Instance Recognizer\n======================================\n**Allowed options");
    desc.add_options()
            ("help,h", "produce help message")
            ("test_dir,t", po::value<std::string>(&test_dir)->required(), "Directory with test scenes stored as point clouds (.pcd). The camera pose is taken directly from the pcd header fields \"sensor_orientation_\" and \"sensor_origin_\" (if the test directory contains subdirectories, each subdirectory is considered as seperate sequence for multiview recognition)")
            ("multipipeline_xml_config_fn", po::value<std::string>(&multipipeline_xml_config_fn)->default_value(multipipeline_xml_config_fn), "XML config file setting up the multi-pipeline.")
            ("out_dir,o", po::value<std::string>(&out_dir)->default_value(out_dir), "Output directory where recognition results will be stored.")
            ("dbg_dir", po::value<std::string>(&debug_dir)->default_value(debug_dir), "Output directory where debug information (generated object hypotheses) will be stored (skipped if empty)")
            ("groundtruth_dir,g", po::value<std::string>(&gt_dir)->required(), "Root directory containing annotation files (i.e. 4x4 ground-truth pose of each object with filename viewId_ModelId_ModelInstanceCounter.txt")
            ;
    po::variables_map vm;
    po::parsed_options parsed = po::command_line_parser(argc, argv).options(desc).allow_unregistered().run();
    std::vector<std::string> to_pass_further = po::collect_unrecognized(parsed.options, po::include_positional);
    po::store(parsed, vm);
    if (vm.count("help")) { std::cout << desc << std::endl; to_pass_further.push_back("-h"); }
    try { po::notify(vm); }
    catch(std::exception& e) { std::cerr << "Error: " << e.what() << std::endl << std::endl << desc << std::endl;  }


    std::vector<XMLChange> changes;
    changes.push_back( XMLChange("cfg/sift_config.xml", "kdtree_splits_", "512" ) );
    changes.push_back( XMLChange("cfg/sift_config.xml", "kdtree_splits_", "264" ) );
    changes.push_back( XMLChange("cfg/sift_config.xml", "kdtree_splits_", "128" ) );
    changes.push_back( XMLChange("cfg/sift_config.xml", "kdtree_splits_", "64" ) );

    for(size_t eval_id=0; eval_id < changes.size(); eval_id++)
    {
        v4r::io::removeDir("./cfg");
        v4r::io::copyDir("/home/thomas/default_cfg", "./cfg");

        std::cerr << "**********+Evaluating " << eval_id << " of " << changes.size() << " parameter sets." << std::endl;
        const XMLChange &chg = changes[eval_id];
        editXML( chg );

        // create a directory for evaluation
        size_t counter = 0;
        std::stringstream out_tmp;
        do
        {
            out_tmp.str("");
            out_tmp << out_dir << "/" << counter++;
        }while( v4r::io::existsFolder(out_tmp.str()) );
        const std::string out_dir_eval = out_tmp.str();
        std::cout << "Saving results to " << out_dir_eval << std::endl;
        v4r::io::createDirIfNotExist( out_dir_eval );
        v4r::io::copyDir("./cfg", out_dir_eval+"/cfg");

        v4r::apps::ObjectRecognizerParameter or_param (multipipeline_xml_config_fn);
        v4r::apps::ObjectRecognizer<PT> recognizer(or_param);
        recognizer.initialize(to_pass_further);

        std::vector< std::string> sub_folder_names = v4r::io::getFoldersInDirectory( test_dir );
        if(sub_folder_names.empty()) sub_folder_names.push_back("");

        std::vector<double> elapsed_time;

        for (const std::string &sub_folder_name : sub_folder_names)
        {
            std::vector< std::string > views = v4r::io::getFilesInDirectory( test_dir+"/"+sub_folder_name, ".*.pcd", false );
            for (size_t v_id=0; v_id<views.size(); v_id++)
            {
                bf::path test_path = test_dir;
                test_path /= sub_folder_name;
                test_path /= views[v_id];


                LOG(INFO) << "Recognizing file " << test_path.string();
                pcl::PointCloud<PT>::Ptr cloud(new pcl::PointCloud<PT>());
                pcl::io::loadPCDFile( test_path.string(), *cloud);

                pcl::StopWatch t;

                std::vector<typename v4r::ObjectHypothesis<PT>::Ptr > verified_hypotheses = recognizer.recognize(cloud);
                std::vector<v4r::ObjectHypothesesGroup<PT> > generated_object_hypotheses = recognizer.getGeneratedObjectHypothesis();

                elapsed_time.push_back( t.getTime() );

                if ( !out_dir_eval.empty() )  // write results to disk (for each verified hypothesis add a row in the text file with object name, dummy confidence value and object pose in row-major order)
                {
                    std::string out_basename = views[v_id];
                    boost::replace_last(out_basename, ".pcd", ".anno");
                    bf::path out_path = out_dir_eval;
                    out_path /= sub_folder_name;
                    out_path /= out_basename;

                    v4r::io::createDirForFileIfNotExist(out_path.string());

                    // save verified hypotheses
                    std::ofstream f ( out_path.string().c_str() );
                    for ( const v4r::ObjectHypothesis<PT>::Ptr &voh : verified_hypotheses )
                    {
                        f << voh->model_id_ << " (-1.): ";
                        for (size_t row=0; row <4; row++)
                            for(size_t col=0; col<4; col++)
                                f << voh->transform_(row, col) << " ";
                        f << std::endl;
                    }
                    f.close();

                    // save generated hypotheses
                    std::string out_path_generated_hypotheses = out_path.string();
                    boost::replace_last(out_path_generated_hypotheses, ".anno", ".generated_hyps");
                    f.open ( out_path_generated_hypotheses.c_str() );
                    for ( const v4r::ObjectHypothesesGroup<PT> &gohg : generated_object_hypotheses )
                    {
                        for ( const v4r::ObjectHypothesis<PT>::Ptr &goh : gohg.ohs_ )
                        {
                            f << goh->model_id_ << " (-1.): ";
                            for (size_t row=0; row <4; row++)
                                for(size_t col=0; col<4; col++)
                                    f << goh->transform_(row, col) << " ";
                            f << std::endl;

                        }
                    }
                    f.close();
                }
            }
        }

        RecognitionEvaluator e;
        e.setModels_dir(recognizer.getModelsDir());
        e.setTest_dir(test_dir);
        e.setOr_dir(out_dir_eval);
        e.setGt_dir(gt_dir);
        e.setUse_generated_hypotheses(true);
        e.setVisualize(true);
        float recognition_rate = e.compute_recognition_rate_over_occlusion();
        size_t tp, fp, fn;
        e.compute_recognition_rate(tp, fp, fn);

        float median_time;
        std::sort(elapsed_time.begin(), elapsed_time.end());
        median_time =  elapsed_time[ (int)(elapsed_time.size()/2) ];

        float precision = (float)tp / (tp + fp);
        float recall = (float)tp / (tp + fn);
        float fscore = 2 * precision * recall / (precision + recall);
        std::cout << "RECOGNITION RATE: " << recognition_rate << ", median time: " << median_time
                  << ", tp: " << tp << ", fp: " << fp << ", fn: " << fn
                  << ", precision: " << precision << ", recall: " << recall << ", fscore: " << fscore << std::endl;

        std::stringstream counter_str; counter_str << counter;
        bf::path out_param_path = out_dir;
        out_param_path /= counter_str.str() + "_param.txt";
        std::ofstream of ( out_param_path.string() );
        of << chg.tmp_xml_filename_ << " " << chg.node_name_ << " " << chg.value_;
        of.close();


        bf::path out_results_path = out_dir;
        out_results_path /= counter_str.str() + "_results.txt";
        of.open ( out_results_path.string() );
        of << recognition_rate << " " << median_time << " " << tp << " " << fp << " " << fn << " " << precision << " " << recall << " " << fscore;
        of.close();
    }
}

