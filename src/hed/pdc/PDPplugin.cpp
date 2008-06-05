#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/loader/PDPLoader.h>
#include <arc/loader/SecHandlerLoader.h>
#include "simplelistpdp/SimpleListPDP.h"
#include "arcpdp/ArcPDP.h"
#include "pdpserviceinvoker/ArcPDPServiceInvoker.h"
#include "delegationsh/DelegationPDP.h"
#include "countpdp/CountPDP.h"
#include "allowpdp/AllowPDP.h"
#include "denypdp/DenyPDP.h"
#include "arcauthzsh/ArcAuthZ.h"
#include "usernametokensh/UsernameTokenSH.h"

using namespace ArcSec;

pdp_descriptors ARC_PDP_LOADER = {
    { "simplelist.pdp", 0, &ArcSec::SimpleListPDP::get_simplelist_pdp},
    { "arc.pdp", 0, &ArcSec::ArcPDP::get_arc_pdp},
    { "pdpservice.invoker", 0, &ArcSec::ArcPDPServiceInvoker::get_pdpservice_invoker},
    { "delegation.pdp", 0, &ArcSec::DelegationPDP::get_delegation_pdp},
    { "count.pdp", 0, &ArcSec::CountPDP::get_count_pdp},
    { "allow.pdp", 0, &ArcSec::AllowPDP::get_allow_pdp},
    { "deny.pdp", 0, &ArcSec::DenyPDP::get_deny_pdp},
    { NULL, 0, NULL }
};

sechandler_descriptors ARC_SECHANDLER_LOADER = {
    { "arc.authz", 0, &ArcSec::ArcAuthZ::get_sechandler},
    { "usernametoken.handler", 0, &ArcSec::UsernameTokenSH::get_sechandler},
    { NULL, 0, NULL }
};

