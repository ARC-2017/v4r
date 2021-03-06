#include <v4r/common/graph_geometric_consistency.h>
#include <v4r/recognition/local_recognition_pipeline.h>
#include <v4r/features/types.h>

#include <pcl/common/time.h>
#include <pcl/registration/transformation_estimation_svd.h>

namespace v4r
{

template<typename PointT>
void
LocalRecognitionPipeline<PointT>::initialize(const std::string &trained_dir, bool force_retrain)
{
    CHECK ( !local_feature_matchers_.empty() ) << "No local recognizers provided!";

    model_keypoints_.clear();   // need to merge model keypoints from all local recognizers ( like SIFT + SHOT + ...)
    model_kp_idx_range_start_.resize( local_feature_matchers_.size() );

    for(size_t i=0; i<local_feature_matchers_.size(); i++)
    {
        LocalFeatureMatcher<PointT> &r = *local_feature_matchers_[i];
        r.setNormalEstimator(normal_estimator_);
        r.setModelDatabase(m_db_);
        r.setVisualizationParameter(vis_param_);
        r.initialize(trained_dir, force_retrain);

        const std::map<std::string, typename LocalObjectModel::ConstPtr> lomdb_tmp = r.getModelKeypoints();

        for ( auto lo : lomdb_tmp )
        {
            const std::string &model_id = lo.first;
            const LocalObjectModel &lom = *(lo.second);

            std::map<std::string, typename LocalObjectModel::ConstPtr>::const_iterator
                    it_loh = model_keypoints_.find(model_id);
            if ( it_loh != model_keypoints_.end () ) // append correspondences to existing ones
            {
                model_kp_idx_range_start_[i][model_id] = it_loh->second->keypoints_->points.size();
                *(it_loh->second->keypoints_) += *(lom.keypoints_);
                *(it_loh->second->kp_normals_) += *(lom.kp_normals_);
            }
            else
            {
                model_kp_idx_range_start_[i][model_id] = 0;
                LocalObjectModel::Ptr lom_copy (new LocalObjectModel);
                *(lom_copy->keypoints_) = *(lom.keypoints_);
                *(lom_copy->kp_normals_) = *(lom.kp_normals_);
                model_keypoints_ [model_id] = lom_copy;
            }
        }
    }
}

template<typename PointT>
void
LocalRecognitionPipeline<PointT>::correspondenceGrouping ()
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr scene_cloud_xyz (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::copyPointCloud( *scene_, *scene_cloud_xyz );

//#pragma omp parallel for schedule(dynamic)
    typename std::map<std::string, LocalObjectHypothesis<PointT> >::const_iterator it;
    for ( it = local_obj_hypotheses_.begin (); it != local_obj_hypotheses_.end (); ++it )
    {
        const std::string &model_id = it->first;
        const LocalObjectHypothesis<PointT> &loh = it->second;

        std::stringstream desc; desc << "Correspondence grouping for " << model_id << " ( " << loh.model_scene_corresp_->size() << ")" ;
        typename RecognitionPipeline<PointT>::StopWatch t(desc.str());

        pcl::PointCloud<pcl::PointXYZ>::Ptr model_keypoints = model_keypoints_[model_id]->keypoints_;
        pcl::PointCloud<pcl::Normal>::Ptr model_kp_normals = model_keypoints_[model_id]->kp_normals_;

        if( loh.model_scene_corresp_->size() < 3 )
            continue;

        std::sort( loh.model_scene_corresp_->begin(), loh.model_scene_corresp_->end(), LocalObjectHypothesis<PointT>::gcGraphCorrespSorter);
        std::vector < pcl::Correspondences > corresp_clusters;
        cg_algorithm_->setSceneCloud ( scene_cloud_xyz );
        cg_algorithm_->setInputCloud ( model_keypoints );

//        oh.visualize(*scene_, *scene_keypoints_);

        // Graph-based correspondence grouping requires normals but interface does not exist in base class - so need to try pointer casting
        typename GraphGeometricConsistencyGrouping<pcl::PointXYZ, pcl::PointXYZ>::Ptr gcg_algorithm =
                boost::dynamic_pointer_cast<  GraphGeometricConsistencyGrouping<pcl::PointXYZ, pcl::PointXYZ> > (cg_algorithm_);
        if( gcg_algorithm )
            gcg_algorithm->setInputAndSceneNormals(model_kp_normals, scene_normals_);

//        for ( const auto c : *(loh.model_scene_corresp_) )
//        {
//            CHECK( c.index_match < (int) scene_cloud_xyz->points.size() && c.index_match >= 0 );
//            CHECK( c.index_match < (int) scene_normals_->points.size() && c.index_match >= 0 );
//            CHECK( c.index_query < (int) model_keypoints->points.size() && c.index_query >= 0 );
//            CHECK( c.index_query < (int) model_kp_normals->points.size() && c.index_query >= 0 );
//        }

        //we need to pass the keypoints_pointcloud and the specific object hypothesis
        cg_algorithm_->setModelSceneCorrespondences ( loh.model_scene_corresp_ );
        cg_algorithm_->cluster (corresp_clusters);

        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > new_transforms (corresp_clusters.size());
        typename pcl::registration::TransformationEstimationSVD < pcl::PointXYZ, pcl::PointXYZ > t_est;

        for (size_t cluster_id = 0; cluster_id < corresp_clusters.size(); cluster_id++)
            t_est.estimateRigidTransformation (*model_keypoints, *scene_cloud_xyz, corresp_clusters[cluster_id], new_transforms[cluster_id]);

        if(param_.merge_close_hypotheses_) {
            std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > merged_transforms (corresp_clusters.size());
            std::vector<bool> cluster_has_been_taken(corresp_clusters.size(), false);
            const double angle_thresh_rad = param_.merge_close_hypotheses_angle_ * M_PI / 180.f ;

            size_t kept=0;
            for (size_t tf_id = 0; tf_id < new_transforms.size(); tf_id++) {

                if (cluster_has_been_taken[tf_id])
                    continue;

                cluster_has_been_taken[tf_id] = true;
                const Eigen::Vector3f centroid1 = new_transforms[tf_id].block<3, 1> (0, 3);
                const Eigen::Matrix3f rot1 = new_transforms[tf_id].block<3, 3> (0, 0);

                pcl::Correspondences merged_corrs = corresp_clusters[tf_id];

                for(size_t j=tf_id+1; j < new_transforms.size(); j++) {
                    const Eigen::Vector3f centroid2 = new_transforms[j].block<3, 1> (0, 3);
                    const Eigen::Matrix3f rot2 = new_transforms[j].block<3, 3> (0, 0);
                    const Eigen::Matrix3f rot_diff = rot2 * rot1.transpose();

                    double rotx = std::abs( atan2(rot_diff(2,1), rot_diff(2,2)));
                    double roty = std::abs( atan2(-rot_diff(2,0), sqrt(rot_diff(2,1) * rot_diff(2,1) + rot_diff(2,2) * rot_diff(2,2))) );
                    double rotz = std::abs( atan2(rot_diff(1,0), rot_diff(0,0)) );
                    double dist = (centroid1 - centroid2).norm();

                    if ( (dist < param_.merge_close_hypotheses_dist_) && (rotx < angle_thresh_rad) && (roty < angle_thresh_rad) && (rotz < angle_thresh_rad) ) {
                        merged_corrs.insert( merged_corrs.end(), corresp_clusters[j].begin(), corresp_clusters[j].end() );
                        cluster_has_been_taken[j] = true;
                    }
                }

                t_est.estimateRigidTransformation ( *model_keypoints, *scene_cloud_xyz, merged_corrs, merged_transforms[kept] );
                kept++;
            }
            merged_transforms.resize(kept);

            #pragma omp critical
            {
                for(size_t jj=0; jj<merged_transforms.size(); jj++)
                {
                    typename ObjectHypothesis::Ptr new_oh (new ObjectHypothesis);
                    new_oh->model_id_ = model_id;
                    new_oh->class_id_ = "";
                    new_oh->transform_ = merged_transforms[jj];
                    new_oh->confidence_ = corresp_clusters.size();
                    new_oh->corr_ = corresp_clusters[jj];

                    ObjectHypothesesGroup new_ohg;
                    new_ohg.global_hypotheses_ = false;
                    new_ohg.ohs_.push_back( new_oh );
                    obj_hypotheses_.push_back( new_ohg );
                }
                LOG(INFO) << "Merged " << corresp_clusters.size() << " clusters into " << kept << " clusters. Total correspondences: " << loh.model_scene_corresp_->size () << " " << loh.model_id_;
            }
        }
        else {
            #pragma omp critical
            {
                for(size_t jj=0; jj<new_transforms.size(); jj++)
                {
                    typename ObjectHypothesis::Ptr new_oh (new ObjectHypothesis);
                    new_oh->model_id_ = model_id;
                    new_oh->class_id_ = "";
                    new_oh->transform_ = new_transforms[jj];
                    new_oh->confidence_ = corresp_clusters.size();
                    new_oh->corr_ = corresp_clusters[jj];

                    ObjectHypothesesGroup new_ohg;
                    new_ohg.global_hypotheses_ = false;
                    new_ohg.ohs_.push_back( new_oh );
                    obj_hypotheses_.push_back( new_ohg );
                }
            }
        }
    }
}

template<typename PointT>
void
LocalRecognitionPipeline<PointT>::do_recognize()
{
    CHECK ( !generate_hypotheses_ || cg_algorithm_ ) << "Correspondence grouping algorithm not defined!";
    local_obj_hypotheses_.clear();

    // get feature correspondences from all recognizers
    for(size_t r_id=0; r_id < local_feature_matchers_.size(); r_id++)
    {
        typename LocalFeatureMatcher<PointT>::Ptr rec = local_feature_matchers_[r_id];

        rec->setInputCloud(scene_);
        rec->setSceneNormals(scene_normals_);
        rec->recognize();
        std::map<std::string, LocalObjectHypothesis<PointT> > local_hypotheses = rec->getCorrespondences( );

//        std::vector<int> kp_indices;
//        rec->getKeypointIndices(kp_indices);

        for (auto &oh : local_hypotheses)
        {
            const std::string &model_id = oh.first;
            LocalObjectHypothesis<PointT> &loh = oh.second;

            pcl::Correspondences new_corrs = *loh.model_scene_corresp_;

            const pcl::PointCloud<pcl::PointXYZ>::ConstPtr model_keypoints = model_keypoints_[model_id]->keypoints_;
            const pcl::PointCloud<pcl::Normal>::ConstPtr model_kp_normals = model_keypoints_[model_id]->kp_normals_;

            size_t initial_corrs = new_corrs.size();

            for (pcl::Correspondence &c : new_corrs) // add appropriate offset to correspondence index of the model keypoints
                c.index_query += model_kp_idx_range_start_[ r_id ][ model_id ];

            if( rec->getNumEstimators()>1 ) // check for redundancy (e.g. multi-scale feature matching)
            {
                size_t kept = 0;
                for(size_t new_corr_id=0; new_corr_id<new_corrs.size(); new_corr_id++) // add appropriate offset to correspondence index of the model keypoints
                {
                    pcl::Correspondence &new_c = new_corrs[new_corr_id];

//                    CHECK( new_c.index_match < (int) scene_->points.size() && new_c.index_match >= 0 );
//                    CHECK( new_c.index_match < (int) scene_normals_->points.size() && new_c.index_match >= 0 );
//                    CHECK( new_c.index_query < (int) model_keypoints->points.size() && new_c.index_query >= 0 );
//                    CHECK( new_c.index_query < (int) model_kp_normals->points.size() && new_c.index_query >= 0 );

                    const Eigen::Vector3f &new_scene_xyz = scene_->points[new_c.index_match].getVector3fMap();
                    const Eigen::Vector3f &new_scene_normal = scene_normals_->points[new_c.index_match].getNormalVector3fMap();
                    const Eigen::Vector3f &new_model_xyz = model_keypoints->points[new_c.index_query].getVector3fMap();
                    const Eigen::Vector3f &new_model_normal = model_kp_normals->points[new_c.index_query].getNormalVector3fMap();

                    bool is_redundant = false;
                    for(size_t old_corr_id=0; old_corr_id<kept; old_corr_id++)
                    {
                        pcl::Correspondence &exist_c = new_corrs[old_corr_id];


//                        CHECK( exist_c.index_match < (int) scene_->points.size() && exist_c.index_match >= 0 );
//                        CHECK( exist_c.index_match < (int) scene_normals_->points.size() && exist_c.index_match >= 0 );
//                        CHECK( exist_c.index_query < (int) model_keypoints->points.size() && exist_c.index_query >= 0 );
//                        CHECK( exist_c.index_query < (int) model_kp_normals->points.size() && exist_c.index_query >= 0 );

                        const Eigen::Vector3f &exist_scene_xyz = scene_->points[exist_c.index_match].getVector3fMap();
                        const Eigen::Vector3f &exist_scene_normal = scene_normals_->points[exist_c.index_match].getNormalVector3fMap();
                        const Eigen::Vector3f &exist_model_xyz = model_keypoints->points[exist_c.index_query].getVector3fMap();
                        const Eigen::Vector3f &exist_model_normal = model_kp_normals->points[exist_c.index_query].getNormalVector3fMap();

                        if ( (exist_scene_xyz-new_scene_xyz).norm() < param_.min_dist_ &&
                             (exist_model_xyz-new_model_xyz).norm() < param_.min_dist_ &&
                              exist_scene_normal.dot(new_scene_normal) > param_.max_dotp_ &&
                              exist_model_normal.dot(new_model_normal) > param_.max_dotp_ )
                        {
                            is_redundant = true;
                            break;
                        }
                    }
                    if(!is_redundant)
                    {
                        new_corrs[kept++] = new_c;
                    }
                }
                new_corrs.resize(kept);
            }

            auto it_mp_oh = local_obj_hypotheses_.find( model_id );
            if( it_mp_oh == local_obj_hypotheses_.end() )   // no feature correspondences exist yet
                local_obj_hypotheses_.insert( oh );
            else
            {
                pcl::Correspondences &old_corrs = *it_mp_oh->second.model_scene_corresp_;

                size_t kept=0; // check for redundancy
                for(size_t new_corr_id=0; new_corr_id<new_corrs.size(); new_corr_id++)
                {
                    const pcl::Correspondence &new_c = new_corrs[new_corr_id];
                    const Eigen::Vector3f &new_scene_xyz = scene_->points[new_c.index_match].getVector3fMap();
                    const Eigen::Vector3f &new_scene_normal = scene_normals_->points[new_c.index_match].getNormalVector3fMap();
                    const Eigen::Vector3f &new_model_xyz = model_keypoints->points[new_c.index_query].getVector3fMap();
                    const Eigen::Vector3f &new_model_normal = model_kp_normals->points[new_c.index_query].getNormalVector3fMap();

                    size_t is_redundant = false;
                    for(size_t old_corr_id=0; old_corr_id<old_corrs.size(); old_corr_id++)
                    {
                        pcl::Correspondence &old_c = old_corrs[old_corr_id];
                        const Eigen::Vector3f &old_scene_xyz = scene_->points[old_c.index_match].getVector3fMap();
                        const Eigen::Vector3f &old_scene_normal = scene_normals_->points[old_c.index_match].getNormalVector3fMap();
                        const Eigen::Vector3f &old_model_xyz = model_keypoints->points[old_c.index_query].getVector3fMap();
                        const Eigen::Vector3f &old_model_normal = model_kp_normals->points[old_c.index_query].getNormalVector3fMap();

                        if ( (old_scene_xyz-new_scene_xyz).norm() < param_.min_dist_ &&
                             (old_model_xyz-new_model_xyz).norm() < param_.min_dist_ &&
                              old_scene_normal.dot(new_scene_normal) > param_.max_dotp_ &&
                              old_model_normal.dot(new_model_normal) > param_.max_dotp_ )
                        {
                            is_redundant = true;

                            // take the correspondence with the smaller distance
                            if( new_c.distance < old_c.distance )
                                old_c = new_c;

                            break;
                        }
                    }
                    if(!is_redundant)
                        new_corrs[kept++] = new_c;
                }
                LOG(INFO) << "Kept " << kept << " out of " << initial_corrs << " correspondences.";
                new_corrs.resize(kept);

                old_corrs.insert(  old_corrs.end(),new_corrs.begin(), new_corrs.end() );
            }
        }
    }

    if(generate_hypotheses_)
        correspondenceGrouping();
}

template class V4R_EXPORTS LocalRecognitionPipeline<pcl::PointXYZRGB>;
}
