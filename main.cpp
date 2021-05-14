#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "open62541.h"

const UA_Logger *logger = UA_Log_Stdout;


UA_Client *new_client() {
    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    return client;
}

int find_servers(std::string discovyeryServer) {

    /*
     * Example for calling FindServersOnNetwork
     */

    {
        UA_ServerOnNetwork *serverOnNetwork = NULL;
        size_t serverOnNetworkSize = 0;

        UA_Client *client = new_client();

        UA_StatusCode retval = UA_Client_findServersOnNetwork(client, discovyeryServer.c_str(), 0, 0,
                                                              0, NULL, &serverOnNetworkSize, &serverOnNetwork);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(logger, UA_LOGCATEGORY_SERVER,
                         "Could not call FindServersOnNetwork service. "
                         "Is the discovery server started? StatusCode %s",
                         UA_StatusCode_name(retval));
            UA_Client_delete(client);
            return (int) retval;
        }

        // output all the returned/registered servers
        for (size_t i = 0; i < serverOnNetworkSize; i++) {
            UA_ServerOnNetwork *server = &serverOnNetwork[i];
            printf("Server[%lu]: %.*s", (unsigned long) i,
                   (int) server->serverName.length, server->serverName.data);
            printf("\n\tRecordID: %d", server->recordId);
            printf("\n\tDiscovery URL: %.*s", (int) server->discoveryUrl.length,
                   server->discoveryUrl.data);
            printf("\n\tCapabilities: ");
            for (size_t j = 0; j < server->serverCapabilitiesSize; j++) {
                printf("%.*s,", (int) server->serverCapabilities[j].length,
                       server->serverCapabilities[j].data);
            }
            printf("\n\n");
        }

        UA_Array_delete(serverOnNetwork, serverOnNetworkSize,
                        &UA_TYPES[UA_TYPES_SERVERONNETWORK]);
    }

    /* Example for calling FindServers */
    UA_ApplicationDescription *applicationDescriptionArray = NULL;
    size_t applicationDescriptionArraySize = 0;

    UA_StatusCode retval;
    {
        UA_Client *client = new_client();
        retval = UA_Client_findServers(client, discovyeryServer.c_str(), 0, NULL, 0, NULL,
                                       &applicationDescriptionArraySize, &applicationDescriptionArray);
        UA_Client_delete(client);
    }
    if (retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(logger, UA_LOGCATEGORY_SERVER, "Could not call FindServers service. "
                                                    "Is the discovery server started? StatusCode %s",
                     UA_StatusCode_name(retval));
        return (int) retval;
    }

    // output all the returned/registered servers
    for (size_t i = 0; i < applicationDescriptionArraySize; i++) {
        UA_ApplicationDescription *description = &applicationDescriptionArray[i];
        printf("Server[%lu]: %.*s", (unsigned long) i, (int) description->applicationUri.length,
               description->applicationUri.data);
        printf("\n\tName: %.*s", (int) description->applicationName.text.length,
               description->applicationName.text.data);
        printf("\n\tApplication URI: %.*s", (int) description->applicationUri.length,
               description->applicationUri.data);
        printf("\n\tProduct URI: %.*s", (int) description->productUri.length,
               description->productUri.data);
        printf("\n\tType: ");
        switch (description->applicationType) {
            case UA_APPLICATIONTYPE_SERVER:
                printf("Server");
                break;
            case UA_APPLICATIONTYPE_CLIENT:
                printf("Client");
                break;
            case UA_APPLICATIONTYPE_CLIENTANDSERVER:
                printf("Client and Server");
                break;
            case UA_APPLICATIONTYPE_DISCOVERYSERVER:
                printf("Discovery Server");
                break;
            default:
                printf("Unknown");
        }
        printf("\n\tDiscovery URLs:");
        for (size_t j = 0; j < description->discoveryUrlsSize; j++) {
            printf("\n\t\t[%lu]: %.*s", (unsigned long) j,
                   (int) description->discoveryUrls[j].length,
                   description->discoveryUrls[j].data);
        }
        printf("\n\n");
    }


    /*
     * Now that we have the list of available servers, call get endpoints on all of them
     */

    printf("-------- Server Endpoints --------\n");

    for (size_t i = 0; i < applicationDescriptionArraySize; i++) {
        UA_ApplicationDescription *description = &applicationDescriptionArray[i];
        if (description->discoveryUrlsSize == 0) {
            UA_LOG_INFO(logger, UA_LOGCATEGORY_CLIENT,
                        "[GetEndpoints] Server %.*s did not provide any discovery urls. Skipping.",
                        (int) description->applicationUri.length, description->applicationUri.data);
            continue;
        }

        printf("\nEndpoints for Server[%lu]: %.*s\n", (unsigned long) i,
               (int) description->applicationUri.length, description->applicationUri.data);

        UA_Client *client = new_client();

        char *discoveryUrl = (char *) UA_malloc(sizeof(char) * description->discoveryUrls[0].length + 1);
        memcpy(discoveryUrl, description->discoveryUrls[0].data, description->discoveryUrls[0].length);
        discoveryUrl[description->discoveryUrls[0].length] = '\0';

        UA_EndpointDescription *endpointArray = NULL;
        size_t endpointArraySize = 0;
        retval = UA_Client_getEndpoints(client, discoveryUrl, &endpointArraySize, &endpointArray);
        UA_free(discoveryUrl);
        if (retval != UA_STATUSCODE_GOOD) {
            UA_Client_disconnect(client);
            UA_Client_delete(client);
            break;
        }

        for (size_t j = 0; j < endpointArraySize; j++) {
            UA_EndpointDescription *endpoint = &endpointArray[j];
            printf("\n\tEndpoint[%lu]:", (unsigned long) j);
            printf("\n\t\tEndpoint URL: %.*s", (int) endpoint->endpointUrl.length, endpoint->endpointUrl.data);
            printf("\n\t\tTransport profile URI: %.*s", (int) endpoint->transportProfileUri.length,
                   endpoint->transportProfileUri.data);
            printf("\n\t\tSecurity Mode: ");
            switch (endpoint->securityMode) {
                case UA_MESSAGESECURITYMODE_INVALID:
                    printf("Invalid");
                    break;
                case UA_MESSAGESECURITYMODE_NONE:
                    printf("None");
                    break;
                case UA_MESSAGESECURITYMODE_SIGN:
                    printf("Sign");
                    break;
                case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT:
                    printf("Sign and Encrypt");
                    break;
                default:
                    printf("No valid security mode");
                    break;
            }
            printf("\n\t\tSecurity profile URI: %.*s", (int) endpoint->securityPolicyUri.length,
                   endpoint->securityPolicyUri.data);
            printf("\n\t\tSecurity Level: %d", endpoint->securityLevel);
        }

        UA_Array_delete(endpointArray, endpointArraySize, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
        UA_Client_delete(client);
    }

    printf("\n");

    UA_Array_delete(applicationDescriptionArray, applicationDescriptionArraySize,
                    &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);

    return (int) UA_STATUSCODE_GOOD;
}

/// speichert die schon ausgegebenen Nodes
std::vector<std::string> nodes;

void browse(UA_Client *client, UA_NodeId &nodeId, int depth=0) {
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = nodeId;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
    for (size_t i = 0; i < bResp.resultsSize; ++i) {
        for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
            UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
            std::string node;
            if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
                std::stringstream s;
                s << "ns=" << ref->nodeId.nodeId.namespaceIndex << ";i=" << ref->nodeId.nodeId.identifier.numeric;
                node = s.str();
            } else if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
                std::stringstream s;
                s << "ns=" << ref->nodeId.nodeId.namespaceIndex << ";s=";
                std::string ns((char *) ref->nodeId.nodeId.identifier.string.data,
                               ref->nodeId.nodeId.identifier.string.length);
                node = s.str() + ns;
                //printf("ns=%d;s=%.*s\t%.*s\t%.*s\n", ref->nodeId.nodeId.namespaceIndex,
                //(int) ref->nodeId.nodeId.identifier.string.length,
                //        ref->nodeId.nodeId.identifier.string.data,
                //        (int) ref->browseName.name.length, ref->browseName.name.data,
                //        (int) ref->displayName.text.length, ref->displayName.text.data);
            }
            /* TODO: distinguish further types */
            if (!node.empty() && std::find(nodes.begin(), nodes.end(), node) == nodes.end() && depth<100) {
                // nur wenn node noch nicht vorhanden
                nodes.push_back(node);
                std::string nodeClass;
                switch (ref->nodeClass) {
                    case UA_NODECLASS_UNSPECIFIED : nodeClass = "Unspecified"; break;
                    case UA_NODECLASS_OBJECT : nodeClass = "Object"; break;
                    case UA_NODECLASS_VARIABLE : nodeClass = "Variable"; break;
                    case UA_NODECLASS_METHOD : nodeClass = "Method"; break;
                    case UA_NODECLASS_OBJECTTYPE : nodeClass = "Objecttype"; break;
                    case UA_NODECLASS_VARIABLETYPE : nodeClass = "Variabletype"; break;
                    case UA_NODECLASS_REFERENCETYPE : nodeClass = "Referencetype"; break;
                    case UA_NODECLASS_DATATYPE : nodeClass = "Datatype"; break;
                    case UA_NODECLASS_VIEW : nodeClass = "View"; break;
                }
                printf("%s\t%d\t%s\t%.*s\t%.*s\n", nodeClass.c_str(), depth, node.c_str(), (int) ref->browseName.name.length,
                       ref->browseName.name.data, (int) ref->displayName.text.length,
                       ref->displayName.text.data);
                if (ref->nodeClass == UA_NODECLASS_OBJECT) {
                    // nur Rekursion ausfruehren, wenn es eine tiefer liegende Node ist als die Base
                    // Es kann Rückverweise geben
                    browse(client, ref->nodeId.nodeId, depth++);
                }
            }
        }
    }
    //UA_BrowseRequest_deleteMembers(&bReq);
    //UA_BrowseResponse_deleteMembers(&bResp);
}

int browse(std::string url) {
    UA_Client *client = new_client();
    /* Connect to a server */
    /* anonymous connect would be: retval = UA_Client_connect(client, "opc.tcp://localhost:4840"); */
    UA_StatusCode retval = UA_Client_connect(client, url.c_str());
    if (retval != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return (int) retval;
    }

    /* Browse some objects */
    printf("Browsing nodes in objects folder:\n");
    printf("NODECLASS\tDEPTH\tNAMESPACE/NODEID\tBROWSENAME\tDISPLAYNAME\n");
    UA_NodeId nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    browse(client, nodeId);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "simple opc/ua client test routines" << std::endl;
        std::cerr << "-d discoveryServer-Url - search for opc/ua Servers with discoveryServer." << std::endl;
        std::cerr << "-b Server-Url - list all Items from Server." << std::endl;
        std::cerr << "-g Server-Url ItemName - get Value of Item." << std::endl;
        std::cerr << "-s Server-Url ItemName Value - set Value of Item." << std::endl;
        return 1;
    }
    std::string option(argv[1]);
    if (option.size() != 2 || option[0] != '-') {
        std::cerr << "unknown option: " << option << std::endl;
        return 2;
    }
    switch (option[1]) {
        case 'd': // Discovery
            return find_servers(argc > 2 ? argv[2] : "");
        case 'b': // Discovery
            return browse(argc > 2 ? argv[2] : "");
        default:
            std::cerr << "unknown option: " << option << std::endl;
            return 2;
    }

    return 0;
}

