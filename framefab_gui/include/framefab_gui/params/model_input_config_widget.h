//
// Created by yijiangh on 6/17/17.
//

#ifndef FRAMEFAB_RVIZ_PLUGIN_MODEL_INPUT_CONFIG_WIDGET_H
#define FRAMEFAB_RVIZ_PLUGIN_MODEL_INPUT_CONFIG_WIDGET_H

#include "framefab_gui/parameter_window_base.h"

#include <framefab_msgs/ModelInputParameters.h>

namespace Ui
{
class ModelInputConfigWindow;
}

namespace framefab_gui
{

class ModelInputConfigWidget : public ParameterWindowBase
{
  Q_OBJECT
 public:
  ModelInputConfigWidget(framefab_msgs::ModelInputParameters params);
  framefab_msgs::ModelInputParameters& params() { return params_; }

  virtual void update_display_fields();
  virtual void update_internal_fields();

 protected:
  virtual int get_unit_combobox_value();

 protected Q_SLOTS:
  virtual void get_file_path_handler();

 private:
  framefab_msgs::ModelInputParameters params_;
  Ui::ModelInputConfigWindow* ui_;
  std::string last_filepath_;
};
}

#endif //FRAMEFAB_RVIZ_PLUGIN_MODEL_INPUT_CONFIG_WIDGET_H