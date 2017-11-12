//
// Created by yijiangh on 6/22/17.
//

#ifndef FRAMEFAB_GUI_PROCESS_PLANNING_STATE_H
#define FRAMEFAB_GUI_PROCESS_PLANNING_STATE_H

#include "framefab_gui/gui_state.h"
#include <ros/ros.h>

#include <actionlib/client/simple_action_client.h>
#include <framefab_msgs/ProcessPlanningAction.h>

namespace framefab_gui
{

class ProcessPlanningState : public GuiState
{
  Q_OBJECT
 public:
  // Constructor
  ProcessPlanningState(const int index, const bool use_ladder_graph_record);

  // Entry and exit classes
  virtual void onStart(FrameFabWidget& gui);
  virtual void onExit(FrameFabWidget& gui);

  // Handlers for the fixed buttons
  virtual void onNext(FrameFabWidget& gui);
  virtual void onBack(FrameFabWidget& gui);
  virtual void onReset(FrameFabWidget& gui);

 private:
  void makeRequest();
  void processPlanningDoneCallback(
      const actionlib::SimpleClientGoalState& state,
      const framefab_msgs::ProcessPlanningResultConstPtr& result);
  void processPlanningActiveCallback();
  void processPlanningFeedbackCallback(
      const framefab_msgs::ProcessPlanningFeedbackConstPtr& feedback);

  Q_SIGNALS:
  void feedbackReceived(QString feedback);

 private Q_SLOTS:
  void setFeedbackText(QString feedback);

 private:
  int selected_path_index_;
  bool use_ladder_graph_record_;

  actionlib::SimpleActionClient<framefab_msgs::ProcessPlanningAction> process_planning_action_client_;
  FrameFabWidget* gui_ptr_;
};
}

#endif //FRAMEFAB_GUI_PROCESS_PLANNING_STATE_H
