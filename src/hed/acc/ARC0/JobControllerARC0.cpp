// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <map>
#include <glibmm.h>

#include <arc/data/DataBuffer.h>
#include <arc/data/DataHandle.h>
#include <arc/data/DataMover.h>
#include <arc/data/URLMap.h>
#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/Utils.h>

#ifdef WIN32
#include <arc/win32.h>
#define _NO_OLDNAMES
#endif


#include "JobControllerARC0.h"
#include "FTPControl.h"

namespace Arc {

  Logger JobControllerARC0::logger(JobController::logger, "ARC0");

  JobControllerARC0::JobControllerARC0(Config *cfg)
    : JobController(cfg, "ARC0") {}

  JobControllerARC0::~JobControllerARC0() {}

  Plugin* JobControllerARC0::Instance(PluginArgument *arg) {
    ACCPluginArgument *accarg = dynamic_cast<ACCPluginArgument*>(arg);
    if (!accarg)
      return NULL;
    return new JobControllerARC0((Config*)(*accarg));
  }

  void JobControllerARC0::GetJobInformation() {

    std::map<std::string, std::list<Job*> > jobsbyhost;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++)
      jobsbyhost[it->InfoEndpoint.ConnectionURL() + '/' +
                 it->InfoEndpoint.Path()].push_back(&*it);

    for (std::map<std::string, std::list<Job*> >::iterator hostit =
           jobsbyhost.begin(); hostit != jobsbyhost.end(); hostit++) {

      URL infourl = (*hostit->second.begin())->InfoEndpoint;

      // merge filters
      std::string filter = "(|";
      for (std::list<Job*>::iterator it = hostit->second.begin();
           it != hostit->second.end(); it++)
        filter += (*it)->InfoEndpoint.LDAPFilter();
      filter += ")";

      infourl.ChangeLDAPFilter(filter);

      DataHandle handler(infourl);
      DataBuffer buffer;

      if (!handler) {
        logger.msg(ERROR, "Can't create information handle - "
                   "is the ARC ldap DMC available?");
        return;
      }

      if (!handler->StartReading(buffer))
        return;

      int handle;
      unsigned int length;
      unsigned long long int offset;
      std::string result;

      while (buffer.for_write() || !buffer.eof_read())
        if (buffer.for_write(handle, length, offset, true)) {
          result.append(buffer[handle], length);
          buffer.is_written(handle);
        }

      if (!handler->StopReading())
        return;

      XMLNode xmlresult(result);

      for (std::list<Job*>::iterator it = hostit->second.begin();
           it != hostit->second.end(); it++) {

        XMLNodeList jobinfolist =
          xmlresult.XPathLookup("//nordugrid-job-globalid"
                                "[nordugrid-job-globalid='" +
                                (*it)->JobID.str() + "']", NS());

        if (jobinfolist.empty())
          continue;

        XMLNode& jobinfo = *jobinfolist.begin();

        if (jobinfo["nordugrid-job-status"])
          (*it)->State = (std::string)jobinfo["nordugrid-job-status"];
        if (jobinfo["nordugrid-job-globalowner"])
          (*it)->Owner = (std::string)jobinfo["nordugrid-job-globalowner"];
        if (jobinfo["nordugrid-job-execcluster"])
          (*it)->ExecutionCE =
            (std::string)jobinfo["nordugrid-job-execcluster"];
        if (jobinfo["nordugrid-job-execqueue"])
          (*it)->Queue = (std::string)jobinfo["nordugrid-job-execqueue"];
        if (jobinfo["nordugrid-job-submissionui"])
          (*it)->SubmissionHost =
            (std::string)jobinfo["nordugrid-job-submissionui"];
        if (jobinfo["nordugrid-job-submissiontime"])
          (*it)->SubmissionTime =
            (std::string)jobinfo["nordugrid-job-submissiontime"];
        if (jobinfo["nordugrid-job-sessiondirerasetime"])
          (*it)->WorkingAreaEraseTime =
            (std::string)jobinfo["nordugrid-job-sessiondirerasetime"];
        if (jobinfo["nordugrid-job-proxyexpirationtime"])
          (*it)->ProxyExpirationTime =
            (std::string)jobinfo["nordugrid-job-proxyexpirationtime"];
        if (jobinfo["nordugrid-job-completiontime"])
          (*it)->EndTime =
            (std::string)jobinfo["nordugrid-job-completiontime"];
        if (jobinfo["nordugrid-job-cpucount"])
          (*it)->UsedSlots = stringtoi(jobinfo["nordugrid-job-cpucount"]);
        if (jobinfo["nordugrid-job-usedcputime"])
          (*it)->UsedTotalCPUTime =
            (std::string)jobinfo["nordugrid-job-usedcputime"];
        if (jobinfo["nordugrid-job-usedwalltime"])
          (*it)->UsedWallTime =
            (std::string)jobinfo["nordugrid-job-usedwalltime"];
        if (jobinfo["nordugrid-job-exitcode"])
          (*it)->ExitCode = stringtoi(jobinfo["nordugrid-job-exitcode"]);
        if (jobinfo["Mds-validfrom"]) {
          (*it)->CreationTime = (std::string)(jobinfo["Mds-validfrom"]);
          if (jobinfo["Mds-validto"]) {
            Time Validto = (std::string)(jobinfo["Mds-validto"]);
            (*it)->Validity = Validto - (*it)->CreationTime;
          }
        }
        if (jobinfo["nordugrid-job-stdout"])
          (*it)->StdOut = (std::string)(jobinfo["nordugrid-job-stdout"]);
        if (jobinfo["nordugrid-job-stderr"])
          (*it)->StdErr = (std::string)(jobinfo["nordugrid-job-stderr"]);
        if (jobinfo["nordugrid-job-stdin"])
          (*it)->StdIn = (std::string)(jobinfo["nordugrid-job-stdin"]);
        if (jobinfo["nordugrid-job-reqcputime"])
          (*it)->RequestedTotalCPUTime =
            (std::string)(jobinfo["nordugrid-job-reqcputime"]);
        if (jobinfo["nordugrid-job-reqwalltime"])
          (*it)->RequestedWallTime =
            (std::string)(jobinfo["nordugrid-job-reqwalltime"]);
        if (jobinfo["nordugrid-job-rerunable"])
          (*it)->RestartState =
            (std::string)(jobinfo["nordugrid-job-rerunable"]);
        if (jobinfo["nordugrid-job-queuerank"])
          (*it)->WaitingPosition =
            stringtoi(jobinfo["nordugrid-job-queuerank"]);
        if (jobinfo["nordugrid-job-comment"])
          (*it)->OtherMessages =
            (std::string)(jobinfo["nordugrid-job-comment"]);
        if (jobinfo["nordugrid-job-usedmem"])
          (*it)->UsedMainMemory =
            stringtoi(jobinfo["nordugrid-job-usedmem"]);
        if (jobinfo["nordugrid-job-errors"])
          for (XMLNode n = jobinfo["nordugrid-job-errors"]; n; ++n)
            (*it)->Error.push_back((std::string)n);
        if (jobinfo["nordugrid-job-jobname"])
          (*it)->Name = (std::string)(jobinfo["nordugrid-job-jobname"]);
        if (jobinfo["nordugrid-job-gmlog"])
          (*it)->LogDir = (std::string)(jobinfo["nordugrid-job-gmlog"]);
        if (jobinfo["nordugrid-job-clientsofware"])
          (*it)->SubmissionClientName =
            (std::string)(jobinfo["nordugrid-job-clientsoftware"]);
        if (jobinfo["nordugrid-job-executionnodes"])
          for (XMLNode n = jobinfo["nordugrid-job-executionnodes"]; n; ++n)
            (*it)->ExecutionNode.push_back((std::string)n);
        if (jobinfo["nordugrid-job-runtimeenvironment"])
          for (XMLNode n = jobinfo["nordugrid-job-runtimeenvironment"]; n; ++n)
            (*it)->UsedApplicationEnvironment.push_back((std::string)n);
      }
    }
  }

  bool JobControllerARC0::GetJob(const Job& job,
                                 const std::string& downloaddir) {

    logger.msg(DEBUG, "Downloading job: %s", job.JobID.str());

    std::string path = job.JobID.Path();
    std::string::size_type pos = path.rfind('/');
    std::string jobidnum = path.substr(pos + 1);

    std::list<std::string> files = GetDownloadFiles(job.JobID);

    URL src(job.JobID);
    URL dst(downloaddir.empty() ? jobidnum : downloaddir + '/' + jobidnum);

    std::string srcpath = src.Path();
    std::string dstpath = dst.Path();

    if (srcpath[srcpath.size() - 1] != '/')
      srcpath += '/';
    if (dstpath[dstpath.size() - 1] != '/')
      dstpath += '/';

    bool ok = true;

    for (std::list<std::string>::iterator it = files.begin();
         it != files.end(); it++) {
      src.ChangePath(srcpath + *it);
      dst.ChangePath(dstpath + *it);
      if (!ARCCopyFile(src, dst)) {
        logger.msg(ERROR, "Failed dowloading %s to %s", src.str(), dst.str());
        ok = false;
      }
    }

    return ok;
  }

  bool JobControllerARC0::CleanJob(const Job& job, bool force) {

    logger.msg(DEBUG, "Cleaning job: %s", job.JobID.str());

    FTPControl ctrl;
    if (!ctrl.Connect(job.JobID, proxyPath, certificatePath, keyPath, timeout)) {
      logger.msg(ERROR, "Failed to connect for job cleaning");
      return false;
    }

    std::string path = job.JobID.Path();
    std::string::size_type pos = path.rfind('/');
    std::string jobpath = path.substr(0, pos);
    std::string jobidnum = path.substr(pos + 1);

    if (!ctrl.SendCommand("CWD " + jobpath, timeout)) {
      logger.msg(ERROR, "Failed sending CWD command for job cleaning");
      return false;
    }

    if (!ctrl.SendCommand("RMD " + jobidnum, timeout)) {
      logger.msg(ERROR, "Failed sending RMD command for job cleaning");
      return false;
    }

    if (!ctrl.Disconnect(timeout)) {
      logger.msg(ERROR, "Failed to disconnect after job cleaning");
      return false;
    }

    logger.msg(DEBUG, "Job cleaning successful");

    return true;

  }

  bool JobControllerARC0::CancelJob(const Job& job) {

    logger.msg(DEBUG, "Cleaning job: %s", job.JobID.str());

    FTPControl ctrl;
    if (!ctrl.Connect(job.JobID, proxyPath, certificatePath, keyPath, timeout)) {
      logger.msg(ERROR, "Failed to connect for job cleaning");
      return false;
    }

    std::string path = job.JobID.Path();
    std::string::size_type pos = path.rfind('/');
    std::string jobpath = path.substr(0, pos);
    std::string jobidnum = path.substr(pos + 1);

    if (!ctrl.SendCommand("CWD " + jobpath, timeout)) {
      logger.msg(ERROR, "Failed sending CWD command for job cancelling");
      return false;
    }

    if (!ctrl.SendCommand("DELE " + jobidnum, timeout)) {
      logger.msg(ERROR, "Failed sending DELE command for job cancelling");
      return false;
    }

    if (!ctrl.Disconnect(timeout)) {
      logger.msg(ERROR, "Failed to disconnect after job cancelling");
      return false;
    }

    logger.msg(DEBUG, "Job cancelling successful");

    return true;
  }

  bool JobControllerARC0::RenewJob(const Job& job){
    
    logger.msg(DEBUG, "Renewing credentials for job: %s", job.JobID.str());

    FTPControl ctrl;
    if (!ctrl.Connect(job.JobID, proxyPath, certificatePath, keyPath, timeout)) {
      logger.msg(ERROR, "Failed to connect for credential renewal");
      return false;
    }

    std::string path = job.JobID.Path();
    std::string::size_type pos = path.rfind('/');
    std::string jobpath = path.substr(0, pos);
    std::string jobidnum = path.substr(pos + 1);

    if (!ctrl.SendCommand("CWD " + jobpath, timeout)) {
      logger.msg(ERROR, "Failed sending CWD command for credentials renewal");
      return false;
    }

    if (!ctrl.SendCommand("CWD " + jobidnum, timeout)) {
      logger.msg(ERROR, "Failed sending CWD command for credentials renewal");
      return false;
    }

    if (!ctrl.Disconnect(timeout)) {
      logger.msg(ERROR, "Failed to disconnect after credentials renewal");
      return false;
    }

    logger.msg(DEBUG, "Renewal of credentials was successful");
    

    return true;
  }

  bool JobControllerARC0::ResumeJob(const Job& job){

    std::cout << "Resuming job "<<job.JobID.str()<<" at state "<< job.RestartState << std::endl;
    if (job.RestartState.empty()){
      logger.msg(ERROR, "Job %s does not report a resumable state",job.JobID.str());
      return false;
    }
     
    RenewJob(job);

    // dump rsl into temporary file
    std::string urlstr = job.JobID.str();
    std::string::size_type pos = urlstr.rfind('/');
    if (pos==std::string::npos || pos==0)
      logger.msg(ERROR,"Illegal jobid specified");
    std::string jobnr = urlstr.substr(pos+1);
    urlstr = urlstr.substr(0, pos) + "/new/action";
    logger.msg(DEBUG, "HER: %s",urlstr);
    
    std::string rsl("&(action=restart)(jobid=" + jobnr + ")");

    std::string filename = Glib::build_filename(Glib::get_tmp_dir(), "arcresume.XXXXXX");
    int tmp_h = Glib::mkstemp(filename);
    if (tmp_h == -1) {
      logger.msg(ERROR,"Could not create temporary file: %s", filename);
      return false;
    }   
    std::ofstream outfile(filename.c_str(), std::ofstream::binary);
    outfile.write(rsl.c_str(), rsl.size());
    if (outfile.fail()) {
      logger.msg(ERROR,"Could not write temporary file: %s", filename);
      return false;
    }   
    outfile.close();
    
    // Send temporary file to cluster
    DataMover mover;
    FileCache cache;
    std::string failure;
    URL source_url(filename);
    URL dest_url(urlstr);
    DataHandle source(source_url);
    DataHandle destination(dest_url);
    source->AssignCredentials(proxyPath, certificatePath, keyPath, caCertificatesDir);
    source->SetTries(1);
    destination->AssignCredentials(proxyPath, certificatePath, keyPath, caCertificatesDir);
    destination->SetTries(1);
    if (!mover.Transfer(*source, *destination, cache, URLMap(),
                        0, 0, 0, timeout, failure)) {
      if (!failure.empty())
        logger.msg(INFO, "Current transfer FAILED: %s", failure);
      else
        logger.msg(INFO, "Current transfer FAILED");
      mover.Delete(*destination);
      return false;
    }
    else
      logger.msg(INFO, "Current transfer complete");

    //Cleaning up
    source->Remove();

    logger.msg(DEBUG, "Job resumed successful");

    return true;
  }

  bool JobControllerARC0::PatchInputFileLocation(const Job& job, JobDescription& jobDesc) const {
    return false;
  }

  URL JobControllerARC0::GetFileUrlForJob(const Job& job,
                                          const std::string& whichfile) {

    URL url(job.JobID);

    if (whichfile == "stdout")
      url.ChangePath(url.Path() + '/' + job.StdOut);
    else if (whichfile == "stderr")
      url.ChangePath(url.Path() + '/' + job.StdErr);
    else if (whichfile == "gmlog") {
      std::string path = url.Path();
      path.insert(path.rfind('/'), "/info");
      url.ChangePath(path + "/errors");
    }

    return url;
  }

  bool JobControllerARC0::GetJobDescription(const Job& job, std::string& desc_str) {

    std::string jobid = job.JobID.str();
    logger.msg(INFO, "Trying to retrieve job description"
               "of %s from cluster", jobid);

    std::string::size_type pos = jobid.rfind("/");
    if (pos == std::string::npos) {
      logger.msg(ERROR, "invalid jobid: %s", jobid);
      return false;
    }
    std::string cluster = jobid.substr(0, pos);
    std::string shortid = jobid.substr(pos + 1);

    // Transfer job description
    DataMover mover;
    mover.secure(false);
    mover.passive(true);
    mover.verbose(false);
    mover.force_to_meta(false);
    mover.retry(true);
    FileCache cache;
    std::string failure;
    URL source_url(cluster + "/info/" + shortid + "/description");
    std::string localfile = "/tmp/" + shortid + "/description";
    URL dest_url(localfile);
    DataHandle source(source_url);
    DataHandle destination(dest_url);
    source->AssignCredentials(proxyPath, certificatePath, keyPath, caCertificatesDir);
    source->SetTries(1);
    destination->AssignCredentials(proxyPath, certificatePath, keyPath, caCertificatesDir);
    destination->SetTries(1);
    if (!mover.Transfer(*source, *destination, cache, URLMap(),
                        0, 0, 0, timeout, failure)) {
      if (!failure.empty())
        logger.msg(INFO, "Current transfer FAILED: %s", failure);
      else
        logger.msg(INFO, "Current transfer FAILED");
      mover.Delete(*destination);
      return false;
    }
    else
      logger.msg(INFO, "Current transfer complete");

    // Read job description from file
    std::ifstream descriptionfile(localfile.c_str());

    if (!descriptionfile) {
      logger.msg(ERROR, "Can not open job description file: %s", localfile);
      return false;
    }

    descriptionfile.seekg(0, std::ios::end);
    std::streamsize length = descriptionfile.tellg();
    descriptionfile.seekg(0, std::ios::beg);

    char *buffer = new char[length + 1];
    descriptionfile.read(buffer, length);
    descriptionfile.close();

    buffer[length] = '\0';

    desc_str = (std::string)buffer;
    //Cleaning up
    delete[] buffer;
    destination->Remove();

    // Extracting original client xrsl
    pos = desc_str.find("clientxrsl");
    if (pos != std::string::npos) {
      logger.msg(DEBUG, "clientxrsl found");
      std::string::size_type pos1 = desc_str.find("&", pos);
      if (pos1 == std::string::npos) {
        logger.msg(ERROR, "could not find start of clientxrsl");
        return false;
      }
      std::string::size_type pos2 = desc_str.find(")\"", pos1);
      if (pos2 == std::string::npos) {
        logger.msg(ERROR, "could not find end of clientxrsl");
        return false;
      }
      desc_str.erase(pos2 + 1);
      desc_str.erase(0, pos1);
      for (std::string::size_type i = 0; i != std::string::npos;) {
        i = desc_str.find("\"\"", i);
        if (i != std::string::npos)
          desc_str.erase(i, 1);
      }
      logger.msg(VERBOSE, "Job description:%s", desc_str);
    }
    else {
      logger.msg(ERROR, "clientxrsl not found");
      return false;
    }
    JobDescription desc;
    desc.Parse(desc_str);
    if (!desc) {
      logger.msg(ERROR, "Invalid JobDescription:");
      std::cout << desc_str << std::endl;
      return false;
    }
    logger.msg(INFO, "Valid JobDescription found");
    return true;
  }

} // namespace Arc
