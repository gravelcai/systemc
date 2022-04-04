/*****************************************************************************

  Licensed to Accellera Systems Initiative Inc. (Accellera) under one or
  more contributor license agreements.  See the NOTICE file distributed
  with this work for additional information regarding copyright ownership.
  Accellera licenses this file to you under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with the
  License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
  implied.  See the License for the specific language governing
  permissions and limitations under the License.

 *****************************************************************************/

/*****************************************************************************

  simulation_callbacks.cpp -- Test of simulation stage callbacks

  Note: requires simulation stage callback support enabled in the kernel
        SC_ENABLE_STAGE_CALLBACKS / --enable-stage-callbacks

  Original Author: Philipp A. Hartmann, OFFIS, 2013-05-17

 *****************************************************************************/

/*****************************************************************************

  MODIFICATION LOG - modifiers, enter your name, affiliation, date and
  changes you are making here.

      Name, Affiliation, Date:
  Description of Modification:

 *****************************************************************************/

#include <systemc.h>

#ifdef BENCHMARK
#  include "rusage_timer.h"
#  define  ROUNDS 20
#  define  NUM_TIMED_TRIGGERS 2000000
#  define  NUM_DELTA_TRIGGERS 50
#  define  VERBOSE 0
#else
#  define  ROUNDS 2
#  define  NUM_TIMED_TRIGGERS 10
#  define  NUM_DELTA_TRIGGERS 1
#  define  VERBOSE 1
#endif

#ifndef REGISTER_CALLBACKS
#  define REGISTER_CALLBACKS 1
#endif
#ifndef EXTRA_METHOD
#  define EXTRA_METHOD 0
#endif

#define TIMED_THREAD 1

#if TIMED_THREAD
#  define TIMED_WAIT    wait
#  define TIMED_PROCESS SC_THREAD
#else
#  define TIMED_WAIT    next_trigger
#  define TIMED_PROCESS SC_METHOD
#endif

#if REGISTER_CALLBACKS
#  define CALLBACK_MASK ( SC_POST_UPDATE | SC_PRE_TIMESTEP )
#else
// during SC_RUNNING (for EXTRA_METHOD)
#  define CALLBACK_MASK ( SC_STAGE_NONE )
#endif

static const sc_dt::uint64 max_rounds         = ROUNDS;
static const sc_dt::uint64 max_timed_triggers = NUM_TIMED_TRIGGERS;
static const sc_dt::uint64 max_delta_triggers = NUM_DELTA_TRIGGERS;
static const sc_time  delay(1, SC_NS);


class stage_tracer : public sc_module, public sc_stage_callback_if
{
public:
  SC_HAS_PROCESS(stage_tracer);
  stage_tracer( sc_module_name = sc_core::sc_gen_unique_name("stage_tracer") )
    : cb_count(0)
  {
#if REGISTER_CALLBACKS
    sc_register_stage_callback( *this, CALLBACK_MASK );
#endif
  }

  void stage_callback(const sc_stage& stage)
  {
    cb_count++;

#   if VERBOSE
    {
      std::string ttp;
      if( !sc_pending_activity() ) {
        ttp = "MAX";
      } else {
        ttp = sc_time_to_pending_activity().to_string();
      }
      std::cout << name()
                << "::stage_callback, sc_status = "
                << sc_get_status()
		<< ", sc_stage = " << stage
                << ", time = " << sc_time_stamp()
                << " -> pending activity: " << ttp
                << std::endl;
    }
#   endif
  }

  ~stage_tracer()
      { print_static_stage_stats( "[destructor]" ); }

  void print_static_stage_stats( const char* stage )
  {
#if VERBOSE
      std::cout << name()
                << ": " << stage << ": "
                << cb_count << " callbacks called."
                << std::endl;
#endif
  }

private:
  sc_dt::uint64 cb_count;
};

SC_MODULE(activities)
{
  SC_CTOR(activities)
    : timed_count(), delta_count()
  {
    TIMED_PROCESS(timed);
      sensitive << timed_ev;
      dont_initialize();
    SC_METHOD(delta);
      sensitive << delta_ev;
      dont_initialize();
#if EXTRA_METHOD
    SC_METHOD(extra);
      sensitive << timed_ev;
      dont_initialize();
#endif
  }

  void notify_round()
    { timed_ev.notify(SC_ZERO_TIME); }

private:
  void timed()
  {
#   if TIMED_THREAD
    while(1)
#   endif
    {
      if( timed_count >= max_timed_triggers ) {
        timed_count = 0;
        TIMED_WAIT();
      } else {
        verbose();
        ++timed_count;
        if( max_delta_triggers )
          delta_ev.notify(SC_ZERO_TIME);
        timed_ev.notify(delay);
        TIMED_WAIT();
      }
    }
  }

  void delta()
  {
    ++delta_count;
    verbose();
    if( delta_count >= max_delta_triggers ) {
      delta_count = 0;
    } else {
      delta_ev.notify(SC_ZERO_TIME);
    }
  }

  void extra()
  {
    if( sc_pending_activity_at_current_time() ) {
      pt.stage_callback((sc_stage)(SC_STAGE_NONE));
      next_trigger(SC_ZERO_TIME);
    } else if (sc_time_to_pending_activity()== sc_max_time()-sc_time_stamp() ) {
      next_trigger();
    } else {
      pt.stage_callback((sc_stage)(SC_STAGE_NONE));
      next_trigger(sc_time_to_pending_activity());
    }
  }

  void verbose()
  {
#if VERBOSE
    std::cout
      << sc_get_current_process_handle().name()
      << ": " << sc_time_stamp()
      << ": " << timed_count << "/" << delta_count
      << std::endl;
#endif
  }

private:
  stage_tracer pt;
  sc_dt::uint64 timed_count, delta_count;
  sc_event timed_ev, delta_ev;
};


int sc_main(int, char*[])
{
  activities   top("top");

  sc_start(SC_ZERO_TIME);
  for(unsigned i=0; i<max_rounds; ++i)
  {
#ifdef BENCHMARK
    rusage_timer timer;
#endif
    top.notify_round();
    sc_start();
#ifdef BENCHMARK
    std::cout << timer.to_seconds() << std::endl;
#endif
  }
  sc_stop();
  return 0;
}
