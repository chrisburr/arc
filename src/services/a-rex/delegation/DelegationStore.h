#include <string>
#include <list>
#include <map>

#include <arc/delegation/DelegationInterface.h>

#include "FileRecord.h"

namespace ARex {

class DelegationStore: public Arc::DelegationContainerSOAP {
 public:
  class Consumer {
   public:
    std::string id;
    std::string client;
    std::string path;
    Consumer(const std::string& id_, const std::string& client_, const std::string& path_):
       id(id_),client(client_),path(path_) {
    };
  };
 private:
  Glib::Mutex lock_;
  FileRecord fstore_;
  std::map<Arc::DelegationConsumerSOAP*,Consumer> acquired_;
 public:
  DelegationStore(const std::string& base);
  operator bool(void) { return (bool)fstore_; };
  bool operator!(void) { return !fstore_; };
  virtual Arc::DelegationConsumerSOAP* AddConsumer(std::string& id,const std::string& client);
  virtual Arc::DelegationConsumerSOAP* FindConsumer(const std::string& id,const std::string& client);
  virtual void TouchConsumer(Arc::DelegationConsumerSOAP* c,const std::string& credentials);
  virtual void ReleaseConsumer(Arc::DelegationConsumerSOAP* c);
  virtual void RemoveConsumer(Arc::DelegationConsumerSOAP* c);
  /** Periodic management of stored consumers */
  virtual void CheckConsumers(void);
  std::string FindCred(const std::string& id,const std::string& client);
  bool LockCred(const std::string& lock_id, const std::list<std::string>& ids,const std::string& client);
  bool ReleaseCred(const std::string& lock_id);
};

} // namespace ARex

