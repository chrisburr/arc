#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

#include <arc/loader/Loader.h>
#include <arc/message/MCCLoader.h>
#include <arc/message/Plexer.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/PayloadStream.h>
#include <arc/message/SecAttr.h>
#include <arc/ws-addressing/WSA.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>
#include <arc/FileUtils.h>
#include <arc/Utils.h>
#include "job.h"
#include "grid-manager/log/JobLog.h"
#include "grid-manager/run/RunPlugin.h"
#include "arex.h"

namespace ARex {

#define DEFAULT_INFOPROVIDER_WAKEUP_PERIOD (600)
#define DEFAULT_INFOSYS_MAX_CLIENTS (1)
#define DEFAULT_JOBCONTROL_MAX_CLIENTS (100)
#define DEFAULT_DATATRANSFER_MAX_CLIENTS (100)
 
static const std::string BES_FACTORY_ACTIONS_BASE_URL("http://schemas.ggf.org/bes/2006/08/bes-factory/BESFactoryPortType/");
static const std::string BES_FACTORY_NPREFIX("bes-factory");
static const std::string BES_FACTORY_NAMESPACE("http://schemas.ggf.org/bes/2006/08/bes-factory");

static const std::string BES_MANAGEMENT_ACTIONS_BASE_URL("http://schemas.ggf.org/bes/2006/08/bes-management/BESManagementPortType/");
static const std::string BES_MANAGEMENT_NPREFIX("bes-management");
static const std::string BES_MANAGEMENT_NAMESPACE("http://schemas.ggf.org/bes/2006/08/bes-management");

static const std::string BES_ARC_NPREFIX("a-rex");
static const std::string BES_ARC_NAMESPACE("http://www.nordugrid.org/schemas/a-rex");

static const std::string DELEG_ARC_NPREFIX("arcdeleg");
static const std::string DELEG_ARC_NAMESPACE("http://www.nordugrid.org/schemas/delegation");

static const std::string BES_GLUE2PRE_NPREFIX("glue2pre");
static const std::string BES_GLUE2PRE_NAMESPACE("http://schemas.ogf.org/glue/2008/05/spec_2.0_d41_r01");

static const std::string BES_GLUE2_NPREFIX("glue2");
static const std::string BES_GLUE2_NAMESPACE("http://schemas.ogf.org/glue/2009/03/spec/2/0");

static const std::string BES_GLUE2D_NPREFIX("glue2d");
static const std::string BES_GLUE2D_NAMESPACE("http://schemas.ogf.org/glue/2009/03/spec_2.0_r1");

static const std::string ES_TYPES_NPREFIX("estypes");
static const std::string ES_TYPES_NAMESPACE("http://www.eu-emi.eu/es/2010/12/types");

static const std::string ES_CREATE_NPREFIX("escreate");
static const std::string ES_CREATE_NAMESPACE("http://www.eu-emi.eu/es/2010/12/creation/types");

static const std::string ES_DELEG_NPREFIX("esdeleg");
static const std::string ES_DELEG_NAMESPACE("http://www.eu-emi.eu/es/2010/12/delegation/types");

static const std::string ES_RINFO_NPREFIX("esrinfo");
static const std::string ES_RINFO_NAMESPACE("http://www.eu-emi.eu/es/2010/12/resourceinfo/types");

static const std::string ES_MANAG_NPREFIX("esmanag");
static const std::string ES_MANAG_NAMESPACE("http://www.eu-emi.eu/es/2010/12/activitymanagement/types");

static const std::string ES_AINFO_NPREFIX("esainfo");
static const std::string ES_AINFO_NAMESPACE("http://www.eu-emi.eu/es/2010/12/activity/types");

static const std::string WSRF_NAMESPACE("http://docs.oasis-open.org/wsrf/rp-2");

#define AREX_POLICY_OPERATION_URN "http://www.nordugrid.org/schemas/policy-arc/types/a-rex/operation"
#define AREX_POLICY_OPERATION_ADMIN "Admin"
#define AREX_POLICY_OPERATION_INFO  "Info"

// Id: http://www.nordugrid.org/schemas/policy-arc/types/arex/joboperation
// Value: 
//        Create - creation of new job
//        Modify - modification of job paramaeters - change state, write data.
//        Read   - accessing job information - get status information, read data.
// Id: http://www.nordugrid.org/schemas/policy-arc/types/arex/operation
// Value: 
//        Admin  - administrator level operation
//        Info   - information about service

class ARexSecAttr: public Arc::SecAttr {
 public:
  ARexSecAttr(const std::string& action);
  ARexSecAttr(const Arc::XMLNode op);
  virtual ~ARexSecAttr(void);
  virtual operator bool(void) const;
  virtual bool Export(Arc::SecAttrFormat format,Arc::XMLNode &val) const;
  virtual std::string get(const std::string& id) const;
 protected:
  std::string action_;
  std::string id_;
  virtual bool equal(const Arc::SecAttr &b) const;
};

ARexSecAttr::ARexSecAttr(const std::string& action) {
  id_=JOB_POLICY_OPERATION_URN;
  action_=action;
}

ARexSecAttr::ARexSecAttr(const Arc::XMLNode op) {
  if(MatchXMLNamespace(op,BES_FACTORY_NAMESPACE)) {
    if(MatchXMLName(op,"CreateActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_CREATE;
    } else if(MatchXMLName(op,"GetActivityStatuses")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    } else if(MatchXMLName(op,"TerminateActivities")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"GetActivityDocuments")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    } else if(MatchXMLName(op,"GetFactoryAttributesDocument")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_INFO;
    }
  } else if(MatchXMLNamespace(op,BES_MANAGEMENT_NAMESPACE)) {
    if(MatchXMLName(op,"StopAcceptingNewActivities")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_ADMIN;
    } else if(MatchXMLName(op,"StartAcceptingNewActivities")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_ADMIN;
    }
  } else if(MatchXMLNamespace(op,BES_ARC_NAMESPACE)) {
    if(MatchXMLName(op,"ChangeActivityStatus")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"MigrateActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"CacheCheck")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_INFO;
    }
  } else if(MatchXMLNamespace(op,DELEG_ARC_NAMESPACE)) {
    if(MatchXMLName(op,"DelegateCredentialsInit")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_CREATE;
    } else if(MatchXMLName(op,"UpdateCredentials")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    }
  } else if(MatchXMLNamespace(op,WSRF_NAMESPACE)) {
    id_=AREX_POLICY_OPERATION_URN;
    action_=AREX_POLICY_OPERATION_INFO;
  } else if(MatchXMLNamespace(op,ES_CREATE_NAMESPACE)) {
    if(MatchXMLName(op,"CreateActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_CREATE;
    }
  } else if(MatchXMLNamespace(op,ES_DELEG_NAMESPACE)) {
    if(MatchXMLName(op,"InitDelegation")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_CREATE;
    } else if(MatchXMLName(op,"PutDelegation")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"GetDelegationInfo")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    }
  } else if(MatchXMLNamespace(op,ES_RINFO_NAMESPACE)) {
    if(MatchXMLName(op,"GetResourceInfo")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_INFO;
    } else if(MatchXMLName(op,"QueryResourceInfo")) {
      id_=AREX_POLICY_OPERATION_URN;
      action_=AREX_POLICY_OPERATION_INFO;
    }
  } else if(MatchXMLNamespace(op,ES_MANAG_NAMESPACE)) {
    if(MatchXMLName(op,"PauseActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"ResumeActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"ResumeActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"NotifyService")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"CancelActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"WipeActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"RestartActivity")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_MODIFY;
    } else if(MatchXMLName(op,"GetActivityStatus")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    } else if(MatchXMLName(op,"GetActivityInfo")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    }
  } else if(MatchXMLNamespace(op,ES_AINFO_NAMESPACE)) {
    if(MatchXMLName(op,"ListActivities")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    } else if(MatchXMLName(op,"GetActivityStatus")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    } else if(MatchXMLName(op,"GetActivityInfo")) {
      id_=JOB_POLICY_OPERATION_URN;
      action_=JOB_POLICY_OPERATION_READ;
    }
  }
}

ARexSecAttr::~ARexSecAttr(void) {
}

ARexSecAttr::operator bool(void) const {
  return !action_.empty();
}

bool ARexSecAttr::equal(const SecAttr &b) const {
  try {
    const ARexSecAttr& a = (const ARexSecAttr&)b;
    return ((id_ == a.id_) && (action_ == a.action_));
  } catch(std::exception&) { };
  return false;
}

bool ARexSecAttr::Export(Arc::SecAttrFormat format,Arc::XMLNode &val) const {
  if(format == UNDEFINED) {
  } else if(format == ARCAuth) {
    Arc::NS ns;
    ns["ra"]="http://www.nordugrid.org/schemas/request-arc";
    val.Namespaces(ns); val.Name("ra:Request");
    Arc::XMLNode item = val.NewChild("ra:RequestItem");
    if(!action_.empty()) {
      Arc::XMLNode action = item.NewChild("ra:Action");
      action=action_;
      action.NewAttribute("Type")="string";
      action.NewAttribute("AttributeId")=id_;
    };
    return true;
  } else {
  };
  return false;
}

std::string ARexSecAttr::get(const std::string& id) const {
  if(id == "ACTION") return action_;
  if(id == "NAMESPACE") return id_;
  return "";
};

static Arc::XMLNode BESFactoryResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(BES_FACTORY_NPREFIX + ":" + opname + "Response");
  Arc::WSAHeader(res).Action(BES_FACTORY_ACTIONS_BASE_URL + opname + "Response");
  return response;
}

static Arc::XMLNode BESManagementResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(BES_MANAGEMENT_NPREFIX + ":" + opname + "Response");
  Arc::WSAHeader(res).Action(BES_MANAGEMENT_ACTIONS_BASE_URL + opname + "Response");
  return response;
}

static Arc::XMLNode BESARCResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(BES_ARC_NPREFIX + ":" + opname + "Response");
  return response;
}

static Arc::XMLNode ESCreateResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(ES_CREATE_NPREFIX + ":" + opname + "Response");
  return response;
}

/*
static Arc::XMLNode ESDelegResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(ES_DELEG_NPREFIX + ":" + opname + "Response");
  return response;
}
*/

static Arc::XMLNode ESRInfoResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(ES_RINFO_NPREFIX + ":" + opname + "Response");
  return response;
}

static Arc::XMLNode ESManagResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(ES_MANAG_NPREFIX + ":" + opname + "Response");
  return response;
}

static Arc::XMLNode ESAInfoResponse(Arc::PayloadSOAP& res,const char* opname) {
  Arc::XMLNode response = res.NewChild(ES_AINFO_NPREFIX + ":" + opname + "Response");
  return response;
}

//static Arc::LogStream logcerr(std::cerr);

static Arc::Plugin* get_service(Arc::PluginArgument* arg) {
    Arc::ServicePluginArgument* srvarg =
            arg?dynamic_cast<Arc::ServicePluginArgument*>(arg):NULL;
    if(!srvarg) return NULL;
    ARexService* arex = new ARexService((Arc::Config*)(*srvarg),arg);
    if(!*arex) { delete arex; arex=NULL; };
    return arex;
}

class ARexConfigContext:public Arc::MessageContextElement, public ARexGMConfig {
 public:
  ARexConfigContext(GMConfig& config,const std::string& uname,const std::string& grid_name,const std::string& service_endpoint):
    ARexGMConfig(config,uname,grid_name,service_endpoint) { };
  virtual ~ARexConfigContext(void) { };
};

void CountedResource::Acquire(void) {
  lock_.lock();
  while((limit_ >= 0) && (count_ >= limit_)) {
    cond_.wait(lock_);
  };
  ++count_;
  lock_.unlock();
}

void CountedResource::Release(void) {
  lock_.lock();
  --count_;
  cond_.signal();
  lock_.unlock();
}

void CountedResource::MaxConsumers(int maxconsumers) {
  limit_ = maxconsumers;
}

CountedResource::CountedResource(int maxconsumers):
                        limit_(maxconsumers),count_(0) {
}

CountedResource::~CountedResource(void) {
}

class CountedResourceLock {
 private:
  CountedResource& r_;
 public:
  CountedResourceLock(CountedResource& resource):r_(resource) {
    r_.Acquire();
  };
  ~CountedResourceLock(void) {
    r_.Release();
  };
};

static std::string GetPath(std::string url){
  std::string::size_type ds, ps;
  ds=url.find("//");
  if (ds==std::string::npos)
    ps=url.find("/");
  else
    ps=url.find("/", ds+2);
  if (ps==std::string::npos)
    return "";
  else
    return url.substr(ps);
}


Arc::MCC_Status ARexService::StopAcceptingNewActivities(ARexGMConfig& /*config*/,Arc::XMLNode /*in*/,Arc::XMLNode /*out*/) {
  return Arc::MCC_Status();
}

Arc::MCC_Status ARexService::StartAcceptingNewActivities(ARexGMConfig& /*config*/,Arc::XMLNode /*in*/,Arc::XMLNode /*out*/) {
  return Arc::MCC_Status();
}

Arc::MCC_Status ARexService::make_soap_fault(Arc::Message& outmsg, const char* resp) {
  Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(ns_,true);
  Arc::SOAPFault* fault = outpayload?outpayload->Fault():NULL;
  if(fault) {
    fault->Code(Arc::SOAPFault::Sender);
    if(!resp) {
      fault->Reason("Failed processing request");
    } else {
      fault->Reason(resp);
    };
  };
  outmsg.Payload(outpayload);
  return Arc::MCC_Status(Arc::STATUS_OK);
}

Arc::MCC_Status ARexService::make_http_fault(Arc::Message& outmsg,int code,const char* resp) {
  Arc::PayloadRaw* outpayload = new Arc::PayloadRaw();
  outmsg.Payload(outpayload);
  outmsg.Attributes()->set("HTTP:CODE",Arc::tostring(code));
  if(resp) outmsg.Attributes()->set("HTTP:RESPONSE",resp);
  return Arc::MCC_Status(Arc::STATUS_OK);
}

Arc::MCC_Status ARexService::make_fault(Arc::Message& /*outmsg*/) {
  // That will cause 500 Internal Error in HTTP
  return Arc::MCC_Status();
}

Arc::MCC_Status ARexService::make_empty_response(Arc::Message& outmsg) {
  Arc::PayloadRaw* outpayload = new Arc::PayloadRaw();
  outmsg.Payload(outpayload);
  return Arc::MCC_Status(Arc::STATUS_OK);
}

ARexConfigContext* ARexService::get_configuration(Arc::Message& inmsg) {
  ARexConfigContext* config = NULL;
  Arc::MessageContextElement* mcontext = (*inmsg.Context())["arex.gmconfig"];
  if(mcontext) {
    try {
      config = dynamic_cast<ARexConfigContext*>(mcontext);
    } catch(std::exception& e) { };
  };
  if(config) return config;
  // TODO: do configuration detection
  // TODO: do mapping to local unix name
  std::string uname;
  uname=inmsg.Attributes()->get("SEC:LOCALID");
  if(uname.empty()) uname=uname_;
  if(uname.empty()) {
    if(getuid() == 0) {
      logger_.msg(Arc::ERROR, "Will not map to 'root' account by default");
      return NULL;
    };
    struct passwd pwbuf;
    char buf[4096];
    struct passwd* pw;
    if(getpwuid_r(getuid(),&pwbuf,buf,sizeof(buf),&pw) == 0) {
      if(pw && pw->pw_name) {
        uname = pw->pw_name;
      };
    };
  };
  if(uname.empty()) {
    logger_.msg(Arc::ERROR, "No local account name specified");
    return NULL;
  };
  logger_.msg(Arc::DEBUG,"Using local account '%s'",uname);
  std::string grid_name = inmsg.Attributes()->get("TLS:IDENTITYDN");
  std::string endpoint = endpoint_;
  if(endpoint.empty()) {
    std::string http_endpoint = inmsg.Attributes()->get("HTTP:ENDPOINT");
    std::string tcp_endpoint = inmsg.Attributes()->get("TCP:ENDPOINT");
    bool https_proto = !grid_name.empty();
    endpoint = tcp_endpoint;
    if(https_proto) {
      endpoint="https"+endpoint;
    } else {
      endpoint="http"+endpoint;
    };
    endpoint+=GetPath(http_endpoint);
  };
  config=new ARexConfigContext(config_,uname,grid_name,endpoint);
  if(config) {
    if(*config) {
      inmsg.Context()->Add("arex.gmconfig",config);
    } else {
      delete config; config=NULL;
      logger_.msg(Arc::ERROR, "Failed to acquire grid-manager's configuration");
    };
  };
  return config;
}

static std::string GetPath(Arc::Message &inmsg,std::string &base) {
  base = inmsg.Attributes()->get("HTTP:ENDPOINT");
  Arc::AttributeIterator iterator = inmsg.Attributes()->getAll("PLEXER:EXTENSION");
  std::string path;
  if(iterator.hasMore()) {
    // Service is behind plexer
    path = *iterator;
    if(base.length() > path.length()) base.resize(base.length()-path.length());
  } else {
    // Standalone service
    path=Arc::URL(base).Path();
    base.resize(0);
  };
  return path;
}

#define SOAP_NOT_SUPPORTED { \
  logger_.msg(Arc::ERROR, "SOAP operation is not supported: %s", op.Name()); \
  delete outpayload; \
  return make_soap_fault(outmsg,"Operation not supported"); \
}


Arc::MCC_Status ARexService::process(Arc::Message& inmsg,Arc::Message& outmsg) {
  // Split request path into parts: service, job and file path. 
  // TODO: make it HTTP independent
  std::string endpoint;
  std::string method = inmsg.Attributes()->get("HTTP:METHOD");
  std::string id = GetPath(inmsg,endpoint);
  std::string clientid = (inmsg.Attributes()->get("TCP:REMOTEHOST"))+":"+(inmsg.Attributes()->get("TCP:REMOTEPORT"));
  if((inmsg.Attributes()->get("PLEXER:PATTERN").empty()) && id.empty()) id=endpoint;
  logger_.msg(Arc::VERBOSE, "process: method: %s", method);
  logger_.msg(Arc::VERBOSE, "process: endpoint: %s", endpoint);
  while(id[0] == '/') id=id.substr(1);
  std::string subpath;
  {
    std::string::size_type p = id.find('/');
    if(p != std::string::npos) {
      subpath = id.substr(p);
      id.resize(p);
      while(subpath[0] == '/') subpath=subpath.substr(1);
    };
  };
  logger_.msg(Arc::VERBOSE, "process: id: %s", id);
  logger_.msg(Arc::VERBOSE, "process: subpath: %s", subpath);

  // Sort out request
  Arc::PayloadSOAP* inpayload = NULL;
  Arc::XMLNode op;
  if(method == "POST") {
    logger_.msg(Arc::VERBOSE, "process: POST");
    // Both input and output are supposed to be SOAP
    // Extracting payload
    try {
      inpayload = dynamic_cast<Arc::PayloadSOAP*>(inmsg.Payload());
    } catch(std::exception& e) { };
    if(!inpayload) {
      logger_.msg(Arc::ERROR, "input is not SOAP");
      return make_soap_fault(outmsg);
    };
    if(logger_.getThreshold() <= Arc::VERBOSE) {
        std::string str;
        inpayload->GetDoc(str, true);
        logger_.msg(Arc::VERBOSE, "process: request=%s",str);
    };
    // Analyzing request
    op = inpayload->Child(0);
    if(!op) {
      logger_.msg(Arc::ERROR, "input does not define operation");
      return make_soap_fault(outmsg);
    };
    logger_.msg(Arc::VERBOSE, "process: operation: %s",op.Name());
    // Adding A-REX attributes
    inmsg.Auth()->set("AREX",new ARexSecAttr(op));
  } else if(method == "GET") {
    inmsg.Auth()->set("AREX",new ARexSecAttr(std::string(JOB_POLICY_OPERATION_READ)));
  } else if(method == "HEAD") {
    inmsg.Auth()->set("AREX",new ARexSecAttr(std::string(JOB_POLICY_OPERATION_READ)));
  } else if(method == "PUT") {
    inmsg.Auth()->set("AREX",new ARexSecAttr(std::string(JOB_POLICY_OPERATION_MODIFY)));
  }

  if(!ProcessSecHandlers(inmsg,"incoming")) {
    logger_.msg(Arc::ERROR, "Security Handlers processing failed");
    return make_soap_fault(outmsg, "Not authorized");
  };

  // Process grid-manager configuration if not done yet
  ARexConfigContext* config = get_configuration(inmsg);
  if(!config) {
    logger_.msg(Arc::ERROR, "Can't obtain configuration");
    // Service is not operational
    return Arc::MCC_Status();
  };
  config->ClearAuths();
  config->AddAuth(inmsg.Auth());
  config->AddAuth(inmsg.AuthContext());

  // Identify which of served endpoints request is for.
  // Using simplified algorithm - POST for SOAP messages,
  // GET and PUT for data transfer
  if(method == "POST") {
    // Check if request is for top of tree (BES factory) or particular 
    // job (listing activity)
    if(id.empty()) {
      // Factory operations
      logger_.msg(Arc::VERBOSE, "process: factory endpoint");
      Arc::PayloadSOAP* outpayload = new Arc::PayloadSOAP(ns_);
      Arc::PayloadSOAP& res = *outpayload;
      // Preparing known namespaces
      outpayload->Namespaces(ns_);
      if(config_.ARCInterfaceEnabled() && MatchXMLNamespace(op,BES_FACTORY_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"CreateActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          CreateActivity(*config,op,BESFactoryResponse(res,"CreateActivity"),clientid);
        } else if(MatchXMLName(op,"GetActivityStatuses")) {
          CountedResourceLock cl_lock(beslimit_);
          GetActivityStatuses(*config,op,BESFactoryResponse(res,"GetActivityStatuses"));
        } else if(MatchXMLName(op,"TerminateActivities")) {
          CountedResourceLock cl_lock(beslimit_);
          TerminateActivities(*config,op,BESFactoryResponse(res,"TerminateActivities"));
        } else if(MatchXMLName(op,"GetActivityDocuments")) {
          CountedResourceLock cl_lock(beslimit_);
          GetActivityDocuments(*config,op,BESFactoryResponse(res,"GetActivityDocuments"));
        } else if(MatchXMLName(op,"GetFactoryAttributesDocument")) {
          CountedResourceLock cl_lock(beslimit_);
          GetFactoryAttributesDocument(*config,op,BESFactoryResponse(res,"GetFactoryAttributesDocument"));
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.ARCInterfaceEnabled() && MatchXMLNamespace(op,BES_MANAGEMENT_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"StopAcceptingNewActivities")) {
          CountedResourceLock cl_lock(beslimit_);
          StopAcceptingNewActivities(*config,op,BESManagementResponse(res,"StopAcceptingNewActivities"));
        } else if(MatchXMLName(op,"StartAcceptingNewActivities")) {
          CountedResourceLock cl_lock(beslimit_);
          StartAcceptingNewActivities(*config,op,BESManagementResponse(res,"StartAcceptingNewActivities"));
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.EMIESInterfaceEnabled() && MatchXMLNamespace(op,ES_CREATE_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"CreateActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESCreateActivities(*config,op,ESCreateResponse(res,"CreateActivity"),clientid);
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.EMIESInterfaceEnabled() && MatchXMLNamespace(op,ES_RINFO_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"GetResourceInfo")) {
          CountedResourceLock cl_lock(infolimit_);
          ESGetResourceInfo(*config,op,ESRInfoResponse(res,"GetResourceInfo"));
        } else if(MatchXMLName(op,"QueryResourceInfo")) {
          CountedResourceLock cl_lock(infolimit_);
          ESQueryResourceInfo(*config,op,ESRInfoResponse(res,"QueryResourceInfo"));
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.EMIESInterfaceEnabled() && MatchXMLNamespace(op,ES_MANAG_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"PauseActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESPauseActivity(*config,op,ESManagResponse(res,"PauseActivity"));
        } else if(MatchXMLName(op,"ResumeActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESResumeActivity(*config,op,ESManagResponse(res,"ResumeActivity"));
        } else if(MatchXMLName(op,"NotifyService")) {
          CountedResourceLock cl_lock(beslimit_);
          ESNotifyService(*config,op,ESManagResponse(res,"NotifyService"));
        } else if(MatchXMLName(op,"CancelActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESCancelActivity(*config,op,ESManagResponse(res,"CancelActivity"));
        } else if(MatchXMLName(op,"WipeActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESWipeActivity(*config,op,ESManagResponse(res,"WipeActivity"));
        } else if(MatchXMLName(op,"RestartActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          ESRestartActivity(*config,op,ESManagResponse(res,"RestartActivity"));
        } else if(MatchXMLName(op,"GetActivityStatus")) {
          CountedResourceLock cl_lock(beslimit_);
          ESGetActivityStatus(*config,op,ESManagResponse(res,"GetActivityStatus"));
        } else if(MatchXMLName(op,"GetActivityInfo")) {
          CountedResourceLock cl_lock(beslimit_);
          ESGetActivityInfo(*config,op,ESManagResponse(res,"GetActivityInfo"));
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.EMIESInterfaceEnabled() && MatchXMLNamespace(op,ES_AINFO_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"ListActivities")) {
          CountedResourceLock cl_lock(beslimit_);
          ESListActivities(*config,op,ESAInfoResponse(res,"ListActivities"));
        } else if(MatchXMLName(op,"GetActivityStatus")) {
          CountedResourceLock cl_lock(beslimit_);
          ESGetActivityStatus(*config,op,ESAInfoResponse(res,"GetActivityStatus"));
        } else if(MatchXMLName(op,"GetActivityInfo")) {
          CountedResourceLock cl_lock(beslimit_);
          ESGetActivityInfo(*config,op,ESAInfoResponse(res,"GetActivityInfo"));
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(config_.ARCInterfaceEnabled() && MatchXMLNamespace(op,BES_ARC_NAMESPACE)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        if(MatchXMLName(op,"ChangeActivityStatus")) {
          CountedResourceLock cl_lock(beslimit_);
          ChangeActivityStatus(*config,op,BESARCResponse(res,"ChangeActivityStatus"));
        } else if(MatchXMLName(op,"MigrateActivity")) {
          CountedResourceLock cl_lock(beslimit_);
          MigrateActivity(*config,op,BESFactoryResponse(res,"MigrateActivity"),clientid);
        } else if(MatchXMLName(op,"CacheCheck")) {
          CacheCheck(*config,*inpayload,*outpayload);
        } else {
          SOAP_NOT_SUPPORTED;
        }
      } else if(delegation_stores_.MatchNamespace(*inpayload)) {
        // Aplying known namespaces
        inpayload->Namespaces(ns_);
        CountedResourceLock cl_lock(beslimit_);
        std::string credentials;
        if(!delegation_stores_.Process(config->GmConfig().DelegationDir(),*inpayload,*outpayload,config->GridName(),credentials)) {
          delete outpayload;
          return make_soap_fault(outmsg);
        };
        if((!credentials.empty()) && (MatchXMLNamespace(op,DELEG_ARC_NAMESPACE))) {
          // Only ARC delegation is done per job
          UpdateCredentials(*config,op,outpayload->Child(),credentials);
        };
      } else if(config_.ARCInterfaceEnabled() && MatchXMLNamespace(op,WSRF_NAMESPACE)) {
        CountedResourceLock cl_lock(infolimit_);
        /*
        Arc::SOAPEnvelope* out_ = infodoc_.Arc::InformationInterface::Process(*inpayload);
        if(out_) {
          out_->Swap(*outpayload);
          delete out_;
        } else {
          delete outpayload;
          return make_soap_fault(outmsg);
        };
        */
        delete outpayload;
        Arc::MessagePayload* mpayload = infodoc_.Process(*inpayload);
        if(!mpayload) {
          return make_soap_fault(outmsg);
        };
        try {
          outpayload = dynamic_cast<Arc::PayloadSOAP*>(mpayload);
        } catch(std::exception& e) { };
        outmsg.Payload(mpayload);
        if(logger_.getThreshold() <= Arc::VERBOSE) {
          if(outpayload) {
            std::string str;
            outpayload->GetDoc(str, true);
            logger_.msg(Arc::VERBOSE, "process: response=%s",str);
          } else {
            logger_.msg(Arc::VERBOSE, "process: response is not SOAP");
          };
        };
        if(!ProcessSecHandlers(outmsg,"outgoing")) {
          logger_.msg(Arc::ERROR, "Security Handlers processing failed");
          delete outmsg.Payload(NULL);
          return Arc::MCC_Status();
        };
        return Arc::MCC_Status(Arc::STATUS_OK);
      } else {
        SOAP_NOT_SUPPORTED;
      };
      if(logger_.getThreshold() <= Arc::VERBOSE) {
        std::string str;
        outpayload->GetDoc(str, true);
        logger_.msg(Arc::VERBOSE, "process: response=%s",str);
      };
      outmsg.Payload(outpayload);
    } else {
      // Listing operations for session directories
      // TODO: proper failure like interface is not supported
    };
    if(!ProcessSecHandlers(outmsg,"outgoing")) {
      logger_.msg(Arc::ERROR, "Security Handlers processing failed");
      delete outmsg.Payload(NULL);
      return Arc::MCC_Status();
    };
    return Arc::MCC_Status(Arc::STATUS_OK);
  } else if(method == "GET") {
    // HTTP plugin either provides buffer or stream
    logger_.msg(Arc::VERBOSE, "process: GET");
    CountedResourceLock cl_lock(datalimit_);
    // TODO: in case of error generate some content
    Arc::MCC_Status ret = Get(inmsg,outmsg,*config,id,subpath);
    if(ret) {
      if(!ProcessSecHandlers(outmsg,"outgoing")) {
        logger_.msg(Arc::ERROR, "Security Handlers processing failed");
        delete outmsg.Payload(NULL);
        return Arc::MCC_Status();
      };
    };
    return ret;
  } else if(method == "HEAD") {
    Arc::MCC_Status ret = Head(inmsg,outmsg,*config,id,subpath);
    if(ret) {
      if(!ProcessSecHandlers(outmsg,"outgoing")) {
        logger_.msg(Arc::ERROR, "Security Handlers processing failed");
        delete outmsg.Payload(NULL);
        return Arc::MCC_Status();
      };
    };
    return ret;
  } else if(method == "PUT") {
    logger_.msg(Arc::VERBOSE, "process: PUT");
    CountedResourceLock cl_lock(datalimit_);
    Arc::MCC_Status ret = Put(inmsg,outmsg,*config,id,subpath);
    if(!ret) return make_fault(outmsg);
    // Put() does not generate response yet
    ret=make_empty_response(outmsg);
    if(ret) {
      if(!ProcessSecHandlers(outmsg,"outgoing")) {
        logger_.msg(Arc::ERROR, "Security Handlers processing failed");
        delete outmsg.Payload(NULL);
        return Arc::MCC_Status();
      };
    };
    return ret;
  } else if(!method.empty()) {
    logger_.msg(Arc::VERBOSE, "process: method %s is not supported",method);
    return make_http_fault(outmsg,501,"Not Implemented");
  } else {
    logger_.msg(Arc::VERBOSE, "process: method is not defined");
    return Arc::MCC_Status();
  };
  return Arc::MCC_Status();
}

static void information_collector_starter(void* arg) {
  if(!arg) return;
  ((ARexService*)arg)->InformationCollector();
}

ARexService::ARexService(Arc::Config *cfg,Arc::PluginArgument *parg):Arc::Service(cfg,parg),
              logger_(Arc::Logger::rootLogger, "A-REX"),
              infodoc_(true),
              inforeg_(NULL),
              infoprovider_wakeup_period_(0),
              all_jobs_count_(0),
              gm_(NULL) {
  valid = false;
  config_.SetJobLog(new JobLog());
  config_.SetContPlugins(new ContinuationPlugins());
  config_.SetCredPlugin(new RunPlugin());
  // logger_.addDestination(logcerr);
  // Define supported namespaces
  ns_[BES_ARC_NPREFIX]=BES_ARC_NAMESPACE;
  ns_[BES_GLUE2_NPREFIX]=BES_GLUE2_NAMESPACE;
  ns_[BES_GLUE2PRE_NPREFIX]=BES_GLUE2PRE_NAMESPACE;
  ns_[BES_GLUE2D_NPREFIX]=BES_GLUE2D_NAMESPACE;
  ns_[BES_FACTORY_NPREFIX]=BES_FACTORY_NAMESPACE;
  ns_[BES_MANAGEMENT_NPREFIX]=BES_MANAGEMENT_NAMESPACE;
  ns_[DELEG_ARC_NPREFIX]=DELEG_ARC_NAMESPACE;
  ns_[ES_TYPES_NPREFIX]=ES_TYPES_NAMESPACE;
  ns_[ES_CREATE_NPREFIX]=ES_CREATE_NAMESPACE;
  ns_[ES_DELEG_NPREFIX]=ES_DELEG_NAMESPACE;
  ns_[ES_RINFO_NPREFIX]=ES_RINFO_NAMESPACE;
  ns_[ES_MANAG_NPREFIX]=ES_MANAG_NAMESPACE;
  ns_[ES_AINFO_NPREFIX]=ES_AINFO_NAMESPACE;
  ns_["wsa"]="http://www.w3.org/2005/08/addressing";
  ns_["jsdl"]="http://schemas.ggf.org/jsdl/2005/11/jsdl";
  ns_["wsrf-bf"]="http://docs.oasis-open.org/wsrf/bf-2";
  ns_["wsrf-r"]="http://docs.oasis-open.org/wsrf/r-2";
  ns_["wsrf-rw"]="http://docs.oasis-open.org/wsrf/rw-2";
  // Obtain information from configuration

  endpoint_=(std::string)((*cfg)["endpoint"]);
  uname_=(std::string)((*cfg)["usermap"]["defaultLocalName"]);
  std::string gmconfig=(std::string)((*cfg)["gmconfig"]);
  if (Arc::lower((std::string)((*cfg)["publishStaticInfo"])) == "yes") {
    publishstaticinfo_=true;
  } else {
    publishstaticinfo_=false;
  }
  config_.SetDelegations(&delegation_stores_);
  if(gmconfig.empty()) {
    // No external configuration file means configuration is
    // directly embedded into this configuration node.
    config_.SetXMLNode(*cfg);
    // Create temporary file with this <Service> node. This is mainly for
    // external GM processes such as infoproviders and LRMS scripts so that in
    // the case of multiple A-REXes in one HED they know which one to serve. In
    // future hopefully this can be replaced by passing the service id to those
    // scripts instead. The temporary file is deleted in this destructor.
    Arc::TmpFileCreate(gmconfig, "", getuid(), getgid(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    logger.msg(Arc::DEBUG, "Storing configuration in temporary file %s", gmconfig);
    cfg->SaveToFile(gmconfig);
    config_.SetConfigFile(gmconfig);
    config_.SetConfigIsTemp(true);
    if (!config_.Load()) {
      logger_.msg(Arc::ERROR, "Failed to process service configuration");
      return;
    }
  }
  else {
    // External configuration file
    config_.SetConfigFile(gmconfig);
    if (!config_.Load()) {
      logger_.msg(Arc::ERROR, "Failed to process configuration in %s", gmconfig);
      return;
    }
  }
  // Check for mandatory commands in configuration
  if (config_.ControlDir().empty()) {
    logger.msg(Arc::ERROR, "No control directory set in configuration");
    return;
  }
  if (config_.SessionRoots().empty()) {
    logger.msg(Arc::ERROR, "No session directory set in configuration");
    return;
  }
  if (config_.DefaultLRMS().empty()) {
    logger.msg(Arc::ERROR, "No LRMS set in configuration");
    return;
  }
  // create control directory if not yet done
  if(!config_.CreateControlDirectory()) {
    logger_.msg(Arc::ERROR, "Failed to create control directory %s", config_.ControlDir());
    return;
  }

  // Set default queue if none given
  if(config_.DefaultQueue().empty() && (config_.Queues().size() == 1)) {
    config_.SetDefaultQueue(config_.Queues().front());
  }
  std::string gmrun_ = (std::string)((*cfg)["gmrun"]);
  common_name_ = (std::string)((*cfg)["commonName"]);
  long_description_ = (std::string)((*cfg)["longDescription"]);
  lrms_name_ = (std::string)((*cfg)["LRMSName"]);
  // Must be URI. URL may be too restrictive, but is safe.
  if(!Arc::URL(lrms_name_)) {
    if (!lrms_name_.empty()) {
      logger_.msg(Arc::ERROR, "Provided LRMSName is not a valid URL: %s",lrms_name_);
    } else {
      logger_.msg(Arc::WARNING, "No LRMSName is provided. This is needed if you wish to completely comply with the BES specifications.");
    };
    // Filling something to make it follow BES specs
    lrms_name_ = "uri:undefined";
  };
  // TODO: check for enumeration values
  os_name_ = (std::string)((*cfg)["OperatingSystem"]);
  std::string debugLevel = (std::string)((*cfg)["debugLevel"]);
  if(!debugLevel.empty()) {
    logger_.setThreshold(Arc::string_to_level(debugLevel));
  };
  int valuei;
  if ((!(*cfg)["InfoproviderWakeupPeriod"]) ||
      (!Arc::stringto((std::string)((*cfg)["InfoproviderWakeupPeriod"]),infoprovider_wakeup_period_))) {
    infoprovider_wakeup_period_ = DEFAULT_INFOPROVIDER_WAKEUP_PERIOD;
  };
  if ((!(*cfg)["InfosysInterfaceMaxClients"]) ||
      (!Arc::stringto((std::string)((*cfg)["InfosysInterfaceMaxClients"]),valuei))) {
    valuei = DEFAULT_INFOSYS_MAX_CLIENTS;
  };
  infolimit_.MaxConsumers(valuei);
  if ((!(*cfg)["JobControlInterfaceMaxClients"]) ||
      (!Arc::stringto((std::string)((*cfg)["JobControlInterfaceMaxClients"]),valuei))) {
    valuei = DEFAULT_JOBCONTROL_MAX_CLIENTS;
  };
  beslimit_.MaxConsumers(valuei);
  if ((!(*cfg)["DataTransferInterfaceMaxClients"]) ||
      (!Arc::stringto((std::string)((*cfg)["DataTransferInterfaceMaxClients"]),valuei))) {
    valuei = DEFAULT_DATATRANSFER_MAX_CLIENTS;
  };
  datalimit_.MaxConsumers(valuei);

  // Run grid-manager in thread
  if((gmrun_.empty()) || (gmrun_ == "internal")) {
    gm_=new GridManager(config_);
    if(!(*gm_)) {
      logger_.msg(Arc::ERROR, "Failed to run Grid Manager thread");
      delete gm_; gm_=NULL; return;
    };
  };
  CreateThreadFunction(&information_collector_starter,this);
  valid=true;
  inforeg_ = new Arc::InfoRegisters(*cfg,this);
}

ARexService::~ARexService(void) {
  if(inforeg_) delete inforeg_;
  thread_count_.RequestCancel();
  if(gm_) delete gm_; // This should stop all GM-related threads too
  if(config_.CredPlugin()) delete config_.CredPlugin();
  if(config_.ContPlugins()) delete config_.ContPlugins();
  if(config_.GetJobLog()) delete config_.GetJobLog();
  if(config_.ConfigIsTemp()) unlink(config_.ConfigFile().c_str());
  thread_count_.WaitForExit(); // Here A-REX threads are waited for
}

} // namespace ARex

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "a-rex", "HED:SERVICE", NULL, 0, &ARex::get_service },
    { NULL, NULL, NULL, 0, NULL }
};
