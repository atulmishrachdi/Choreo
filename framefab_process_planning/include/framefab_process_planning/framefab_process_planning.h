#ifndef FRAMEFAB_PROCESS_PLANNING_H
#define FRAMEFAB_PROCESS_PLANNING_H

#include <framefab_msgs/ProcessPlanning.h>

#include <descartes_core/robot_model.h>
#include <pluginlib/class_loader.h>

/*
 * This class wraps Descartes planning methods and provides functionality for configuration
 * and for planning for framefab support and spatial element printing paths.
 *
 * The general plannning approach is:
 *
 */
namespace framefab_process_planning
{

class ProcessPlanningManager
{
public:
  ProcessPlanningManager(const std::string& world_frame,
                         const std::string& hotend_group, const std::string& hotend_tcp,
                         const std::string& robot_model_plugin);

  bool handlePrintPlanning(framefab_msgs::ProcessPlanning::Request& req,
                           framefab_msgs::ProcessPlanning::Response& res) { return true; }

private:
  descartes_core::RobotModelPtr hotend_model_;
  moveit::core::RobotModelConstPtr moveit_model_;
  pluginlib::ClassLoader<descartes_core::RobotModel> plugin_loader_; // kept around so code doesn't get unloaded
  std::string hotend_group_name_;
};
}

#endif