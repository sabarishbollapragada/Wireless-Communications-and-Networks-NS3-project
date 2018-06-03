/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2013 Budiarto Herman
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
 * Original work authors (from lte-enb-rrc.cc):
 * - Nicola Baldo <nbaldo@cttc.es>
 * - Marco Miozzo <mmiozzo@cttc.es>
 * - Manuel Requena <manuel.requena@cttc.es>
 *
 * Converted to handover algorithm interface by:
 * - Budiarto Herman <budiarto.herman@magister.fi>
 */

#include "a2-a4-rsrq-handover-algorithm.h"
#include <ns3/log.h>
#include <ns3/uinteger.h>


//My Code *****************************************
#include "a3-rsrp-handover-algorithm.h"
#include <ns3/lte-common.h>
#include <list>
#include <ns3/double.h>

bool b=false;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("A2A4RsrqHandoverAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (A2A4RsrqHandoverAlgorithm);


///////////////////////////////////////////
// Handover Management SAP forwarder
///////////////////////////////////////////


A2A4RsrqHandoverAlgorithm::A2A4RsrqHandoverAlgorithm ()
  : m_a2MeasId (0),
    m_a3MeasId (0),
    m_a4MeasId (0),
    m_servingCellThreshold (30),
    m_neighbourCellOffset (1),
    m_handoverManagementSapUser (0)
{
  NS_LOG_FUNCTION (this);
  m_handoverManagementSapProvider = new MemberLteHandoverManagementSapProvider<A2A4RsrqHandoverAlgorithm> (this);
}


A2A4RsrqHandoverAlgorithm::~A2A4RsrqHandoverAlgorithm ()
{
  NS_LOG_FUNCTION (this);
}


TypeId
A2A4RsrqHandoverAlgorithm::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::A2A4RsrqHandoverAlgorithm")
    .SetParent<LteHandoverAlgorithm> ()
    .SetGroupName("Lte")
    .AddConstructor<A2A4RsrqHandoverAlgorithm> ()
    .AddAttribute ("ServingCellThreshold",
                   "If the RSRQ of the serving cell is worse than this "
                   "threshold, neighbour cells are consider for handover. "
                   "Expressed in quantized range of [0..34] as per Section "
                   "9.1.7 of 3GPP TS 36.133.",
                   UintegerValue (30),
                   MakeUintegerAccessor (&A2A4RsrqHandoverAlgorithm::m_servingCellThreshold),
                   MakeUintegerChecker<uint8_t> (0, 34))
    .AddAttribute ("NeighbourCellOffset",
                   "Minimum offset between the serving and the best neighbour "
                   "cell to trigger the handover. Expressed in quantized "
                   "range of [0..34] as per Section 9.1.7 of 3GPP TS 36.133.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&A2A4RsrqHandoverAlgorithm::m_neighbourCellOffset),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("Hysteresis",
                   "Handover margin (hysteresis) in dB "
                   "(rounded to the nearest multiple of 0.5 dB)",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&A2A4RsrqHandoverAlgorithm::m_hysteresisDb),
                   MakeDoubleChecker<uint8_t> (0.0, 15.0)) // Hysteresis IE value range is [0..30] as per Section 6.3.5 of 3GPP TS 36.331
    .AddAttribute ("TimeToTrigger",
                   "Time during which neighbour cell's RSRP "
                   "must continuously higher than serving cell's RSRP "
                   "in order to trigger a handover",
                   TimeValue (MilliSeconds (256)), // 3GPP time-to-trigger median value as per Section 6.3.5 of 3GPP TS 36.331
                   MakeTimeAccessor (&A2A4RsrqHandoverAlgorithm::m_timeToTrigger),
                   MakeTimeChecker ())
  ;
  return tid;
}


void
A2A4RsrqHandoverAlgorithm::SetLteHandoverManagementSapUser (LteHandoverManagementSapUser* s)
{
  NS_LOG_FUNCTION (this << s);
  m_handoverManagementSapUser = s;
}


LteHandoverManagementSapProvider*
A2A4RsrqHandoverAlgorithm::GetLteHandoverManagementSapProvider ()
{
  NS_LOG_FUNCTION (this);
  return m_handoverManagementSapProvider;
}


void
A2A4RsrqHandoverAlgorithm::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC (this << " requesting Event A2 measurements"
                     << " (threshold=" << (uint16_t) m_servingCellThreshold << ")");
  LteRrcSap::ReportConfigEutra reportConfigA2;
  reportConfigA2.eventId = LteRrcSap::ReportConfigEutra::EVENT_A2;
  reportConfigA2.threshold1.choice = LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ;
  reportConfigA2.threshold1.range = m_servingCellThreshold;
  reportConfigA2.triggerQuantity = LteRrcSap::ReportConfigEutra::RSRQ;
  reportConfigA2.reportInterval = LteRrcSap::ReportConfigEutra::MS240;
  m_a2MeasId = m_handoverManagementSapUser->AddUeMeasReportConfigForHandover (reportConfigA2);

  //My Code ****************************************
  uint8_t hysteresisIeValue = EutranMeasurementMapping::ActualHysteresis2IeValue (m_hysteresisDb);
  NS_LOG_LOGIC (this << " requesting Event A3 measurements"
                     << " (hysteresis=" << (uint16_t) hysteresisIeValue << ")"
                     << " (ttt=" << m_timeToTrigger.GetMilliSeconds () << ")");

  LteRrcSap::ReportConfigEutra reportConfigA3;
  reportConfigA3.eventId = LteRrcSap::ReportConfigEutra::EVENT_A3;
  reportConfigA2.threshold2.choice = LteRrcSap::ThresholdEutra::THRESHOLD_RSRP;
  reportConfigA2.threshold2.range = 0;
  reportConfigA3.a3Offset = 0;
  reportConfigA3.hysteresis =  hysteresisIeValue;
  reportConfigA3.timeToTrigger =m_timeToTrigger.GetMilliSeconds ();
  reportConfigA3.reportOnLeave = false;
  reportConfigA3.triggerQuantity = LteRrcSap::ReportConfigEutra::RSRP;
  reportConfigA3.reportInterval = LteRrcSap::ReportConfigEutra::MS1024;
  m_a3MeasId = m_handoverManagementSapUser->AddUeMeasReportConfigForHandover (reportConfigA3);



  NS_LOG_LOGIC (this << " requesting Event A4 measurements"
                     << " (threshold=0)");
  LteRrcSap::ReportConfigEutra reportConfigA4;
  reportConfigA4.eventId = LteRrcSap::ReportConfigEutra::EVENT_A4;
  reportConfigA4.threshold1.choice = LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ;
  reportConfigA4.threshold1.range = 0; // intentionally very low threshold
  reportConfigA4.triggerQuantity = LteRrcSap::ReportConfigEutra::RSRQ;
  reportConfigA4.reportInterval = LteRrcSap::ReportConfigEutra::MS480;
  m_a4MeasId = m_handoverManagementSapUser->AddUeMeasReportConfigForHandover (reportConfigA4);

  LteHandoverAlgorithm::DoInitialize ();
}


void
A2A4RsrqHandoverAlgorithm::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete m_handoverManagementSapProvider;
}


void
A2A4RsrqHandoverAlgorithm::DoReportUeMeas (uint16_t rnti,
                                           LteRrcSap::MeasResults measResults)
{
  NS_LOG_FUNCTION (this << rnti << (uint16_t) measResults.measId);

  if (measResults.measId == m_a2MeasId)
    { std::cout << "detected A2 event"<<std::endl;
      NS_ASSERT_MSG (measResults.rsrqResult <= m_servingCellThreshold,
                     "Invalid UE measurement report");
      EvaluateHandover (rnti, measResults.rsrqResult);
     
    }

   //My Code *******************************************
  else if(measResults.measId == m_a3MeasId)
     {
        std::cout << "Detected A3 event"<<std::endl;
         
       // EvaluateHandover2 (rnti, measResults.rsrpResult);


//Taking care of handover after the occurence of A3 event

//NS_LOG_FUNCTION (this << rnti << (uint16_t) servingCellRsrp);

  MeasurementTable_t::iterator it1;
  it1 = m_neighbourCellMeasures.find (rnti);

  if (it1 == m_neighbourCellMeasures.end ())
    {

     
      NS_LOG_WARN ("Skipping handover evaluation for RNTI " << rnti << " because neighbour cells information is not found");
    }
  else
    {
      
      // Find the best neighbour cell (eNB)
      NS_LOG_LOGIC ("Number of neighbour cells = " << it1->second.size ());
      uint16_t bestNeighbourCellId = 0;
      uint8_t bestNeighbourRsrp = 0;
      MeasurementRow_t::iterator it2;
      for (std::list <LteRrcSap::MeasResultEutra>::iterator it = measResults.measResultListEutra.begin ();
               it != measResults.measResultListEutra.end ();
               ++it)
        {  
          
         if (it->haveRsrpResult)
                {  
                  if ((bestNeighbourRsrp < it->rsrpResult)
                      && IsValidNeighbour (it->physCellId))
                    { 
                      bestNeighbourCellId = it->physCellId;
                      bestNeighbourRsrp = it->rsrpResult;
                               
                    }
                }
        }

      // Trigger Handover, if needed
      if (bestNeighbourCellId > 0 && b==true)
        {

                      std::cout << "Using Hybrid Algorithm"<<std::endl; 
          NS_LOG_LOGIC ("Best neighbour cellId " << bestNeighbourCellId);

          
            
              NS_LOG_LOGIC ("Trigger Handover to cellId " << bestNeighbourCellId);
              NS_LOG_LOGIC ("target cell RSRQ " << (uint16_t) bestNeighbourRsrp);
              //NS_LOG_LOGIC ("serving cell RSRQ " << (uint16_t) servingCellRsrp);

              // Inform eNodeB RRC about handover
              m_handoverManagementSapUser->TriggerHandover (rnti,
                                                            bestNeighbourCellId);
         }
            
         else if(bestNeighbourCellId > 0)
           {        
                 std::cout << "Using A3 RSRP Algorithm" <<std::endl; 
                 NS_LOG_LOGIC ("Trigger Handover to cellId " << bestNeighbourCellId);
                 NS_LOG_LOGIC ("target cell RSRQ " << (uint16_t) bestNeighbourRsrp);
                 // NS_LOG_LOGIC ("serving cell RSRQ " << (uint16_t) servingCellRsrp);

              // Inform eNodeB RRC about handover
              m_handoverManagementSapUser->TriggerHandover (rnti,
                                                            bestNeighbourCellId);
           }
        

    } // end of else of if (it1 == m_neighbourCellMeasures.end ())

//} // end of EvaluateMeasurementReport



     }
  else if (measResults.measId == m_a4MeasId)
    {
      if (measResults.haveMeasResultNeighCells
          && !measResults.measResultListEutra.empty ())
        {
          for (std::list <LteRrcSap::MeasResultEutra>::iterator it = measResults.measResultListEutra.begin ();
               it != measResults.measResultListEutra.end ();
               ++it)
            {
              NS_ASSERT_MSG (it->haveRsrqResult == true,
                             "RSRQ measurement is missing from cellId " << it->physCellId);
              UpdateNeighbourMeasurements (rnti, it->physCellId, it->rsrqResult);
            }
        }
      else
        {
          NS_LOG_WARN (this << " Event A4 received without measurement results from neighbouring cells");
        }
    }
  else
    {
      NS_LOG_WARN ("Ignoring measId " << (uint16_t) measResults.measId);
    }

} // end of DoReportUeMeas


void
A2A4RsrqHandoverAlgorithm::EvaluateHandover (uint16_t rnti,
                                             uint8_t servingCellRsrq)
{
  
  NS_LOG_FUNCTION (this << rnti << (uint16_t) servingCellRsrq);

  MeasurementTable_t::iterator it1;
  it1 = m_neighbourCellMeasures.find (rnti);

  if (it1 == m_neighbourCellMeasures.end ())
    {
      NS_LOG_WARN ("Skipping handover evaluation for RNTI " << rnti << " because neighbour cells information is not found");
    }
  else
    {
      // Find the best neighbour cell (eNB)
      NS_LOG_LOGIC ("Number of neighbour cells = " << it1->second.size ());
      uint16_t bestNeighbourCellId = 0;
      uint8_t bestNeighbourRsrq = 0;
      MeasurementRow_t::iterator it2;
      for (it2 = it1->second.begin (); it2 != it1->second.end (); ++it2)
        {
          if ((it2->second->m_rsrq > bestNeighbourRsrq)
              && IsValidNeighbour (it2->first))
            {
              bestNeighbourCellId = it2->first;
              bestNeighbourRsrq = it2->second->m_rsrq;
                
            }
        }

      // Trigger Handover, if needed
      if (bestNeighbourCellId > 0)
        {
          NS_LOG_LOGIC ("Best neighbour cellId " << bestNeighbourCellId);

          if ((bestNeighbourRsrq - servingCellRsrq) >= m_neighbourCellOffset)
            { 
              /*NS_LOG_LOGIC ("Trigger Handover to cellId " << bestNeighbourCellId);
              NS_LOG_LOGIC ("target cell RSRQ " << (uint16_t) bestNeighbourRsrq);
              NS_LOG_LOGIC ("serving cell RSRQ " << (uint16_t) servingCellRsrq);

              // Inform eNodeB RRC about handover
              m_handoverManagementSapUser->TriggerHandover (rnti,
                                                            bestNeighbourCellId);*/
                b=true;
            }
        }

    } // end of else of if (it1 == m_neighbourCellMeasures.end ())

} // end of EvaluateMeasurementReport

          
            
        
bool
A2A4RsrqHandoverAlgorithm::IsValidNeighbour (uint16_t cellId)
{
  NS_LOG_FUNCTION (this << cellId);

  /**
   * \todo In the future, this function can be expanded to validate whether the
   *       neighbour cell is a valid target cell, e.g., taking into account the
   *       NRT in ANR and whether it is a CSG cell with closed access.
   */

  return true;
}


void
A2A4RsrqHandoverAlgorithm::UpdateNeighbourMeasurements (uint16_t rnti,
                                                        uint16_t cellId,
                                                        uint8_t rsrq)
{
  NS_LOG_FUNCTION (this << rnti << cellId << (uint16_t) rsrq);
  MeasurementTable_t::iterator it1;
  it1 = m_neighbourCellMeasures.find (rnti);

  if (it1 == m_neighbourCellMeasures.end ())
    {
      // insert a new UE entry
      MeasurementRow_t row;
      std::pair<MeasurementTable_t::iterator, bool> ret;
      ret = m_neighbourCellMeasures.insert (std::pair<uint16_t, MeasurementRow_t> (rnti, row));
      NS_ASSERT (ret.second);
      it1 = ret.first;
    }

  NS_ASSERT (it1 != m_neighbourCellMeasures.end ());
  Ptr<UeMeasure> neighbourCellMeasures;
  std::map<uint16_t, Ptr<UeMeasure> >::iterator it2;
  it2 = it1->second.find (cellId);

  if (it2 != it1->second.end ())
    {
      neighbourCellMeasures = it2->second;
      neighbourCellMeasures->m_cellId = cellId;
      neighbourCellMeasures->m_rsrp = 0;
      neighbourCellMeasures->m_rsrq = rsrq;
    }
  else
    {
      // insert a new cell entry
      neighbourCellMeasures = Create<UeMeasure> ();
      neighbourCellMeasures->m_cellId = cellId;
      neighbourCellMeasures->m_rsrp = 0;
      neighbourCellMeasures->m_rsrq = rsrq;
      it1->second[cellId] = neighbourCellMeasures;
    }

} // end of UpdateNeighbourMeasurements


} // end of namespace ns3
