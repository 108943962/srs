/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <srs_app_pithy_print.hpp>

#include <stdlib.h>
#include <map>

#include <srs_kernel_log.hpp>
#include <srs_app_config.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_utility.hpp>

SrsStageInfo::SrsStageInfo(int _stage_id)
{
    stage_id = _stage_id;
    nb_clients = 0;
    age = 0;
    
    update_print_time();
    
    _srs_config->subscribe(this);
}

SrsStageInfo::~SrsStageInfo()
{
    _srs_config->unsubscribe(this);
}

void SrsStageInfo::update_print_time()
{
    interval = _srs_config->get_pithy_print();
}

void SrsStageInfo::elapse(srs_utime_t diff)
{
    age += diff;
}

bool SrsStageInfo::can_print()
{
    srs_utime_t can_print_age = nb_clients * interval;
    
    bool can_print = age >= can_print_age;
    if (can_print) {
        age = 0;
    }
    
    return can_print;
}

srs_error_t SrsStageInfo::on_reload_pithy_print()
{
    update_print_time();
    return srs_success;
}

static std::map<int, SrsStageInfo*> _srs_stages;

SrsPithyPrint::SrsPithyPrint(int _stage_id)
{
    stage_id = _stage_id;
    client_id = enter_stage();
    previous_tick = srs_get_system_time();
    _age = 0;
}

///////////////////////////////////////////////////////////
// pithy-print consts values
///////////////////////////////////////////////////////////
// the pithy stage for all play clients.
#define SRS_CONSTS_STAGE_PLAY_USER 1
// the pithy stage for all publish clients.
#define SRS_CONSTS_STAGE_PUBLISH_USER 2
// the pithy stage for all forward clients.
#define SRS_CONSTS_STAGE_FORWARDER 3
// the pithy stage for all encoders.
#define SRS_CONSTS_STAGE_ENCODER 4
// the pithy stage for all hls.
#define SRS_CONSTS_STAGE_HLS 5
// the pithy stage for all ingesters.
#define SRS_CONSTS_STAGE_INGESTER 6
// the pithy stage for all edge.
#define SRS_CONSTS_STAGE_EDGE 7
// the pithy stage for all stream caster.
#define SRS_CONSTS_STAGE_CASTER 8
// the pithy stage for all http stream.
#define SRS_CONSTS_STAGE_HTTP_STREAM 9
// the pithy stage for all http stream cache.
#define SRS_CONSTS_STAGE_HTTP_STREAM_CACHE 10
// for the ng-exec stage.
#define SRS_CONSTS_STAGE_EXEC 11
// for the rtc play
#define SRS_CONSTS_STAGE_RTC_PLAY 12
// for the rtc send
#define SRS_CONSTS_STAGE_RTC_SEND 13
// for the rtc recv
#define SRS_CONSTS_STAGE_RTC_RECV 14

SrsPithyPrint* SrsPithyPrint::create_rtmp_play()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_PLAY_USER);
}

SrsPithyPrint* SrsPithyPrint::create_rtmp_publish()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_PUBLISH_USER);
}

SrsPithyPrint* SrsPithyPrint::create_hls()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_HLS);
}

SrsPithyPrint* SrsPithyPrint::create_forwarder()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_FORWARDER);
}

SrsPithyPrint* SrsPithyPrint::create_encoder()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_ENCODER);
}

SrsPithyPrint* SrsPithyPrint::create_exec()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_EXEC);
}

SrsPithyPrint* SrsPithyPrint::create_ingester()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_INGESTER);
}

SrsPithyPrint* SrsPithyPrint::create_edge()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_EDGE);
}

SrsPithyPrint* SrsPithyPrint::create_caster()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_CASTER);
}

SrsPithyPrint* SrsPithyPrint::create_http_stream()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_HTTP_STREAM);
}

SrsPithyPrint* SrsPithyPrint::create_http_stream_cache()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_HTTP_STREAM_CACHE);
}

SrsPithyPrint* SrsPithyPrint::create_rtc_play()
{
    return new SrsPithyPrint(SRS_CONSTS_STAGE_RTC_PLAY);
}

SrsPithyPrint* SrsPithyPrint::create_rtc_send(int fd)
{
    return new SrsPithyPrint(fd<<16 | SRS_CONSTS_STAGE_RTC_SEND);
}

SrsPithyPrint* SrsPithyPrint::create_rtc_recv(int fd)
{
    return new SrsPithyPrint(fd<<16 | SRS_CONSTS_STAGE_RTC_RECV);
}

SrsPithyPrint::~SrsPithyPrint()
{
    leave_stage();
}

int SrsPithyPrint::enter_stage()
{
    SrsStageInfo* stage = NULL;
    
    std::map<int, SrsStageInfo*>::iterator it = _srs_stages.find(stage_id);
    if (it == _srs_stages.end()) {
        stage = new SrsStageInfo(stage_id);
        _srs_stages[stage_id] = stage;
    } else {
        stage = it->second;
    }
    
    srs_assert(stage != NULL);
    client_id = stage->nb_clients++;
    
    srs_verbose("enter stage, stage_id=%d, client_id=%d, nb_clients=%d",
                stage->stage_id, client_id, stage->nb_clients);
    
    return client_id;
}

void SrsPithyPrint::leave_stage()
{
    SrsStageInfo* stage = _srs_stages[stage_id];
    srs_assert(stage != NULL);
    
    stage->nb_clients--;
    
    srs_verbose("leave stage, stage_id=%d, client_id=%d, nb_clients=%d",
                stage->stage_id, client_id, stage->nb_clients);
}

void SrsPithyPrint::elapse()
{
    SrsStageInfo* stage = _srs_stages[stage_id];
    srs_assert(stage != NULL);
    
    srs_utime_t diff = srs_get_system_time() - previous_tick;
    diff = srs_max(0, diff);
    
    stage->elapse(diff);
    _age += diff;
    previous_tick = srs_get_system_time();
}

bool SrsPithyPrint::can_print()
{
    SrsStageInfo* stage = _srs_stages[stage_id];
    srs_assert(stage != NULL);
    
    return stage->can_print();
}

srs_utime_t SrsPithyPrint::age()
{
    return _age;
}


