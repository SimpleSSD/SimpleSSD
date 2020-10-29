// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

GetFeature::GetFeature(ObjectData &o, Subsystem *s) : Command(o, s) {}

GetFeature::~GetFeature() {}

void GetFeature::setRequest(ControllerData *cdata, AbstractNamespace *,
                            SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint16_t fid = entry->dword10 & 0x00FF;
  bool save = entry->dword10 & 0x80000000;
  uint8_t uuid = entry->dword14 & 0x7F;

  debugprint_command(tag,
                     "ADMIN   | Get Features | Feature %d | NSID %d | UUID %u",
                     fid, entry->namespaceID, uuid);

  // Make response
  tag->createResponse();

  if (save) {
    // We does not support save - no power cycle in simulation
    tag->cqc->makeStatus(
        true, false, StatusType::CommandSpecificStatus,
        CommandSpecificStatusCode::FeatureIdentifierNotSaveable);
  }
  else {
    switch ((FeatureID)fid) {
      case FeatureID::Arbitration:
        tag->cqc->getData()->dword0 =
            tag->arbitrator->getArbitrationData()->data;

        break;
      case FeatureID::PowerManagement:
        tag->cqc->getData()->dword0 = tag->controller->getFeature()->pm.data;

        break;
      case FeatureID::TemperatureThreshold: {
        uint8_t sel = (entry->dword11 >> 20) & 0x03;
        uint8_t idx = (entry->dword11 >> 16) & 0x0F;

        if (idx > 9 || sel > 1) {
          tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                               GenericCommandStatusCode::Invalid_Field);
        }
        else {
          if (sel == 0) {
            tag->cqc->getData()->dword0 =
                tag->controller->getFeature()->overThresholdList[idx];
          }
          else {
            tag->cqc->getData()->dword0 =
                tag->controller->getFeature()->underThresholdList[idx];
          }
        }

      } break;
      case FeatureID::ErrorRecovery:
        tag->cqc->getData()->dword0 = tag->controller->getFeature()->er.data;

        break;
      case FeatureID::VolatileWriteCache: {
        bool enabled = readConfigUint(Section::InternalCache,
                                      ICL::Config::Key::CacheMode) != 0;

        tag->cqc->getData()->dword0 = enabled ? 1 : 0;
      } break;
      case FeatureID::NumberOfQueues:
        tag->cqc->getData()->dword0 = tag->controller->getFeature()->noq.data;

        break;
      case FeatureID::InterruptCoalescing: {
        Feature::InterruptCoalescing ic;
        uint64_t time;
        uint16_t count;

        tag->interrupt->getCoalescing(time, count);

        ic.time = (uint8_t)(time / 100'000'000ull);
        ic.thr = (uint8_t)(count - 1);

        tag->cqc->getData()->dword0 = ic.data;
      } break;
      case FeatureID::InterruptVectorConfiguration: {
        Feature::InterruptVectorConfiguration ivc;

        ivc.data = entry->dword11;
        ivc.cd = tag->interrupt->isEnabled(ivc.iv) == 0;

        tag->cqc->getData()->dword0 = ivc.data;
      } break;
      case FeatureID::WriteAtomicityNormal:
        tag->cqc->getData()->dword0 = tag->controller->getFeature()->wan;

        break;
      case FeatureID::AsynchronousEventConfiguration:
        tag->cqc->getData()->dword0 = tag->controller->getFeature()->aec.data;

        break;
      default:
        tag->cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                             GenericCommandStatusCode::Invalid_Field);

        break;
    }
  }

  // We can finish immediately
  subsystem->complete(tag);
}

void GetFeature::getStatList(std::vector<Stat> &, std::string) noexcept {}

void GetFeature::getStatValues(std::vector<double> &) noexcept {}

void GetFeature::resetStatValues() noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
