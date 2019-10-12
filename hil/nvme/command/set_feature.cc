// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/set_feature.hh"

#include "hil/nvme/command/feature.hh"
#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

SetFeature::SetFeature(ObjectData &o, Subsystem *s) : Command(o, s) {}

void SetFeature::setRequest(ControllerData *cdata, SQContext *req) {
  auto tag = createTag(cdata, req);
  auto entry = req->getData();

  // Get parameters
  uint16_t fid = entry->dword10 & 0x00FF;
  bool save = entry->dword10 & 0x80000000;
  uint8_t uuid = entry->dword14 & 0x7F;

  debugprint_command(tag,
                     "ADMIN   | Set Features | Feature %d | NSID %d | UUID %u",
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
        tag->arbitrator->getArbitrationData()->data = entry->dword11;
        tag->arbitrator->applyArbitrationData();

        break;
      case FeatureID::PowerManagement:
        tag->controller->getFeature()->pm.data = entry->dword11;

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
            tag->controller->getFeature()->overThresholdList[idx] =
                (uint16_t)entry->dword11;
          }
          else {
            tag->controller->getFeature()->underThresholdList[idx] =
                (uint16_t)entry->dword11;
          }
        }

      } break;
      case FeatureID::ErrorRecovery:
        tag->controller->getFeature()->er.data = entry->dword11;

        break;
      case FeatureID::VolatileWriteCache: {
        auto pHIL = subsystem->getHIL();

        std::visit([dword11 = entry->dword11](
                       auto &&hil) { hil->setCache(dword11 == 1); },
                   pHIL);
      } break;
      case FeatureID::NumberOfQueues: {
        auto feature = tag->controller->getFeature();

        feature->noq.data = entry->dword11;
        tag->arbitrator->requestIOQueues(feature->noq.nsq, feature->noq.ncq);

        tag->cqc->getData()->dword0 = feature->noq.data;
      } break;
      case FeatureID::InterruptCoalescing: {
        Feature::InterruptCoalescing ic;

        ic.data = entry->dword11;

        tag->interrupt->configureCoalescing(ic.time * 100'000'000ull,
                                            ic.thr + 1);
      } break;
      case FeatureID::InterruptVectorConfiguration: {
        Feature::InterruptVectorConfiguration ivc;

        ivc.data = entry->dword11;

        tag->interrupt->enableCoalescing(ivc.cd == 0, ivc.iv);
      } break;
      case FeatureID::WriteAtomicityNormal:
        tag->controller->getFeature()->wan = entry->dword11;

        break;
      case FeatureID::AsynchronousEventConfiguration:
        // We only supports Namespace Attribute Notices.
        // See constructor of Feature.

        // tag->subsystem->getFeature()->aec.data = entry->dword11;
        tag->cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                             CommandSpecificStatusCode::FeatureNotChangeable);

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

void SetFeature::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetFeature::getStatValues(std::vector<double> &) noexcept {}

void SetFeature::resetStatValues() noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
