/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Jens Mittag, Stylianos Papanastasiou
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
 * Authors:
 *      Jens Mittag <jens.mittag@kit.edu>
 *      Stylianos Papanastasiou <stylianos@gmail.com>
 */

#include "ns3/config.h"
#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/packet.h"
#include "ns3/enum.h"
#include "ns3/physim-wifi-helper.h"
#include "ns3/random-variable.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/data-rate.h"
#include "ns3/physim-wifi-phy-tag.h"
#include "physim-wifi-transmitter-receiver-test.h"

NS_LOG_COMPONENT_DEFINE ("PhySimWifiTransmitterReceiverTest");

namespace ns3 {

PhySimWifiTransmitterReceiverTest::PhySimWifiTransmitterReceiverTest ()
  : TestCase ("PhySim transmission over perfect channel test case")
{
}

PhySimWifiTransmitterReceiverTest::~PhySimWifiTransmitterReceiverTest ()
{
}

void
PhySimWifiTransmitterReceiverTest::DoRun (void)
{
  SetErrorStatus (false);

  // Provide known seed for predictable results
  SeedManager::SetSeed (1);

  std::string modes[] = { "OfdmRate3MbpsBW10MHz","OfdmRate4_5MbpsBW10MHz","OfdmRate6MbpsBW10MHz","OfdmRate9MbpsBW10MHz","OfdmRate12MbpsBW10MHz","OfdmRate18MbpsBW10MHz","OfdmRate24MbpsBW10MHz","OfdmRate27MbpsBW10MHz"};
  for (int i = 0; i < 8; ++i)
    {
      RunSingle (modes[i]);
      if (!m_success)
        {
          NS_LOG_DEBUG ("FAIL: Frame transmission over a perfect channel test for" << modes[i]);
        }
      else
        {
          NS_LOG_DEBUG ("PASS: Frame transmission over a perfect channel test for " << modes[i]);
        }
      NS_TEST_ASSERT_MSG_EQ ( m_success, true, "Transmitter Receiver test failed for " << modes[i] << " : frame was not successfully decoded under ideal channel conditions");
    }
  return ;
}


bool
PhySimWifiTransmitterReceiverTest::RunSingle (std::string wifiMode)
{
  m_success = true;

  // Disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  // Do not use a fixed scrambler
  Config::SetDefault ("ns3::PhySimScrambler::UseFixedScrambler", BooleanValue (false) );

  // Configure PhySimWifiPhy
  Config::SetDefault ("ns3::PhySimWifiPhy::TxPowerEnd", DoubleValue (1.0) );
  Config::SetDefault ("ns3::PhySimWifiPhy::TxPowerStart", DoubleValue (1.0) );
  Config::SetDefault ("ns3::PhySimWifiPhy::TxPowerLevels", UintegerValue (1) );
  Config::SetDefault ("ns3::PhySimWifiPhy::TxGain", DoubleValue (0.0) );
  Config::SetDefault ("ns3::PhySimWifiPhy::RxGain", DoubleValue (0.0) );

  // Set the correct Channel Estimator implementation
  Config::SetDefault ("ns3::PhySimWifiPhy::ChannelEstimator", StringValue ("ns3::PhySimChannelFrequencyOffsetEstimator") );

  // Configure PHY layer to reflect IEEE 802.11a 10Mhz OFDM mode
  Config::SetDefault ("ns3::PhySimWifiPhy::Standard", EnumValue (WIFI_PHY_STANDARD_80211_10Mhz));

  // Disable Noise for perfect channel
  Config::SetDefault ("ns3::PhySimInterferenceHelper::DisableNoise", BooleanValue (true) );

  // Only allow hard decisions
  Config::SetDefault ("ns3::PhySimConvolutionalEncoder::SoftViterbiDecision", BooleanValue (false));
  Config::SetDefault ("ns3::PhySimOFDMSymbolCreator::SoftViterbiDecision", BooleanValue (false));

  // Channel without propagation loss
  PhySimWifiChannelHelper wifiChannel;
  wifiChannel.AddPropagationLoss ("ns3::PhySimPropagationLossModel");

  // In the future, we can also add more
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  PhySimWifiPhyHelper wifiPhy = PhySimWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (wifiMode),
                                "NonUnicastMode", StringValue (wifiMode));

  NodeContainer nodes;
  nodes.Create (2);

  // Install WifiPhy and set up mobility
  wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (1.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);

  // Add packet socket handlers
  PacketSocketHelper packetSocket;
  packetSocket.Install (nodes);

  // Configure packet socket for receiving node 0
  PacketSocketAddress socketOn0;
  socketOn0.SetAllDevices ();
  socketOn0.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  socketOn0.SetProtocol (1);
  PacketSinkHelper sink ("ns3::PacketSocketFactory", Address (socketOn0));

  // Install OnOffApplication on transmitting node 1
  // Configure to only send 1 packet
  PacketSocketAddress socketTo0;
  socketTo0.SetAllDevices ();
  socketTo0.SetPhysicalAddress (Mac48Address::GetBroadcast ());
  socketTo0.SetProtocol (1);
  OnOffHelper onOff ("ns3::PacketSocketFactory", Address (socketTo0));
  onOff.SetAttribute ("PacketSize", UintegerValue (512));
  onOff.SetAttribute ("OnTime", RandomVariableValue (ConstantVariable (1.0)));
  onOff.SetAttribute ("OffTime", RandomVariableValue (ConstantVariable (0.0)));
  // Set the data rate
  onOff.SetAttribute ("DataRate", DataRateValue (DataRate ("4kb/s")));

  // Configure start/stop times of the application/sink
  ApplicationContainer app;
  app = sink.Install (nodes.Get (1));
  app = onOff.Install (nodes.Get (0));
  app.Start (Seconds (0.0));
  app.Stop (Seconds (2.0));

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxOk", MakeCallback (&PhySimWifiTransmitterReceiverTest::PhyRxOkTrace, this) );
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/RxError", MakeCallback (&PhySimWifiTransmitterReceiverTest::PhyRxErrorTrace, this) );

  Simulator::Run ();
  Simulator::Destroy ();

  return true;
}

void
PhySimWifiTransmitterReceiverTest::PhyRxOkTrace (std::string context, Ptr<const Packet> p, Ptr<const PhySimWifiPhyTag> tag)
{
}

void
PhySimWifiTransmitterReceiverTest::PhyRxErrorTrace (std::string context, Ptr<const Packet> p, Ptr<const PhySimWifiPhyTag> tag)
{
  m_success = false;
}

} // namespace ns3