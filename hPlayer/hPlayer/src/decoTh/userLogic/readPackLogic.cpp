#include "readPackLogic.h"
#include "strFun.h"
#include "loop.h"
#include "cmdutils.h"
#include "playerDataRpc.h"
#include "decoTh.h"
#include "gLog.h"

readPackLogic:: readPackLogic (decoTh& rDecoTh):m_rDecoTh(rDecoTh)
{
}

readPackLogic:: ~readPackLogic ()
{
}

int  readPackLogic:: onLoopBegin()
{
    int  nRet = 0;
    do {
    } while (0);
    return nRet;
}

static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {
    return stream_id < 0 ||
           queue->abort_request ||
           (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
           queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList pkt1;
    int ret;

    if (q->abort_request)
       return -1;


    pkt1.pkt = pkt;
    pkt1.serial = q->serial;

    ret = av_fifo_write(q->pkt_list, &pkt1, 1);
    if (ret < 0)
        return ret;
    q->nb_packets++;
    q->size += pkt1.pkt->size + sizeof(pkt1);
    q->duration += pkt1.pkt->duration;
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacket *pkt1;
    int ret;

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }
    av_packet_move_ref(pkt1, pkt);

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt1);
    SDL_UnlockMutex(q->mutex);

    if (ret < 0)
        av_packet_free(&pkt1);

    return ret;
}

static int packet_queue_put_nullpacket(PacketQueue *q, AVPacket *pkt, int stream_index)
{
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

static int decode_interrupt_cb(void *ctx)
{
    VideoState *is = (VideoState *)ctx;
    return is->abort_request;
}

static int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->url, "rtp:", 4)
                 || !strncmp(s->url, "udp:", 4)
                )
    )
        return 1;
    return 0;
}

static int video_decoder_start(Decoder *d, int (*fn)(void *), const char *thread_name, void* arg)
{
    packet_queue_start(d->queue);
    auto pThis = (readPackLogic*)arg;
    initvideoDecAskMsg msg;
    pThis->getDecoTh().sendMsg(msg);
    
    return 0;
}

int video_stream_component_open(VideoState *is, int stream_index,  readPackLogic* pThis)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    const AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    int sample_rate;
    AVChannelLayout ch_layout = { 0 };
    int ret = 0;
    int stream_lowres = lowres;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
        goto fail;
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : is->last_audio_stream    = stream_index; forced_codec_name =    audio_codec_name; break;
        case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; forced_codec_name = subtitle_codec_name; break;
        case AVMEDIA_TYPE_VIDEO   : is->last_video_stream    = stream_index; forced_codec_name =    video_codec_name; break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        if (forced_codec_name) av_log(NULL, AV_LOG_WARNING,
                                      "No codec could be found with name '%s'\n", forced_codec_name);
        else                   av_log(NULL, AV_LOG_WARNING,
                                      "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
        ret = AVERROR(EINVAL);
        goto fail;
    }

    avctx->codec_id = codec->id;
    if (stream_lowres > codec->max_lowres) {
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        stream_lowres = codec->max_lowres;
    }
    avctx->lowres = stream_lowres;

    if (fast)
        avctx->flags2 |= AV_CODEC_FLAG2_FAST;

    ret = filter_codec_opts(codec_opts, avctx->codec_id, ic,
                            ic->streams[stream_index], codec, &opts, NULL);
    if (ret < 0)
        goto fail;

    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);

    av_dict_set(&opts, "flags", "+copy_opaque", AV_DICT_MULTIKEY);

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        ret = create_hwaccel(&avctx->hw_device_ctx);
        if (ret < 0)
            goto fail;
    }

    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        goto fail;
    }
    ret = check_avoptions(opts);
    if (ret < 0)
        goto fail;

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        {
            AVFilterContext *sink;

            is->audio_filter_src.freq           = avctx->sample_rate;
            ret = av_channel_layout_copy(&is->audio_filter_src.ch_layout, &avctx->ch_layout);
            if (ret < 0)
                goto fail;
            is->audio_filter_src.fmt            = avctx->sample_fmt;
            if ((ret = configure_audio_filters(is, afilters, 0)) < 0)
                goto fail;
            sink = is->out_audio_filter;
            sample_rate    = av_buffersink_get_sample_rate(sink);
            ret = av_buffersink_get_ch_layout(sink, &ch_layout);
            if (ret < 0)
                goto fail;
        }

        /* prepare audio output */
        if ((ret = audio_open(is, &ch_layout, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio FIFO fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
            goto fail;
        if (is->ic->iformat->flags & AVFMT_NOTIMESTAMPS) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
        if ((ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is)) < 0)
            goto out;
        SDL_PauseAudioDevice(audio_dev, 0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
            goto fail;
            packet_queue_start(is->viddec.queue);
            {
                initvideoDecAskMsg msg;
                pThis->getDecoTh().sendMsg(msg);
            }
            // if ((ret = video_decoder_start(&is->viddec, video_thread, "video_decoder", this)) < 0)
                // goto out;
            is->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread)) < 0)
            goto fail;
        if ((ret = decoder_start(&is->subdec, subtitle_thread, "subtitle_decoder", is)) < 0)
            goto out;
        break;
    default:
        break;
    }
    goto out;

fail:
    avcodec_free_context(&avctx);
out:
    av_channel_layout_uninit(&ch_layout);
    av_dict_free(&opts);

    return ret;
}



/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
    return 0;
}
int  readPackLogic:: initThis()
{
    int  nRet = 0;
    do {
        VideoState *is = getVideoState();
        auto& ic = m_ic;
        int& ret = m_ret;
        int err, i;
        int st_index[AVMEDIA_TYPE_NB];
        // AVPacket *pkt = NULL;
        // int pkt_in_play_range = 0;
        const AVDictionaryEntry *t;
        auto&  wait_mutex = m_wait_mutex;
        wait_mutex = SDL_CreateMutex();
        int scan_all_pmts_set = 0;
        // int64_t pkt_ts;

        if (!wait_mutex) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
            ret = AVERROR(ENOMEM);
            nRet = procPacketFunRetType_exitNow;
            // goto fail;
            break;
        }

        memset(st_index, -1, sizeof(st_index));
        is->eof = 0;
        auto& pkt = m_pkt;
        pkt = av_packet_alloc();
        if (!pkt) {
            av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
            ret = AVERROR(ENOMEM);
            nRet = procPacketFunRetType_exitNow;
            // goto fail;
            break;
        }
        ic = avformat_alloc_context();
        if (!ic) {
            av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
            ret = AVERROR(ENOMEM);
            nRet = procPacketFunRetType_exitNow;
            // goto fail;
            break;
        }
        ic->interrupt_callback.callback = decode_interrupt_cb;
        ic->interrupt_callback.opaque = is;
        if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
            av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
            scan_all_pmts_set = 1;
        }
        err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
        if (err < 0) {
            print_error(is->filename, err);
            ret = -1;
            nRet = procPacketFunRetType_exitNow;
            //goto fail;
            break;
        }
        if (scan_all_pmts_set)
            av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);
        remove_avoptions(&format_opts, codec_opts);

        ret = check_avoptions(format_opts);
        if (ret < 0) {
            nRet = procPacketFunRetType_exitNow;
            // goto fail;
            break;
        }
        is->ic = ic;

        if (genpts)
            ic->flags |= AVFMT_FLAG_GENPTS;

        if (find_stream_info) {
            AVDictionary **opts;
            int orig_nb_streams = ic->nb_streams;

            err = setup_find_stream_info_opts(ic, codec_opts, &opts);
            if (err < 0) {
                av_log(NULL, AV_LOG_ERROR,
                        "Error setting up avformat_find_stream_info() options\n");
                ret = err;
                nRet = procPacketFunRetType_exitNow;
                // goto fail;
                break;
            }

            err = avformat_find_stream_info(ic, opts);

            for (i = 0; i < orig_nb_streams; i++)
                av_dict_free(&opts[i]);
            av_freep(&opts);

            if (err < 0) {
                av_log(NULL, AV_LOG_WARNING,
                        "%s: could not find codec parameters\n", is->filename);
                ret = -1;
                // goto fail;
                nRet = procPacketFunRetType_exitNow;
                break;
            }
        }

        if (ic->pb)
            ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

        if (seek_by_bytes < 0)
            seek_by_bytes = !(ic->iformat->flags & AVFMT_NO_BYTE_SEEK) &&
                !!(ic->iformat->flags & AVFMT_TS_DISCONT) &&
                strcmp("ogg", ic->iformat->name);

        is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

        if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
            window_title = av_asprintf("%s - %s", t->value, input_filename);

        /* if seeking requested, we execute it */
        if (start_time != AV_NOPTS_VALUE) {
            int64_t timestamp;

            timestamp = start_time;
            /* add the stream start time */
            if (ic->start_time != AV_NOPTS_VALUE)
                timestamp += ic->start_time;
            ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
            if (ret < 0) {
                av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                        is->filename, (double)timestamp / AV_TIME_BASE);
            }
        }

        is->realtime = is_realtime(ic);

        if (show_status)
            av_dump_format(ic, 0, is->filename, 0);

        for (i = 0; i < ic->nb_streams; i++) {
            AVStream *st = ic->streams[i];
            enum AVMediaType type = st->codecpar->codec_type;
            st->discard = AVDISCARD_ALL;
            if (type >= 0 && wanted_stream_spec[type] && st_index[type] == -1)
                if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)
                    st_index[type] = i;
        }
        for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
            if (wanted_stream_spec[i] && st_index[i] == -1) {
                av_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", wanted_stream_spec[i], av_get_media_type_string((AVMediaType)i));
                st_index[i] = INT_MAX;
            }
        }

        if (!video_disable)
            st_index[AVMEDIA_TYPE_VIDEO] =
                av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                        st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
        if (!audio_disable)
            st_index[AVMEDIA_TYPE_AUDIO] =
                av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                        st_index[AVMEDIA_TYPE_AUDIO],
                        st_index[AVMEDIA_TYPE_VIDEO],
                        NULL, 0);
        if (!video_disable && !subtitle_disable)
            st_index[AVMEDIA_TYPE_SUBTITLE] =
                av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                        st_index[AVMEDIA_TYPE_SUBTITLE],
                        (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                         st_index[AVMEDIA_TYPE_AUDIO] :
                         st_index[AVMEDIA_TYPE_VIDEO]),
                        NULL, 0);

        is->show_mode = show_mode;
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
            AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
            AVCodecParameters *codecpar = st->codecpar;
            AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
            if (codecpar->width)
                set_default_window_size(codecpar->width, codecpar->height, sar);
        }

        /* open the streams */
        if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
            stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
        }

        ret = -1;
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
            ret = video_stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO], this);
        }
        if (is->show_mode == VideoState::SHOW_MODE_NONE)
            is->show_mode = ret >= 0 ? VideoState::SHOW_MODE_VIDEO : VideoState::SHOW_MODE_RDFT;

        if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
            stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
        }

        if (is->video_stream < 0 && is->audio_stream < 0) {
            av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
                    is->filename);
            ret = -1;
            // goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }
        if (infinite_buffer < 0 && is->realtime) {
            infinite_buffer = 1;
        }
    } while (0);
    return nRet;
}

void  readPackLogic:: sendExitNtfToSub()
{
    do {
        videoDecExitNtfAskMsg msg;
        decoTh& rTh =    getDecoTh();
        rTh.sendMsg(msg);
        setState (readState_waiteSubExit);
    } while (0);
}

int  readPackLogic:: onLoopFrame()
{
    int  nRet = 0;
    do {
        auto thisState = state ();
        if (readState_mainNotInit == thisState || /*readState_playEnd == thisState ||*/ readState_waiteSubExit == thisState || readState_willExit == thisState) [[unlikely]]{
            break;
        }
        if (readState_thisNeetInit == thisState) [[unlikely]]{
            nRet = initThis ();
            if (procPacketFunRetType_exitNow & nRet || procPacketFunRetType_exitAfterLoop & nRet) [[unlikely]]{
                break;
            }
            setState (readState_ok);
        }
        // for (;;) {
        auto& ic = m_ic;
        int& ret = m_ret;
        auto& pkt = m_pkt;
        auto&  wait_mutex = m_wait_mutex;
        VideoState *is = getVideoState();
        if (is->abort_request)[[unlikely]] {
            gInfo("is->abort_request will Exit");
            sendExitNtfToSub();
            // nRet = procPacketFunRetType_exitNow;
            break;
        }
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return = av_read_pause(ic);
            else
                av_read_play(ic);
        }
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            break;
        }
#endif
        if (is->seek_req) {
            int64_t seek_target = is->seek_pos;
            int64_t seek_min    = is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
            // FIXME the +-2 is due to rounding being not done in the correct direction in generation
            //      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,
                        "%s: error while seeking\n", is->ic->url);
            } else {
                if (is->audio_stream >= 0)
                    packet_queue_flush(&is->audioq);
                if (is->subtitle_stream >= 0)
                    packet_queue_flush(&is->subtitleq);
                if (is->video_stream >= 0)
                    packet_queue_flush(&is->videoq);
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                    set_clock(&is->extclk, NAN, 0);
                } else {
                    set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;
            if (is->paused)
                step_to_next_frame(is);
        }
        if (is->queue_attachments_req) {
            if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if ((ret = av_packet_ref(pkt, &is->video_st->attached_pic)) < 0) {
                    // goto fail;
                    // nRet = procPacketFunRetType_exitNow;
                    sendExitNtfToSub();
                    break;
                }
                packet_queue_put(&is->videoq, pkt);
                packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer<1 &&
                (is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
                 || (stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) &&
                     stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
                     stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            break;
        }
        if (!is->paused &&
                (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
                (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {
            if (loop != 1 && (!loop || --loop)) {
                stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else if (autoexit) {
                ret = AVERROR_EOF;
                // goto fail;
                // nRet = procPacketFunRetType_exitNow;
                sendExitNtfToSub();
                break;
            }
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, pkt, is->subtitle_stream);
                is->eof = 1;
            }
            if (ic->pb && ic->pb->error) {
                /*
                if (autoexit)
                    goto fail;
                else
                    break;
                */
                if (autoexit) {
                    // nRet = procPacketFunRetType_exitNow;
                    sendExitNtfToSub();
                }
                break;
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            setState (readState_playEnd);
            // break;
        } else {
            is->eof = 0;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        auto stream_start_time = ic->streams[pkt->stream_index]->start_time;
        auto pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        auto pkt_in_play_range = duration == AV_NOPTS_VALUE ||
            (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
            av_q2d(ic->streams[pkt->stream_index]->time_base) -
            (double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
            <= ((double)duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            packet_queue_put(&is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
        } else {
            av_packet_unref(pkt);
        }
        // }
    } while (0);
    return nRet;
}

int  readPackLogic:: onLoopEnd()
{
    int  nRet = 0;
    do {
        clean ();
    } while (0);
    return nRet;
}

void        readPackLogic:: clean()
{
    do {
        VideoState *is = getVideoState();
        if (m_ic && !is->ic) {
            avformat_close_input(&m_ic);
        }
        av_packet_free(&m_pkt);
        if (m_ret != 0) {
            SDL_Event event;
            event.type = FF_QUIT_EVENT;
            event.user.data1 = is;
            SDL_PushEvent(&event);
            m_ret = 0;
        }
        if (m_wait_mutex) {
            SDL_DestroyMutex(m_wait_mutex);
            m_wait_mutex = nullptr;
        }
    } while (0);
}

readPackLogic::readState   readPackLogic:: state ()
{
    return m_readState;
}

void   readPackLogic:: setState(readState  st)
{
    m_readState = st;
}

decoTh&     readPackLogic:: getDecoTh()
{
    return m_rDecoTh;
}

