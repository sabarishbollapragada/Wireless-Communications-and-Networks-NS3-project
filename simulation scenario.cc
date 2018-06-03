/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jaume.nin@cttc.cat>
 */

#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <ns3/buildings-helper.h>
#include "ns3/radio-bearer-stats-calculator.h"
#include "ns3/lte-global-pathloss-database.h"
#include <iomanip>
#include <string>
#include "ns3/propagation-loss-model.h"

#include <ns3/log.h>
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("EpcFirstExample");

void
    NotifyConnectionEstablishedUe (std::string context,
                                   uint64_t imsi,
                                   uint16_t cellid,
                                   uint16_t rnti)
    {
      std::cout << Simulator::Now ().GetSeconds () << " " << context
                << " UE IMSI " << imsi
                << ": connected to CellId " << cellid
                << " with RNTI " << rnti
                << std::endl;
    }
    
    void
    NotifyHandoverStartUe (std::string context,
                           uint64_t imsi,
                           uint16_t cellid,
                           uint16_t rnti,
                           uint16_t targetCellId)
    {
      std::cout << Simulator::Now ().GetSeconds () << " " << context
                << " UE IMSI " << imsi
                << ": previously connected to CellId " << cellid
                << " with RNTI " << rnti
                << ", doing handover to CellId " << targetCellId
                << std::endl;
    }
    
    void
    NotifyHandoverEndOkUe (std::string context,
                           uint64_t imsi,
                           uint16_t cellid,
                           uint16_t rnti)
    {
      std::cout << Simulator::Now ().GetSeconds () << " " << context
                << " UE IMSI " << imsi
                << ": successful handover to CellId " << cellid
                << " with RNTI " << rnti
                << std::endl;
    }
    
    void
    NotifyConnectionEstablishedEnb (std::string context,
                                    uint64_t imsi,
                                    uint16_t cellid,
                                    uint16_t rnti)
    {
      std::cout << Simulator::Now ().GetSeconds () << " " << context
                << " eNB CellId " << cellid
                << ": successful connection of UE with IMSI " << imsi
                << " RNTI " << rnti
                << std::endl;
    }
    
    void
    NotifyHandoverStartEnb (std::string context,
                            uint64_t imsi,
                            uint16_t cellid,
                            uint16_t rnti,
                            uint16_t targetCellId)
    {
      std::cout << Simulator::Now ().GetSeconds () << " " << context
                << " eNB CellId " << cellid
                << ": start handover of UE with IMSI " << imsi
                << " RNTI " << rnti
                << " to CellId " << targetCellId
               << std::endl;
   }
   
   void
   NotifyHandoverEndOkEnb (std::string context,
                           uint64_t imsi,
                           uint16_t cellid,
                           uint16_t rnti)
   {
     std::cout << Simulator::Now ().GetSeconds () << " " << context
               << " eNB CellId " << cellid
               << ": completed handover of UE with IMSI " << imsi
               << " RNTI " << rnti
               << std::endl;
   }

int
main (int argc, char *argv[])
{

  uint16_t numberOfNodes = 5;
  double simTime = 2.1;
  double distance = 60.0;
  double interPacketInterval = 100;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numberOfNodes", "Number of eNodeBs + UE pairs", numberOfNodes);
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
  cmd.Parse(argc, argv);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
 
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  lteHelper->SetSchedulerType ("ns3::TtaFfMacScheduler");    
 // lteHelper->SetHandoverAlgorithmType ("ns3::A3RsrpHandoverAlgorithm"); 
  lteHelper->SetHandoverAlgorithmType ("ns3::A2A4RsrqHandoverAlgorithm");
   lteHelper->SetHandoverAlgorithmAttribute ("ServingCellThreshold",
                                            UintegerValue (30));
  lteHelper->SetHandoverAlgorithmAttribute ("NeighbourCellOffset",
                                            UintegerValue (1));

    lteHelper->SetHandoverAlgorithmAttribute ("Hysteresis",
                                              DoubleValue (3.0));
    lteHelper->SetHandoverAlgorithmAttribute ("TimeToTrigger",
                                              TimeValue (MilliSeconds (256)));

     
        

  

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create(2);
  ueNodes.Create(numberOfNodes);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numberOfNodes; i++)
    {
      positionAlloc->Add (Vector(distance * i, 0, 0));
    }
  MobilityHelper mobility;
  //mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
 // mobility.SetPositionAllocator(positionAlloc);
 mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
  "MinX", DoubleValue (4.0),
  "MinY", DoubleValue (0.0),
  "DeltaX", DoubleValue (8.0),
  "DeltaY", DoubleValue (20.0),
  "GridWidth", UintegerValue (3),
  "LayoutType", StringValue ("RowFirst"));
 /*mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue("12.0"),
                                 "Y", StringValue("14.0"),
                                 "Theta",StringValue(" ns3::UniformRandomVariable[Min=0.0|Max=6.2830] "),
                                 "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=12]"));*/

 mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
 //mobility.SetPositionAllocator(positionAlloc);
 mobility.Install(ueNodes.Get(1));
 mobility.Install(ueNodes.Get(2));
 mobility.Install(ueNodes.Get(3));
 mobility.Install(ueNodes.Get(0));
 
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (ueNodes.Get(4));
  ueNodes.Get (4)->GetObject<MobilityModel> ()->SetPosition (Vector (12, 0, 0));
  ueNodes.Get (4)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, 15, 0));

 //MobilityHelper mobility1;
 mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
 //mobility.SetPositionAllocator(positionAlloc);
 mobility.Install(enbNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }
   

    lteHelper->Attach (ueLteDevs.Get(4), enbLteDevs.Get(0));
    lteHelper->Attach (ueLteDevs.Get(3), enbLteDevs.Get(0));
    lteHelper->Attach (ueLteDevs.Get(1), enbLteDevs.Get(0));
    lteHelper->Attach (ueLteDevs.Get(0), enbLteDevs.Get(1));
    lteHelper->Attach (ueLteDevs.Get(2), enbLteDevs.Get(1));


  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1234;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      ++ulPort;
      ++otherPort;
      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
      serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
      serverApps.Add (packetSinkHelper.Install (ueNodes.Get(u)));

      UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
      dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      ulClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

      UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
      client.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      client.SetAttribute ("MaxPackets", UintegerValue(1000000));

      clientApps.Add (dlClient.Install (remoteHost));
      clientApps.Add (ulClient.Install (ueNodes.Get(u)));
      if (u+1 < ueNodes.GetN ())
        {
          clientApps.Add (client.Install (ueNodes.Get(u+1)));
        }
      else
        {
          clientApps.Add (client.Install (ueNodes.Get(0)));
        }
    }
  serverApps.Start (Seconds (0.01));
  clientApps.Start (Seconds (0.01));


     // Add X2 inteface
     lteHelper->AddX2Interface (enbNodes);
     // X2-based Handover
    // lteHelper->HandoverRequest (Seconds (0.500), ueLteDevs.Get (4), enbLteDevs.Get (0), enbLteDevs.Get (1));


  lteHelper->EnableTraces ();
  lteHelper->EnableMacTraces ();
  // Uncomment to enable PCAP tracing
  p2ph.EnablePcapAll("lena-epc-first");

   // connect custom trace sinks for RRC connection establishment and handover notification
     Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                      MakeCallback (&NotifyConnectionEstablishedEnb));
     Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                      MakeCallback (&NotifyConnectionEstablishedUe));
     Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                      MakeCallback (&NotifyHandoverStartEnb));
     Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                      MakeCallback (&NotifyHandoverStartUe));
     Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                      MakeCallback (&NotifyHandoverEndOkEnb));
     Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                      MakeCallback (&NotifyHandoverEndOkUe));




  
   

  AnimationInterface anim ("animationepc.xml"); 
  anim.SetConstantPosition (pgw,40.0,13.0);
  anim.SetConstantPosition (enbNodes.Get(0),10.0,4.0);
  anim.SetConstantPosition (enbNodes.Get(1),10.0,20.0);
  anim.SetConstantPosition (ueNodes.Get(2),10.0,15.0);
  anim.SetConstantPosition (remoteHostContainer.Get(0),40.0,40.0);


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
        monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      if (i->first > 2)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      //    std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
  
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

        }
    }
  /*GtkConfigStore config;
  config.ConfigureAttributes();*/
  //flowmon->SerializeToXmlFile ("flowepc.xml", bool enableHistograms, bool enableProbes);
  monitor->SerializeToXmlFile ("flowmonitorstats.xml", true, true);

 
  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy();
  return 0;

}

