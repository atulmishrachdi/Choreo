#ifndef FRAMEFAB_GUI_FRAMEFAB_WIDGET_H
#define FRAMEFAB_GUI_FRAMEFAB_WIDGET_H

#include <QWidget>

#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>

#include <framefab_gui/gui_state.h>
#include <framefab_gui/params_submenu.h>
#include <framefab_gui/selection/selection_widget.h>

namespace Ui
{
class FrameFabWidget;
}

namespace framefab_gui
{
/**
 * @brief The FrameFabWidget class works in states:
 * 1. System Init State
 * 2. Path Planning State           (req Model Info)
 * 3. Select Path State             (user select path for process planning)
 * 4. Process Planning State        (motion planning)
 * 5. Select Plan State             (User selects plans)
 * 6. Simulating State              (simulated robot execution motion)
 * 7. PostProcessing State          (output results to Grasshopper for fine-tuned motion compensation)
 */
class FrameFabWidget : public QWidget
{
  Q_OBJECT
 public:
  FrameFabWidget(QWidget* parent = 0);

  virtual ~FrameFabWidget();

  // Interface for the states to interact with
  void setText(const std::string& txt);
  void appendText(const std::string& txt);

  void setButtonsEnabled(bool enabled);
  void setParamsButtonEnabled(bool enabled);

  void showStatusWindow();
  void setLabelText(const std::string& txt);

  ros::NodeHandle&  nodeHandle() { return nh_; }
  ParamsSubmenu&    params() { return *params_; }
  SelectionWidget& selection_widget() { return *selection_widget_; }

 protected:
  void loadParameters();

 protected Q_SLOTS:
  // Button Handlers
  void onNextButton();
  void onBackButton();
  void onResetButton();
  void onParamsButton();

  void onParamsSave();
  void onParamsAccept();

  // State Change
  void changeState(GuiState* new_state);

 protected:
  // UI
  Ui::FrameFabWidget* ui_;
  ParamsSubmenu* params_;
  SelectionWidget* selection_widget_;

  // ROS specific stuff
  ros::NodeHandle nh_;

  // Current state
  GuiState* active_state_;

  // Params Save
  ros::ServiceClient framefab_parameters_client_;
};
}

#endif // FRAMEFAB_GUI_FRAMEFAB_WIDGET_H
