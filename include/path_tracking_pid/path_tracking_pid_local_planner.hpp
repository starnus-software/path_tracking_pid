#pragma once

#include "./controller.hpp"
#include <atomic>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <memory>
#include <string>
#include <vector>

#include <costmap_2d/costmap_2d_ros.h>
#include <dynamic_reconfigure/server.h>
#include <mbf_costmap_core/costmap_controller.h>
#include <mbf_msgs/ExePathResult.h>
#include <nav_core/base_local_planner.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/Path.h>
#include <path_tracking_pid/PidConfig.h>
#include <path_tracking_pid/PidFeedback.h>
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float64.h>
#include <tf/transform_listener.h>
#include <tf2_ros/buffer.h>

#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#define MAP_PARALLEL_THRESH 0.2
constexpr double DT_MAX=1.5;


BOOST_GEOMETRY_REGISTER_POINT_2D(geometry_msgs::Point, double, cs::cartesian, x, y)

namespace path_tracking_pid
{
class TrackingPidLocalPlanner : public nav_core::BaseLocalPlanner, public mbf_costmap_core::CostmapController
{
private:
  typedef boost::geometry::model::ring<geometry_msgs::Point> polygon_t;

  inline polygon_t union_(polygon_t polygon1, polygon_t polygon2)
  {
    std::vector<polygon_t> output_vec;
    boost::geometry::union_(polygon1, polygon2, output_vec);
    return output_vec.at(0);  // Only first vector element is filled
  }

public:
  TrackingPidLocalPlanner();
  ~TrackingPidLocalPlanner();

  /**
    * @brief Initialize local planner
    * @param name The name of the planner
    * @param tf a pointer to TransformListener in TF Buffer
    * @param costmap Costmap indicating free/occupied space
    */
  void initialize(std::string name, tf2_ros::Buffer* tf, costmap_2d::Costmap2DROS* costmap);

  /**
    * @brief Set the plan we should be following
    * @param global_plan Plan to follow as closely as we can
    * @return whether the plan was successfully updated or not
    */
  bool setPlan(const std::vector<geometry_msgs::PoseStamped>& global_plan);

  /**
   * @brief Calculates the velocity command based on the current robot pose given by pose. See the interface in move
   * base.
   * @param cmd_vel Output the velocity command
   * @return true if succeded.
   */
  bool computeVelocityCommands(geometry_msgs::Twist& cmd_vel);  // NOLINT

  /**
   * @brief Calculates the velocity command based on the current robot pose given by pose. The velocity
   * and message are not set. See the interface in move base flex.
   * @param pose Current robot pose
   * @param velocity
   * @param cmd_vel Output the velocity command
   * @param message
   * @return a status code defined in the move base flex ExePath action.
   */
  uint32_t computeVelocityCommands(const geometry_msgs::PoseStamped& pose, const geometry_msgs::TwistStamped& velocity,
                                   geometry_msgs::TwistStamped& cmd_vel, std::string& message);  // NOLINT

  /**
   * @brief Returns true, if the goal is reached. Currently does not respect the parameters give
   * @return true, if the goal is reached
   */
  bool isGoalReached();

  /**
   * @brief Returns true, if the goal is reached. Currently does not respect the parameters given.
   * @param dist_tolerance Tolerance in distance to the goal
   * @param angle_tolerance Tolerance in the orientation to the goals orientation
   * @return true, if the goal is reached
   */
  bool isGoalReached(double dist_tolerance, double angle_tolerance);

  /**
   * @brief Canceles the planner.
   * @return True on cancel success.
   */
  bool cancel();

  /** Enumeration for custom SUCCESS feedback codes. See default ones:
   * https://github.com/magazino/move_base_flex/blob/master/mbf_msgs/action/ExePath.action
  */
  enum
  {
    SUCCESS = 0,
    GRACEFULLY_CANCELLING = 1
  };

private:
  /**
   * Accept a new configuration for the PID controller
   * @param config the new configuration
   * @param level
   */
  void reconfigure_pid(path_tracking_pid::PidConfig& config, uint32_t level);

  void pauseCallback(std_msgs::Bool pause);

  void curOdomCallback(const nav_msgs::Odometry& odom_msg);

  void velMaxExternalCallback(const std_msgs::Float64& msg);

  uint8_t projectedCollisionCost();

  nav_msgs::Odometry latest_odom_;
  ros::Time prev_time_;
  ros::Duration prev_dt_;

  path_tracking_pid::Controller pid_controller_;

  std::vector<geometry_msgs::PoseStamped> global_plan_;
  nav_msgs::Path received_path_;

  // Obstacle collision detection
  costmap_2d::Costmap2DROS* costmap_;

  // Cancel flags (multi threaded, so atomic bools)
  std::atomic<bool> active_goal_{false};
  std::atomic<bool> cancel_requested_{false};
  std::atomic<bool> cancel_in_progress_{false};

  // dynamic reconfiguration
  boost::recursive_mutex config_mutex_;
  std::unique_ptr<dynamic_reconfigure::Server<path_tracking_pid::PidConfig>> pid_server_;

  tf2_ros::Buffer* tf_;
  geometry_msgs::TransformStamped tfCurPoseStamped_;

  ros::Publisher debug_pub_;  // Debugging of controller internal parameters

  // Rviz visualization
  ros::Publisher marker_pub_;
  ros::Publisher path_pub_;
  ros::Publisher predicted_pub_;
  ros::Publisher collision_marker_pub_;

  ros::Subscriber sub_odom_;
  ros::Publisher feedback_pub_;

  ros::Subscriber sub_vel_max_external_;

  std::string map_frame_;
  std::string base_link_frame_;
  bool initialized_ = false;

  // Used for tricycle model
  bool use_tricycle_model_;
  std::string steered_wheel_frame_;
  geometry_msgs::TransformStamped tf_base_to_steered_wheel_stamped_;

  // Controller logic
  bool controller_debug_enabled_ = false;
};
};  // end namespace path_tracking_pid
