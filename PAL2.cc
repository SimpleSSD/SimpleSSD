/**
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Jie Zhang <jie@camelab.org>
 */

#include "PAL2.h"

#ifndef SIMPLESSD_STANDALONE
#include "debug/PAL.hh"
#endif

PAL2::PAL2(PALStatistics* statistics, BaseConfig *c, Latency *l)
{
  stats = statistics;
  gconf = c;
  lat = l;

  uint32_t *OriginalSizes = gconf->OriginalSizes;
  RearrangedSizes[6] = OriginalSizes[6];
  for (int i=0; i<6; i++)
  {
    RearrangedSizes[i] = OriginalSizes[ gconf->AddrSeq[i] ];

    DPRINTF(PAL,"PAL: [%d] ORI(%s): %u --> REARR(%s): %u\n", i, ADDR_STRINFO[ i ], OriginalSizes[ i ], ADDR_STRINFO[ gconf->AddrSeq[i] ], RearrangedSizes[i]); //Use DPRINTF here
  }

  ChTimeSlots = new TimeSlot*[gconf->NumChannel];
  std::memset( ChTimeSlots, 0, sizeof(TimeSlot*) * gconf->NumChannel );

  DieTimeSlots = new TimeSlot*[gconf->GetTotalNumDie()];
  std::memset( DieTimeSlots, 0, sizeof(TimeSlot*) * gconf->GetTotalNumDie() );

  MergedTimeSlots = new TimeSlot*[1];
  MergedTimeSlots[0] = NULL;

  ChFreeSlots = new std::map<uint64_t, std::map<uint64_t, uint64_t>* >[gconf->NumChannel];
  ChStartPoint = new uint64_t[gconf->NumChannel];
  for (unsigned i = 0; i < gconf->NumChannel; i++) ChStartPoint[i] = 0;

  DieFreeSlots = new std::map<uint64_t, std::map<uint64_t, uint64_t>* >[gconf->GetTotalNumDie()];
  DieStartPoint = new uint64_t[gconf->GetTotalNumDie()];
  for (unsigned i = 0; i < gconf->GetTotalNumDie(); i++) DieStartPoint[i] = 0;

  //currently, hard code pre-dma, mem-op and post-dma values
  for (unsigned i = 0; i < gconf->NumChannel; i++){
    std::map<uint64_t, uint64_t> * tmp;
    switch (gconf->NANDType) {
      case NAND_SLC:
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV)] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV) + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][1500000/lat->SPDIV] = tmp;
      break;
      case NAND_MLC:
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV)] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV) + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][1500000/lat->SPDIV] = tmp;
      break;
      case NAND_TLC:
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][100000/lat->SPDIV + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV)] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][185000000*2/(lat->PGDIV*lat->SPDIV) + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      ChFreeSlots[i][1500000/lat->SPDIV] = tmp;
      break;
      default:
      printf("unsupported NAND types!\n");
      std::terminate();
      break;
    }
  }

  for (unsigned i = 0; i < gconf->GetTotalNumDie(); i++){
    std::map<uint64_t, uint64_t> * tmp;
    switch (gconf->NANDType) {
      case NAND_SLC:
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][25000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][300000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][2000000000 + 100000/lat->SPDIV] = tmp;
      break;
      case NAND_MLC:
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][40000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][90000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][500000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][1300000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][3500000000 + 100000/lat->SPDIV] = tmp;
      break;
      case NAND_TLC:
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][58000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][78000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][107000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][558000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][2201000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][5001000000 + 100000/lat->SPDIV] = tmp;
      tmp = new std::map<uint64_t, uint64_t>;
      DieFreeSlots[i][2274000000 + 100000/lat->SPDIV] = tmp;
      break;
      default:
      printf("unsupported NAND types!\n");
      std::terminate();
      break;

    }
  }

}

PAL2::~PAL2()
{
  FlushTimeSlots(MAX64);
  delete ChTimeSlots;
  delete DieTimeSlots;
  delete MergedTimeSlots;

  delete ChFreeSlots;
  delete DieFreeSlots;
}
void PAL2::TimelineScheduling(Command& req)
{
  //ensure we can erase multiple blocks from single request
  unsigned erase_block = 1;
  if(req.operation == OPER_ERASE){
    for (int i = 5; i>= 0; i--){
      if ((gconf->AddrSeq)[i] != 5) {
        erase_block *= RearrangedSizes[i];
      }
      else{
        break;
      }
    }
  }
  /*=========== CONFLICT data gather ============*/
  for (unsigned cur_command = 0; cur_command < erase_block; cur_command++){
#if GATHER_RESOURCE_CONFLICT
    uint8_t confType = CONFLICT_NONE;
    uint64_t confLength = 0;
#endif
    req.ppn = req.ppn - (req.ppn & (erase_block - 1)) + cur_command;
    CPDPBP reqCPD;
    PPNdisassemble((uint64_t*) &(req.ppn), &reqCPD );
    uint32_t reqCh = reqCPD.Channel;
    uint32_t reqDieIdx = CPDPBPtoDieIdx( &reqCPD );
    TimeSlot *tsDMA0 = NULL, *tsMEM = NULL, *tsDMA1 = NULL;
    uint64_t tickDMA0 = 0, tickMEM = 0, tickDMA1 = 0;
    uint64_t latDMA0, latMEM, latDMA1, DMA0tickFrom, MEMtickFrom, DMA1tickFrom, totalLat;
    uint64_t latANTI; //anticipate time slot
    bool conflicts; //check conflict when scheduling
    latDMA0 = lat->GetLatency(reqCPD.Page, req.operation, BUSY_DMA0);
    latMEM  = lat->GetLatency(reqCPD.Page, req.operation, BUSY_MEM );
    latDMA1 = lat->GetLatency(reqCPD.Page, req.operation, BUSY_DMA1);
    latANTI = lat->GetLatency(reqCPD.Page, OPER_READ, BUSY_DMA0);
    //Start Finding available Slot
    DMA0tickFrom = curTick(); //get Current System Time
    while (1) //LOOP0
    {
      while (1) //LOOP1
      {
        // 1a) LOOP1 - Find DMA0 available slot in ChTimeSlots
        if (! FindFreeTime( ChFreeSlots[reqCh], latDMA0, DMA0tickFrom, tickDMA0, conflicts)){
          if (DMA0tickFrom < ChStartPoint[reqCh]){
            DMA0tickFrom = ChStartPoint[reqCh];
            conflicts = true;
          }
          else conflicts = false;
          tickDMA0 = ChStartPoint[reqCh];

        }
        else{
          if (conflicts) DMA0tickFrom = tickDMA0;
        }
        /*=========== CONFLICT check - DMA0 ============*/
#if GATHER_RESOURCE_CONFLICT
        if (conflicts && !(confType & CONFLICT_MEM) )
        {
          confType |= CONFLICT_DMA0;
        }
#endif


        // 2b) LOOP1 - Find MEM avaiable slot in DieTimeSlots
        MEMtickFrom = DMA0tickFrom;
        if (! FindFreeTime( DieFreeSlots[reqDieIdx], (latDMA0 + latMEM), MEMtickFrom, tickMEM, conflicts)){
          if (MEMtickFrom < DieStartPoint[reqDieIdx]){
            MEMtickFrom = DieStartPoint[reqDieIdx];
            conflicts = true;
          }
          else{
            conflicts = false;
          }
          tickMEM = DieStartPoint[reqDieIdx];

        }
        else{
          if (conflicts) MEMtickFrom = tickMEM;
        }
        if (tickMEM == tickDMA0) break;
        DMA0tickFrom = MEMtickFrom;
        uint64_t tickDMA0_vrfy;
        if (!FindFreeTime( ChFreeSlots[reqCh], latDMA0, DMA0tickFrom, tickDMA0_vrfy, conflicts)){
          tickDMA0_vrfy = ChStartPoint[reqCh];
        }
        if (tickDMA0_vrfy == tickDMA0) break;
        /*=========== CONFLICT check - MEM ============*/
#if GATHER_RESOURCE_CONFLICT
        if ( conflicts && !(confType & CONFLICT_DMA0) )
        {
          confType |= CONFLICT_MEM;
        }
#endif

      } //LOOP1

      // 3) Find DMA1 available slot
      DMA1tickFrom = DMA0tickFrom + (latDMA0 + latMEM);
      if (!FindFreeTime( ChFreeSlots[reqCh], latDMA1 + latANTI, DMA1tickFrom, tickDMA1, conflicts)){
        if(DMA1tickFrom < ChStartPoint[reqCh]){
          DMA1tickFrom = ChStartPoint[reqCh];
          conflicts = true;
        }
        else conflicts = false;
        tickDMA1 = ChStartPoint[reqCh];

      }
      else{
        if (conflicts) DMA1tickFrom = tickDMA1;
      }

      // 4) Re-verify MEM slot with including MEM(DMA0_start ~ DMA1_end)
      totalLat = (DMA1tickFrom+latDMA1 + latANTI) - DMA0tickFrom;
      uint64_t tickMEM_vrfy;
      if (!FindFreeTime( DieFreeSlots[reqDieIdx], totalLat, DMA0tickFrom, tickMEM_vrfy, conflicts)){
        tickMEM_vrfy = DieStartPoint[reqDieIdx];
      }
      if (tickMEM_vrfy == tickMEM) break;
      /*=========== CONFLICT check - DMA1 ============*/
#if GATHER_RESOURCE_CONFLICT
      {
        confType |= CONFLICT_DMA1;
      }
#endif

      DMA0tickFrom = tickMEM_vrfy; // or re-search for next available resource!
    }//LOOP0

    // 5) Assggn dma0, dma1, mem
    {
      InsertFreeSlot(ChFreeSlots[reqCh], latDMA0, DMA0tickFrom, tickDMA0, ChStartPoint[reqCh], 0);

      if (!FindFreeTime( ChFreeSlots[reqCh], latDMA1 + latANTI, DMA1tickFrom, tickDMA1, conflicts)){
        if(DMA1tickFrom < ChStartPoint[reqCh]){
          DMA1tickFrom = ChStartPoint[reqCh];
          conflicts = true;
        }
        else conflicts = false;
        tickDMA1 = ChStartPoint[reqCh];

      }
      else{
        if (conflicts) DMA1tickFrom = tickDMA1;
      }
      if (DMA1tickFrom > tickDMA1) InsertFreeSlot(ChFreeSlots[reqCh], latDMA1, DMA1tickFrom + latANTI, tickDMA1, ChStartPoint[reqCh], 0);
      else InsertFreeSlot(ChFreeSlots[reqCh], latDMA1, tickDMA1 + latANTI, tickDMA1, ChStartPoint[reqCh], 0);


      //temporarily use previous MergedTimeSlots design
      InsertFreeSlot(DieFreeSlots[reqDieIdx], totalLat, DMA0tickFrom, tickMEM, DieStartPoint[reqDieIdx], 0);
      if (tsDMA0 != NULL) delete tsDMA0;
      if (DMA0tickFrom < tickDMA0)tsDMA0 = new TimeSlot(tickDMA0, latDMA0);
      else tsDMA0 = new TimeSlot(DMA0tickFrom, latDMA0);
      if (tsDMA1 != NULL) delete tsDMA1;
      if (DMA1tickFrom < tickDMA1)tsDMA0 = new TimeSlot(tickDMA1 + latANTI, latDMA1);
      else tsDMA1 = new TimeSlot(DMA1tickFrom + latANTI, latDMA1);
      if (tsMEM != NULL) delete tsMEM;
      if (DMA0tickFrom < tickMEM)tsMEM = new TimeSlot(tickMEM, totalLat);
      else tsMEM = new TimeSlot(DMA0tickFrom, totalLat);

      //******************************************************************//
      if (DMA0tickFrom > tickDMA0) DMA0tickFrom = DMA0tickFrom + latDMA0;
      else DMA0tickFrom = tickDMA0 + latDMA0;
      uint64_t tmpTick = DMA0tickFrom;
      if (! FindFreeTime( ChFreeSlots[reqCh], latANTI * 2, DMA0tickFrom, tickDMA0, conflicts)){
        if (DMA0tickFrom < ChStartPoint[reqCh]){
          DMA0tickFrom = ChStartPoint[reqCh];
          conflicts = true;
        }
        else conflicts = false;
        tickDMA0 = ChStartPoint[reqCh];

      }
      else{
        if (conflicts) DMA0tickFrom = tickDMA0;
      }
      if (DMA0tickFrom == tmpTick)
      InsertFreeSlot(ChFreeSlots[reqCh], latANTI * 2, DMA0tickFrom, tickDMA0, ChStartPoint[reqCh], 1);
      //******************************************************************//
#if 1
      //Manage MergedTimeSlots
      if (MergedTimeSlots[0] == NULL)
      {
        MergedTimeSlots[0] = new TimeSlot( tsMEM->StartTick, tsMEM->EndTick-tsMEM->StartTick+1 );
      }
      else
      {
        TimeSlot* cur = MergedTimeSlots[0];
        uint64_t s = tsMEM->StartTick;
        uint64_t e = tsMEM->EndTick;
        TimeSlot *spos = NULL, *epos = NULL;
        int spnt = 0, epnt = 0; //inside(0), rightside(1)

        //find s position
        cur = MergedTimeSlots[0];
        while (cur)
        {
          if( cur->StartTick<=s && s<=cur->EndTick )
          {
            spos = cur;
            spnt = 0;//inside
            break;
          }

          if ( (cur->Next == NULL) ||
          (cur->Next && (s < cur->Next->StartTick) ) )
          {
            spos = cur;
            spnt = 1;//rightside
            break;
          }

          cur = cur->Next;
        }

        //find e position
        cur = MergedTimeSlots[0];
        while (cur)
        {
          if( cur->StartTick<=e && e<=cur->EndTick )
          {
            epos = cur;
            epnt = 0;//inside
            break;
          }

          if ( (cur->Next == NULL) ||
          (cur->Next && (e < cur->Next->StartTick) ) )
          {
            epos = cur;
            epnt = 1;//rightside
            break;
          }
          cur = cur->Next;
        }

        //merge
        if( !( (spos || epos) && (spos==epos && spnt==0 && epnt==0) ) ) //if both side is in a merged slot, skip
        {
          if (spos)
          {
            if (spnt == 1) //rightside
            {
              TimeSlot* tmp = new TimeSlot(tsMEM->StartTick, tsMEM->EndTick-tsMEM->StartTick+1); //duration will be updated later
              tmp->Next = spos->Next;
              if (spos == epos)
              {
                epos = tmp;
              }
              spos->Next = tmp; //overlapping temporary now;
              spos = tmp; //update spos
            }
          }
          else
          {
            if (!epos) //both new
            {
              TimeSlot* tmp = new TimeSlot( tsMEM->StartTick, tsMEM->EndTick-tsMEM->StartTick+1 ); //copy one
              tmp->Next = MergedTimeSlots[0];
              MergedTimeSlots[0] = tmp;
            }
            else if (epos)
            {
              TimeSlot* tmp = new TimeSlot( tsMEM->StartTick, 999 ); //duration will be updated later
              tmp->Next = MergedTimeSlots[0];
              MergedTimeSlots[0] = tmp;
              spos = tmp;
            }
          }

          if (epos)
          {
            if (epnt == 0)
            {
              spos->EndTick = epos->EndTick;
            }
            else if(epnt == 1)
            {
              spos->EndTick = tsMEM->EndTick;
            }
            //remove [ spos->Next ~ epos ]
            cur = spos->Next;
            spos->Next = epos->Next;
            while (cur)
            {
              TimeSlot* rem = cur;
              cur = cur->Next;
              delete rem;
              if (rem == epos) break;
            }
          }
        }
      }
#endif //END MERGE TIMESLOT
    }

    //print Log
#if 1 //DBG_PRINT_BUSY
    if (1)
    {
# if DBG_PRINT_REQUEST
      DPRINTF(PAL,"PAL: %s PPN 0x%" PRIX64 " ch%02d die%05d : REQTime  %" PRIu64 "\n",
        OPER_STRINFO[req.operation], req.ppn, reqCPD.Channel, reqDieIdx, req.arrived); //Use DPRINTF here
      printCPDPBP(&reqCPD);
      DPRINTF(PAL,"PAL: %s PPN 0x%" PRIX64 " ch%02d die%05d : DMA0 %" PRIu64 " ~ %" PRIu64 " (%" PRIu64 ") , MEM  %" PRIu64 " ~ %" PRIu64 " (%" PRIu64 ") , DMA1 %" PRIu64 " ~ %" PRIu64 " (%" PRIu64 ")\n",
        OPER_STRINFO[req.operation], req.ppn, reqCPD.Channel, reqDieIdx,
        tsDMA0->StartTick, tsDMA0->EndTick, (tsDMA0->EndTick - tsDMA0->StartTick + 1),
        tsMEM->StartTick,  tsMEM->EndTick,  (tsMEM->EndTick  - tsMEM->StartTick + 1)-(tsDMA0->EndTick - tsDMA0->StartTick + 1)-(tsDMA1->EndTick - tsDMA1->StartTick + 1),
        tsDMA1->StartTick, tsDMA1->EndTick, (tsDMA1->EndTick - tsDMA1->StartTick + 1)); //Use DPRINTF here

      DPRINTF(PAL,"PAL: %s PPN 0x%" PRIX64 " ch%02d die%05d : REQ~DMA0start(%" PRIu64 "), DMA0~DMA1end(%" PRIu64 ")\n",
        OPER_STRINFO[req.operation], req.ppn, reqCPD.Channel, reqDieIdx, (tsDMA0->StartTick-1 - req.arrived + 1),
        (tsDMA1->EndTick - tsDMA0->StartTick + 1) ); //Use DPRINTF here
# endif
# if GATHER_RESOURCE_CONFLICT
      if (confType != CONFLICT_NONE)
      {
        confLength = tsDMA0->StartTick - req.arrived;
      }
# endif
    }
#endif

    // 6) Write-back latency on RequestLL
    req.finished = tsDMA1->EndTick;

    // categorize the time spent for read/write operation
    std::map<uint64_t, uint64_t>::iterator e;
    e = OpTimeStamp[req.operation].find(tsDMA0->StartTick);
    if (e != OpTimeStamp[req.operation].end()){
      if (e->second < tsDMA1->EndTick) e->second = tsDMA1->EndTick;
    }
    else{
      OpTimeStamp[req.operation][tsDMA0->StartTick] = tsDMA1->EndTick;
    }
    FlushOpTimeStamp();

    // Update stats
#if 1
    stats->UpdateLastTick(tsDMA1->EndTick);
# if GATHER_RESOURCE_CONFLICT
    stats->AddLatency( req, &reqCPD, reqDieIdx, tsDMA0, tsMEM, tsDMA1, confType, confLength );
# else
    stats->AddLatency( req, &reqCPD, reqDieIdx, tsDMA0, tsMEM, tsDMA1 );
# endif
#endif

    if (req.operation == OPER_ERASE || req.mergeSnapshot)
    {
      //MergeATimeSlot(ChTimeSlots[reqCh], tsDMA0);
      //MergeATimeSlot(DieTimeSlots[reqDieIdx]);
      stats->MergeSnapshot();
    }
  }
}

void PAL2::submit(Command &cmd) {
  TimelineScheduling(cmd);
}

uint8_t PAL2::VerifyTimeLines(uint8_t print_on)
{
  uint8_t ret = 0;
  if (print_on) printf("[ Verify Timelines ]\n");

  for (uint32_t c = 0; c < gconf->NumChannel; c++)
  {
    TimeSlot *tsDBG = ChTimeSlots[c];
    TimeSlot *prev = tsDBG;
    uint64_t ioCnt = 0;
    uint64_t cntVrfy = 0;
    uint64_t utilTime = 0;
    uint64_t idleTime = 0;
    if (!tsDBG) {
      printf("WARN: no entry in CH%02d\n", c);
      continue;
    }
    ioCnt++;
    utilTime += (tsDBG->EndTick - tsDBG->StartTick + 1);
    tsDBG = tsDBG->Next;
    //printf("TimeSlot - CH%02d Vrfy : ", c);
    while (tsDBG)
    {
      if(!( (prev->EndTick    < tsDBG->StartTick) &&
      (tsDBG->StartTick < tsDBG->EndTick) ))
      {
        if(print_on) printf( "CH%02d VERIFY FAILED: %" PRIu64 "~%" PRIu64 ", %" PRIu64 "~%" PRIu64 "\n", c, prev->StartTick, prev->EndTick, tsDBG->StartTick, tsDBG->EndTick );
        cntVrfy++;
      }
      ioCnt++;
      utilTime += (tsDBG->EndTick - tsDBG->StartTick + 1);
      idleTime += (tsDBG->StartTick - prev->EndTick - 1);

      prev = tsDBG;
      tsDBG = tsDBG->Next;
    }
    if (cntVrfy)
    {
      if(print_on)
      {
        printf("TimeSlot - CH%02d Vrfy : FAIL %" PRIu64 "\n", c, cntVrfy);

        TimeSlot* tsDBG = ChTimeSlots[c];
        printf("TimeSlot - CH%d : ", c);
        while (tsDBG)
        {

          printf("%" PRIu64 "~%" PRIu64 ", ", tsDBG->StartTick, tsDBG->EndTick);
          tsDBG = tsDBG->Next;
        }
        printf("\n");
      }
      ret|=1;
    }
    if(print_on) printf("TimeSlot - CH%02d UtilTime : %" PRIu64 " , IdleTime : %" PRIu64 " , Count: %" PRIu64 "\n", c, utilTime, idleTime, ioCnt);
  }

  for (uint32_t d=0;d<gconf->GetTotalNumDie();d++)
  {
    TimeSlot *tsDBG = DieTimeSlots[d];
    TimeSlot *prev = tsDBG;
    uint64_t ioCnt = 0;
    uint64_t cntVrfy = 0;
    uint64_t idleTime = 0;
    uint64_t utilTime = 0;
    if (!tsDBG){
      printf("WARN: no entry in DIE%05d\n", d);
      continue;
    }
    ioCnt++;
    utilTime += (tsDBG->EndTick - tsDBG->StartTick + 1);
    tsDBG = tsDBG->Next;
    //printf("TimeSlot - DIE%02d Vrfy : ", d);
    while (tsDBG)
    {

      if(!( (prev->EndTick    < tsDBG->StartTick) &&
      (tsDBG->StartTick < tsDBG->EndTick) ))
      {
        if(print_on) printf( "DIE%05d VERIFY FAILED: %" PRIu64 "~%" PRIu64 ", %" PRIu64 "~%" PRIu64 "\n", d, prev->StartTick, prev->EndTick, tsDBG->StartTick, tsDBG->EndTick );
        cntVrfy++;
      }
      ioCnt++;
      utilTime += (tsDBG->EndTick - tsDBG->StartTick + 1);
      idleTime += (tsDBG->StartTick - prev->EndTick - 1);


      prev = tsDBG;
      tsDBG = tsDBG->Next;
    }
    if (cntVrfy)
    {
      if(print_on)
      {
        printf("TimeSlot - DIE%05d Vrfy : FAIL %" PRIu64 "\n", d, cntVrfy);

        TimeSlot* tsDBG = DieTimeSlots[d];
        printf("TimeSlot - DIE%d : ", d);
        while (tsDBG)
        {

          printf("%" PRIu64 "~%" PRIu64 ", ", tsDBG->StartTick, tsDBG->EndTick);
          tsDBG = tsDBG->Next;
        }
        printf("\n");
      }
      ret|=2;
    }
    if(print_on) printf("TimeSlot - DIE%05d UtilTime : %" PRIu64 " , IdleTime : %" PRIu64 " , Count: %" PRIu64 "\n", d, utilTime, idleTime, ioCnt);
  }

  return ret;
}


TimeSlot* PAL2::InsertAfter(TimeSlot* tgtTimeSlot, uint64_t tickLen, uint64_t startTick)
{
  TimeSlot* curTS;
  TimeSlot* newTS;

  curTS = tgtTimeSlot;



  newTS = new TimeSlot(startTick, tickLen);
  newTS->Next = curTS->Next;

  curTS->Next = newTS;

  return newTS;
}

TimeSlot* PAL2::FlushATimeSlot(TimeSlot* tgtTimeSlot, uint64_t currentTick)
{

  TimeSlot* cur = tgtTimeSlot;
  while (cur)
  {
    if (cur->EndTick < currentTick)
    {

      TimeSlot* rem = cur;
      cur = cur->Next;
      delete rem;
      continue;
    }
    break;
  }
  return cur;
}

void PAL2::MergeATimeSlot(TimeSlot* tgtTimeSlot)
{
  TimeSlot* cur = tgtTimeSlot;
  while (cur)
  {
    if (cur->Next)
    {
      TimeSlot* rem = cur->Next;
      cur->EndTick = rem->EndTick;
      cur->Next = rem->Next;
      delete rem;
    }
    else{
      cur = cur->Next;
    }
  }
}

void PAL2::MergeATimeSlot(TimeSlot* startTimeSlot, TimeSlot* endTimeSlot){
  TimeSlot* cur = startTimeSlot;
  while (cur)
  {
    if (cur->Next && cur->Next == endTimeSlot){
      TimeSlot* rem = cur->Next;
      cur->EndTick = rem->EndTick;
      cur->Next = rem->Next;
      delete rem;
      break;
    }
    else if (cur->Next)
    {
      TimeSlot* rem = cur->Next;
      cur->EndTick = rem->EndTick;
      cur->Next = rem->Next;
      delete rem;
    }
    else{
      cur = cur->Next;
    }
  }
}

void PAL2::MergeATimeSlotCH(TimeSlot* tgtTimeSlot)
{
  TimeSlot* cur = tgtTimeSlot;
  while (cur)
  {
    while (cur->Next && (cur->Next)->StartTick - cur->EndTick == 1)
    {
      TimeSlot* rem = cur->Next;
      cur->EndTick = rem->EndTick;
      cur->Next = rem->Next;
      delete rem;
    }
    cur = cur->Next;
  }
  TimeSlot* test = tgtTimeSlot;
  int counter = 0;
  while (test)
  {
    counter++;
    test = test->Next;
  }

  printf("TimeAfterSlot length: %d\n",counter);

}

void PAL2::MergeATimeSlotDIE(TimeSlot* tgtTimeSlot)
{
  TimeSlot* cur = tgtTimeSlot;
  while (cur)
  {
    while (cur->Next && (cur->Next)->StartTick - cur->EndTick == 1)
    {
      TimeSlot* rem = cur->Next;
      cur->EndTick = rem->EndTick;
      cur->Next = rem->Next;
      delete rem;
    }
    cur = cur->Next;
  }
  TimeSlot* test = tgtTimeSlot;
  int counter = 0;
  while (test)
  {
    counter++;
    test = test->Next;
  }
  printf("TimeAfterSlot length: %d\n",counter);

}
TimeSlot* PAL2::FlushATimeSlotBusyTime(TimeSlot* tgtTimeSlot, uint64_t currentTick, uint64_t* TimeSum)
{
  TimeSlot* cur = tgtTimeSlot;
  while (cur)
  {
    if (cur->EndTick < currentTick)
    {
      //cnt++;
      TimeSlot* rem = cur;
      cur = cur->Next;
      *TimeSum += ( rem->EndTick - rem->StartTick + 1 );
      delete rem;
      continue;
    }
    break;
  }
  return cur;
}

void PAL2::FlushOpTimeStamp() //currently only used during garbage collection
{
  //flush OpTimeStamp
  std::map<uint64_t, uint64_t>::iterator e;
  for (unsigned Oper = 0; Oper < 3; Oper++){
    uint64_t start_tick = (uint64_t)-1, end_tick = (uint64_t)-1;
    for (e = OpTimeStamp[Oper].begin(); e != OpTimeStamp[Oper].end(); ){
      if (start_tick == (uint64_t)-1 && end_tick == (uint64_t)-1){
        start_tick = e->first; end_tick = e->second;
      }
      else if (e->first < end_tick && e->second <= end_tick){
      }
      else if (e->first < end_tick && e->second > end_tick){
        end_tick = e->second;

      }
      else if (e->first >= end_tick){
        stats->OpBusyTime[Oper] += end_tick - start_tick + 1;
        start_tick = e->first; end_tick = e->second;
      }
      OpTimeStamp[Oper].erase(e);
      e = OpTimeStamp[Oper].begin();
    }
    stats->OpBusyTime[Oper] += end_tick - start_tick + 1;
  }
}
void PAL2::InquireBusyTime(uint64_t currentTick)
{
  stats->SampledExactBusyTime = stats->ExactBusyTime;
  //flush OpTimeStamp
  std::map<uint64_t, uint64_t>::iterator e;
  for (unsigned Oper = 0; Oper < 3; Oper++){
    uint64_t start_tick = (uint64_t)-1, end_tick = (uint64_t)-1;
    for (e = OpTimeStamp[Oper].begin(); e != OpTimeStamp[Oper].end(); ){
      if (e->second > currentTick) break;
      if (start_tick == (uint64_t)-1 && end_tick == (uint64_t)-1){
        start_tick = e->first; end_tick = e->second;
      }
      else if (e->first < end_tick && e->second <= end_tick){
      }
      else if (e->first < end_tick && e->second > end_tick){
        end_tick = e->second;

      }
      else if (e->first >= end_tick){
        stats->OpBusyTime[Oper] += end_tick - start_tick + 1;
        start_tick = e->first; end_tick = e->second;
      }
      OpTimeStamp[Oper].erase(e);
      e = OpTimeStamp[Oper].begin();
    }
    stats->OpBusyTime[Oper] += end_tick - start_tick + 1;
  }
  TimeSlot* cur = MergedTimeSlots[0];

  while (cur)
  {
    if (cur->EndTick < currentTick)
    {
      TimeSlot* rem = cur;
      cur = cur->Next;
      stats->SampledExactBusyTime += ( rem->EndTick - rem->StartTick + 1 );
      continue;
    }
    else if (cur->EndTick >= currentTick && cur->StartTick < currentTick)
    {
      TimeSlot* rem = cur;
      stats->SampledExactBusyTime += ( currentTick - rem->StartTick + 1 );
      break;
    }
    break;
  }
}

void PAL2::FlushTimeSlots(uint64_t currentTick)
{


  for(uint32_t i=0;i<gconf->NumChannel;i++)
  {
    ChTimeSlots[i] = FlushATimeSlot(ChTimeSlots[i], currentTick);
  }

  for(uint32_t i=0;i<gconf->GetTotalNumDie();i++)
  {
    DieTimeSlots[i] = FlushATimeSlot(DieTimeSlots[i], currentTick);
  }

  MergedTimeSlots[0] = FlushATimeSlotBusyTime( MergedTimeSlots[0], currentTick, &(stats->ExactBusyTime) );


  stats->Access_Capacity.update();
  stats->Ticks_Total.update();

}

void PAL2::FlushFreeSlots(uint64_t currentTick){
  for(uint32_t i=0;i<gconf->NumChannel;i++)
  {
    FlushAFreeSlot(ChFreeSlots[i], currentTick);
  }
  for(uint32_t i=0;i<gconf->GetTotalNumDie();i++)
  {
    FlushAFreeSlot(DieFreeSlots[i], currentTick);
  }
  MergedTimeSlots[0] = FlushATimeSlotBusyTime( MergedTimeSlots[0], currentTick, &(stats->ExactBusyTime) );
  stats->Access_Capacity.update();
  stats->Ticks_Total.update();
}

void PAL2::FlushAFreeSlot(std::map<uint64_t, std::map<uint64_t, uint64_t>* >& tgtFreeSlot, uint64_t currentTick){
  for (std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator e = tgtFreeSlot.begin(); e != tgtFreeSlot.end(); e++){
    std::map<uint64_t, uint64_t>::iterator f = (e->second)->begin();
    std::map<uint64_t, uint64_t>::iterator g = (e->second)->end();

    for (; f != g;){
      //count++;
      if (f->second < currentTick)(e->second)->erase(f++);
      else break;
    }
  }
}



TimeSlot* PAL2::FindFreeTime(TimeSlot* tgtTimeSlot, uint64_t tickLen, uint64_t fromTick)
{
  TimeSlot* cur = tgtTimeSlot;
  TimeSlot* next = NULL;
  while(cur)
  {
    //printf("FindFreeTime.FIND : cur->ST=%llu, cur->ET=%llu, next=0x%X\n", cur->StartTick, cur->EndTick, (unsigned int)next); //DBG
    if(cur->Next)
    next = cur->Next;
    else
    break;

    if (cur->EndTick < fromTick && fromTick < next->StartTick)
    {
      if( ( next->StartTick - fromTick ) >= tickLen )
      {
        //printf("FindFreeTime.RET A: cur->ET=%llu, next->ST=%llu, ft=%llu, tickLen=%llu\n", cur->EndTick, next->StartTick, fromTick, tickLen); //DBG
        break;
      }
    }
    else if( fromTick <= cur->EndTick )
    {
      if( (next->StartTick - (cur->EndTick + 1) ) >= tickLen )
      {
        break;
      }
    }
    cur = cur->Next;
  }

  //                   v-- this condition looks strange? but [minus of uint64_t values] do not work correctly without this!
  if ( tgtTimeSlot && (tgtTimeSlot->StartTick > fromTick) && ( (tgtTimeSlot->StartTick - fromTick) >= tickLen ) )
  {
    cur = NULL; // if there is an available time slot before first cmd, return NULL!
  }

  return cur;
}

bool PAL2::FindFreeTime(std::map<uint64_t, std::map<uint64_t, uint64_t>* > & tgtFreeSlot, uint64_t tickLen, uint64_t & tickFrom, uint64_t & startTick,  bool & conflicts){
  std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator e = tgtFreeSlot.upper_bound(tickLen);
  if (e == tgtFreeSlot.end()){
    e--; //Jie: tgtFreeSlot is not empty
    std::map<uint64_t, uint64_t>::iterator f = (e->second)->upper_bound(tickFrom);
    if (f != (e->second)->begin()){
      f--;
      if (f->second >= tickLen + tickFrom - (uint64_t)1){
        startTick = f->first;
        conflicts = false;
        return true;
      }
      f++;
    }
    while(f != (e->second)->end()){
      if (f->second >= tickLen + f->first - 1){
        startTick = f->first;
        conflicts = true;
        return true;
      }
      f++;
    }
    //reach this means no FreeSlot satisfy the requirement; allocate unused slots
    //startTick will be updated in upper function
    conflicts = false;
    return false;
  }
  if (e != tgtFreeSlot.begin()) e--;
  uint64_t minTick = (uint64_t)-1;
  while (e != tgtFreeSlot.end()){

    std::map<uint64_t, uint64_t>::iterator f = (e->second)->upper_bound(tickFrom);
    if (f != (e->second)->begin()){
      f--;
      if (f->second >= tickLen + tickFrom - (uint64_t)1){ //this free slot is best fit one, skip checking others
        startTick = f->first;
        conflicts = false;
        return true;
      }
      f++;
    }
    while (f != (e->second)->end()){
      if (f->second >= tickLen + f->first - (uint64_t)1){
        if (minTick == (uint64_t)-1 || minTick > f->first){
          conflicts = true;
          minTick = f->first;
        }
        break;
      }
      f++;
    }
    e++;
  }
  if (minTick == (uint64_t)-1){
    //startTick will be updated in upper function
    conflicts = false;
    return false;
  }
  else{
    startTick = minTick;
    return true;
  }

}

void PAL2::InsertFreeSlot(std::map<uint64_t, std::map<uint64_t, uint64_t>* >& tgtFreeSlot, uint64_t tickLen, uint64_t tickFrom, uint64_t startTick, uint64_t & startPoint, bool split){
  if (startTick == startPoint){
    if (tickFrom == startTick){
      if (split) AddFreeSlot(tgtFreeSlot, tickLen, startPoint);
      startPoint = startPoint + tickLen; //Jie: just need to shift startPoint
    }
    else{
      assert(tickFrom > startTick);
      if (split) AddFreeSlot(tgtFreeSlot, tickLen, tickFrom);
      startPoint = tickFrom + tickLen;
      AddFreeSlot(tgtFreeSlot, tickFrom - startTick, startTick);
    }
  }
  else {
    std::pair<std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator, std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator> ePair;
    ePair = tgtFreeSlot.equal_range(tickLen);
    std::map<uint64_t, uint64_t>::iterator f;
    std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator e = tgtFreeSlot.upper_bound(tickLen);
    if (e != tgtFreeSlot.begin()) e--;
    while (e != tgtFreeSlot.end()){
      f = e->second->find(startTick);
      if (f != e->second->end()){
        uint64_t tmpStartTick = f->first;
        uint64_t tmpEndTick = f->second;
        e->second->erase(f);
        if (tmpStartTick < tickFrom){
          AddFreeSlot(tgtFreeSlot, tickFrom - tmpStartTick, tmpStartTick);
          if (split) AddFreeSlot(tgtFreeSlot, tickLen, tickFrom);
          assert(tmpEndTick - tickFrom + 1 >= tickLen);
          if (tmpEndTick > tickLen + tickFrom - (uint64_t)1){

            AddFreeSlot(tgtFreeSlot, tmpEndTick - (tickFrom + tickLen -1), tickFrom + tickLen);
          }
        }
        else{
          assert(tmpStartTick == tickFrom);
          assert(tmpEndTick - tickFrom + 1 >= tickLen);
          if (split) AddFreeSlot(tgtFreeSlot, tickLen, tmpStartTick);
          if (tmpEndTick > tickLen + tickFrom - (uint64_t)1){

            AddFreeSlot(tgtFreeSlot, tmpEndTick - (tickFrom + tickLen -1), tmpStartTick + tickLen);
          }
        }
        break;
      }
      e++;

    }
  }
}

void PAL2::AddFreeSlot(std::map<uint64_t, std::map<uint64_t, uint64_t>* >& tgtFreeSlot, uint64_t tickLen, uint64_t tickFrom){
  std::map<uint64_t, std::map<uint64_t, uint64_t>* >::iterator e;
  e = tgtFreeSlot.upper_bound(tickLen);
  if (e != tgtFreeSlot.begin()){
    e--;
    (e->second)->insert(std::pair<uint64_t,uint64_t>(tickFrom, tickFrom + tickLen - (uint64_t)1));
  }
}
//PPN number conversion
uint32_t PAL2::CPDPBPtoDieIdx(CPDPBP* pCPDPBP)
{
  //[Channel][Package][Die];
  uint32_t ret = 0;
  ret += pCPDPBP->Die;
  ret += pCPDPBP->Package * (gconf->NumDie);
  ret += pCPDPBP->Channel * (gconf->NumDie * gconf->NumPackage);

  return ret;
}

void PAL2::printCPDPBP(CPDPBP* pCPDPBP)
{
  uint32_t* pCPDPBP_IDX = ((uint32_t*)pCPDPBP);

  DPRINTF(PAL,"PAL:    %7s | %7s | %7s | %7s | %7s | %7s\n", ADDR_STRINFO[ gconf->AddrSeq[0] ], ADDR_STRINFO[ gconf->AddrSeq[1] ], ADDR_STRINFO[ gconf->AddrSeq[2] ], ADDR_STRINFO[ gconf->AddrSeq[3] ], ADDR_STRINFO[ gconf->AddrSeq[4] ], ADDR_STRINFO[ gconf->AddrSeq[5] ]); //Use DPRINTF here
  DPRINTF(PAL,"PAL:    %7u | %7u | %7u | %7u | %7u | %7u\n", pCPDPBP_IDX[ gconf->AddrSeq[0] ], pCPDPBP_IDX[ gconf->AddrSeq[1] ], pCPDPBP_IDX[ gconf->AddrSeq[2] ], pCPDPBP_IDX[ gconf->AddrSeq[3] ], pCPDPBP_IDX[ gconf->AddrSeq[4] ], pCPDPBP_IDX[ gconf->AddrSeq[5] ]); //Use DPRINTF here
}

void PAL2::PPNdisassemble(uint64_t* pPPN, CPDPBP* pCPDPBP)
{
  uint32_t* pCPDPBP_IDX = ((uint32_t*)pCPDPBP);
  uint64_t tmp_MOD = *pPPN;
  uint8_t * AS = gconf->AddrSeq;
  uint32_t* RS = RearrangedSizes;
  for (uint32_t i = 0; i < 6; i++){
    pCPDPBP_IDX[i] = 0;
  }
  if (RS[6] == 0) //there is no misalignment
  {
    pCPDPBP_IDX[ AS[0] ] = tmp_MOD / (RS[5] *RS[4] * RS[3] * RS[2] * RS[1]);
    tmp_MOD = tmp_MOD % (RS[5] * RS[4] * RS[3] * RS[2] * RS[1]);

    pCPDPBP_IDX[ AS[1] ] = tmp_MOD / (RS[5] *RS[4] * RS[3] * RS[2]);
    tmp_MOD = tmp_MOD % (RS[5] * RS[4] * RS[3] * RS[2]);

    pCPDPBP_IDX[ AS[2] ] = tmp_MOD / (RS[5] *RS[4] * RS[3]);
    tmp_MOD = tmp_MOD % (RS[5] * RS[4] * RS[3]);

    pCPDPBP_IDX[ AS[3] ] = tmp_MOD / (RS[5] *RS[4]);
    tmp_MOD = tmp_MOD % (RS[5] * RS[4]);

    pCPDPBP_IDX[ AS[4] ] = tmp_MOD / (RS[5]);
    tmp_MOD = tmp_MOD % (RS[5]);

    pCPDPBP_IDX[ AS[5] ] = tmp_MOD;
  }
  else{
    uint32_t tmp_size = RS[6] * RS[5] *RS[4] * RS[3] * RS[2] * RS[1] * RS[0];
    for (unsigned i = 0; i < (5-AS[6]-1); i++){
      tmp_size /= RS[i];
      pCPDPBP_IDX[ AS[i] ] = tmp_MOD / tmp_size;
      tmp_MOD = tmp_MOD % tmp_size;
    }
    tmp_size /= RS[6];
    uint32_t tmp = tmp_MOD / tmp_size;
    tmp_MOD = tmp_MOD % tmp_size;
    for (unsigned i = (5-AS[6]-1); i < 6; i++){
      tmp_size /= RS[i];
      pCPDPBP_IDX[ AS[i] ] = tmp_MOD / tmp_size;
      tmp_MOD = tmp_MOD % tmp_size;
    }
    pCPDPBP_IDX[ AS[5-AS[6]] ] *= tmp;
  }

  #if DBG_PRINT_PPN
  DPRINTF(PAL,"PAL:     0x%llX (%llu) ==>\n", *pPPN,*pPPN); //Use DPRINTF here
  printCPDPBP(pCPDPBP);
  #endif
}

void PAL2::AssemblePPN(CPDPBP* pCPDPBP, uint64_t* pPPN)
{
  uint64_t AddrPPN = 0;


  //with re-arrange ... I hate current structure! too dirty even made with FOR-LOOP! (less-readability)
  uint32_t *pCPDPBP_IDX = ((uint32_t*)pCPDPBP);
  uint8_t  *AS = gconf->AddrSeq;
  uint32_t *RS = RearrangedSizes;
  AddrPPN += pCPDPBP_IDX[ AS[5] ];
  AddrPPN += pCPDPBP_IDX[ AS[4] ] * (RS[5] );
  AddrPPN += pCPDPBP_IDX[ AS[3] ] * (RS[5] * RS[4]);
  AddrPPN += pCPDPBP_IDX[ AS[2] ] * (RS[5] * RS[4] * RS[3]);
  AddrPPN += pCPDPBP_IDX[ AS[1] ] * (RS[5] * RS[4] * RS[3] * RS[2]);
  AddrPPN += pCPDPBP_IDX[ AS[0] ] * (RS[5] * RS[4] * RS[3] * RS[2] * RS[1]);

  *pPPN = AddrPPN;

  #if DBG_PRINT_PPN
  printCPDPBP(pCPDPBP);
  DPRINTF(PAL,"PAL    ==> 0x%llx (%llu)\n", *pPPN,*pPPN); //Use DPRINTF here
  #endif
}
