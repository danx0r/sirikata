#include "osc.h"
#include <string>
#include <sstream>
#include <iostream> //rkh
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"
#include "ip/IpEndpointName.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#define ADDRESS "192.168.176.75"
#define PORT 57120

namespace oscplugin {
//osc_input_vars gOSCvars;


// if dynamic IP is enabled, ip's will be selected from an array populated by the user
// currently, ips are set in the client and assed in
void getIPAddress (int index) {
    std::cout << "testing OSC library" << std::endl;
}

#define OUTPUT_BUFFER_SIZE 2048

void sendOSCmessage(mito_data data) {
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    UdpTransmitSocket socket( IpEndpointName( ADDRESS, PORT ));
    p.Clear();

    p << osc::BeginMessage( "/mito" )
    << data.user_id
    << data.path_id
    << data.cell_id
    << data.global_x
    << data.global_y
    << data.global_z
    << data.relative_x
    << data.relative_y
    << osc::EndMessage;

    if (p.IsReady()) {
        socket.Send( p.Data(), p.Size() );
    }
}

/*
void sendOSCbundle(ball_coordinates currentClient) {

    std::string str(currentClient.port);
    std::istringstream strin(str);
    int port;
    strin >> port;

    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    UdpTransmitSocket socket( IpEndpointName( currentClient.hostname, port ));
    p.Clear();

    p << osc::BeginBundle()
    << osc::BeginMessage( "/x" ) << currentClient.origin[0] << osc::EndMessage
    << osc::BeginMessage( "/y" ) << currentClient.origin[1] << osc::EndMessage
    << osc::BeginMessage( "/z" ) << currentClient.origin[2] << osc::EndMessage
    << osc::EndBundle;

    if (p.IsReady()) {
        socket.Send( p.Data(), p.Size() );
    }
}
*/
}

