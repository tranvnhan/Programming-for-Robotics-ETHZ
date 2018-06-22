#include "husky_highlevel_controller/HuskyHighlevelController.hpp"

namespace husky_highlevel_controller {

HuskyHighlevelController::HuskyHighlevelController(ros::NodeHandle& nodeHandle)
    : nodeHandle_(nodeHandle) {
  std::string topic;
  int queue_size;

  // initialize parameters
  pause = false;
  p_gain = 0;
  x_dot = 0;
  threshold = 4;

  if (!nodeHandle.getParam("topic", topic)) {
    ROS_ERROR("Could not find topic parameter!");
  }

  if (!nodeHandle.getParam("queue_size", queue_size)) {
    ROS_ERROR("Could not find queue_size parameter!");
  }

  if (!nodeHandle.getParam("p_gain", p_gain)) {
    ROS_ERROR("Could not find p_gain parameter!");
  }

  if (!nodeHandle.getParam("x_dot", x_dot)) {
    ROS_ERROR("Could not find x_dot parameter!");
  }

  // initialize subscriber and publisher
  subscriber_ = nodeHandle.subscribe(topic, queue_size,
                                     &HuskyHighlevelController::topicCallback,
                                     this);
  subscriber_IMU_ = nodeHandle.subscribe(
      "/imu/data", 10, &HuskyHighlevelController::topicCallback_IMU, this);
  publisher_ = nodeHandle.advertise<geometry_msgs::Twist>("/cmd_vel", 1);
  vis_pub_ = nodeHandle.advertise<visualization_msgs::Marker>(
      "/visualization_marker", 0);

  // initialize service server and client
  server_ = nodeHandle_.advertiseService(
      "/pause_husky", &HuskyHighlevelController::serviceCallback, this);
  client_ = nodeHandle_.serviceClient<std_srvs::SetBool>("/pause_husky");
}

HuskyHighlevelController::~HuskyHighlevelController() {
}

void HuskyHighlevelController::topicCallback(
    const sensor_msgs::LaserScan::ConstPtr& msg) {
  int i = 0;
  int size = msg->ranges.size();

  float min_dis = 10000;
  float min_i = 0;

  for (i = 0; i < size; i++) {
    float dist = msg->ranges.at(i);

    if (dist < min_dis) {
      min_dis = dist;
      min_i = i;
    }
  }

//  // Option A: Call service /pause_husky when min_dis <= 1.0
//  if (min_dis <= 1.0) {
//    service_.request.data = true;
//    if (client_.call(service_)) {
//      ROS_INFO("%s\n", service_.response.message.c_str());
//    } else {
//      ROS_ERROR("Failed to call service /pause_husky");
//      return;
//    }
//  }

// Calculate x, y
  float angle_increment = msg->angle_increment;
  float angle_min = msg->angle_min;
  float rad = angle_min + angle_increment * min_i;

  float x, y, z;

  x = min_dis * cos(rad);
  y = min_dis * sin(rad);
  z = 0;

//  ROS_INFO("x: %f \ny: %f", x, y);

// calculate control input
  float angle_dot, x_dot;

  angle_dot = 0;
  x_dot = 0;

  if (pause == true) {
    // pause
    angle_dot = 0;
    x_dot = 0;
  } else {
    // resume
    angle_dot = HuskyHighlevelController::p_gain * (0 - rad);
    x_dot = HuskyHighlevelController::x_dot;
  }

  // publish control input as message
  geometry_msgs::Twist base_cmd;
  base_cmd.linear.x = x_dot;
  base_cmd.angular.z = angle_dot;

  publisher_.publish(base_cmd);

  // publish visualization marker
  visualization_msgs::Marker marker;
  marker.header.frame_id = "base_link";
  marker.header.stamp = ros::Time();
  marker.ns = "my_namespace";
  marker.id = 0;
  marker.type = visualization_msgs::Marker::CUBE;
  marker.action = visualization_msgs::Marker::ADD;
  marker.pose.position.x = x;
  marker.pose.position.y = y;
  marker.pose.position.z = z;
  marker.pose.orientation.x = 0.0;
  marker.pose.orientation.y = 0.0;
  marker.pose.orientation.z = 0.0;
  marker.pose.orientation.w = 1.0;
  marker.scale.x = 1;
  marker.scale.y = 0.1;
  marker.scale.z = 0.1;
  marker.color.a = 1.0;  // Don't forget to set the alpha!
  marker.color.r = 0.0;
  marker.color.g = 1.0;
  marker.color.b = 0.0;
  //only if using a MESH_RESOURCE marker type:
  marker.mesh_resource = "package://pr2_description/meshes/base_v0/base.dae";
  vis_pub_.publish(marker);
}

// Option B: stop the robot after a crash has occurred
void HuskyHighlevelController::topicCallback_IMU(
    const sensor_msgs::Imu::ConstPtr& msg) {
  float acc_x = msg->linear_acceleration.x;
  float acc_y = msg->linear_acceleration.y;

  ROS_INFO("acc_x: %f \nacc_y: %f", acc_x, acc_y);

  if (pow(acc_x, 2) + pow(acc_y, 2) > threshold) {  // threshold depends on value of x_dot
    service_.request.data = true;

    if (client_.call(service_)) {
      ROS_INFO("%s\n", service_.response.message.c_str());
    } else {
      ROS_ERROR("Failed to call service /pause_husky");
      return;
    }
  }
}

bool HuskyHighlevelController::serviceCallback(
    std_srvs::SetBool::Request& request,
    std_srvs::SetBool::Response& response) {

  bool input = request.data;

  if (input == true) {
    pause = true;
    response.message = "pause_husky";
    ROS_INFO("pause_husky");
  } else {
    pause = false;
    response.message = "resume_husky";
    ROS_INFO("resume_husky");
  }

  response.success = true;
  return true;
}

} /* namespace */
