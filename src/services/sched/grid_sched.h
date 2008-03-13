#ifndef __ARC_SCHED_H__
#define __ARC_SCHED_H__

#include <arc/message/Service.h>
#include <arc/delegation/DelegationInterface.h>
#include <arc/infosys/InformationInterface.h>
#include "job_queue.h"
#include "resources_handling.h"


namespace GridScheduler {

// Sched job queue initializator

std::string make_uuid();

class GridSchedulerService: public Arc::Service {
    protected:
        std::string db_path;
        std::string endpoint;
        int period;
        int timeout;
        Arc::NS ns_;
        Arc::Logger logger_;
        Arc::DelegationContainerSOAP delegations_;
        Arc::InformationContainer infodoc_;
        // BES Interface
        Arc::MCC_Status CreateActivity(Arc::XMLNode &in, Arc::XMLNode &out);
        Arc::MCC_Status GetActivityStatuses(Arc::XMLNode &in, Arc::XMLNode &out);
        Arc::MCC_Status TerminateActivities(Arc::XMLNode &in, Arc::XMLNode &out);
        
        Arc::MCC_Status GetFactoryAttributesDocument(Arc::XMLNode &in, 
                                                     Arc::XMLNode &out);
        Arc::MCC_Status StopAcceptingNewActivities(Arc::XMLNode &in, 
                                                   Arc::XMLNode &out);
        Arc::MCC_Status StartAcceptingNewActivities(Arc::XMLNode &in, 
                                                    Arc::XMLNode &out);
        Arc::MCC_Status ChangeActivityStatus(Arc::XMLNode &in, 
                                             Arc::XMLNode &out);
        // iBES Interface
        Arc::MCC_Status GetActivities(Arc::XMLNode &in, Arc::XMLNode &out);
        Arc::MCC_Status ReportActivityStatus(Arc::XMLNode &in, Arc::XMLNode &out);
        Arc::MCC_Status GetActivityStatusChanges(Arc::XMLNode &in, Arc::XMLNode &out);

        // WS-Propoerty Interface
        Arc::MCC_Status GetActivityDocuments(Arc::XMLNode &in, 
                                             Arc::XMLNode &out);
        // Fault handlers
        Arc::MCC_Status make_response(Arc::Message& outmsg);
        Arc::MCC_Status make_fault(Arc::Message& outmsg);
        Arc::MCC_Status make_soap_fault(Arc::Message& outmsg);
        bool IsAcceptingNewActivities;
    public:
        JobQueue sched_queue;
        ResourcesHandling sched_resources;
        GridSchedulerService(Arc::Config *cfg);
        std::string getEndPoint(void) { return endpoint;};
        std::string getDBPath(void) { return db_path;};
        int getPeriod(void) { return period;};
        virtual ~GridSchedulerService(void);
        virtual Arc::MCC_Status process(Arc::Message& inmsg,
                                        Arc::Message& outmsg);
        void InformationCollector(void);
}; // class GridSchedulerService

} // namespace GridScheduler

#endif

