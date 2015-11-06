/******************************************************************************
 * Copyright (c) 2013 Aitor Aldoma
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ******************************************************************************/

/**
*
*      @author Aitor Aldoma
*      @author Thomas Faeulhammer (faeulhammer@acin.tuwien.ac.at)
*      @date Feb, 2013
*      @brief object instance recognizer
*/


#ifndef RECOGNIZER_H_
#define RECOGNIZER_H_

#include <v4r/common/faat_3d_rec_framework_defines.h>
#include <v4r/core/macros.h>
#include <v4r/recognition/hypotheses_verification.h>
#include <v4r/recognition/voxel_based_correspondence_estimation.h>
#include <v4r/recognition/source.h>

#include <pcl/common/common.h>
#include <pcl/common/time.h>
#include <pcl/filters/crop_box.h>
#include <pcl/registration/correspondence_rejection_sample_consensus.h>
#include <pcl/registration/transformation_estimation_svd.h>
#include <pcl/registration/icp.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/registration/transformation_estimation_point_to_plane_lls.h>

namespace v4r
{
    template<typename PointT>
    class V4R_EXPORTS ObjectHypothesis
    {
      typedef Model<PointT> ModelT;
      typedef boost::shared_ptr<ModelT> ModelTPtr;

      private:
        mutable boost::shared_ptr<pcl::visualization::PCLVisualizer> vis_;
        int vp1_;

      public:
        ModelTPtr model_;

        ObjectHypothesis()
        {
            model_scene_corresp_.reset(new pcl::Correspondences);
        }

        pcl::CorrespondencesPtr model_scene_corresp_; //indices between model keypoints (index query) and scene cloud (index match)
        std::vector<int> indices_to_flann_models_;

        void visualize(const typename pcl::PointCloud<PointT> & scene) const;

        ObjectHypothesis & operator=(const ObjectHypothesis &rhs)
        {
            *(this->model_scene_corresp_) = *rhs.model_scene_corresp_;
            this->indices_to_flann_models_ = rhs.indices_to_flann_models_;
            this->model_ = rhs.model_;
            return *this;
        }
    };

    template<typename PointT>
    class V4R_EXPORTS Recognizer
    {
      public:
        class V4R_EXPORTS Parameter
        {
        public:
            int icp_iterations_;    /// @brief number of icp iterations. If 0, no pose refinement will be done.
            int icp_type_; /// @brief defines the icp method being used for pose refinement (0... regular ICP with CorrespondenceRejectorSampleConsensus, 1... crops point cloud of the scene to the bounding box of the model that is going to be refined)
            double voxel_size_icp_;
            double max_corr_distance_; /// @brief defines the margin for the bounding box used when doing pose refinement of the cropped scene to the model
            int normal_computation_method_; /// @brief chosen normal computation method of the V4R library
            bool merge_close_hypotheses_; /// @brief if true, close correspondence clusters (object hypotheses) of the same object model are merged together and this big cluster is refined
            double merge_close_hypotheses_dist_; /// @brief defines the maximum distance of the centroids in meter for clusters to be merged together
            double merge_close_hypotheses_angle_; /// @brief defines the maximum angle in degrees for clusters to be merged together
            int resolution_mm_model_assembly_; /// @brief the resolution in millimeters of the model when it gets assembled into a point cloud

            Parameter(
                    int icp_iterations = 0,
                    int icp_type = 1,
                    double voxel_size_icp = 0.0025f,
                    double max_corr_distance = 0.05f,
                    int normal_computation_method = 2,
                    bool merge_close_hypotheses = true,
                    double merge_close_hypotheses_dist = 0.02f,
                    double merge_close_hypotheses_angle = 10.f,
                    int resolution_mm_model_assembly = 3)
                : icp_iterations_ (icp_iterations),
                  icp_type_ (icp_type),
                  voxel_size_icp_ (voxel_size_icp),
                  max_corr_distance_ (max_corr_distance),
                  normal_computation_method_ (normal_computation_method),
                  merge_close_hypotheses_ (merge_close_hypotheses),
                  merge_close_hypotheses_dist_ (merge_close_hypotheses_dist),
                  merge_close_hypotheses_angle_ (merge_close_hypotheses_angle),
                  resolution_mm_model_assembly_ (resolution_mm_model_assembly)
            {}
        }param_;

      protected:
        typedef Model<PointT> ModelT;
        typedef boost::shared_ptr<ModelT> ModelTPtr;

        typedef typename std::map<std::string, ObjectHypothesis<PointT> > symHyp;

        typedef typename pcl::PointCloud<PointT>::Ptr PointTPtr;
        typedef typename pcl::PointCloud<PointT>::ConstPtr ConstPointTPtr;

        /** \brief Point cloud to be classified */
        PointTPtr scene_;

        /** \brief Point cloud to be classified */
        pcl::PointCloud<pcl::Normal>::Ptr scene_normals_;
        mutable boost::shared_ptr<pcl::visualization::PCLVisualizer> vis_;
        mutable int vp1_, vp2_, vp3_;

        /** @brief: generated object hypotheses from correspondence grouping (before verification) */
        std::vector<ModelTPtr> models_;
        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > transforms_;
        std::vector<PlaneModel<PointT> > planes_;

        /** @brief boolean vector defining if model or plane is verified (models are first in the vector and its size is equal to models_) */
        std::vector<bool> model_or_plane_is_verified_;

        bool requires_segmentation_;
        std::vector<int> indices_;
        pcl::PointIndicesPtr icp_scene_indices_;

        std::string training_dir_; /// \brief Directory containing views of the object

        /** \brief Hypotheses verification algorithm */
        typename boost::shared_ptr<HypothesisVerification<PointT, PointT> > hv_algorithm_;

        void poseRefinement();
        void hypothesisVerification ();


      public:

        Recognizer(const Parameter &p = Parameter())
        {
          param_ = p;
          requires_segmentation_ = false;
        }

        virtual size_t getFeatureType() const
        {
            std::cout << "Get feature type is not implemented for this recognizer. " << std::endl;
            return 0;
        }

        virtual bool acceptsNormals() const
        {
            return false;
        }

        virtual void
        setSaveHypotheses(bool b)
        {
            (void)b;
            PCL_WARN("Set save hypotheses is not implemented for this class.");
        }

        virtual
        void
        getSavedHypotheses(symHyp &oh) const
        {
            (void)oh;
            PCL_WARN("getSavedHypotheses is not implemented for this class.");
        }

        virtual
        bool
        getSaveHypothesesParam() const
        {
            PCL_WARN("getSaveHypotheses is not implemented for this class.");
            return false;
        }

        virtual
        void
        getKeypointCloud(PointTPtr & cloud) const
        {
            (void)cloud;
            PCL_WARN("getKeypointCloud is not implemented for this class.");
        }

        virtual
        void
        getKeypointIndices(pcl::PointIndices & indices) const
        {
            (void)indices;
            PCL_WARN("Get keypoint indices is not implemented for this class.");
        }

        virtual void recognize () = 0;

        virtual typename boost::shared_ptr<Source<PointT> >
        getDataSource () const = 0;

        virtual void reinitialize(const std::vector<std::string> &load_ids = std::vector<std::string>())
        {
            (void)load_ids;
            PCL_WARN("Reinitialize is not implemented for this class.");
        }

        void setHVAlgorithm (const typename boost::shared_ptr<HypothesisVerification<PointT, PointT> > & alg)
        {
          hv_algorithm_ = alg;
        }

        void setInputCloud (const PointTPtr cloud)
        {
          scene_ = cloud;
        }

        /**
         * @brief return all generated object hypotheses
         * @return potential object model in the scene (not aligned to the scene)
         */
        std::vector<ModelTPtr>
        getModels () const
        {
          return models_;
        }


        /**
         * @brief returns only the models for the objects that have been verified
         * @return verified object model of the scene (not aligned to the scene)
         */
        std::vector<ModelTPtr>
        getVerifiedModels () const
        {
            std::vector<ModelTPtr> models_verified;

            if(model_or_plane_is_verified_.size() < models_.size()) {
                std::cout << "Verification vector is not valid. Did you run hyphotheses verification?" << std::endl;
                return models_verified;
            }

            for(size_t i=0; i<models_.size(); i++) {
                if (model_or_plane_is_verified_[i])
                    models_verified.push_back(models_[i]);
            }
            return models_verified;
        }


        /**
         * @brief return all transforms of generated object hypotheses with respect to the scene
         * @return 4x4 homogenous transformation matrix
         */
        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> >
        getTransforms () const
        {
          return transforms_;
        }


        /**
         * @brief returns only the transformations for the objects that have been verified
         * @return 4x4 homogenous transformation matrix of verified model to the scene
         */
        std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> >
        getVerifiedTransforms () const
        {
          std::vector<Eigen::Matrix4f, Eigen::aligned_allocator<Eigen::Matrix4f> > transforms_verified;

          if(model_or_plane_is_verified_.size() < transforms_.size()) {
              std::cout << "Verification vector is not valid. Did you run hyphotheses verification?" << std::endl;
              return transforms_verified;
          }

          for(size_t i=0; i<transforms_.size(); i++) {
              if (model_or_plane_is_verified_[i])
                  transforms_verified.push_back(transforms_[i]);
          }
          return transforms_verified;
        }


        /**
         * @brief Filesystem dir containing training files
         */
        void
        setTrainingDir (const std::string & dir)
        {
          training_dir_ = dir;
        }


        void setSceneNormals(const pcl::PointCloud<pcl::Normal>::Ptr &normals)
        {
            scene_normals_ = normals;
        }


        virtual bool requiresSegmentation() const
        {
          return requires_segmentation_;
        }

        virtual void
        setIndices (const std::vector<int> & indices)
        {
          indices_ = indices;
        }

        void visualize () const;
    };
}
#endif /* RECOGNIZER_H_ */
