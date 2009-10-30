#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "JobStateARC1.h"

namespace Arc {

  JobState::StateType JobStateARC1::StateMap(const std::string& state) {
    if (state == "ACCEPTED")
      return JobState::ACCEPTED;
    else if (state == "PREPARING")
      return JobState::PREPARING;
    else if (state == "SUBMIT")
      return JobState::SUBMITTING;
    else if (state == "INLRMS:Q")
      return JobState::QUEUING;
    else if (state == "INLRMS:R" ||
             state == "INLRMS:EXECUTED" ||
             state == "INLRMS:S" ||
             state == "INLRMS:E")
      return JobState::RUNNING;
    else if (state == "FINISHING")
      return JobState::FINISHING;
    else if (state == "FINISHED")
      return JobState::FINISHED;
    else if (state == "KILLED")
      return JobState::KILLED;
    else if (state == "FAILED")
      return JobState::FAILED;
    else if (state == "DELETED")
      return JobState::DELETED;
    else if (state == "")
      return JobState::UNDEFINED;
    else
      return JobState::OTHER;
  }

}
