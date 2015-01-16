#ifndef DPE_SERVICE_MAIN_DPE_MODEL_DPE_PROJECT_H_
#define DPE_SERVICE_MAIN_DPE_MODEL_DPE_PROJECT_H_

#include "dpe_base/dpe_base.h"
namespace ds
{
class DPEProject;
class DPEProjectState : public base::RefCounted<DPEProjectState>
{
friend class DPEProject;
public:
  enum
  {
    TASK_STATE_PENDING,
    TASK_STATE_RUNNING,
    TASK_STATE_FINISH,
    TASK_STATE_ERROR,
  };

  struct DPETask
  {
    DPETask() : id_(-1), state_(TASK_STATE_PENDING) {}
    int64_t id_;
    int32_t state_;
    std::string input_;
    std::string result_;
  };
public:
  DPEProjectState();
  ~DPEProjectState();

  bool  AddInputData(const std::string& input_data);

  bool  MakeTaskQueue(std::queue<std::pair<int64_t, int32_t> >& task_queue);
  DPETask*  GetTask(int64_t id, int32_t idx = -1);
  std::vector<DPETask>& TaskData(){return task_data_;}

private:
  std::map<int64_t, int32_t>      id2idx_;
  std::vector<DPETask>            task_data_;
};

class DPEProject : public base::RefCounted<DPEProject>
{
public:

public:
  DPEProject();
  ~DPEProject();

  bool  LoadSavedState(DPEProjectState* state);
  bool  SaveProject(DPEProjectState* state);

public:
  static scoped_refptr<DPEProject>  FromFile(const base::FilePath& job_path);

public:
  std::wstring                    job_name_;
  base::FilePath                  job_home_path_;
  base::FilePath                  job_file_path_;

  std::wstring                    compiler_type_;

  std::string                     r_source_path_;
  std::string                     r_worker_path_;
  std::string                     r_sink_path_;

  base::FilePath                  source_path_;
  std::string                     source_data_;
  base::FilePath                  worker_path_;
  std::string                     worker_data_;
  base::FilePath                  sink_path_;
  std::string                     sink_data_;

  std::string                     r_dpe_state_path_;
  base::FilePath                  dpe_state_path_;
  std::string                     dpe_state_checksum_;
};
}
#endif
