//
// Created by yijiangh on 11/28/17.
//

#include "choreo_descartes_planner/capsulated_ladder_tree.h"
#include "choreo_descartes_planner/capsulated_ladder_tree_RRTstar.h"

#include "choreo_descartes_planner/pose_sampling_helpers.h"
#include "pose_verification_helpers.h"

// ladder graph & DAG (solution extraction)
#include <descartes_planner/ladder_graph.h>
#include <descartes_planner/ladder_graph_dag_search.h>

// pose conversion
#include <eigen_conversions/eigen_msg.h>
#include <geometry_msgs/Pose.h>

static const double DEFAULT_UNIT_PROCESS_TIMEOUT = 30.0;

namespace // anon namespace to hide utility functions
{
static bool comparePtrCapVert(const descartes_planner::CapVert* lhs, const descartes_planner::CapVert* rhs)
{
  return (lhs->getCost() < rhs->getCost());
}
} //end util namespace

namespace descartes_planner
{
CapsulatedLadderTreeRRTstar::CapsulatedLadderTreeRRTstar(
    const std::vector<ConstrainedSegment>& segs,
    const std::vector<planning_scene::PlanningScenePtr>& planning_scenes,
    const std::vector<planning_scene::PlanningScenePtr>& planning_scenes_completed)
{
  // sanity check
  assert(segs.size() == planning_scenes.size());

  // intialize cap rungs
  cap_rungs_.reserve(segs.size());
  for(size_t i=0; i < segs.size(); i++)
  {
    CapRung cap_rung;

    // end effector path pts
    auto points = discretizePositions(segs[i].start, segs[i].end, segs[i].linear_disc);
    cap_rung.path_pts_ = points;

    // feasible orientations
    cap_rung.orientations_.reserve(segs[i].orientations.size());
    for (auto& orient : segs[i].orientations)
    {
      cap_rung.orientations_.push_back(orient);
    }

//    if(ConstrainedSegment::PROCESS_TYPE::CONNECT == segs[i].process_type)
//    {
//      // ideal orientation orthogonal to the element
//      cap_rung.ideal_o_to_element_angle = M_PI / 2;
//    }
//    else
//    {
//      // ideal orientation is aligned to the element
//      cap_rung.ideal_o_to_element_angle = 0.0;
//    }

    // planning scene
    cap_rung.planning_scene_ = planning_scenes[i];
    cap_rung.planning_scene_completed_ = planning_scenes_completed[i];

    // input z axis disc
    cap_rung.z_axis_disc_ = segs[i].z_axis_disc;

    // linear speed
    cap_rung.linear_vel_ = segs[i].linear_vel;

    cap_rungs_.push_back(cap_rung);
  }
}

CapsulatedLadderTreeRRTstar::~CapsulatedLadderTreeRRTstar()
{
  for(auto& cap_rung : cap_rungs_)
  {
    for(auto& ptr_vert : cap_rung.ptr_cap_verts_)
    {
      if(NULL != ptr_vert)
      {
        delete ptr_vert;
        ptr_vert = NULL;
      }
    }
  }
}

double CapsulatedLadderTreeRRTstar::solve(descartes_core::RobotModel& model,
                                          double unit_process_timeout,
                                          double rrt_star_timeout)
{
  if(unit_process_timeout < 1.0)
  {
    logWarn("[CLT-RRT] unit process sampling timeout %.2f s, smaller than 1.0, set to default timeout %.2f s",
            unit_process_timeout, DEFAULT_UNIT_PROCESS_TIMEOUT);
    unit_process_timeout = DEFAULT_UNIT_PROCESS_TIMEOUT;
  }

  if(rrt_star_timeout < 5.0)
  {
    rrt_star_timeout = 4*cap_rungs_.size();
    logWarn("[CLT-RRT] rrt star timeout %.2f s, smaller than 5.0s, set to default timeout %.2f s",
            rrt_star_timeout, rrt_star_timeout);
  }

  // find initial solution
  size_t rung_id = 0;
  CapVert* ptr_prev_vert = NULL;

  for(auto& cap_rung : cap_rungs_)
  {
    const auto unit_search_start = ros::Time::now();
    auto unit_search_update = unit_search_start;
    CapVert* ptr_cap_vert = new CapVert(model.getDOF());

    do
    {
      std::vector<Eigen::Affine3d> poses = generateSample(cap_rung, *ptr_cap_vert);

      if(checkFeasibility(model, poses, cap_rung, *ptr_cap_vert))
      {
        ptr_cap_vert->setParentVertPtr(ptr_prev_vert);
        ptr_cap_vert->rung_id_ = rung_id;
        cap_rung.ptr_cap_verts_.push_back(ptr_cap_vert);
        ptr_prev_vert = ptr_cap_vert;
        break;
      }

      unit_search_update = ros::Time::now();
    }
    while((unit_search_update - unit_search_start).toSec() < unit_process_timeout);

    if(cap_rung.ptr_cap_verts_.empty())
    {
      // random sampling fails to find a solution
      ROS_ERROR_STREAM("[CapRRTstar] process #" << rung_id
                                                << " fails to find intial feasible sol within timeout "
                                                << unit_process_timeout << "secs");
      return std::numeric_limits<double>::max();
    }

    ROS_INFO_STREAM("[CLTRRT] ik solutions found for process #" << rung_id);
    rung_id++;
  } // loop over cap_rungs

  double initial_sol_cost = cap_rungs_.back().ptr_cap_verts_.back()->getCost();
  ROS_INFO_STREAM("[CLTRRT] initial sol found! cost: " << initial_sol_cost);
  ROS_INFO_STREAM("[CLTRRT] RRT* improvement starts, computation time: " << rrt_star_timeout);

  // RRT* improve on the tree
  const auto rrt_start_time = ros::Time::now();

  while((ros::Time::now() - rrt_start_time).toSec() < rrt_star_timeout)
  {
    // sample cap_rung
    int rung_id_sample = randomSampleInt(0, cap_rungs_.size()-1);
    auto& cap_rung_sample = cap_rungs_[rung_id_sample];

    // sample cap_vert
    CapVert* ptr_new_vert = new CapVert(model.getDOF());
    auto poses = generateSample(cap_rung_sample, *ptr_new_vert);

    if(checkFeasibility(model, poses, cap_rung_sample, *ptr_new_vert))
    {
      // find nearest node in tree
      double c_min = std::numeric_limits<double>::max();
      CapVert* ptr_nearest_vert = NULL;

      if(rung_id_sample > 0)
      {
        for(auto& ptr_near_vert : this->cap_rungs_[rung_id_sample-1].ptr_cap_verts_)
        {
          double new_near_cost = ptr_near_vert->getCost() + ptr_new_vert->distance(ptr_near_vert);
          if(c_min > new_near_cost)
          {
            ptr_nearest_vert = ptr_near_vert;
            c_min = new_near_cost;
          }
        }
      }

      // add new vert into CL tree (rung_id, parent_vert)
      ptr_new_vert->rung_id_ = rung_id_sample;
      ptr_new_vert->setParentVertPtr(ptr_nearest_vert);
      cap_rung_sample.ptr_cap_verts_.push_back(ptr_new_vert);

      // update vert next (repair tree)
      if(rung_id_sample < this->cap_rungs_.size()-1)
      {
        double new_vert_cost = ptr_new_vert->getCost();
        for(auto& ptr_next_vert : this->cap_rungs_[rung_id_sample+1].ptr_cap_verts_)
        {
          double old_next_cost = ptr_next_vert->getCost();
          double new_next_cost = new_vert_cost + ptr_next_vert->distance(ptr_new_vert);
          if(old_next_cost > new_next_cost)
          {
            ptr_next_vert->setParentVertPtr(ptr_new_vert);
          }
        }
      }
    } // end if capvert feasible
  }

  CapVert* ptr_last_cap_vert = *std::min_element(this->cap_rungs_.back().ptr_cap_verts_.begin(),
                                                 this->cap_rungs_.back().ptr_cap_verts_.end(), comparePtrCapVert);
  double rrt_cost = ptr_last_cap_vert->getCost();

  ROS_INFO_STREAM("[CLTRRT] RRT* sol cost " << rrt_cost
                                            << " after " << rrt_star_timeout << " secs.");
  return rrt_cost;
}

void CapsulatedLadderTreeRRTstar::extractSolution(descartes_core::RobotModel& model,
                                                  std::vector<descartes_core::TrajectoryPtPtr>& sol,
                                                  std::vector<descartes_planner::LadderGraph>& graphs,
                                                  std::vector<int>& graph_indices,
                                                  const bool use_saved_graph)
{
  const auto graph_build_start = ros::Time::now();
  const int dof = model.getDOF();

  if(!use_saved_graph)
  {
    graphs.clear();
    graph_indices.clear();

    // find min cap_vert on last cap_rung
    CapVert* ptr_last_cap_vert = *std::min_element(this->cap_rungs_.back().ptr_cap_verts_.begin(),
                                                   this->cap_rungs_.back().ptr_cap_verts_.end(), comparePtrCapVert);
    while (ptr_last_cap_vert != NULL)
    {
      // construct unit ladder graph for each cap rungpath_pts_
      const auto cap_rung = cap_rungs_[ptr_last_cap_vert->rung_id_];
      double traverse_length = (cap_rung.path_pts_.front() - cap_rung.path_pts_.back()).norm();
      const auto dt = traverse_length / cap_rung.linear_vel_;

      model.setPlanningScene(cap_rung.planning_scene_);
      auto unit_ladder_graph = sampleSingleConfig(model,
                                                  cap_rungs_[ptr_last_cap_vert->rung_id_].path_pts_,
                                                  dt,
                                                  ptr_last_cap_vert->orientation_,
                                                  ptr_last_cap_vert->z_axis_angle_);

      graphs.insert(graphs.begin(), unit_ladder_graph);
      graph_indices.insert(graph_indices.begin(), unit_ladder_graph.size());
      ptr_last_cap_vert = ptr_last_cap_vert->getParentVertPtr();
    }
  }
  else
  {
    graph_indices.clear();
    for(const auto& graph : graphs)
    {
      graph_indices.push_back(graph.size());
    }
  }

  // unify unit ladder graphs into one
  descartes_planner::LadderGraph unified_graph(model.getDOF());
  for(auto& graph : graphs)
  {
    assert(unified_graph.dof() == graph.dof());
    descartes_planner::appendInTime(unified_graph, graph);
  }

  // carry out DAG search on each ladder graph
  descartes_planner::DAGSearch search(unified_graph);
  double cost = search.run();
  auto path_idxs = search.shortestPath();

  sol.clear();
  for (size_t j = 0; j < path_idxs.size(); ++j)
  {
    const auto idx = path_idxs[j];
    const auto* data = unified_graph.vertex(j, idx);
    const auto& tm = unified_graph.getRung(j).timing;
    auto pt = descartes_core::TrajectoryPtPtr(new descartes_trajectory::JointTrajectoryPt(
        std::vector<double>(data, data + dof), tm));
    sol.push_back(pt);
  }

  auto graph_build_end = ros::Time::now();
  ROS_INFO_STREAM("[CLTRRT] Graph construction and searching took: "
                      << (graph_build_end - graph_build_start).toSec() << " seconds");
}
} //end namespace descartes planner