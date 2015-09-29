/*
 * Copyright 2015 Andreas Bircher, ASL, ETH Zurich, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ros/ros.h>
#include <prune_pointcloud/prune.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl_ros/transforms.h>
#include <pcl/filters/filter.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_datatypes.h>
#include <sensor_msgs/PointCloud2.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>

#define SQ(x) ((x)*(x))

PointcloudPruning::Prune::Prune(ros::NodeHandle& n)
    : n_(n)
{
  pointcloudSub_ = n_.subscribe("pointcloudIn", 1, &PointcloudPruning::Prune::pointcloud, this);
  pcl_publisher_ = n_.advertise < sensor_msgs::PointCloud2 > ("pointcloudOut", 1, true);
  loadParams();
}

PointcloudPruning::Prune::~Prune()
{
}

void PointcloudPruning::Prune::pointcloud(const sensor_msgs::PointCloud2::ConstPtr& pointcloudIn)
{
  // Find transforms for all specified vehicles by their tf frames.
  std::vector < tf::Vector3 > agents;
  for (typename std::vector<std::string>::iterator it = vehicle_tf_frames_.begin();
      it != vehicle_tf_frames_.end(); it++) {
    tf::StampedTransform tf_transform;
    ros::Time time_to_lookup = pointcloudIn->header.stamp;
    if (!tf_listener_.canTransform(pointcloudIn->header.frame_id, *it, time_to_lookup)) {
      time_to_lookup = ros::Time(0);
      ROS_WARN("Using latest TF transform instead of timestamp match.");
    }
    try {
      tf_listener_.lookupTransform(pointcloudIn->header.frame_id, *it, time_to_lookup,
                                   tf_transform);
    } catch (tf::TransformException& ex) {
      ROS_ERROR_STREAM("Error getting TF transform from sensor data: " << ex.what());
      return;
    }
    agents.push_back(tf_transform.getOrigin());
  }
  // Prepare pointcloud.
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::fromROSMsg(*pointcloudIn, *cloud);
  // Remove NaN values, if any.
  std::vector<int> indices;
  pcl::removeNaNFromPointCloud(*cloud, *cloud, indices);
  // Iterate through pointcloud and remove all points that are close than squared threshold maxDist2_
  for (pcl::PointCloud<pcl::PointXYZ>::iterator it = cloud->begin(); it != cloud->end(); ++it) {
    for (typename std::vector<tf::Vector3>::iterator itPose = agents.begin();
        itPose != agents.end(); itPose++) {
      if (SQ(it->x - itPose->x()) + SQ(it->y - itPose->y()) + SQ(it->z - itPose->z()) < maxDist2_) {
        cloud->erase(it);
      }
    }
  }
  // Publish pruned pointcloud
  sensor_msgs::PointCloud2::Ptr pointcloudOut(new sensor_msgs::PointCloud2);
  pcl::toROSMsg(*cloud, *pointcloudOut);
  pcl_publisher_.publish(pointcloudOut);
}

void PointcloudPruning::Prune::loadParams()
{

  n_.param("max_dist", maxDist2_, 0.5);
  n_.getParam("vehicle_tf_frames", vehicle_tf_frames_);
  vehicle_tf_frames_.push_back("firefly1/ground_truth");
  vehicle_tf_frames_.push_back("firefly2/ground_truth");
  vehicle_tf_frames_.push_back("firefly3/ground_truth");
  maxDist2_ = pow(maxDist2_, 2.0);
}
