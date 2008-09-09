#include <algorithm>
#include <iostream>

#include <arc/ArcConfig.h>
#include <arc/FileLock.h>
#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/loader/Loader.h>
#include <arc/client/JobController.h>
#include <arc/client/JobSupervisor.h>
#include <arc/client/ClientInterface.h>
#include <arc/client/UserConfig.h>

namespace Arc {

  Logger JobSupervisor::logger(Logger::getRootLogger(), "JobSupervisor");
  
  JobSupervisor::JobSupervisor(const UserConfig& ucfg,
			       const std::list<std::string>& jobs,
			       const std::list<std::string>& clusterselect,
			       const std::list<std::string>& clusterreject,
			       const std::string joblist){

    Config JobIdStorage;
    if(!joblist.empty()){
      FileLock lock(joblist);
      JobIdStorage.ReadFromFile(joblist);
    }

    std::list<std::string> NeededControllers;

    //Need to fix below sorting on clusterselect and clusterreject

    //First, if jobids are specified, only load JobControllers needed by those jobs
    if (!jobs.empty()) {
      logger.msg(DEBUG, "Identifying needed JobControllers according to "
		 "specified job ids");

      for (std::list<std::string>::const_iterator it = jobs.begin();
	   it != jobs.end(); it++) {
	std::list<XMLNode> XMLJobs =
	  JobIdStorage.XPathLookup("//Job[JobID='"+ *it+"']", NS());
	if(!XMLJobs.empty()) {
	  XMLNode &ThisXMLJob = *XMLJobs.begin();
	  if (std::find(NeededControllers.begin(), NeededControllers.end(),
			(std::string) ThisXMLJob["Flavour"]) == NeededControllers.end()){
	    std::string flavour = (std::string) ThisXMLJob["Flavour"];
	    logger.msg(DEBUG, "Need jobController for grid flavour %s", flavour);	
	    NeededControllers.push_back(flavour);
	  }  
	} else{
	  std::cout<<IString("Job Id = %s not found", (*it))<<std::endl;
	}
      }
    }
    else { //load controllers for all grid flavours present in joblist

      logger.msg(DEBUG, "Identifying needed JobControllers according to all jobs present in joblist");

      XMLNodeList ActiveJobs =
	JobIdStorage.XPathLookup("/ArcConfig/Job", NS());

      for (XMLNodeList::iterator JobIter = ActiveJobs.begin();
	   JobIter != ActiveJobs.end(); JobIter++) {
	if (std::find(NeededControllers.begin(), NeededControllers.end(),
		      (std::string)(*JobIter)["Flavour"]) == NeededControllers.end()){
	  std::string flavour = (*JobIter)["Flavour"];
	  logger.msg(DEBUG, "Need jobController for grid flavour %s", flavour);	
	  NeededControllers.push_back((std::string)(*JobIter)["Flavour"]);
	}
      }
    }

    ACCConfig acccfg;
    NS ns;
    Config mcfg(ns);
    acccfg.MakeConfig(mcfg);
    int JobControllerNumber = 0;

    for (std::list<std::string>::const_iterator it = NeededControllers.begin();
	 it != NeededControllers.end(); it++) {
      XMLNode ThisJobController = mcfg.NewChild("ArcClientComponent");
      ThisJobController.NewAttribute("name") = "JobController" + (*it);
      ThisJobController.NewAttribute("id") = "controller" + tostring(JobControllerNumber);
      ucfg.ApplySecurity(ThisJobController);
      ThisJobController.NewChild("joblist") = joblist;
      JobControllerNumber++;
    }

    loader = new Loader(&mcfg);

    for(int i = 0; i < JobControllerNumber; i++){
      JobController *JC = dynamic_cast<JobController*>(loader->getACC("controller" + tostring(i)));
      if(JC) {
	JobControllers.push_back(JC);
	(*JobControllers.rbegin())->FillJobStore(jobs, clusterselect, clusterreject);
      }
    }
  }

  JobSupervisor::~JobSupervisor() {
    if (loader)
      delete loader;
  }

} // namespace Arc
