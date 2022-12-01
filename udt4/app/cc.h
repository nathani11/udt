#include <udt.h>
#include <ccc.h>
#include <unordered_map>
#include <map>
#include <chrono>
#include <iostream>

using namespace std;
class CTCP: public CCC
{
public:
   void init()
   {
      m_bSlowStart = true;
      m_issthresh = 83333;

      m_dPktSndPeriod = 0.0;
      m_dCWndSize = 2.0;

      setACKInterval(2);
      setRTO(1000000);
   }

   virtual void onACK(int32_t ack)
   {
      if (ack == m_iLastACK)
      {
         if (3 == ++ m_iDupACKCount)
            DupACKAction();
         else if (m_iDupACKCount > 3)
            m_dCWndSize += 1.0;
         else
            ACKAction();
      }
      else
      {
         if (m_iDupACKCount >= 3)
            m_dCWndSize = m_issthresh;

         m_iLastACK = ack;
         m_iDupACKCount = 1;

         ACKAction();
      }
   }

   virtual void onTimeout()
   {
      m_issthresh = getPerfInfo()->pktFlightSize / 2;
      if (m_issthresh < 2)
         m_issthresh = 2;

      m_bSlowStart = true;
      m_dCWndSize = 2.0;
   }

protected:
   virtual void ACKAction()
   {
      if (m_bSlowStart)
      {
         m_dCWndSize += 1.0;

         if (m_dCWndSize >= m_issthresh)
            m_bSlowStart = false;
      }
      else
         m_dCWndSize += 1.0/m_dCWndSize;
   }

   virtual void DupACKAction()
   {
      m_bSlowStart = false;

      m_issthresh = getPerfInfo()->pktFlightSize / 2;
      if (m_issthresh < 2)
         m_issthresh = 2;

      m_dCWndSize = m_issthresh + 3;
   }

protected:
   int m_issthresh;
   bool m_bSlowStart;

   int m_iDupACKCount;
   int m_iLastACK;
};


class CUDPBlast: public CCC
{
public:
   CUDPBlast()
   {
      m_dPktSndPeriod = 1000000; 
      m_dCWndSize = 83333.0; 
   }

public:
   void setRate(double mbps)
   {
      m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
   }
};

class Vegas: public CTCP{
   public:
      void init()
      {
         CTCP::init();
         m_dAlpha = 0.0;
         m_dBeta = 0.0;
         m_iAlphaFactor = 2;
         m_iBetaFactor = 4;
         m_dBaseRTT = 0.0;
         m_dExpectedThroughput = 0.0;
         m_dActualThroughput = 0.0;
         m_dLinearIncreaseFactor = 0.009;
         initPktSent = false;
         unordered_map<int32_t, uint64_t> timestamps;
      }
      void onACK(int32_t ack){
         if(initPktSent){
            m_dBaseRTT = (double) m_iRTT / 1000.0; 
         }
         if(!m_bSlowStart){
            m_dExpectedThroughput = (m_dCWndSize * m_iMSS /1000) / m_dBaseRTT;
            m_dActualThroughput = (m_iSndCurrSeqNo - ack) * m_iMSS * 0.001;
            if(m_dExpectedThroughput - m_dActualThroughput < m_dAlpha){
               m_dCWndSize += m_dLinearIncreaseFactor * m_dCWndSize;
            }
            if(m_dExpectedThroughput - m_dActualThroughput > m_dBeta){
               m_dCWndSize -= m_dLinearIncreaseFactor * m_dCWndSize;
            }
         }
         CTCP::onACK(ack);
      }

      void onPktSent(const CPacket* pkt){
         timestamps[pkt->m_iSeqNo] = std::chrono::duration_cast<std::chrono::milliseconds>
         (std::chrono::system_clock::now().time_since_epoch()).count();
      }
      protected:
         double m_dAlpha = 0.0;
         double m_dBeta = 0.0;
         int m_iAlphaFactor = 2;
         int m_iBetaFactor = 4;
         double m_dBaseRTT = 0.0;
         double m_dExpectedThroughput = 0.0;
         double m_dActualThroughput = 0.0;
         double m_dLinearIncreaseFactor = 0.009;
         bool initPktSent = false;
         unordered_map<int32_t, uint64_t> timestamps;
};  
