/* This file is part of the CARTA Image Viewer: https://github.com/CARTAvis/carta-backend
   Copyright 2018, 2019, 2020, 2021 Academia Sinica Institute of Astronomy and Astrophysics (ASIAA),
   Associated Universities, Inc. (AUI) and the Inter-University Institute for Data Intensive Astronomy (IDIA)
   SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "OnMessageTask.h"

#include <algorithm>
#include <cstring>

//#include <fmt/format.h>
//#include <fmt/ostream.h>

#include "EventHeader.h"
#include "Logger/Logger.h"
#include "Util.h"

OnMessageTask* MultiMessageTask::execute() {
    switch (_header.type) {
        case CARTA::EventType::SET_SPATIAL_REQUIREMENTS: {
            CARTA::SetSpatialRequirements message;
            if (message.ParseFromArray(_event_buffer, _event_length)) {
                _session->OnSetSpatialRequirements(message);
            } else {
                spdlog::warn("Bad SET_SPATIAL_REQUIREMENTS message!\n");
            }
            break;
        }
        case CARTA::EventType::SET_SPECTRAL_REQUIREMENTS: {
            CARTA::SetSpectralRequirements message;
            if (message.ParseFromArray(_event_buffer, _event_length)) {
                _session->OnSetSpectralRequirements(message);
            } else {
                spdlog::warn("Bad SET_SPECTRAL_REQUIREMENTS message!\n");
            }
            break;
        }
        case CARTA::EventType::SET_STATS_REQUIREMENTS: {
            CARTA::SetStatsRequirements message;
            if (message.ParseFromArray(_event_buffer, _event_length)) {
                _session->OnSetStatsRequirements(message);
            } else {
                spdlog::warn("Bad SET_STATS_REQUIREMENTS message!\n");
            }
            break;
        }
        case CARTA::EventType::SET_REGION: {
            CARTA::SetRegion message;
            if (message.ParseFromArray(_event_buffer, _event_length)) {
                _session->OnSetRegion(message, _header.request_id);
            } else {
                spdlog::warn("Bad SET_REGION message!\n");
            }
            break;
        }
        case CARTA::EventType::REMOVE_REGION: {
            CARTA::RemoveRegion message;
            if (message.ParseFromArray(_event_buffer, _event_length)) {
                _session->OnRemoveRegion(message);
            } else {
                spdlog::warn("Bad REMOVE_REGION message!\n");
            }
            break;
        }
        default: {
            spdlog::warn("Bad event type in MultiMessageType:execute : ({})", _header.type);
            break;
        }
    }

    return nullptr;
}

OnMessageTask* SetImageChannelsTask::execute() {
    std::pair<CARTA::SetImageChannels, uint32_t> request_pair;
    bool tester;

    _session->ImageChannelLock(fileId);
    //    tester = _session->_set_channel_queue.try_pop(request_pair);
    tester = _session->_set_channel_queues[fileId].try_pop(request_pair);

    _session->ImageChannelTaskSetIdle(fileId);
    _session->ImageChannelUnlock(fileId);

    if (tester) {
        _session->ExecuteSetChannelEvt(request_pair);
    }

    return nullptr;
}

OnMessageTask* SetImageViewTask::execute() {
    _session->_file_settings.ExecuteOne("SET_IMAGE_VIEW", _file_id);
    return nullptr;
}

OnMessageTask* SetCursorTask::execute() {
    _session->_file_settings.ExecuteOne("SET_CURSOR", _file_id);
    return nullptr;
}

OnMessageTask* SetHistogramRequirementsTask::execute() {
    CARTA::SetHistogramRequirements message;
    if (message.ParseFromArray(_event_buffer, _event_length)) {
        _session->OnSetHistogramRequirements(message, _header.request_id);
    }

    return nullptr;
}

OnMessageTask* AnimationTask::execute() {
    bool loop;
    do {
        loop = false;
        if (_session->ExecuteAnimationFrame()) {
            if (_session->CalculateAnimationFlowWindow() > _session->CurrentFlowWindowSize()) {
                _session->SetWaitingTask(true);
            } else {
                //            increment_ref_count();
                //            recycle_as_safe_continuation();
                loop = true;
            }
        } else {
            if (!_session->WaitingFlowEvent()) {
                _session->CancelAnimation();
            }
        }
    } while (loop);

    return nullptr;
}

OnMessageTask* OnAddRequiredTilesTask::execute() {
    _session->OnAddRequiredTiles(_message);
    return nullptr;
}

OnMessageTask* OnSetContourParametersTask::execute() {
    _session->OnSetContourParameters(_message);
    return nullptr;
}


OnMessageTask* RegionDataStreamsTask::execute() {
    _session->RegionDataStreams(_file_id, _region_id);
    return nullptr;
}

OnMessageTask* SpectralProfileTask::execute() {
    _session->SendSpectralProfileData(_file_id, _region_id);
    return nullptr;
}

/**/
OnMessageTask* OnSpectralLineRequestTask::execute() {
    _session->OnSpectralLineRequest(_message, _request_id);
    return nullptr;
}
/**/
