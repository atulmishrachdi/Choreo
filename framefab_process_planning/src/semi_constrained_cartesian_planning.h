//
// Created by yijiangh on 4/10/18.
//

#ifndef FRAMEFAB_MPP_SEMI_CONSTRAINED_CARTESIAN_PLANNING_H
#define FRAMEFAB_MPP_SEMI_CONSTRAINED_CARTESIAN_PLANNING_H

#include <choreo_descartes_planner/constrained_segment.h>

#include <descartes_core/robot_model.h>

#include <framefab_msgs/UnitProcessPlan.h>
#include <framefab_msgs/ElementCandidatePoses.h>
#include <framefab_msgs/AssemblySequencePickNPlace.h>

namespace framefab_process_planning
{
// overhead for spatial extrusion
void CLTRRTforProcessROSTraj(descartes_core::RobotModelPtr model,
                             std::vector <descartes_planner::ConstrainedSegment> &segs,
                             const double clt_rrt_unit_process_timeout,
                             const double clt_rrt_timeout,
                             const std::vector <planning_scene::PlanningScenePtr> &planning_scenes_approach,
                             const std::vector <planning_scene::PlanningScenePtr> &planning_scenes_depart,
                             const std::vector <framefab_msgs::ElementCandidatePoses> &task_seq,
                             std::vector <framefab_msgs::UnitProcessPlan> &plans,
                             const std::string &saved_graph_file_name,
                             bool use_saved_graph);

// TODO: overhead for picknplace
void CLTRRTforProcessROSTraj(descartes_core::RobotModelPtr model,
                             const framefab_msgs::AssemblySequencePickNPlace& as_pnp,
                             const double clt_rrt_unit_process_timeout,
                             const double clt_rrt_timeout,
                             const double& linear_vel,
                             const double& linear_disc,
                             const std::vector<std::vector<planning_scene::PlanningScenePtr>>& planning_scenes_subprocess,
                             std::vector <framefab_msgs::UnitProcessPlan>& plans,
                             const std::string &saved_graph_file_name,
                             bool use_saved_grap);
}

#endif //FRAMEFAB_MPP_SEMI_CONSTRAINED_CARTESIAN_PLANNING_H
