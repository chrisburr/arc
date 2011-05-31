// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATAPOINTLDAP_H__
#define __ARC_DATAPOINTLDAP_H__

#include <list>
#include <string>

#include <arc/Logger.h>
#include <arc/Thread.h>
#include <arc/XMLNode.h>
#include <arc/data/DataPointDirect.h>

namespace Arc {

  class DataPointLDAP
    : public DataPointDirect {
  public:
    DataPointLDAP(const URL& url, const UserConfig& usercfg);
    virtual ~DataPointLDAP();
    static Plugin* Instance(PluginArgument *arg);
    virtual DataStatus StartReading(DataBuffer& buffer);
    virtual DataStatus StartWriting(DataBuffer& buffer,
                                    DataCallback *space_cb = NULL);
    virtual DataStatus StopReading();
    virtual DataStatus StopWriting();
    virtual DataStatus Check();
    virtual DataStatus Remove();
    virtual DataStatus Stat(FileInfo& file, DataPoint::DataPointInfoType verb);
    virtual DataStatus List(std::list<FileInfo>& file, DataPoint::DataPointInfoType verb);
  private:
    XMLNode node;
    XMLNode entry;
    std::map<std::string, XMLNode> dn_cache;
    static void CallBack(const std::string& attr,
                         const std::string& value, void *arg);
    static void ReadThread(void *arg);
    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_DATAPOINTLDAP_H__
