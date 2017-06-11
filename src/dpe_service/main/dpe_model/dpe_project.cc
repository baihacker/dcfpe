#include "dpe_service/main/dpe_model/dpe_project.h"

namespace ds
{
DPEProjectState::DPEProjectState()
{
}

DPEProjectState::~DPEProjectState()
{
}

bool  DPEProjectState::AddInputData(const std::string& input_data)
{
  std::vector<std::string> input_lines;
  Tokenize(input_data, "\r\n", &input_lines);

  const int n = static_cast<int>(input_lines.size());
  std::vector<DPETask>(n).swap(task_data_);

  for (int32_t i = 0; i < n; ++i)
  {
    std::string& str = input_lines[i];

    const int len  = static_cast<int>(str.size());
    int pos = 0;
    while (pos < len && str[pos] != ':') ++pos;

    if (pos < len)
    {
      task_data_[i].input_ = str.substr(pos+1);
      task_data_[i].id_ = atoll(str.substr(0, pos).c_str());
      task_data_[i].state_ = TASK_STATE_PENDING;
      id2idx_[task_data_[i].id_] = i;
    }
    else
    {
      task_data_[i].state_ = TASK_STATE_ERROR;
    }
  }
  return true;
}

bool  DPEProjectState::MakeTaskQueue(std::queue<std::pair<int64_t, int32_t> >& task_queue)
{
  std::queue<std::pair<int64_t, int32_t> >().swap(task_queue);

  const int n = static_cast<int>(task_data_.size());
  for (int32_t i = 0; i < n; ++i)
  {
    if (task_data_[i].state_ == TASK_STATE_PENDING)
    {
      task_queue.push({task_data_[i].id_, i});
    }
  }
  return true;
}

DPEProjectState::DPETask*  DPEProjectState::GetTask(int64_t id, int32_t idx)
{
  const int n = static_cast<int>(task_data_.size());
  if (idx < 0 || idx >= n)
  {
    auto where = id2idx_.find(id);
    if (where != id2idx_.end())
    {
      idx = where->second;
    }
  }
  if (idx < 0 || idx >= n) return NULL;
  if (task_data_[idx].id_ != id) return NULL;

  return &task_data_[idx];
}

DPEProject::DPEProject()
{
}

DPEProject::~DPEProject()
{

}

bool  DPEProject::LoadSavedState(DPEProjectState* state)
{
  // load empty state
  if (!PathExists(dpe_state_path_)) return true;

  std::string data;
  if (!base::ReadFileToString(dpe_state_path_, &data))
  {
    LOG(ERROR) << "DPEProject::Read saved state failed";
    return false;
  }

  if (!dpe_state_checksum_.empty())
  {
    std::string checksum = base::MD5String(data);
    if (dpe_state_checksum_ != checksum)
    {
      return false;
    }
  }

  std::vector<std::string> lines;
  Tokenize(data, "\r\n", &lines);

  const int n = static_cast<int>(lines.size());

  for (int32_t i = 0; i < n; ++i)
  {
    std::string& str = lines[i];

    const int len  = static_cast<int>(str.size());
    int pos = 0;
    while (pos < len && str[pos] != ':') ++pos;

    if (pos < len)
    {
      auto task = state->GetTask(atoll(str.substr(0, pos).c_str()));
      if (task)
      {
        task->state_ = DPEProjectState::TASK_STATE_FINISH;
        task->result_ = str.substr(pos+1);
      }
    }
    else
    {
      return false;
    }
  }
  return true;
}

bool  DPEProject::SaveProject(DPEProjectState* state)
{
  base::DictionaryValue project;
  project.SetString("name", base::SysWideToUTF8(job_name_));
  project.SetString("dpe_state", r_dpe_state_path_);
  project.SetString("source", r_source_path_);
  project.SetString("worker", r_worker_path_);
  project.SetString("sink", r_sink_path_);

  std::string state_data;
  {
    std::stringstream ss;
    for (auto& iter: state->task_data_)
    {
      if (iter.state_ == DPEProjectState::TASK_STATE_FINISH)
      {
        ss << iter.id_ << ":" << iter.result_ << "\n";
      }
    }
    state_data = ss.str();
  }
  project.SetString("dpe_state_checksum", base::MD5String(state_data));

  std::string project_data;
  if (!base::JSONWriter::WriteWithOptions(&project, base::JSONWriter::OPTIONS_PRETTY_PRINT, &project_data))
  {
    LOG(ERROR) << "SaveProject error";
    return false;
  }

  base::FilePath project_backup_path = job_file_path_.AddExtension(L".bak");
  base::FilePath project_state_backup_path = dpe_state_path_.AddExtension(L".bak");

  base::File::Error err;
  base::ReplaceFile(job_file_path_, project_backup_path, &err);
  base::ReplaceFile(dpe_state_path_, project_state_backup_path, &err);

  base::WriteFile(job_file_path_, project_data.c_str(), static_cast<int>(project_data.size()));
  base::WriteFile(dpe_state_path_, state_data.c_str(), static_cast<int>(state_data.size()));

  return true;
}

scoped_refptr<DPEProject> DPEProject::FromFile(const base::FilePath& job_path)
{
  std::string data;
  if (!base::ReadFileToString(job_path, &data))
  {
    LOG(ERROR) << "DPEProject::Start ReadFileToString failed";
    return NULL;
  }

  base::Value* project = base::JSONReader::Read(data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!project)
  {
    LOG(ERROR) << "DPEProject::Start ParseProject failed";
    return NULL;
  }

  scoped_refptr<DPEProject> ret = new DPEProject();
  bool ok = false;
  do
  {
    base::DictionaryValue* dv = NULL;
    if (!project->GetAsDictionary(&dv)) break;

    ret->job_home_path_ = job_path.DirName();
    ret->job_file_path_ = job_path;

    std::string val;
    if (dv->GetString("name", &val))
    {
      ret->job_name_ = base::SysUTF8ToWide(val);
    }
    else
    {
      ret->job_name_ = L"Unknown";
    }

    if (dv->GetString("dpe_state", &ret->r_dpe_state_path_))
    {
      ret->dpe_state_path_ = ret->job_home_path_.Append(base::UTF8ToNative(ret->r_dpe_state_path_));
    }
    else
    {
      ret->r_dpe_state_path_ = "dpe_state.txt";
      ret->dpe_state_path_ = ret->job_home_path_.Append(base::UTF8ToNative(ret->r_dpe_state_path_));
    }

    if (dv->GetString("dpe_state_checksum", &val))
    {
      ret->dpe_state_checksum_ = val;
    }

    if (!dv->GetString("source", &ret->r_source_path_))
    {
      LOG(ERROR) << "DPEProject::No source";
      break;
    }
    if (!dv->GetString("worker", &ret->r_worker_path_))
    {
      LOG(ERROR) << "DPEProject::No worker";
      break;
    }
    if (!dv->GetString("sink", &ret->r_sink_path_))
    {
      LOG(ERROR) << "DPEProject::No sink";
      break;
    }

    ret->source_path_ = ret->job_home_path_.Append(base::UTF8ToNative(ret->r_source_path_));
    ret->worker_path_ = ret->job_home_path_.Append(base::UTF8ToNative(ret->r_worker_path_));
    ret->sink_path_ = ret->job_home_path_.Append(base::UTF8ToNative(ret->r_sink_path_));

    if (!base::ReadFileToString(ret->source_path_, &ret->source_data_))
    {
      LOG(ERROR) << "DPEProject::ReadFileToString failed: source";
      break;
    }
    if (!base::ReadFileToString(ret->worker_path_, &ret->worker_data_))
    {
      LOG(ERROR) << "DPEProject::ReadFileToString failed: worker";
      break;
    }
    if (!base::ReadFileToString(ret->sink_path_, &ret->sink_data_))
    {
      LOG(ERROR) << "DPEProject::SReadFileToString failed: sink";
      break;
    }

    ok = true;
  } while (false);

  delete project;
  if (!ok) return NULL;
  return ret;
}
}