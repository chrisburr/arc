// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// This define is needed to have maximal values for types with fixed size
#define __STDC_LIMIT_MACROS
#include <stdlib.h>
#include <map>

#include <arc/StringConv.h>
#include <arc/client/ClientInterface.h>
#include <arc/message/MCCLoader.h>

namespace Arc {

  Logger ClientInterface::logger(Logger::getRootLogger(), "ClientInterface");

  static void xml_add_element(XMLNode xml, XMLNode element) {
    if ((std::string)(element.Attribute("overlay")) != "add")
      if (element.Size() > 0) {
        std::string element_name = element.Name(); // FullName ?
        std::string element_id = (std::string)(element.Attribute("name"));
        for (XMLNode x = xml[element_name]; (bool)x; x = x[1]) {
          if (!element_id.empty())
            if (element_id != (std::string)(x.Attribute("name")))
              continue;
          for (int n = 0;; ++n) {
            XMLNode e = element.Child(n);
            if (!e)
              break;
            xml_add_element(x, e);
          }
        }
        return;
      }
    xml.NewChild(element, 0);
    return;
  }

  static void xml_add_elements(XMLNode xml, XMLNode elements) {
    for (int n = 0;; ++n) {
      XMLNode e = elements.Child(n);
      if (!e)
        break;
      xml_add_element(xml, e);
    }
  }

  static XMLNode ConfigMakeComponent(XMLNode chain, const char *name,
                                     const char *id, const char *next = NULL) {
    XMLNode comp = chain.NewChild("Component");

    // Make sure namespaces and names are corre
    comp.NewAttribute("name") = name;
    comp.NewAttribute("id") = id;
    if (next)
      comp.NewChild("next").NewAttribute("id") = next;
    return comp;
  }

  static XMLNode ConfigFindComponent(XMLNode chain, const char *name,
                                     const char *id) {
    XMLNode comp = chain["Component"];
    for (; (bool)comp; ++comp)
      if ((comp.Attribute("name") == name) &&
          (comp.Attribute("id") == id))
        return comp;
  }

  ClientInterface::ClientInterface(const BaseConfig& cfg)
    : xmlcfg(NS()),
      //Need to add all of the configuration namespaces here?
      loader() {
    cfg.MakeConfig(xmlcfg);
    xmlcfg.NewChild("Chain");
    cfg.overlay.New(overlay);
  }

  ClientInterface::~ClientInterface() {
    if (loader)
      delete loader;
  }

  void ClientInterface::Load() {
    if (!loader) {
      if (overlay)
        Overlay(overlay);
      loader = new MCCLoader(xmlcfg);
    }
  }

  void ClientInterface::Overlay(XMLNode cfg) {
    xml_add_elements(xmlcfg, cfg);
  }

  void ClientInterface::AddSecHandler(XMLNode mcccfg, XMLNode handlercfg) {
    // Insert SecHandler configuration into MCC configuration block
    // Make sure namespaces and names are correct
    mcccfg.NewChild(handlercfg).Name("SecHandler");
  }

  void ClientInterface::AddPlugin(XMLNode mcccfg, const std::string& libname, const std::string& libpath) {
    if (!libpath.empty()) {
      XMLNode mm = mcccfg["ModuleManager"];
      if (!mm)
        mcccfg.NewChild("ModuleManager", 0);
      XMLNode mp = mm["Path"];
      for (; (bool)mp; ++mp)
        if (mp == libpath)
          break;
      if (!mp)
        mm.NewChild("Path") = libpath;
    }
    if (!libname.empty()) {
      XMLNode pl = mcccfg["Plugins"];
      for (; (bool)pl; ++pl)
        if (pl["Name"] == libname)
          break;
      if (!pl)
        mcccfg.NewChild("Plugins", 0).NewChild("Name") = libname;
    }
  }

  ClientTCP::ClientTCP(const BaseConfig& cfg, const std::string& host,
                       int port, SecurityLayer sec)
    : ClientInterface(cfg),
      tcp_entry(NULL),
      tls_entry(NULL) {
    XMLNode comp = ConfigMakeComponent(xmlcfg["Chain"], "tcp.client", "tcp");
    comp.NewAttribute("entry") = "tcp";
    comp = comp.NewChild("Connect");
    comp.NewChild("Host") = host;
    comp.NewChild("Port") = tostring(port);

    if (sec == TLSSec) {
      comp = ConfigMakeComponent(xmlcfg["Chain"], "tls.client", "tls", "tcp");
      if (!cfg.key.empty())
        comp.NewChild("KeyPath") = cfg.key;
      if (!cfg.cert.empty())
        comp.NewChild("CertificatePath") = cfg.cert;
      if (!cfg.proxy.empty())
        comp.NewChild("ProxyPath") = cfg.proxy;
      if (!cfg.cafile.empty())
        comp.NewChild("CACertificatePath") = cfg.cafile;
      if (!cfg.cadir.empty())
        comp.NewChild("CACertificatesDir") = cfg.cadir;
      comp.NewAttribute("entry") = "tls";
    }
    else if (sec == GSISec) {
      comp = ConfigMakeComponent(xmlcfg["Chain"], "gsi.client", "gsi", "tcp");
      if (!cfg.key.empty())
        comp.NewChild("KeyPath") = cfg.key;
      if (!cfg.cert.empty())
        comp.NewChild("CertificatePath") = cfg.cert;
      if (!cfg.proxy.empty())
        comp.NewChild("ProxyPath") = cfg.proxy;
      if (!cfg.cafile.empty())
        comp.NewChild("CACertificatePath") = cfg.cafile;
      if (!cfg.cadir.empty())
        comp.NewChild("CACertificatesDir") = cfg.cadir;
      comp.NewAttribute("entry") = "gsi";
    }
  }

  ClientTCP::~ClientTCP() {}

  void ClientTCP::Load() {
    ClientInterface::Load();
    if (!tls_entry)
      tls_entry = (*loader)["tls"];
    if (!tls_entry)
      tls_entry = (*loader)["gsi"];
    if (!tcp_entry)
      tcp_entry = (*loader)["tcp"];
  }

  MCC_Status ClientTCP::process(PayloadRawInterface *request,
                                PayloadStreamInterface **response, bool tls) {
    Load();
    *response = NULL;
    if (tls && !tls_entry)
      return MCC_Status();
    if (!tls && !tcp_entry)
      return MCC_Status();
    MessageAttributes attributes_req;
    MessageAttributes attributes_rep;
    Message reqmsg;
    Message repmsg;
    reqmsg.Attributes(&attributes_req);
    reqmsg.Context(&context);
    reqmsg.Payload(request);
    repmsg.Attributes(&attributes_rep);
    repmsg.Context(&context);

    MCC_Status r;
    if (tls)
      r = tls_entry->process(reqmsg, repmsg);
    else
      r = tcp_entry->process(reqmsg, repmsg);

    if (repmsg.Payload() != NULL)
      try {
        *response =
          dynamic_cast<PayloadStreamInterface*>(repmsg.Payload());
      } catch (std::exception&) {
        delete repmsg.Payload();
      }
    return r;
  }

  void ClientTCP::AddSecHandler(XMLNode handlercfg, SecurityLayer sec, const std::string& libname, const std::string& libpath) {
    if (sec == TLSSec)
      ClientInterface::AddSecHandler(
        ConfigFindComponent(xmlcfg["Chain"], "tls.client", "tls"),
        handlercfg);
    else if (sec == GSISec)
      ClientInterface::AddSecHandler(
        ConfigFindComponent(xmlcfg["Chain"], "gsi.client", "gsi"),
        handlercfg);
    else
      ClientInterface::AddSecHandler(
        ConfigFindComponent(xmlcfg["Chain"], "tcp.client", "tcp"),
        handlercfg);
    for (XMLNode pl = handlercfg["Plugins"]; (bool)pl; ++pl)
      AddPlugin(xmlcfg, pl["Name"]);
    AddPlugin(xmlcfg, libname, libpath);
  }

  ClientHTTP::ClientHTTP(const BaseConfig& cfg, const URL& url)
    : ClientTCP(cfg, url.Host(), url.Port(),
                url.Protocol() == "https" ? TLSSec
                  : url.Protocol() == "httpg" ? GSISec
                      : NoSec),
      http_entry(NULL),
      host(url.Host()),
      port(url.Port()),
      sec(url.Protocol() == "https" ? TLSSec
            : url.Protocol() == "httpg" ? GSISec
                : NoSec) {
    XMLNode comp = ConfigMakeComponent(xmlcfg["Chain"], "http.client", "http",
                                       sec == TLSSec ? "tls" :
                                       sec == GSISec ? "gsi" : "tcp");
    comp.NewAttribute("entry") = "http";
    comp.NewChild("Method") = "POST"; // Override using attributes if needed
    comp.NewChild("Endpoint") = url.str();
  }

  ClientHTTP::~ClientHTTP() {}

  void ClientHTTP::Load() {
    ClientTCP::Load();
    if (!http_entry)
      http_entry = (*loader)["http"];
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    std::map<std::string, std::string> attributes;
    return process(method, "", attributes, 0, UINT64_MAX, request, info, response);
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 std::map<std::string, std::string>& attributes,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    return process(method, "", attributes, 0, UINT64_MAX, request, info, response);
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 const std::string& path,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    std::map<std::string, std::string> attributes;
    return process(method, path, attributes, 0, UINT64_MAX, request, info, response);
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 const std::string& path,
                                 std::map<std::string, std::string>& attributes,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    return process(method, path, attributes, 0, UINT64_MAX, request, info, response);
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 const std::string& path,
                                 uint64_t range_start, uint64_t range_end,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    std::map<std::string, std::string> attributes;
    return process(method, path, attributes, range_start, range_end, request, info, response);
  }

  MCC_Status ClientHTTP::process(const std::string& method,
                                 const std::string& path,
                                 std::map<std::string, std::string>& attributes,
                                 uint64_t range_start, uint64_t range_end,
                                 PayloadRawInterface *request,
                                 HTTPClientInfo *info,
                                 PayloadRawInterface **response) {
    Load();
    *response = NULL;
    if (!http_entry)
      return MCC_Status();
    MessageAttributes attributes_req;
    MessageAttributes attributes_rep;
    Message reqmsg;
    Message repmsg;
    reqmsg.Attributes(&attributes_req);
    reqmsg.Context(&context);
    reqmsg.Payload(request);
    repmsg.Attributes(&attributes_rep);
    repmsg.Context(&context);
    reqmsg.Attributes()->set("HTTP:METHOD", method);
    if (!path.empty()) {
      std::string url(sec == TLSSec ? "https" :
                      sec == GSISec ? "httpg" : "http");
      url += "://" + host;
      if (port > 0)
        url += ":" + tostring(port);
      if (path[0] != '/')
        url += "/";
      url += path;
      reqmsg.Attributes()->set("HTTP:ENDPOINT", url);
    }
    if (range_end != UINT64_MAX)
      reqmsg.Attributes()->set("HTTP:Range", "bytes=" + tostring(range_start) +
                               "-" + tostring(range_end));
    else if (range_start != 0)
      reqmsg.Attributes()->set("HTTP:Range", "bytes=" +
                               tostring(range_start) + "-");
    std::map<std::string, std::string>::iterator it;
    for (it = attributes.begin(); it != attributes.end(); it++) {
      std::string key("HTTP:");
      key.append((*it).first);
      reqmsg.Attributes()->set(key, (*it).second);
    }
    MCC_Status r = http_entry->process(reqmsg, repmsg);
    info->code = stringtoi(repmsg.Attributes()->get("HTTP:CODE"));
    info->reason = repmsg.Attributes()->get("HTTP:REASON");
    info->size = stringtoull(repmsg.Attributes()->get("HTTP:content-length"));
    std::string lm;
    lm = repmsg.Attributes()->get("HTTP:last-modified");
    if (lm.size() > 11)
      info->lastModified = lm;
    info->type = repmsg.Attributes()->get("HTTP:content-type");
    info->cookie = repmsg.Attributes()->get("HTTP:set-cookie");
    info->location = repmsg.Attributes()->get("HTTP:location");
    if (repmsg.Payload() != NULL)
      try {
        *response = dynamic_cast<PayloadRawInterface*>(repmsg.Payload());
      } catch (std::exception&) {
        delete repmsg.Payload();
      }
    return r;
  }

  void ClientHTTP::AddSecHandler(XMLNode handlercfg, const std::string& libname, const std::string& libpath) {
    ClientInterface::AddSecHandler(
      ConfigFindComponent(xmlcfg["Chain"], "http.client", "http"),
      handlercfg);
    for (XMLNode pl = handlercfg["Plugins"]; (bool)pl; ++pl)
      AddPlugin(xmlcfg, pl["Name"]);
    AddPlugin(xmlcfg, libname, libpath);
  }

  ClientSOAP::ClientSOAP(const BaseConfig& cfg, const URL& url)
    : ClientHTTP(cfg, url),
      soap_entry(NULL) {
    XMLNode comp =
      ConfigMakeComponent(xmlcfg["Chain"], "soap.client", "soap", "http");
    comp.NewAttribute("entry") = "soap";
    // Add the ws-security configuration for the soap client
    // Currently only usernametoken is supported
    if ((cfg.wsstype == USERNAMETOKEN) &&
        (!(cfg.wssinfo.password_encoding).empty()) &&
        (!(cfg.wssinfo.username).empty()) &&
        (!(cfg.wssinfo.password).empty())) {
      XMLNode sechandler = comp.NewChild("SecHandler");
      sechandler.NewAttribute("name") = "usernametoken.handler";
      sechandler.NewAttribute("id") = "usernametoken";
      sechandler.NewAttribute("event") = "outgoing";
      sechandler.NewChild("Process") = "generate";
      sechandler.NewChild("PasswordEncoding") = cfg.wssinfo.password_encoding;
      sechandler.NewChild("Username") = cfg.wssinfo.username;
      sechandler.NewChild("Password") = cfg.wssinfo.password;
    }
    //Process other WSSType
  }

  ClientSOAP::~ClientSOAP() {}

  MCC_Status ClientSOAP::process(PayloadSOAP *request,
                                 PayloadSOAP **response) {
    return process("", request, response);
  }

  void ClientSOAP::Load() {
    ClientHTTP::Load();
    if (!soap_entry)
      soap_entry = (*loader)["soap"];
  }

  MCC_Status ClientSOAP::process(const std::string& action,
                                 PayloadSOAP *request,
                                 PayloadSOAP **response) {
    Load();
    *response = NULL;
    if (!soap_entry)
      return MCC_Status();
    MessageAttributes attributes_req;
    MessageAttributes attributes_rep;
    Message reqmsg;
    Message repmsg;
    reqmsg.Attributes(&attributes_req);
    reqmsg.Context(&context);
    reqmsg.Payload(request);
    repmsg.Attributes(&attributes_rep);
    repmsg.Context(&context);
    if (!action.empty())
      attributes_req.set("SOAP:ACTION", action);
    MCC_Status r = soap_entry->process(reqmsg, repmsg);
    if (repmsg.Payload() != NULL)
      try {
        *response = dynamic_cast<PayloadSOAP*>(repmsg.Payload());
      } catch (std::exception&) {
        delete repmsg.Payload();
      }
    return r;
  }

  void ClientSOAP::AddSecHandler(XMLNode handlercfg, const std::string& libname, const std::string& libpath) {
    ClientInterface::AddSecHandler(
      ConfigFindComponent(xmlcfg["Chain"], "soap.client", "soap"),
      handlercfg);
    for (XMLNode pl = handlercfg["Plugins"]; (bool)pl; ++pl)
      AddPlugin(xmlcfg, pl["Name"]);
    AddPlugin(xmlcfg, libname, libpath);
  }

  SecHandlerConfig::SecHandlerConfig(const std::string& name, const std::string& event)
    : XMLNode("<?xml version=\"1.0\"?><SecHandler/>") {
    NewAttribute("name") = name;
    if (!event.empty())
      NewAttribute("event") = event;
  }

  DNListHandlerConfig::DNListHandlerConfig(const std::list<std::string>& dns, const std::string& event)
    : SecHandlerConfig("arc.authz", event) {
    // Loading PDP which deals with DN lists
    NewChild("Plugins").NewChild("Name") = "arcshc";
    XMLNode pdp = NewChild("PDP");
    pdp.NewAttribute("name") = "simplelist.pdp";
    for (std::list<std::string>::const_iterator dn = dns.begin();
         dn != dns.end(); ++dn)
      pdp.NewChild("DN") = (*dn);
  }

  void DNListHandlerConfig::AddDN(const std::string& dn) {
    XMLNode pdp = operator[]("PDP");
    pdp.NewChild("DN") = dn;
  }

  ARCPolicyHandlerConfig::ARCPolicyHandlerConfig(const std::string& event)
    : SecHandlerConfig("arc.authz", event) {}

  ARCPolicyHandlerConfig::ARCPolicyHandlerConfig(XMLNode policy, const std::string& event)
    : SecHandlerConfig("arc.authz", event) {
    // Loading PDP which deals with ARC policies
    NewChild("Plugins").NewChild("Name") = "arcshc";
    XMLNode pdp = NewChild("PDP");
    pdp.NewAttribute("name") = "arc.pdp";
    pdp.NewChild(policy);
  }

  void ARCPolicyHandlerConfig::AddPolicy(XMLNode policy) {
    XMLNode pdp = operator[]("PDP");
    pdp.NewChild(policy);
  }

  void ARCPolicyHandlerConfig::AddPolicy(const std::string& policy) {
    XMLNode p(policy);
    XMLNode pdp = operator[]("PDP");
    pdp.NewChild(p);
  }


} // namespace Arc
