/******************************************************************************
 * Copyright (c) 2013 Aitor Aldoma, Thomas Faeulhammer
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


#include <pcl/common/common.h>
#include <opencv2/opencv.hpp>
#include <v4r/core/macros.h>
#ifndef V4R_PCL_OPENCV_H_
#define V4R_PCL_OPENCV_H_

namespace v4r
{
  template<typename PointT>
  V4R_EXPORTS
  cv::Mat
  ConvertPCLCloud2Image (const typename pcl::PointCloud<PointT> &pcl_cloud, bool crop = false);

  /**
   *@brief converts a point cloud to an image and crops it to a fixed size
   * @param[in] cloud
   * @param[in] cluster_idx object indices
   * @param[in] desired output height of the image
   * @param[in] desired output width of the image
   * @param[in] desired margin for the bounding box
   */
  template<typename PointT>
  V4R_EXPORTS
  cv::Mat
  ConvertPCLCloud2FixedSizeImage(const typename pcl::PointCloud<PointT> &cloud, const std::vector<int> &cluster_idx,
                                 size_t out_height = 256, size_t out_width = 256, size_t margin = 10,
                                 cv::Scalar bg_color = cv::Scalar(255,255,255), bool do_closing_operation = false);



  /**
   *@brief converts a point cloud to an image and crops it to a fixed size
   * @param[in] cloud
   * @param[in] cluster_idx object indices
   * @param[in] desired output height of the image
   * @param[in] desired output width of the image
   */
  template<typename PointT>
  V4R_EXPORTS
  cv::Mat
  ConvertPCLCloud2Image(const typename pcl::PointCloud<PointT> &cloud, const std::vector<int> &cluster_idx, size_t out_height, size_t out_width);


  /**
   * @brief computes a binary image from a point cloud which pixels are true for pixel occupied when raytracing the point cloud
   *
   */
  template<class PointT>
  V4R_EXPORTS
  std::vector<bool>
  ConvertPCLCloud2OccupancyImage (const typename pcl::PointCloud<PointT> &cloud,
                                  int width = 640,
                                  int height = 480,
                                  float f = 525.5f,
                                  float cx = 319.5f,
                                  float cy = 239.5f);


  template<class PointT>
  V4R_EXPORTS
  cv::Mat
  ConvertPCLCloud2DepthImage (const typename pcl::PointCloud<PointT> &pcl_cloud);


  template<class PointT>
  V4R_EXPORTS
  cv::Mat
  ConvertUnorganizedPCLCloud2Image (const typename pcl::PointCloud<PointT> &pcl_cloud,
                                    bool crop = false,
                                    float bg_r = 255.0f,
                                    float bg_g = 255.0f,
                                    float bg_b = 255.0f,
                                    int width = 640,
                                    int height = 480,
                                    float f = 525.5f,
                                    float cx = 319.5f,
                                    float cy = 239.5f);

   /**
     * @brief computes the depth map of a point cloud with fixed size output
     * @param RGB-D cloud
     * @param indices of the points belonging to the object
     * @param out_height
     * @param out_width
     * @return depth image (float)
     */
    template<typename PointT>
    V4R_EXPORTS
    cv::Mat
    ConvertPCLCloud2DepthImageFixedSize(const pcl::PointCloud<PointT> &cloud, const std::vector<int> &cluster_idx, size_t out_height, size_t out_width);

    /**
      * @brief computes the depth map of a point cloud with fixed size output
      * @param RGB-D cloud
      * @param indices of the points belonging to the object
      * @param crop if true, image will be cropped to object specified by the indices
      * @param remove background... if true, will set pixel not specified by the indices to a specific background color (e.g. black)
      * @return margin in pixel from the image boundaries to the maximal extent of the object (only if crop is set to true)
      */
    template<typename PointT>
    V4R_EXPORTS
    cv::Mat
    pcl2cvMat (const typename pcl::PointCloud<PointT> &pcl_cloud,
               const std::vector<int> &indices, bool crop = false, bool remove_background = true, int margin = 10);


    /**
      * @brief computes the depth map of a point cloud with fixed size output
      * @param RGB-D cloud
      * @param indices of the points belonging to the object
      * @param out_height
      * @param out_width
      * @return depth image (unsigned int)
      */
     template<typename PointT>
     V4R_EXPORTS
     cv::Mat
     ConvertPCLCloud2UnsignedDepthImageFixedSize(const pcl::PointCloud<PointT> &cloud, const std::vector<int> &cluster_idx, size_t out_height, size_t out_width);
}

#endif /* PCL_OPENCV_H_ */
