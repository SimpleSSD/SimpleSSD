#include "ftl_statistics.hh"
#include <stdio.h>


FTLStats::FTLStats(){

  sim_read_active_time = 0;
  sim_write_active_time = 0;
  sim_rw_active_time = 0;

  read_active_last_update = 0;
  write_active_last_update = 0;
  rw_active_last_update = 0;

  sim_read_outstanding_count = 0;
  sim_write_outstanding_count = 0;
  sim_rw_outstanding_count = 0;

  last_epoch_collected = 0;

  host_sim_read_count = 0;
  host_sim_write_count = 0;
  host_sim_read_capacity = 0;
  host_sim_write_capacity = 0;

  // epoch Statistics
  epoch_number = 0;
  next_epoch_read_count = 0;
  next_epoch_write_count = 0;
  next_epoch_read_capacity = 0;
  next_epoch_write_capacity = 0;
  next_epoch_read_lat_sum = 0;
  next_epoch_write_lat_sum = 0;
  next_epoch_read_size_sum = 0;
  next_epoch_write_size_sum = 0;

  reset_epoch_stats(epoch_number);

}

void FTLStats::reset_epoch_stats(int epoch_number){

  vector<RequestInterval>::iterator it;

  for (it = events.begin(); it < events.end();){
    if (it->epoch_number == epoch_number)
    it = events.erase(it);
    else it++;
  }

  for (it = read_events.begin(); it < read_events.end(); ){
    if (it->epoch_number == epoch_number)
    it = read_events.erase(it);
    else it++;
  }

  for (it = write_events.begin(); it < write_events.end(); ){
    if ( it->epoch_number == epoch_number)
    it = write_events.erase(it);
    else it++;
  }
  current_epoch_read_count = next_epoch_read_count;
  next_epoch_read_count = 0;
  current_epoch_write_count = next_epoch_write_count;
  next_epoch_write_count = 0;
  current_epoch_read_capacity = next_epoch_read_capacity;
  next_epoch_read_capacity = 0;
  current_epoch_write_capacity = next_epoch_write_capacity;
  next_epoch_write_capacity = 0;
  current_epoch_read_lat_sum = next_epoch_read_lat_sum;
  next_epoch_read_lat_sum = 0;
  current_epoch_write_lat_sum = next_epoch_write_lat_sum;
  next_epoch_write_lat_sum = 0;
  current_epoch_read_size_sum = next_epoch_read_size_sum;
  next_epoch_read_size_sum = 0;
  current_epoch_write_size_sum = next_epoch_write_size_sum;
  next_epoch_write_size_sum = 0;

}

void FTLStats::print_epoch_stats(Tick sim_time){

  int epoch_number = sim_time / EPOCH_INTERVAL;
  if (sim_time % EPOCH_INTERVAL != 0) epoch_number++;

  collect_epoch_stats(epoch_number);

  DPRINTF(FTLOut, "FTL Host epoch %d , time: %" PRIu64 " \n", epoch_number, sim_time);
  DPRINTF(FTLOut, "FTL Host read count %d \n", (int) host_epoch_read_count.Get() );
  DPRINTF(FTLOut, "FTL Host read size %lf KB\n", host_epoch_read_size.Get());
  DPRINTF(FTLOut, "FTL Host read latency %lf us\n", host_epoch_read_latency.Get());
  DPRINTF(FTLOut, "FTL Host read capacity %lf KB \n", host_epoch_read_capacity.Get());
  DPRINTF(FTLOut, "FTL Host write count %d \n", (int)host_epoch_write_count.Get());
  DPRINTF(FTLOut, "FTL Host write size %lf KB\n", host_epoch_write_size.Get());
  DPRINTF(FTLOut, "FTL Host write latency %lf us\n", host_epoch_write_latency.Get());
  DPRINTF(FTLOut, "FTL Host write capacity %lf KB\n", host_epoch_write_capacity.Get());

  // BW
  DPRINTF(FTLOut, "FTL Host read  BW (active): %lf MB/s \n", host_epoch_read_BW_active.Get());
  DPRINTF(FTLOut, "FTL Host write BW (active): %lf MB/s \n", host_epoch_write_BW_active.Get());
  DPRINTF(FTLOut, "FTL Host rw    BW (active): %lf MB/s \n", host_epoch_rw_BW_active.Get());
  DPRINTF(FTLOut, "FTL Host read  BW (total ): %lf MB/s \n", host_epoch_read_BW_total.Get());
  DPRINTF(FTLOut, "FTL Host write BW (total ): %lf MB/s \n", host_epoch_write_BW_total.Get());
  DPRINTF(FTLOut, "FTL Host rw    BW (total ): %lf MB/s \n", host_epoch_rw_BW_total.Get());
  DPRINTF(FTLOut, "FTL Host read  BW (only  ): %lf MB/s \n", host_epoch_read_BW_only.Get());
  DPRINTF(FTLOut, "FTL Host write BW (only  ): %lf MB/s \n", host_epoch_write_BW_only.Get());

  // IOPS
  DPRINTF(FTLOut, "FTL Host read  IOPS (active): %lf \n", host_epoch_read_IOPS_active.Get());
  DPRINTF(FTLOut, "FTL Host write IOPS (active): %lf \n", host_epoch_write_IOPS_active.Get());
  DPRINTF(FTLOut, "FTL Host rw    IOPS (active): %lf \n", host_epoch_rw_IOPS_active.Get());
  DPRINTF(FTLOut, "FTL Host read  IOPS (total ): %lf \n", host_epoch_read_IOPS_total.Get());
  DPRINTF(FTLOut, "FTL Host write IOPS (total ): %lf \n", host_epoch_write_IOPS_total.Get());
  DPRINTF(FTLOut, "FTL Host rw    IOPS (total ): %lf \n", host_epoch_rw_IOPS_total.Get());
  DPRINTF(FTLOut, "FTL Host read  IOPS (only  ): %lf \n", host_epoch_read_IOPS_only.Get());
  DPRINTF(FTLOut, "FTL Host write IOPS (only  ): %lf \n", host_epoch_write_IOPS_only.Get());

  reset_epoch_stats(epoch_number);
}
void FTLStats::print_final_stats(Tick sim_time){

  int epoch_number = sim_time / EPOCH_INTERVAL;
  if (sim_time % EPOCH_INTERVAL != 0) epoch_number++;

  //collect_epoch_stats(epoch_number);
  print_simulation_stats(sim_time);
  /*
  DPRINTF(FTLOut, "FTL Host epoch summarized statistics at time %lld \n", sim_time);
  DPRINTF(FTLOut, "FTL Host read count %s \n", host_epoch_read_count.print());
  DPRINTF(FTLOut, "FTL Host read size %s KB\n", host_epoch_read_count.print());
  DPRINTF(FTLOut, "FTL Host read latency %s us\n", host_epoch_read_latency.print());
  DPRINTF(FTLOut, "FTL Host read capacity %s KB\n", host_epoch_read_capacity.print());
  DPRINTF(FTLOut, "FTL Host write count %s \n", host_epoch_write_count.print());
  DPRINTF(FTLOut, "FTL Host write size %s KB\n", host_epoch_write_size.print());
  DPRINTF(FTLOut, "FTL Host write latency %s us\n", host_epoch_write_latency.print());
  DPRINTF(FTLOut, "FTL Host write capacity %s KB\n", host_epoch_write_capacity.print());

  // BW
  DPRINTF(FTLOut, "FTL Host read  BW (active): %s MB/s \n", host_epoch_read_BW_active.print());
  DPRINTF(FTLOut, "FTL Host write BW (active): %s MB/s \n", host_epoch_write_BW_active.print());
  DPRINTF(FTLOut, "FTL Host rw    BW (active): %s MB/s \n", host_epoch_rw_BW_active.print());
  DPRINTF(FTLOut, "FTL Host read  BW (total ): %s MB/s \n", host_epoch_read_BW_total.print());
  DPRINTF(FTLOut, "FTL Host write BW (total ): %s MB/s \n", host_epoch_write_BW_total.print());
  DPRINTF(FTLOut, "FTL Host rw    BW (total ): %s MB/s \n", host_epoch_rw_BW_total.print());
  DPRINTF(FTLOut, "FTL Host read  BW (only  ): %s MB/s \n", host_epoch_read_BW_only.print());
  DPRINTF(FTLOut, "FTL Host write BW (only  ): %s MB/s \n", host_epoch_write_BW_only.print());

  // IOPS
  DPRINTF(FTLOut, "FTL Host read  IOPS (active): %s \n", host_epoch_read_IOPS_active.print());
  DPRINTF(FTLOut, "FTL Host write IOPS (active): %s \n", host_epoch_write_IOPS_active.print());
  DPRINTF(FTLOut, "FTL Host rw    IOPS (active): %s \n", host_epoch_rw_IOPS_active.print());
  DPRINTF(FTLOut, "FTL Host read  IOPS (total ): %s \n", host_epoch_read_IOPS_total.print());
  DPRINTF(FTLOut, "FTL Host write IOPS (total ): %s \n", host_epoch_write_IOPS_total.print());
  DPRINTF(FTLOut, "FTL Host rw    IOPS (total ): %s \n", host_epoch_rw_IOPS_total.print());
  DPRINTF(FTLOut, "FTL Host read  IOPS (only  ): %s \n", host_epoch_read_IOPS_only.print());
  DPRINTF(FTLOut, "FTL Host write IOPS (only  ): %s \n", host_epoch_write_IOPS_only.print());
  */
}

void FTLStats::collect_epoch_stats(int ep_num){

  double current_epoch_read_size = 0;
  double current_epoch_read_latency = 0;

  double current_epoch_write_size = 0;
  double current_epoch_write_latency = 0;

  if (current_epoch_read_count != 0) {
    current_epoch_read_latency = current_epoch_read_lat_sum / (double)current_epoch_read_count;
    current_epoch_read_size = current_epoch_read_size_sum / (double)current_epoch_read_count;
  }
  if (current_epoch_write_count != 0) {
    current_epoch_write_latency = current_epoch_write_lat_sum / (double)current_epoch_write_count;
    current_epoch_write_size = current_epoch_write_size_sum / (double)current_epoch_write_count;
  }

  host_epoch_read_count           .update(current_epoch_read_count);
  host_epoch_read_size            .update(current_epoch_read_size);
  host_epoch_read_latency         .update(current_epoch_read_latency);
  host_epoch_read_capacity        .update(current_epoch_read_capacity);

  host_epoch_write_count          .update(current_epoch_write_count);
  host_epoch_write_size           .update(current_epoch_write_size);
  host_epoch_write_latency        .update(current_epoch_write_latency);
  host_epoch_write_capacity       .update(current_epoch_write_capacity);


  Tick total_time         = epoch_total_time(&events, ep_num);
  Tick active_time        = epoch_active_time(&events, ep_num);
  Tick read_active_time   = epoch_active_time(&read_events, ep_num);
  Tick write_active_time  = epoch_active_time(&write_events, ep_num);

  if (total_time == 0) {
    cout << "ERROR total time should never be zero " << endl;
    return;
  }
  host_epoch_read_BW_total        .update((current_epoch_read_capacity * USEC * USEC) / (KBYTE * total_time) );
  host_epoch_write_BW_total       .update(current_epoch_write_capacity * USEC * USEC / (KBYTE * total_time));
  host_epoch_rw_BW_total          .update((current_epoch_read_capacity + current_epoch_write_capacity) * USEC * USEC/ (KBYTE * total_time));
  host_epoch_read_IOPS_total      .update(current_epoch_read_count * USEC * USEC / (total_time));
  host_epoch_write_IOPS_total     .update(current_epoch_write_count * USEC * USEC / (total_time));
  host_epoch_rw_IOPS_total        .update((current_epoch_read_count + current_epoch_write_count) * USEC * USEC  / (total_time));

  if ((current_epoch_read_count == 0) || (current_epoch_read_capacity == 0)) {
    host_epoch_read_BW_active       .update(0);
    host_epoch_read_BW_only         .update(0);
    host_epoch_read_IOPS_active     .update(0);
    host_epoch_read_IOPS_only       .update(0);
  }else {
    host_epoch_read_BW_active       .update(current_epoch_read_capacity * USEC * USEC / (KBYTE * active_time) );
    host_epoch_read_BW_only         .update(current_epoch_read_capacity * USEC * USEC / (KBYTE * read_active_time) );
    host_epoch_read_IOPS_active     .update(current_epoch_read_count * USEC * USEC / (active_time));
    host_epoch_read_IOPS_only       .update(current_epoch_read_count * USEC * USEC / (read_active_time));
  }

  if ((current_epoch_write_count == 0) || (current_epoch_write_capacity == 0)) {
    host_epoch_write_BW_active      .update(0);
    host_epoch_write_BW_only        .update(0);
    host_epoch_write_IOPS_active    .update(0);
    host_epoch_write_IOPS_only      .update(0);
  }else {
    host_epoch_write_BW_active      .update(current_epoch_write_capacity * USEC * USEC / (KBYTE * active_time));
    host_epoch_write_BW_only        .update(current_epoch_write_capacity * USEC * USEC / (KBYTE * write_active_time));
    host_epoch_write_IOPS_active    .update(current_epoch_write_count * USEC * USEC / (active_time));
    host_epoch_write_IOPS_only      .update(current_epoch_write_count * USEC * USEC / (write_active_time));
  }

  if ((current_epoch_read_count + current_epoch_write_count == 0) || (current_epoch_read_capacity + current_epoch_write_capacity == 0)) {
    host_epoch_rw_BW_active         .update(0);
    host_epoch_rw_IOPS_active       .update(0);
  }else{
    host_epoch_rw_BW_active         .update((current_epoch_read_capacity + current_epoch_write_capacity) * USEC * USEC / (KBYTE  * active_time));
    host_epoch_rw_IOPS_active       .update((current_epoch_read_count + current_epoch_write_count) * USEC * USEC / (active_time));
  }

}

Tick FTLStats::epoch_total_time(vector<RequestInterval> * ev, int epoch_number){

  Tick start_time = (epoch_number - 1) * EPOCH_INTERVAL;

  vector<RequestInterval>::iterator it;
  for (it = ev->begin(); it != ev->end(); it++){
    if (it->epoch_number != epoch_number) continue;
    Tick s = it->arrived;
    if (start_time > s) start_time = s;
  }
  Tick extra_time = 0;
  if (start_time < ((epoch_number - 1) * EPOCH_INTERVAL))
  extra_time =  ((epoch_number - 1) * EPOCH_INTERVAL) - start_time;

  return extra_time + EPOCH_INTERVAL;
}


// bool myfunction(pair<Tick, int> i, pair<Tick, int> j){return (i.first < j.first);}
Tick FTLStats::epoch_active_time(vector<RequestInterval> * ev, int epoch_number){
  Tick at =   0;
  vector<pair<Tick, int> > sort_list;
  vector<RequestInterval >::iterator it;
  for (it = ev->begin(); it != ev->end(); it++){
    if (it->epoch_number != epoch_number) continue;
    Tick s = it->arrived;
    Tick e = it->left;
    sort_list.push_back(pair<Tick, int> (s, 1));
    sort_list.push_back(pair<Tick, int>(e, -1));
  }
  std::sort(sort_list.begin(), sort_list.end(), [] (pair<Tick, int> const& a, pair<Tick, int> const& b) { return a.first < b.first; });

  int reqcount = 0;

  vector<pair<Tick, int> >::iterator vit;

  Tick prev_time = 0;
  for (vit = sort_list.begin(); vit != sort_list.end(); vit++){
    if (reqcount != 0) {
      at += vit->first - prev_time;
    }
    if (vit->second > 0) reqcount++; else reqcount--;
    prev_time = vit->first;
  }
  return at;
}

void FTLStats::updateStats(Command *cmd){

  if (cmd == NULL) return;

  Tick arrived_tick = cmd->arrived;
  Tick left_tick = cmd->finished;

  if (cmd->operation == OPER_READ)
    read_req_leave(left_tick);
  else if (cmd->operation == OPER_WRITE)
    write_req_leave(left_tick);
  else
    return;

  add_req_pair(arrived_tick , left_tick, cmd->operation);


  int ep_num =( left_tick / EPOCH_INTERVAL) + 1;

  update_stats_for_request(cmd, ep_num);

}

void FTLStats::update_stats_for_request(Command *cmd, int ep_num){
  if (cmd == NULL) return;
  if (ep_num == last_epoch_collected + 1){
    // update for current epoch
    if (cmd->operation == OPER_READ) {
      current_epoch_read_count++;
      current_epoch_read_size_sum += cmd->size / 1024; // convert from sector to KB
      current_epoch_read_lat_sum += cmd->getLatency() / USEC; // convert from ps to us
      current_epoch_read_capacity += cmd->size / 1024;

      host_sim_read_count++;
      host_sim_read_size      .update (cmd->size / 1024);
      host_sim_read_latency   .update(cmd->getLatency() / USEC);
      host_sim_read_capacity += cmd->size / 1024;
    }else if (cmd->operation == OPER_WRITE){
      current_epoch_write_count++;
      current_epoch_write_size_sum += cmd->size / 1024; // convert from sector to KB
      current_epoch_write_lat_sum += cmd->getLatency() / USEC; // convert from ps to us
      current_epoch_write_capacity += cmd->size / 1024;

      host_sim_write_count++;
      host_sim_write_size     .update (cmd->size / 1024);
      host_sim_write_latency  .update(cmd->getLatency() / USEC);
      host_sim_write_capacity += cmd->size / 1024 ;  // convert to KB
    }

  }
  else {
    // update for next epoch
    if (cmd->operation == OPER_READ) {
      next_epoch_read_count++;
      next_epoch_read_size_sum += cmd->size / 1024; // convert from sector to KB
      next_epoch_read_lat_sum += cmd->getLatency() / USEC; // convert from ps to us
      next_epoch_read_capacity += cmd->size / 1024;

      host_sim_read_count++;
      host_sim_read_size      .update (cmd->size / 1024);
      host_sim_read_latency   .update(cmd->getLatency() / USEC);
      host_sim_read_capacity += cmd->size / 1024;

    }else if (cmd->operation == OPER_WRITE){
      next_epoch_write_count++;
      next_epoch_write_size_sum += cmd->size / 1024; // convert from sector to KB
      next_epoch_write_lat_sum += cmd->getLatency() / USEC; // convert from ps to us
      next_epoch_write_capacity += cmd->size / 1024;

      host_sim_write_count++;
      host_sim_write_size     .update (cmd->size / 1024);
      host_sim_write_latency  .update(cmd->getLatency() / USEC);
      host_sim_write_capacity += cmd->size / 1024 ;  // convert to KB
    }
  }

}


void FTLStats::add_req_pair(Tick arrived_tick, Tick left_tick, int operation){
  int epoch_number = (left_tick / EPOCH_INTERVAL) + 1;
  vector<RequestInterval>::iterator it;
  for (it = events.begin(); it != events.end(); it++){
    if (it->epoch_number != epoch_number) continue;
    Tick it_arrived = it->arrived;
    Tick it_left = it->left;
    if ( arrived_tick <= it_arrived && left_tick >= it_arrived){
      it->arrived = arrived_tick;
      if (left_tick > it_left)
      it->left = left_tick;
      break;
    }
    if (arrived_tick >= it_arrived && arrived_tick <= it_left) {
      if (left_tick >= it_left){
        it->left = left_tick;
      }
      break;
    }
  }
  if (it == events.end()){
    RequestInterval ri(arrived_tick, left_tick, epoch_number);
    events.push_back(ri);
  }
  if (operation == OPER_READ) {
    for (it = read_events.begin(); it != read_events.end(); it++){
      if (it->epoch_number != epoch_number) continue;
      Tick it_arrived = it->arrived;
      Tick it_left = it->left;
      if ( arrived_tick <= it_arrived && left_tick >= it_arrived){
        it->arrived = arrived_tick;
        if (left_tick > it_left)
        it->left = left_tick;
        break;
      }
      if (arrived_tick >= it_arrived && arrived_tick <= it_left) {
        if (left_tick >= it_left){
          it->left = left_tick;
        }
        break;
      }
    }
    if (it == read_events.end()){
      RequestInterval ri(arrived_tick, left_tick, epoch_number);
      read_events.push_back(ri);
    }

  }
  else {
    for (it = write_events.begin(); it != write_events.end(); it++){
      if (it->epoch_number != epoch_number) continue;
      Tick it_arrived = it->arrived;
      Tick it_left = it->left;
      if ( arrived_tick <= it_arrived && left_tick >= it_arrived){
        it->arrived = arrived_tick;
        if (left_tick > it_left)
        it->left = left_tick;
        break;
      }
      if (arrived_tick >= it_arrived && arrived_tick <= it_left) {
        if (left_tick >= it_left){
          it->left = left_tick;
        }
        break;
      }
    }
    if (it == write_events.end()){
      RequestInterval ri (arrived_tick, left_tick, epoch_number);
      write_events.push_back(ri);
    }
  }
}

void FTLStats::read_req_arrive(Tick arrive_time){
  // call rw_req_arrive to update overall statistics
  rw_req_arrive(arrive_time);

  sim_read_outstanding_count++;

  if (arrive_time < read_active_last_update) return;

  if (sim_read_outstanding_count > 1)
  sim_read_active_time += arrive_time - read_active_last_update;

  read_active_last_update = arrive_time;
}

void FTLStats::write_req_arrive(Tick arrive_time){
  // call rw_req_arrive to update overall statistics
  rw_req_arrive(arrive_time);

  sim_write_outstanding_count++;

  if (arrive_time < write_active_last_update) return;

  if (sim_write_outstanding_count > 1)
  sim_write_active_time += arrive_time - write_active_last_update;

  write_active_last_update = arrive_time;
}

void FTLStats::rw_req_arrive(Tick arrive_time){
  sim_rw_outstanding_count++;

  if (arrive_time < rw_active_last_update) return;

  if (sim_rw_outstanding_count > 1)
  sim_rw_active_time += arrive_time - rw_active_last_update;

  rw_active_last_update = arrive_time;
}

void FTLStats::read_req_leave (Tick leave_time){
  // call rw_req_leave to update overall statistics
  rw_req_leave(leave_time);

  sim_read_outstanding_count--;

  if (leave_time < read_active_last_update) return;

  sim_read_active_time += leave_time - read_active_last_update;
  read_active_last_update = leave_time;
}

void FTLStats::write_req_leave (Tick leave_time){
  // call rw_req_leave to update overall statistics
  rw_req_leave(leave_time);
  sim_write_outstanding_count--;
  if (leave_time < write_active_last_update) return;
  sim_write_active_time += leave_time - write_active_last_update;
  write_active_last_update = leave_time;
}

void FTLStats::rw_req_leave (Tick leave_time){
  sim_rw_outstanding_count--;
  if (leave_time < rw_active_last_update) return;
  sim_rw_active_time += leave_time - rw_active_last_update;
  rw_active_last_update = leave_time;
}
Tick FTLStats::get_read_active_time (Tick current_time){
  // make sure active time is updated up to now
  //    if (current_time != read_active_last_update)
  //    cout << "ERROR Active time for read is not updated! " << endl;

  // active time should be LE total time
  //    if (sim_read_active_time > current_time)
  //    cout << "ERROR Active time of read is more than total time! " << endl;

  return sim_read_active_time;
}

Tick FTLStats::get_write_active_time (Tick current_time){
  // make sure active time is updated up to now
  //if (current_time != write_active_last_update && write_active_last_update != 0)
  //    cout << "ERROR Active time for write is not updated! " << endl;

  // active time should be LE total time
  //    if (sim_write_active_time > current_time)
  //    cout << "ERROR Active time of write is more than total time! " << endl;

  return sim_write_active_time;

}
Tick FTLStats::get_rw_active_time(Tick current_time){
  // make sure active time is updated up to now
  //    if (current_time != rw_active_last_update)
  //    cout << "ERROR Active time for rw is not updated! " << endl;

  // active time should be LE total time
  //    if (sim_rw_active_time > current_time)
  //    cout << "ERROR Active time of rw is more than total time! " << endl;

  return sim_rw_active_time;

}

void FTLStats::print_simulation_stats(Tick sim_time){
  //    DPRINTF(FTLOut, "FTL Host simulation statistics , time: %lu \n", sim_time);

  //    DPRINTF(FTLOut, "FTL Host sim read  count %d \n",        (int)host_sim_read_count);
  //    DPRINTF(FTLOut, "FTL Host sim read  size ");             host_sim_read_size.print();
  //    DPRINTF(FTLOut, "FTL Host sim read  latency ");          host_sim_read_latency.print();
  //    DPRINTF(FTLOut, "FTL Host sim read  capacity %.2f KB\n", host_sim_read_capacity);
  //    DPRINTF(FTLOut, "FTL Host sim write count %d \n",        (int)host_sim_write_count);
  //    DPRINTF(FTLOut, "FTL Host sim write size ");             host_sim_write_size.print();
  //    DPRINTF(FTLOut, "FTL Host sim write latency ");          host_sim_write_latency.print();
  //    DPRINTF(FTLOut, "FTL Host sim write capacity %.2f KB\n", host_sim_write_capacity);

  Tick active_time = get_rw_active_time(sim_time);
  Tick total_time = sim_time;
  Tick read_active_time = get_read_active_time(sim_time);
  Tick write_active_time = get_write_active_time(sim_time);


  if (active_time != 0){
    host_sim_read_BW_active     .update(host_sim_read_capacity * USEC * USEC / (KBYTE * active_time)); // to convert ps to s
    host_sim_write_BW_active    .update(host_sim_write_capacity * USEC * USEC  / (KBYTE * active_time));
    host_sim_rw_BW_active       .update((host_sim_read_capacity + host_sim_write_capacity) * USEC * USEC / (KBYTE * active_time));

    host_sim_read_IOPS_active   .update(host_sim_read_count * USEC * USEC / (active_time));
    host_sim_write_IOPS_active  .update(host_sim_write_count * USEC * USEC / (active_time));
    host_sim_rw_IOPS_active     .update((host_sim_read_count + host_sim_write_count)  * USEC * USEC / (active_time));
  }
  if (total_time != 0){
    host_sim_read_BW_total      .update(host_sim_read_capacity * USEC * USEC / (KBYTE * total_time));
    host_sim_write_BW_total     .update(host_sim_write_capacity * USEC * USEC / (KBYTE * total_time));
    host_sim_rw_BW_total        .update((host_sim_read_capacity + host_sim_write_capacity) * USEC * USEC / (KBYTE * total_time));

    host_sim_read_IOPS_total    .update(host_sim_read_count * USEC * USEC / (total_time));
    host_sim_write_IOPS_total   .update(host_sim_write_count * USEC * USEC / (total_time));
    host_sim_rw_IOPS_total      .update((host_sim_read_count + host_sim_write_count) * USEC * USEC  / (total_time));
  }
  if (read_active_time != 0){
    host_sim_read_BW_only       .update(host_sim_read_capacity * USEC * USEC / (KBYTE * read_active_time));
    host_sim_read_IOPS_only     .update(host_sim_read_count * USEC * USEC / (read_active_time));
  }
  if (write_active_time != 0){
    host_sim_write_BW_only      .update(host_sim_write_capacity * USEC * USEC / (KBYTE * write_active_time));
    host_sim_write_IOPS_only    .update(host_sim_write_count * USEC * USEC / (write_active_time));
  }
  //    DPRINTF(FTLOut, "FTL Host sim read  BW (active): %.2f MB/s\n", host_sim_read_BW_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write BW (active): %.2f MB/s\n", host_sim_write_BW_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim rw    BW (active): %.2f MB/s\n", host_sim_rw_BW_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim read  IOPS (active): %.2f \n"  , host_sim_read_IOPS_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write IOPS (active): %.2f \n"  , host_sim_write_IOPS_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim rw    IOPS (active): %.2f \n"  , host_sim_rw_IOPS_active.Get());
  //    DPRINTF(FTLOut, "FTL Host sim read  BW (total ): %.2f MB/s\n", host_sim_read_BW_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write BW (total ): %.2f MB/s\n", host_sim_write_BW_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim rw    BW (total ): %.2f MB/s\n", host_sim_rw_BW_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim read  IOPS (total ): %.2f \n"  , host_sim_read_IOPS_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write IOPS (total ): %.2f \n"  , host_sim_write_IOPS_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim rw    IOPS (total ): %.2f \n"  , host_sim_rw_IOPS_total.Get());
  //    DPRINTF(FTLOut, "FTL Host sim read  BW (only  ): %.2f MB/s\n", host_sim_read_BW_only.Get());
  //    DPRINTF(FTLOut, "FTL Host sim read  IOPS (only  ): %.2f \n"  , host_sim_read_IOPS_only.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write BW (only  ): %.2f MB/s\n", host_sim_write_BW_only.Get());
  //    DPRINTF(FTLOut, "FTL Host sim write IOPS (only  ): %.2f \n"  , host_sim_write_IOPS_only.Get());
}
