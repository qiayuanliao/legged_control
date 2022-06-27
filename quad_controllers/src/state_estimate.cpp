//
// Created by qiayuan on 2021/11/15.
//
#include "quad_controllers/state_estimate.h"

#include <ocs2_robotic_tools/common/RotationTransforms.h>
#include <ocs2_robotic_tools/common/RotationDerivativesTransforms.h>
#include <ocs2_centroidal_model/AccessHelperFunctions.h>

namespace quad_ros
{
StateEstimateBase::StateEstimateBase(ros::NodeHandle& nh, PinocchioInterface& pinocchio_interface,
                                     const CentroidalModelInfo& centroidal_model_info,
                                     const std::vector<HybridJointHandle>& hybrid_joint_handles)
  : pinocchio_interface_(pinocchio_interface)
  , centroidal_model_info_(centroidal_model_info)
  , centroidal_conversions_(pinocchio_interface_, centroidal_model_info_)
  , hybrid_joint_handles_(hybrid_joint_handles)
{
  odom_pub_ = std::make_shared<realtime_tools::RealtimePublisher<nav_msgs::Odometry>>(nh, "/odom", 100);
}

FromTopicStateEstimate::FromTopicStateEstimate(ros::NodeHandle& nh, PinocchioInterface& pinocchio_interface,
                                               const CentroidalModelInfo& centroidal_model_info,
                                               const std::vector<HybridJointHandle>& hybrid_joint_handles_)
  : StateEstimateBase(nh, pinocchio_interface, centroidal_model_info, hybrid_joint_handles_)
{
  sub_ = nh.subscribe<nav_msgs::Odometry>("/ground_truth/state", 100, &FromTopicStateEstimate::callback, this);
}

void FromTopicStateEstimate::callback(const nav_msgs::Odometry::ConstPtr& msg)
{
  buffer_.writeFromNonRT(*msg);
}

vector_t FromTopicStateEstimate::update()
{
  nav_msgs::Odometry odom = *buffer_.readFromRT();
  Eigen::Quaternion<scalar_t> quat(odom.pose.pose.orientation.w, odom.pose.pose.orientation.x,
                                   odom.pose.pose.orientation.y, odom.pose.pose.orientation.z);
  vector_t rbd_state(2 * centroidal_model_info_.generalizedCoordinatesNum);
  vector_t zyx = quatToZyx(quat);
  rbd_state.segment<3>(0) = zyx;
  rbd_state.segment<3>(3) << odom.pose.pose.position.x, odom.pose.pose.position.y, odom.pose.pose.position.z;
  rbd_state.segment<3>(centroidal_model_info_.generalizedCoordinatesNum) << odom.twist.twist.angular.x,
      odom.twist.twist.angular.y, odom.twist.twist.angular.z;
  rbd_state.segment<3>(centroidal_model_info_.generalizedCoordinatesNum + 3) << odom.twist.twist.linear.x,
      odom.twist.twist.linear.y, odom.twist.twist.linear.z;
  for (size_t i = 0; i < hybrid_joint_handles_.size(); ++i)
  {
    rbd_state(6 + i) = hybrid_joint_handles_[i].getPosition();
    rbd_state(centroidal_model_info_.generalizedCoordinatesNum + 6 + i) = hybrid_joint_handles_[i].getVelocity();
  }
  return centroidal_conversions_.computeCentroidalStateFromRbdModel(rbd_state);
}

}  // namespace quad_ros
