#include <services/surface_blending_service.h>
#include <godel_msgs/TrajectoryExecution.h>

// Process Planning
#include <godel_msgs/BlendProcessPlanning.h>
#include <godel_msgs/KeyenceProcessPlanning.h>
#include <godel_msgs/PathPlanning.h>

#include <godel_param_helpers/godel_param_helpers.h>

// topics and services
const static std::string SAVE_DATA_BOOL_PARAM = "save_data";
const static std::string SAVE_LOCATION_PARAM = "save_location";
const static std::string TRAJECTORY_PLANNING_SERVICE = "trajectory_planner";
const static std::string SURFACE_DETECTION_SERVICE = "surface_detection";
const static std::string SURFACE_BLENDING_PARAMETERS_SERVICE = "surface_blending_parameters";
const static std::string SELECT_SURFACE_SERVICE = "select_surface";
const static std::string PROCESS_PATH_SERVICE = "process_path";
const static std::string PATH_GENERATION_SERVICE = "process_path_generator";
const static std::string VISUALIZE_BLENDING_PATH_SERVICE = "visualize_path_generator";
const static std::string RENAME_SURFACE_SERVICE = "rename_surface";
const static std::string PATH_EXECUTION_SERVICE = "path_execution";
const static std::string GET_MOTION_PLANS_SERVICE = "get_available_motion_plans";
const static std::string SELECT_MOTION_PLAN_SERVICE = "select_motion_plan";
const static std::string LOAD_SAVE_MOTION_PLAN_SERVICE = "load_save_motion_plan";

const static std::string BLEND_PROCESS_EXECUTION_SERVICE = "blend_process_execution";
const static std::string SCAN_PROCESS_EXECUTION_SERVICE = "scan_process_execution";
const static std::string BLEND_PROCESS_PLANNING_SERVICE = "blend_process_planning";
const static std::string SCAN_PROCESS_PLANNING_SERVICE = "keyence_process_planning";

const static std::string TOOL_PATH_PREVIEW_TOPIC = "tool_path_preview";
const static std::string EDGE_VISUALIZATION_TOPIC = "edge_visualization";
const static std::string BLEND_VISUALIZATION_TOPIC = "blend_visualization";
const static std::string SCAN_VISUALIZATION_TOPIC = "scan_visualization";
const static std::string SELECTED_SURFACES_CHANGED_TOPIC = "selected_surfaces_changed";
const static std::string ROBOT_SCAN_PATH_PREVIEW_TOPIC = "robot_scan_path_preview";
const static std::string PUBLISH_REGION_POINT_CLOUD = "publish_region_point_cloud";
const static std::string REGION_POINT_CLOUD_TOPIC = "region_colored_cloud";

const static std::string EDGE_IDENTIFIER = "_edge_";

//  tool visual properties
//const static float TOOL_DIA = .050;
//const static float TOOL_THK = .005;
//const static float TOOL_SHAFT_DIA = .006;
//const static float TOOL_SHAFT_LEN = .045;
//const static std::string TOOL_FRAME_ID = "process_tool";

// Default filepaths and namespaces for caching stored parameters
const static std::string BLEND_PARAMS_FILE = "godel_blending_parameters.msg";
const static std::string PATH_PLANNING_PARAMS_FILE = "godel_path_planning_parameters.msg";

const static std::string BLEND_TOOL_PLUGIN_PARAM = "blend_tool_planning_plugin_name";
const static std::string SCAN_TOOL_PLUGIN_PARAM = "scan_tool_planning_plugin_name";

// action server name
const static std::string BLEND_EXE_ACTION_SERVER_NAME = "blend_process_execution_as";
const static std::string PROCESS_PLANNING_ACTION_SERVER_NAME = "process_planning_as";
const static std::string SELECT_MOTION_PLAN_ACTION_SERVER_NAME = "select_motion_plan_as";
const static int PROCESS_EXE_BUFFER = 5;  // Additional time [s] buffer between when blending should end and timeout

SurfaceBlendingService::SurfaceBlendingService() : publish_region_point_cloud_(false), save_data_(false),
  blend_exe_client_(BLEND_EXE_ACTION_SERVER_NAME, true),
  scan_exe_client_(SCAN_EXE_ACTION_SERVER_NAME, true),
  process_planning_server_(nh_, PROCESS_PLANNING_ACTION_SERVER_NAME,
                           boost::bind(&SurfaceBlendingService::processPlanningActionCallback, this, _1), false),
  select_motion_plan_server_(nh_, SELECT_MOTION_PLAN_ACTION_SERVER_NAME,
                             boost::bind(&SurfaceBlendingService::selectMotionPlansActionCallback, this, _1), false)
{}

bool SurfaceBlendingService::init()
{
  using namespace godel_surface_detection;

  ros::NodeHandle ph("~");

  // loading parameters
  ph.getParam(SAVE_DATA_BOOL_PARAM, save_data_);
  ph.getParam(SAVE_LOCATION_PARAM, save_location_);

  // Load the 'prefix' that will be combined with parameters msg base names to save to disk
  ph.param<std::string>("param_cache_prefix", param_cache_prefix_, "");

  if (!this->load_path_planning_parameters(param_cache_prefix_ + PATH_PLANNING_PARAMS_FILE))
    ROS_WARN("Unable to load blending process parameters.");

  if (!this->load_blend_parameters(param_cache_prefix_ + BLEND_PARAMS_FILE))
    ROS_WARN("Unable to load blending process parameters.");

  //TODO: make sure action (sequence) planning component is loaded

  // save default parameters
  default_surf_detection_params_ = surface_detection_.params_;
  default_blending_plan_params_ = blending_plan_params_;

//  if (surface_server_.init() && data_coordinator_.init())
//  {
//    // adding callbacks
//    ROS_INFO_STREAM("Surface detection service initialization succeeded");
//  }
//  else
//  {
//    ROS_ERROR_STREAM("Surface detection service had an initialization error");
//  }

  // start server
//  interactive::InteractiveSurfaceServer::SelectionCallback f =
//      boost::bind(&SurfaceBlendingService::publish_selected_surfaces_changed, this);
//  surface_server_.add_selection_callback(f);

  // service clients
  process_path_client_ = nh_.serviceClient<godel_msgs::PathPlanning>(PATH_GENERATION_SERVICE);

  // Process Execution Parameters
  blend_planning_client_ = nh_.serviceClient<godel_msgs::BlendProcessPlanning>(BLEND_PROCESS_PLANNING_SERVICE);

  surf_blend_parameters_server_ =
      nh_.advertiseService(SURFACE_BLENDING_PARAMETERS_SERVICE,
                          &SurfaceBlendingService::surface_blend_parameters_server_callback, this);

  surface_detect_server_ = nh_.advertiseService(
      SURFACE_DETECTION_SERVICE, &SurfaceBlendingService::surface_detection_server_callback, this);

  select_surface_server_ = nh_.advertiseService(
      SELECT_SURFACE_SERVICE, &SurfaceBlendingService::select_surface_server_callback, this);

  get_motion_plans_server_ = nh_.advertiseService(
      GET_MOTION_PLANS_SERVICE, &SurfaceBlendingService::getMotionPlansCallback, this);

  load_save_motion_plan_server_ = nh_.advertiseService(
      LOAD_SAVE_MOTION_PLAN_SERVICE, &SurfaceBlendingService::loadSaveMotionPlanCallback, this);

  rename_suface_server_ = nh_.advertiseService(RENAME_SURFACE_SERVICE,
                                              &SurfaceBlendingService::renameSurfaceCallback, this);

  // publishers
  selected_surf_changed_pub_ = nh_.advertise<godel_msgs::SelectedSurfacesChanged>(SELECTED_SURFACES_CHANGED_TOPIC, 1);
  point_cloud_pub_ = nh_.advertise<sensor_msgs::PointCloud2>(REGION_POINT_CLOUD_TOPIC, 1);
  tool_path_markers_pub_ = nh_.advertise<visualization_msgs::MarkerArray>(TOOL_PATH_PREVIEW_TOPIC, 1, true);
  blend_visualization_pub_ = nh_.advertise<geometry_msgs::PoseArray>(BLEND_VISUALIZATION_TOPIC, 1, true);
  edge_visualization_pub_ = nh_.advertise<geometry_msgs::PoseArray>(EDGE_VISUALIZATION_TOPIC, 1, true);
  scan_visualization_pub_ = nh_.advertise<geometry_msgs::PoseArray>(SCAN_VISUALIZATION_TOPIC, 1, true);

  // action servers
  process_planning_server_.start();
  select_motion_plan_server_.start();

  return true;
}

void SurfaceBlendingService::run()
{
  surface_server_.run();

  ros::Duration loop_duration(1.0f);
  while (ros::ok())
  {
    if (publish_region_point_cloud_ && !region_cloud_msg_.data.empty())
    {
      point_cloud_pub_.publish(region_cloud_msg_);
    }

    loop_duration.sleep();
  }
}

// Blending Parameters
bool SurfaceBlendingService::load_blend_parameters(const std::string& filename)
{
  using godel_param_helpers::loadParam;
  using godel_param_helpers::loadBoolParam;

  if (godel_param_helpers::fromFile(filename, blending_plan_params_))
  {
    return true;
  }
  // otherwise default to the parameter server
  ros::NodeHandle nh("~/blending_plan");
  return loadParam(nh, "tool_radius", blending_plan_params_.tool_radius) &&
         loadParam(nh, "margin", blending_plan_params_.margin) &&
         loadParam(nh, "overlap", blending_plan_params_.overlap) &&
         loadParam(nh, "approach_spd", blending_plan_params_.approach_spd) &&
         loadParam(nh, "blending_spd", blending_plan_params_.blending_spd) &&
         loadParam(nh, "retract_spd", blending_plan_params_.retract_spd) &&
         loadParam(nh, "traverse_spd", blending_plan_params_.traverse_spd) &&
         loadParam(nh, "discretization", blending_plan_params_.discretization) &&
         loadParam(nh, "safe_traverse_height", blending_plan_params_.safe_traverse_height) &&
         loadParam(nh, "min_boundary_length", blending_plan_params_.min_boundary_length);
}


void SurfaceBlendingService::save_blend_parameters(const std::string& filename)
{
  if (!godel_param_helpers::toFile(filename, blending_plan_params_))
  {
    ROS_WARN_STREAM("Unable to save blending-plan parameters to: " << filename);
  }
}


bool SurfaceBlendingService::load_path_planning_parameters(const std::string & filename)
{
  if(godel_param_helpers::fromFile(filename, path_planning_params_))
  {
    return true;
  }
  return false;
}


void SurfaceBlendingService::save_path_planning_parameters(const std::string & filename)
{
  if(!godel_param_helpers::toFile(filename, path_planning_params_))
  {
    ROS_WARN_STREAM("Unable to save path-planning parameters to: " << filename);
  }
}


// Profilimeter parameters
bool SurfaceBlendingService::load_scan_parameters(const std::string& filename)
{
  using godel_param_helpers::loadParam;
  using godel_param_helpers::loadBoolParam;

  if (godel_param_helpers::fromFile(filename, scan_plan_params_))
  {
    return true;
  }

  // otherwise we load default parameters from the param server
  ros::NodeHandle nh("~/scan_plan");
  return loadParam(nh, "traverse_speed", scan_plan_params_.traverse_spd) &&
         loadParam(nh, "margin", scan_plan_params_.margin) &&
         loadParam(nh, "overlap", scan_plan_params_.overlap) &&
         loadParam(nh, "scan_width", scan_plan_params_.scan_width) &&
         loadParam(nh, "min_qa_value", scan_plan_params_.min_qa_value) &&
         loadParam(nh, "max_qa_value", scan_plan_params_.max_qa_value) &&
         loadParam(nh, "approach_distance", scan_plan_params_.approach_distance) &&
         loadParam(nh, "window_width", scan_plan_params_.window_width);
}

void SurfaceBlendingService::processPlanningActionCallback(const godel_msgs::ProcessPlanningGoalConstPtr &goal_in)
{
  switch (goal_in->action)
  {
    case godel_msgs::ProcessPlanningGoal::GENERATE_MOTION_PLAN_AND_PREVIEW:
    {
      ensenso::EnsensoGuard guard; // turns off ensenso for planning and turns it on when this goes out of scope
      process_planning_feedback_.last_completed = "Recieved request to generate motion plan";
      process_planning_server_.publishFeedback(process_planning_feedback_);
      trajectory_library_ = generateMotionLibrary(goal_in->params);
      process_planning_feedback_.last_completed = "Finished planning. Visualizing...";
      process_planning_server_.publishFeedback(process_planning_feedback_);
      visualizePaths();
      process_planning_result_.succeeded = true;
      process_planning_server_.setSucceeded(process_planning_result_);
      break;
    }
    case godel_msgs::ProcessPlanningGoal::PREVIEW_TOOL_PATH:
    {
      process_planning_feedback_.last_completed = "Recieved request to preview tool path";
      process_planning_server_.publishFeedback(process_planning_feedback_);
      break;
    }

    default:
    {
      ROS_ERROR_STREAM("Unknown action code '" << goal_in->action << "' request");
      break;
    }
  }

  process_planning_result_.succeeded = false;
}

bool SurfaceBlendingService::surface_blend_parameters_server_callback(
    godel_msgs::SurfaceBlendingParameters::Request& req,
    godel_msgs::SurfaceBlendingParameters::Response& res)
{
  switch (req.action)
  {
  case godel_msgs::SurfaceBlendingParameters::Request::GET_CURRENT_PARAMETERS:

    res.surface_detection = surface_detection_.params_;
    res.robot_scan = robot_scan_.params_;
    res.blending_plan = blending_plan_params_;
    res.scan_plan = scan_plan_params_;
    res.path_params = path_planning_params_;
    break;

  case godel_msgs::SurfaceBlendingParameters::Request::GET_DEFAULT_PARAMETERS:

    res.surface_detection = default_surf_detection_params_;
    res.robot_scan = default_robot_scan_params__;
    res.blending_plan = default_blending_plan_params_;
    res.scan_plan = default_scan_params_;
    res.path_params = default_path_planning_params_;
    break;

  // Update the current parameters in this service
  case godel_msgs::SurfaceBlendingParameters::Request::SET_PARAMETERS:
  case godel_msgs::SurfaceBlendingParameters::Request::SAVE_PARAMETERS:
    surface_detection_.params_ = req.surface_detection;
    robot_scan_.params_ = req.robot_scan;
    blending_plan_params_ = req.blending_plan;
    scan_plan_params_ = req.scan_plan;
    path_planning_params_ = req.path_params;

    if (req.action == godel_msgs::SurfaceBlendingParameters::Request::SAVE_PARAMETERS)
    {
      this->save_blend_parameters(param_cache_prefix_ + BLEND_PARAMS_FILE);
      this->save_scan_parameters(param_cache_prefix_ + SCAN_PARAMS_FILE);
      this->save_path_planning_parameters(param_cache_prefix_ + PATH_PLANNING_PARAMS_FILE);
      robot_scan_.save_parameters(param_cache_prefix_ + ROBOT_SCAN_PARAMS_FILE);
      surface_detection_.save_parameters(param_cache_prefix_ + SURFACE_DETECTION_PARAMS_FILE);
    }
    break;
  }

  return true;
}


void SurfaceBlendingService::selectMotionPlansActionCallback(const godel_msgs::SelectMotionPlanGoalConstPtr& goal_in)
{
  godel_msgs::SelectMotionPlanResult res;

  // If plan does not exist, abort and return
  if (trajectory_library_.get().find(goal_in->name) == trajectory_library_.get().end())
  {
    ROS_WARN_STREAM("Motion plan " << goal_in->name << " does not exist. Cannot execute.");
    res.code = godel_msgs::SelectMotionPlanResponse::NO_SUCH_NAME;
    select_motion_plan_server_.setAborted(res);
    return;
  }

  bool is_blend = trajectory_library_.get()[goal_in->name].type == godel_msgs::ProcessPlan::BLEND_TYPE;

  // Send command to execution server
  godel_msgs::ProcessExecutionActionGoal goal;
  goal.goal.trajectory_approach = trajectory_library_.get()[goal_in->name].trajectory_approach;
  goal.goal.trajectory_depart = trajectory_library_.get()[goal_in->name].trajectory_depart;
  goal.goal.trajectory_process = trajectory_library_.get()[goal_in->name].trajectory_process;
  goal.goal.wait_for_execution = goal_in->wait_for_execution;
  goal.goal.simulate = goal_in->simulate;

  actionlib::SimpleActionClient<godel_msgs::ProcessExecutionAction> *exe_client =
      (is_blend ? &blend_exe_client_ : &scan_exe_client_);
  exe_client->sendGoal(goal.goal);

  ros::Duration process_time(goal.goal.trajectory_depart.points.back().time_from_start);
  ros::Duration buffer_time(PROCESS_EXE_BUFFER);
  if(exe_client->waitForResult(process_time + buffer_time))
  {
    res.code = godel_msgs::SelectMotionPlanResult::SUCCESS;
    select_motion_plan_server_.setSucceeded(res);
  }
  else
  {
    res.code=godel_msgs::SelectMotionPlanResult::TIMEOUT;
    select_motion_plan_server_.setAborted(res);
  }
}

void SurfaceBlendingService::visualizePaths()
{
  visualizePathPoses();

  visualizePathStrips();
}

void SurfaceBlendingService::visualizePathPoses()
{
  // Publish poses
  geometry_msgs::PoseArray blend_poses, edge_poses, scan_poses;
  blend_poses.header.frame_id = edge_poses.header.frame_id = scan_poses.header.frame_id = "world_frame";
  blend_poses.header.stamp = edge_poses.header.stamp = scan_poses.header.stamp = ros::Time::now();

  for (const auto& path : process_path_results_.blend_poses_)
  {
    for (const auto& pose_array : path)
    {
      blend_poses.poses.insert(blend_poses.poses.end(), pose_array.poses.begin(), pose_array.poses.end());
    }
  }

  for(const auto& pose_array : process_path_results_.edge_poses_)
    edge_poses.poses.insert(edge_poses.poses.end(), pose_array.poses.begin(), pose_array.poses.end());

  for (const auto& path : process_path_results_.scan_poses_)
  {
    for(const auto& pose_array : path)
    {
      scan_poses.poses.insert(scan_poses.poses.end(), pose_array.poses.begin(), pose_array.poses.end());
    }
  }

  blend_visualization_pub_.publish(blend_poses);
  edge_visualization_pub_.publish(edge_poses);
  scan_visualization_pub_.publish(scan_poses);
}

static visualization_msgs::Marker makeLineStripMarker(const std::string& ns, const int id, const std_msgs::ColorRGBA& color,
                                                      const geometry_msgs::PoseArray& segment)
{
  visualization_msgs::Marker marker;
  marker.header.frame_id = "world_frame";
  marker.header.stamp = ros::Time::now();
  marker.id = id;
  marker.ns = ns;
  marker.pose.orientation.w = 1;
  marker.action = visualization_msgs::Marker::ADD;
  marker.type = visualization_msgs::Marker::LINE_STRIP;
  marker.scale.x = 0.004;
  marker.color = color;
  marker.lifetime = ros::Duration(0.0);

  for (const auto& pose : segment.poses)
    marker.points.push_back(pose.position);

  return marker;
}

void SurfaceBlendingService::visualizePathStrips()
{
  visualization_msgs::MarkerArray path_visualization;

  // Visualize Blending Paths
  std_msgs::ColorRGBA blend_color;
  blend_color.r = 1.0;
  blend_color.b = 0.0;
  blend_color.g = 0.0;
  blend_color.a = 0.8;

  const std::string blend_ns = "blend_paths";

  int blend_path_id = 0;

  for (const auto& path : process_path_results_.blend_poses_) // for a given surface
  {
    for (const auto& segment : path) // for a given path segment on the surface
    {
      path_visualization.markers.push_back(makeLineStripMarker(blend_ns, blend_path_id++, blend_color, segment));
    }
  }

  // Visualize scan paths
  std_msgs::ColorRGBA scan_color;
  scan_color.r = 1.0;
  scan_color.b = 0.0;
  scan_color.g = 1.0;
  scan_color.a = 0.8;

  const std::string scan_ns = "scan_paths";

  int scan_path_id = 0;

  for (const auto& path : process_path_results_.scan_poses_) // for a given surface
  {
    for (const auto& segment : path) // for a given path segment on the surface
    {
      path_visualization.markers.push_back(makeLineStripMarker(scan_ns, scan_path_id++, scan_color, segment));
    }
  }

  tool_path_markers_pub_.publish(path_visualization);
}

std::string SurfaceBlendingService::getBlendToolPlanningPluginName() const
{
  ros::NodeHandle pnh ("~");
  std::string name;
  if (!pnh.getParam(BLEND_TOOL_PLUGIN_PARAM, name))
  {
    ROS_WARN("Unable to load blend tool planning plugin from ros param '%s'",
             BLEND_TOOL_PLUGIN_PARAM.c_str());
  }

  return name;
}

std::string SurfaceBlendingService::getScanToolPlanningPluginName() const
{
  ros::NodeHandle pnh ("~");
  std::string name;
  if (!pnh.getParam(SCAN_TOOL_PLUGIN_PARAM, name))
  {
    ROS_WARN("Unable to load scan tool planning plugin from ros param '%s'",
             SCAN_TOOL_PLUGIN_PARAM.c_str());
  }

  return name;
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "framefab_core_service");
  ros::AsyncSpinner spinner(4);
  spinner.start();
  FrameFabCoreService core_srv;

  if (core_srv.init())
  {
    ROS_INFO("FrameFab Core Service successfully initialized and now running");
    core_srv.run();
  }

  ros::waitForShutdown();
}
