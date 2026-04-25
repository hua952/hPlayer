#include "decoTh.h"
#include "decoThUserLogic.h"
#include "gLog.h"
#include "cppCom.h"
#include "tSingleton.h"

extern "C"
{
    // #include "ffplayCom.h"

void cpp_check_external_clock_speed(VideoState *is) {
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rVidPackQ = rGlobal.vidPackQ;
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto& rExtclk = rGlobal.m_extclk;
    auto vSize = rVidPackQ.size();
    auto aSize = rAudioPackQ.size();
   if (is->video_stream >= 0 && vSize <= EXTERNAL_CLOCK_MIN_FRAMES ||
       // is->audio_stream >= 0 && is->audioq.nb_packets <= EXTERNAL_CLOCK_MIN_FRAMES) {
       is->audio_stream >= 0 && aSize <= EXTERNAL_CLOCK_MIN_FRAMES) {
       // set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
       rExtclk.setClockSpeed(FFMAX(EXTERNAL_CLOCK_SPEED_MIN, rExtclk.speed() - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((is->video_stream < 0 || vSize > EXTERNAL_CLOCK_MAX_FRAMES) &&
              (is->audio_stream < 0 || aSize > EXTERNAL_CLOCK_MAX_FRAMES)) {
       // set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
       rExtclk.setClockSpeed(FFMIN(EXTERNAL_CLOCK_SPEED_MAX, rExtclk.speed() + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = rExtclk.speed();
       if (speed != 1.0) {
           // set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
           rExtclk.setClockSpeed(speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
       }
   }
}

}

decoThUserLogic::decoThUserLogic (decoTh& rServer):m_rdecoTh(rServer)
{
}

int decoThUserLogic::onLoopBegin()
{
    int nRet = 0;
    do {
    } while (0);
    return nRet;
}

static int cpp_decoder_init(cppDecoder& rD, AVCodecContext *avctx, packQue* queue) {
    memset(&rD, 0, sizeof(rD));
    rD.pkt = av_packet_alloc();
    if (!rD.pkt)
        return AVERROR(ENOMEM);
    rD.avctx = avctx;
    rD.queue = queue;
    rD.start_pts = AV_NOPTS_VALUE;
    rD.pkt_serial = -1;
    return 0;
}
/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rAudclk = rGlobal.m_audclk;

    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        // diff = get_clock(&is->audclk) - cpp_get_master_clock(is);
        diff = rAudclk.getClock() - cpp_get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
                }
                av_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
                        diff, avg_diff, wanted_nb_samples - nb_samples,
                        is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}
/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(VideoState *is)
{
    int data_size, resampled_data_size;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    cppFrame *af;

    auto& rGlobal = tSingleton<globalData>::single();
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto& rSampQ = rGlobal.m_sampQ;

    if (is->paused)
        return -1;

    do {
#if defined(_WIN32)
        while (rSampQ.size() == 0) {
            if ((av_gettime_relative() - audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
                return -1;
            av_usleep (1000);
        }
#endif
        // if (!(af = frame_queue_peek_readable(&is->sampq)))
        if (!(af = rSampQ.que().front())) {
            return -1;
        }
        // frame_queue_next(&is->sampq);
        rSampQ.que().pop();
    } while (af->serial != rAudioPackQ.serial());

    data_size = av_samples_get_buffer_size(NULL, af->frame->ch_layout.nb_channels,
                                           af->frame->nb_samples,
                                           (AVSampleFormat)(af->frame->format), 1);

    wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

    if (af->frame->format        != is->audio_src.fmt            ||
        av_channel_layout_compare(&af->frame->ch_layout, &is->audio_src.ch_layout) ||
        af->frame->sample_rate   != is->audio_src.freq           ||
        (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)) {
        int ret;
        swr_free(&is->swr_ctx);
        ret = swr_alloc_set_opts2(&is->swr_ctx,
                            &is->audio_tgt.ch_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                            &af->frame->ch_layout, (AVSampleFormat)(af->frame->format), af->frame->sample_rate,
                            0, NULL);
        if (ret < 0 || swr_init(is->swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                    af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)(af->frame->format)), af->frame->ch_layout.nb_channels,
                    is->audio_tgt.freq, av_get_sample_fmt_name((AVSampleFormat)(is->audio_tgt.fmt)), is->audio_tgt.ch_layout.nb_channels);
            swr_free(&is->swr_ctx);
            return -1;
        }
        if (av_channel_layout_copy(&is->audio_src.ch_layout, &af->frame->ch_layout) < 0)
            return -1;
        is->audio_src.freq = af->frame->sample_rate;
        is->audio_src.fmt = (AVSampleFormat)(af->frame->format);
    }

    if (is->swr_ctx) {
        const uint8_t **in = (const uint8_t **)af->frame->extended_data;
        uint8_t **out = &is->audio_buf1;
        int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
        int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.ch_layout.nb_channels, out_count, is->audio_tgt.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        if (wanted_nb_samples != af->frame->nb_samples) {
            if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                        wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                return -1;
            }
        }
        av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
        if (!is->audio_buf1)
            return AVERROR(ENOMEM);
        len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(is->swr_ctx) < 0)
                swr_free(&is->swr_ctx);
        }
        is->audio_buf = is->audio_buf1;
        resampled_data_size = len2 * is->audio_tgt.ch_layout.nb_channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
    } else {
        is->audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    audio_clock0 = is->audio_clock;
    /* update the audio clock with the pts */
    if (!isnan(af->pts))
        is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
    else
        is->audio_clock = NAN;
    is->audio_clock_serial = af->serial;
#ifdef DEBUG
    {
        static double last_clock;
        printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
               is->audio_clock - last_clock,
               is->audio_clock, audio_clock0);
        last_clock = is->audio_clock;
    }
#endif
    return resampled_data_size;
}

/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}
/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rAudClk = rGlobal.m_audclk;
    auto& rExtclk = rGlobal.m_extclk;

    VideoState *is = (VideoState*)opaque;
    int audio_size, len1;

    audio_callback_time = av_gettime_relative();

    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
           audio_size = audio_decode_frame(is);
           if (audio_size < 0) {
                /* if error, just output silence */
               is->audio_buf = NULL;
               is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
           } else {
               if (is->show_mode != VideoState::SHOW_MODE_VIDEO)
                   update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
            memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        else {
            memset(stream, 0, len1);
            if (!is->muted && is->audio_buf)
                SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, AUDIO_S16SYS, len1, is->audio_volume);
        }
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(is->audio_clock)) {
        // set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        // sync_clock_to_slave(&is->extclk, &is->audclk);
        rAudClk.setClockAt(is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
        cpp_sync_clock_to_slave(rExtclk, rAudClk);
    }
}


static int audio_open(void *opaque, AVChannelLayout *wanted_channel_layout, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    int wanted_nb_channels = wanted_channel_layout->nb_channels;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    wanted_nb_channels = wanted_channel_layout->nb_channels;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    while (!(audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                return -1;
            }
        }
        av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d is not supported!\n", spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, spec.channels);
        if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d is not supported!\n", spec.channels);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    if (av_channel_layout_copy(&audio_hw_params->ch_layout, wanted_channel_layout) < 0)
        return -1;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->ch_layout.nb_channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    return spec.size;
}


/* open a given stream. Return 0 if OK */
static int cpp_stream_component_open_base(VideoState *is, int stream_index, logicWorker& rWork)
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
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto& rAudDec = rGlobal.m_audDec;

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

        // if ((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
        if ((ret = cpp_decoder_init(rAudDec, avctx, &rAudioPackQ)) < 0)
            goto fail;
        if (is->ic->iformat->flags & AVFMT_NOTIMESTAMPS) {
            rAudDec.start_pts = is->audio_st->start_time;
            rAudDec.start_pts_tb = is->audio_st->time_base;
        }
        /*
        if ((ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is)) < 0)
            goto out;
            */
        // packet_queue_start(is->auddec.queue);
        rAudioPackQ.start();
        {
            initAudioAskMsg  msg;
            rWork.sendMsg(msg);
        }
        SDL_PauseAudioDevice(audio_dev, 0);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        // if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
        if ((ret = cpp_decoder_init(rGlobal.vidDec, avctx, &rGlobal.vidPackQ)) < 0)
            goto fail;
        /*
        if ((ret = decoder_start(&is->viddec, video_thread, "video_decoder", is)) < 0)
            goto out;
        */
        // packet_queue_start(is->viddec.queue);
        rGlobal.vidPackQ.start();
        {
            initvideoDecAskMsg msg;
            rWork.sendMsg(msg);
        }
        is->queue_attachments_req = 1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];

        // if ((ret = decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread)) < 0)
        if ((ret = cpp_decoder_init(rGlobal.m_subDec, avctx, &rGlobal.m_subPackQ)) < 0)
            goto fail;
        /*
        if ((ret = decoder_start(&is->subdec, subtitle_thread, "subtitle_decoder", is)) < 0)
            goto out;
        */
        // packet_queue_start(is->subdec.queue);
        rGlobal.m_subPackQ.start();
        {
            subtitleqAskMsg msg;
            rWork.sendMsg(msg);
        }
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

static int stream_component_open_base(VideoState *is, int stream_index, logicWorker& rWork)
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
        /*
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

        if ((ret = audio_open(is, &ch_layout, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
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
        packet_queue_start(is->auddec.queue);
        {
            initAudioAskMsg  msg;
            rWork.sendMsg(msg);
        }
        SDL_PauseAudioDevice(audio_dev, 0);
        */
        break;
    case AVMEDIA_TYPE_VIDEO:
        /*
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
            goto fail;
        
        packet_queue_start(is->viddec.queue);
        {
            initvideoDecAskMsg msg;
            rWork.sendMsg(msg);
        }
        is->queue_attachments_req = 1;
        */
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        /*
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];

        if ((ret = decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread)) < 0)
            goto fail;
            */
        /*
        if ((ret = decoder_start(&is->subdec, subtitle_thread, "subtitle_decoder", is)) < 0)
            goto out;
        */
        /*
        packet_queue_start(is->subdec.queue);
        {
            subtitleqAskMsg msg;
            rWork.sendMsg(msg);
        }
        */
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

/*
int cpp_packet_queue_put(PacketQueue *q, AVPacket *pkt)
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
    ret = cpp_packet_queue_put_private(q, pkt1);
    SDL_UnlockMutex(q->mutex);

    if (ret < 0)
        av_packet_free(&pkt1);

    return ret;
}
*/




int decoThUserLogic::onLoopFrame()
{
    int nRet = 0;
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rDecoder = rGlobal.vidDec;
    auto& rVidPackQ = rGlobal.vidPackQ;
    auto& rAudDec = rGlobal.m_audDec;
    auto& rPictQ = rGlobal.m_pictQ;
    auto& rSampQ = rGlobal.m_sampQ;
    auto& rSubPackQ = rGlobal.m_subPackQ;
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto& rExtclk = rGlobal.m_extclk;

    do {
        auto thisState = state ();
        if (readState_mainNotInit == thisState ||  readState_waiteSubExit == thisState || readState_willExit == thisState) [[unlikely]]{
            break;
        }
        if (readState_thisNeetInit == thisState) [[unlikely]]{
            nRet = initThis ();
            if (procPacketFunRetType_exitNow & nRet || procPacketFunRetType_exitAfterLoop & nRet) [[unlikely]]{
                break;
            }
            setState (readState_ok);
        }
        VideoState *is = getVideoState();
        auto& ic = m_ic;
        int& ret = m_ret;
        auto& pkt = m_pkt;
        // auto&  wait_mutex = m_wait_mutex;

    // for (;;) {
        /*
        if (is->abort_request) [[unlikely]] {
            gInfo("is->abort_request will Exit");
            sendExitNtfToSub();
            break;
        }
        */
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
                if (is->audio_stream >= 0) {
                    rAudioPackQ.cleanForSeek();
                    // packet_queue_flush(&is->audioq);
                }
                if (is->subtitle_stream >= 0) {
                    rSubPackQ.cleanForSeek();
                    // packet_queue_flush(&is->subtitleq);
                }
                if (is->video_stream >= 0) {
                    rVidPackQ.cleanForSeek();
                    // packet_queue_flush(&is->videoq);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                   // set_clock(&is->extclk, NAN, 0);
                   rExtclk.setClock(NAN, 0);
                } else {
                   // set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                    rExtclk.setClock(seek_target / (double)AV_TIME_BASE, 0);
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
                    sendExitNtfToSub();
                    break;
                    // goto fail;
                }
                // packet_queue_put(&is->videoq, pkt);
                auto bC = rVidPackQ.procLastUnpushPack();
                if (!bC) {
                    break;
                }
                rVidPackQ.pushPack (pkt);
                pkt->stream_index = is->video_stream;
                rVidPackQ.pushImportant(pkt);
                // packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        /* if the queue are full, no need to read more */
        if (infinite_buffer<1 &&
              (rAudioPackQ.size() + rVidPackQ.size() + rSubPackQ.size() > MAX_QUEUE_SIZE
            // || (stream_has_enough_packets(is->audio_st, is->audio_stream, &is->audioq) &&
            || (!rAudioPackQ.mabeNeetPush()  &&
                // stream_has_enough_packets(is->video_st, is->video_stream, &is->videoq) &&
                !rVidPackQ.mabeNeetPush () &&
                // stream_has_enough_packets(is->subtitle_st, is->subtitle_stream, &is->subtitleq)))) {
                !rSubPackQ.mabeNeetPush ()))) {
            /* wait 10 ms */
            
            // SDL_LockMutex(wait_mutex);
            // SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            // SDL_UnlockMutex(wait_mutex);
            break;
        }
        if (!is->paused &&
            // (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
            (!is->audio_st || (rAudDec.finished == rAudioPackQ.serial() && rSampQ.size() == 0)) &&
            // (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {
            (!is->video_st || (rDecoder.finished == rVidPackQ.serial() && rPictQ.size() == 0))) {
            if (loop != 1 && (!loop || --loop)) {
                stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
            } else if (autoexit) {
                ret = AVERROR_EOF;
                // goto fail;
                sendExitNtfToSub();
                break;
            }
        }
        auto bC = rVidPackQ.procLastUnpushPack();
        if (!bC) {
            break;
        }
        bC = rAudioPackQ.procLastUnpushPack();
        if (!bC) {
            break;
        }
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                if (is->video_stream >= 0) {
                    pkt->stream_index = is->video_stream;
                    rVidPackQ.pushImportant(pkt);
                    bC = rVidPackQ.procLastUnpushPack();
                    if (!bC) {
                        break;
                    }
                    // packet_queue_put_nullpacket(&is->videoq, pkt, is->video_stream);
                }
                if (is->audio_stream >= 0) {
                    pkt->stream_index = is->audio_stream;
                    rAudioPackQ.pushImportant(pkt);
                    bC = rAudioPackQ.procLastUnpushPack();
                    if (!bC) {
                        break;
                    }
                    // packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
                }
                if (is->subtitle_stream >= 0) {
                    pkt->stream_index = is->subtitle_stream ;
                    rSubPackQ.pushImportant(pkt);
                    bC = rSubPackQ.procLastUnpushPack();
                    if (!bC) {
                        break;
                    }
                    // packet_queue_put_nullpacket(&is->subtitleq, pkt, is->subtitle_stream);
                }
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
                    sendExitNtfToSub();
                }
                break;
            }
            /*
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            */
            break;
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
            // packet_queue_put(&is->audioq, pkt);
            rAudioPackQ.pushPack (pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   && !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
            // packet_queue_put(&is->videoq, pkt);
            rVidPackQ.pushPack (pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            // packet_queue_put(&is->subtitleq, pkt);
            rSubPackQ.pushPack (pkt);
        } else {
            av_packet_unref(pkt);
        }
    // }

    ret = 0;
    } while (0);
    return nRet;
}

int decoThUserLogic::onLoopEnd()
{
    int nRet = 0;
    do {
        clean();
    } while (0);
    return nRet;
}

static int decode_interrupt_cb(void *ctx)
{
    /*
    auto pT = (decoThUserLogic*) ctx;
    NeetExitNtfAskMsg msg;
    pT->getServer().sendMsg(msg);
    pT->setState(pT->readState_willExit);
    return 1; //is->abort_request;
    */
    return 0;
}

int  decoThUserLogic:: initThis()
{
    int  nRet = 0;
    do {

        VideoState *is = getVideoState();
        auto& ic = m_ic;
        int& ret = m_ret;
        auto& pkt = m_pkt;
        // auto&  wait_mutex = m_wait_mutex;

        int err, i;
        int st_index[AVMEDIA_TYPE_NB];
        int64_t stream_start_time;
        int pkt_in_play_range = 0;
        const AVDictionaryEntry *t;

        int scan_all_pmts_set = 0;
        int64_t pkt_ts;
/*
        wait_mutex = SDL_CreateMutex();
        if (!wait_mutex) {
            av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
            ret = AVERROR(ENOMEM);
            // goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }
*/
        memset(st_index, -1, sizeof(st_index));
        is->eof = 0;

        pkt = av_packet_alloc();
        if (!pkt) {
            av_log(NULL, AV_LOG_FATAL, "Could not allocate packet.\n");
            ret = AVERROR(ENOMEM);
            //goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }
        ic = avformat_alloc_context();
        if (!ic) {
            av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
            ret = AVERROR(ENOMEM);
            //goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }
        ic->interrupt_callback.callback = decode_interrupt_cb;
        ic->interrupt_callback.opaque = this;
        if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
            av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
            scan_all_pmts_set = 1;
        }
        err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
        if (err < 0) {
            print_error(is->filename, err);
            ret = -1;
            // goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }
        if (scan_all_pmts_set)
            av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);
        remove_avoptions(&format_opts, codec_opts);

        ret = check_avoptions(format_opts);
        if (ret < 0) {
            //goto fail;
            nRet = procPacketFunRetType_exitNow;
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
                //goto fail;
                nRet = procPacketFunRetType_exitNow;
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
                //goto fail;
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
        is->show_mode = (decltype (is->show_mode))(show_mode);
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
            AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
            AVCodecParameters *codecpar = st->codecpar;
            AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
            if (codecpar->width)
                set_default_window_size(codecpar->width, codecpar->height, sar);
        }

        /* open the streams */
        if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
            cpp_stream_component_open_base(is, st_index[AVMEDIA_TYPE_AUDIO], getServer());
        }

        ret = -1;
        if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
            ret = cpp_stream_component_open_base(is, st_index[AVMEDIA_TYPE_VIDEO], getServer());
        }
        if (is->show_mode == VideoState::SHOW_MODE_NONE)
            is->show_mode = ret >= 0 ? VideoState::SHOW_MODE_VIDEO : VideoState::SHOW_MODE_RDFT;

        if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
            stream_component_open_base(is, st_index[AVMEDIA_TYPE_SUBTITLE], getServer());
        }

        if (is->video_stream < 0 && is->audio_stream < 0) {
            av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
                    is->filename);
            ret = -1;
            //goto fail;
            nRet = procPacketFunRetType_exitNow;
            break;
        }

        if (infinite_buffer < 0 && is->realtime) {
            infinite_buffer = 1;
        }
    } while (0);
    return nRet;
}

decoThUserLogic::readState   decoThUserLogic:: state ()
{
    return m_readState;
}

void   decoThUserLogic:: setState(readState  st)
{
    m_readState = st;
}

void decoThUserLogic:: clean()
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
        /*
        if (m_wait_mutex) {
            SDL_DestroyMutex(m_wait_mutex);
            m_wait_mutex = nullptr;
        }
        */
    } while (0);
}

void  decoThUserLogic:: sendExitNtfToSub()
{
    do {
        subtitleqDecExitNtfAskMsg msg;
        getServer().sendMsg(msg);
        setState (readState_waiteSubExit);
    } while (0);
}
void   decoThUserLogic:: sendEmptyAudioPack()
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rAudioPackQ = rGlobal.m_audioPackQ;
    auto pkt = av_packet_alloc();
    auto is = getVideoState();
    pkt->stream_index = is->audio_stream;
    rAudioPackQ.pushImportant(pkt);
    
    // VideoState *is = getVideoState();
    // packet_queue_put_nullpacket(&is->audioq, pkt, is->audio_stream);
}
void     decoThUserLogic:: sendEmptySubtitleqPack()
{
    auto& rGlobal = tSingleton<globalData>::single();
    auto& rSubPackQ = rGlobal.m_subPackQ;
    auto pkt = av_packet_alloc();
    auto is = getVideoState();
    pkt->stream_index = is->subtitle_stream;
    rSubPackQ.pushImportant(pkt);
    /*
    auto pkt = av_packet_alloc();
    VideoState *is = getVideoState();
    packet_queue_put_nullpacket(&is->subtitleq, pkt, is->subtitle_stream);
    */
}

void cpp_decoder_destroy(cppDecoder& rD)
{
}

