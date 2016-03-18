#ifndef V4R_RECOGNITION_MODEL_GHV_H__
#define V4R_RECOGNITION_MODEL_GHV_H__

#include <v4r/core/macros.h>
#include <v4r/recognition/recognition_model_hv.h>
#include <pcl/common/common.h>

namespace v4r
{
  template<typename ModelT>
  class V4R_EXPORTS GHVRecognitionModel : public HVRecognitionModel<ModelT>
  {
    public:
      using HVRecognitionModel<ModelT>::outlier_indices_;
      using HVRecognitionModel<ModelT>::scene_pt_is_explained_;
      using HVRecognitionModel<ModelT>::visible_cloud_;
      using HVRecognitionModel<ModelT>::complete_cloud_;
      using HVRecognitionModel<ModelT>::visible_cloud_normals_;
      using HVRecognitionModel<ModelT>::complete_cloud_normals_;
      using HVRecognitionModel<ModelT>::visible_indices_;
      using HVRecognitionModel<ModelT>::extra_weight_;
      using HVRecognitionModel<ModelT>::pt_color_;
      using HVRecognitionModel<ModelT>::cloud_indices_specified_;
      using HVRecognitionModel<ModelT>::scene_inlier_indices_for_visible_pt_;
      using HVRecognitionModel<ModelT>::scene_inlier_distances_for_visible_pt_;
      using HVRecognitionModel<ModelT>::plane_model_;


      typedef boost::shared_ptr< GHVRecognitionModel> Ptr;
      typedef boost::shared_ptr< GHVRecognitionModel const> ConstPtr;
  };
}

#endif
