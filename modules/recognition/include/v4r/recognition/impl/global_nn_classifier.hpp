/*
 * global_nn_classifier.cpp
 *
 *  Created on: Mar 9, 2012
 *      Author: aitor
 */

#include <v4r/recognition/global_nn_classifier.h>
#include <v4r/io/eigen.h>

namespace v4r
{

template<template<class > class Distance, typename PointInT, typename FeatureT>
  void
  GlobalNNPipeline<Distance, PointInT, FeatureT>::loadFeaturesAndCreateFLANN ()
  {
    std::vector<ModelTPtr> models = source_->getModels();

    for (size_t i = 0; i < models.size (); i++)
    {
      ModelTPtr m = models[i];
      const std::string path = training_dir_ + "/" + m->class_ + "/" + m->id_ + "/" + descr_name_;

//      std::vector<std::string> descriptor_files;
      io::getFilesInDirectory(path, descriptor_files, "", ".*.txt", false);

      bf::path inside = path;
      bf::directory_iterator end_itr;

      for (bf::directory_iterator itr_in (inside); itr_in != end_itr; ++itr_in)
      {
#if BOOST_FILESYSTEM_VERSION == 3
        std::string file_name = (itr_in->path ().filename ()).string();
#else
        std::string file_name = (itr_in->path ()).filename ();
#endif

        std::vector < std::string > strs;
        boost::split (strs, file_name, boost::is_any_of ("_"));

        if (strs[0] == "descriptor")
        {
          std::string full_file_name = itr_in->path ().string ();
          std::vector < std::string > strs2;
          boost::split (strs2, full_file_name, boost::is_any_of ("/"));

          typename pcl::PointCloud<FeatureT>::Ptr signature (new pcl::PointCloud<FeatureT>);
          pcl::io::loadPCDFile (full_file_name, *signature);

          flann_model descr_model;
          descr_model.first = m;
//          int size_feat = sizeof(signature->points[0].histogram) / sizeof(float);
//          descr_model.second.resize (size_feat);
          memcpy (&descr_model.second[0], &signature->points[0].histogram[0], size_feat * sizeof(double));

          flann_models_.push_back (descr_model);
        }
      }
    }

    convertToFLANN (flann_models_, flann_data_);
    flann_index_ = new flann::Index<DistT> (flann_data_, flann::LinearIndexParams ());
    flann_index_->buildIndex ();
  }

template<template<class > class Distance, typename PointInT, typename FeatureT>
  void
  GlobalNNPipeline<Distance, PointInT, FeatureT>::nearestKSearch (flann::Index<DistT> * index, const flann_model &model,
                                                                                         int k, flann::Matrix<int> &indices,
                                                                                         flann::Matrix<double> &distances)
  {
    flann::Matrix<double> p = flann::Matrix<double> (new double[model.second.size ()], 1, model.second.size ());
    memcpy (&p.ptr ()[0], &model.second[0], p.cols * p.rows * sizeof(double));

    indices = flann::Matrix<int> (new int[k], 1, k);
    distances = flann::Matrix<float> (new double[k], 1, k);
    index->knnSearch (p, indices, distances, k, flann::SearchParams (512));
    delete[] p.ptr ();
  }

template<template<class > class Distance, typename PointInT, typename FeatureT>
  void
  GlobalNNPipeline<Distance, PointInT, FeatureT>::classify ()
  {
    categories_.clear ();
    confidences_.clear ();
    first_nn_category_ = std::string ("");

    PointInTPtr in (new pcl::PointCloud<PointInT>);
    std::vector<double> signature;

    if (indices_.size ())
      pcl::copyPointCloud (*input_, indices_, *in);
    else
      in = input_;

    estimator_->estimate (in, signature);
    std::vector<index_score> indices_scores;

    ModelTPtr empty;

    flann_model histogram (empty, signature);
    flann::Matrix<int> indices;
    flann::Matrix<double> distances;
    nearestKSearch (flann_index_, histogram, NN_, indices, distances);

    //gather NN-search results
    double score = 0;
    for (size_t i = 0; i < NN_; ++i)
    {
      score = distances[0][i];
      index_score is;
      is.idx_models_ = indices[0][i];
      is.idx_input_ = static_cast<int> (idx);
      is.score_ = score;
      indices_scores.push_back (is);
      std::cout << i << ": " << indices[0][i] << " with score " << score << " and model id: " << flann_models_[indices_scores[i].idx_models_].first->class_ << "/" << flann_models_[indices_scores[i].idx_models_].first->id_ <<std::endl;
    }

      std::sort (indices_scores.begin (), indices_scores.end (), sortIndexScoresOp);
      flann_model &fm = flann_models_[indices_scores[0].idx_models_];
      first_nn_category_ = fm.first->class_;

      std::cout << "first id: " << fm.first->id_ << std::endl;

      std::map<std::string, double> category_map;
      size_t num_n = std::min (NN_, indices_scores.size ());

      std::map<std::string, double>::iterator it;
      double normalization_term = 0;

      for (size_t i = 0; i < num_n; ++i)
      {
        std::string cat = fm.first->class_;
        it = category_map.find (cat);
        if (it == category_map.end ())
        {
            category_map[cat] = 1;
            //category_map[cat] = indices_scores[i].score_;   // is the confidence better if score is higher or lower?
        }
        else
        {
            it->second++;
            //it->second += indices_scores[i].score_;
        }
        normalization_term += indices_scores[i].score_;
      }

      //------ sort classification result by the confidence value---------
      std::vector<index_score> final_indices_scores;
      for (it = category_map.begin (); it != category_map.end (); it++)
      {
        float prob = static_cast<float> (it->second) / static_cast<float> (num_n);
        //float prob = static_cast<float> (it->second) / static_cast<float> (normalization_term);
        //categories_.push_back (it->first);
        //confidences_.push_back (prob);
        index_score is;
        is.model_name_ = it->first;
        //is.idx_input_ = static_cast<int> (idx);
        is.score_ = prob;
        final_indices_scores.push_back (is);
      }

      std::sort (final_indices_scores.begin (), final_indices_scores.end (), sortIndexScoresOpDesc);

      for (size_t i=0; i < final_indices_scores.size(); i++)
      {
          categories_.push_back (final_indices_scores[i].model_name_);
          confidences_.push_back (final_indices_scores[i].score_);
      }
  }

template<template<class > class Distance, typename PointInT, typename FeatureT>
  void
  GlobalNNPipeline<Distance, PointInT, FeatureT>::initialize (bool force_retrain)
  {

    //use the source to know what has to be trained and what not, checking if the descr_name directory exists
    //unless force_retrain is true, then train everything
    std::vector<ModelTPtr> models = source_->getModels();
    std::cout << "Models size:" << models.size () << std::endl;

    if (force_retrain)
    {
      for (size_t i = 0; i < models.size (); i++)
        source_->removeDescDirectory (*models[i], training_dir_, descr_name_);
    }

    for (size_t i = 0; i < models.size (); i++)
    {
      const ModelTPtr &m = models[i];

      if (!source_->isModelAlreadyTrained (*m, training_dir_, descr_name_))
      {
        for (size_t v = 0; v < m->views_.size (); v++)
        {
          PointInTPtr processed (new pcl::PointCloud<PointInT>);
          //pro view, compute signatures
          std::vector<double> signature;
          estimator_->estimate (m->views_[v], signature);

          //source_->makeModelPersistent (models->at (i), training_dir_, descr_name_, static_cast<int> (v));
          std::string path = source_->getModelDescriptorDir (*m, training_dir_, descr_name_);

          io::createDirIfNotExist(path);

          std::stringstream path_view;
          path_view << path << "/view_" << v << ".pcd";
          pcl::io::savePCDFileBinary (path_view.str (), *processed);

          std::stringstream path_pose;
          path_pose << path << "/pose_" << v << ".txt";
          io::writeMatrixToFile( path_pose.str (), m->poses_[v]);

          std::stringstream path_entropy;
          path_entropy << path << "/entropy_" << v << ".txt";
          io::writeFloatToFile (path_entropy.str (), m->self_occlusions_[v]);

          Eigen::Vector3f centroid;
          pcl::compute3DCentroid(m->views_[v], centroid);

            std::stringstream path_centroid;
            path_centroid << path << "/centroid_" << v << "_" << 0 << ".txt";
            io::writeCentroidToFile (path_centroid.str (), centroid);

            std::stringstream path_descriptor;
            path_descriptor << path << "/descriptor_" << v << "_" << 0 << ".pcd";
            std::ofstream f;
            f.open(path_descriptor.str().c_str());
            for(size_t j=0; j<signature.size(); j++)
                f << signature[j] << " ";
            f.close();
        }

      }
      else
      {
        //else skip model
        std::cout << "The model has already been trained..." << std::endl;
      }
    }

    //load features from disk
    //initialize FLANN structure
    loadFeaturesAndCreateFLANN ();
  }

}
