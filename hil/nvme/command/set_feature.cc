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

SetFeature::SetFeature(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {}

SetFeature::~SetFeature() {}

void SetFeature::setRequest(SQContext *req) {
  sqc = req;
  auto entry = sqc->getData();

  // Get parameters
  uint16_t fid = entry->dword10 & 0x00FF;
  bool save = entry->dword10 & 0x80000000;
  uint8_t uuid = entry->dword14 & 0x7F;

  debugprint_command("ADMIN   | Set Features | Feature %d | NSID %d | UUID %u",
                     fid, entry->namespaceID, uuid);

  // Make response
  createResponse();

  if (save) {
    // We does not support save - no power cycle in simulation
    cqc->makeStatus(true, false, StatusType::CommandSpecificStatus,
                    CommandSpecificStatusCode::FeatureIdentifierNotSaveable);
  }
  else {
    switch ((FeatureID)fid) {
      case FeatureID::Arbitration:
        data.arbitrator->getArbitrationData()->data = entry->dword11;
        data.arbitrator->applyArbitrationData();

        break;
      case FeatureID::PowerManagement:
        data.subsystem->getFeature()->pm.data = entry->dword11;

        break;
      case FeatureID::TemperatureThreshold: {
        uint8_t sel = (entry->dword11 >> 20) & 0x03;
        uint8_t idx = (entry->dword11 >> 16) & 0x0F;

        if (idx > 9 || sel > 1) {
          cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                          GenericCommandStatusCode::Invalid_Field);
        }
        else {
          if (sel == 0) {
            data.subsystem->getFeature()->overThresholdList[idx] =
                (uint16_t)entry->dword11;
          }
          else {
            data.subsystem->getFeature()->underThresholdList[idx] =
                (uint16_t)entry->dword11;
          }
        }

      } break;
      case FeatureID::ErrorRecovery:
        data.subsystem->getFeature()->er.data = entry->dword11;

        break;
      case FeatureID::VolatileWriteCache: {
        auto pHIL = data.subsystem->getHIL();

        std::visit([dword11 = entry->dword11](
                       auto &&hil) { hil->setCache(dword11 == 1); },
                   pHIL);
      } break;
      case FeatureID::NumberOfQueues: {
        auto feature = data.subsystem->getFeature();

        feature->noq.data = entry->dword11;
        data.arbitrator->requestIOQueues(feature->noq.nsq, feature->noq.ncq);

        cqc->getData()->dword0 = feature->noq.data;
      } break;
      case FeatureID::InterruptCoalescing: {
        Feature::InterruptCoalescing ic;

        ic.data = entry->dword11;

        data.interrupt->configureCoalescing(ic.time * 100'000'000ull,
                                            ic.thr + 1);
      } break;
      case FeatureID::InterruptVectorConfiguration: {
        Feature::InterruptVectorConfiguration ivc;

        ivc.data = entry->dword11;

        data.interrupt->enableCoalescing(ivc.cd == 0, ivc.iv);
      } break;
      case FeatureID::WriteAtomicityNormal:
        data.subsystem->getFeature()->wan = entry->dword11;

        break;
      case FeatureID::AsynchronousEventConfiguration:
        data.subsystem->getFeature()->aec.data = entry->dword11;

        break;
      default:
        cqc->makeStatus(true, false, StatusType::GenericCommandStatus,
                        GenericCommandStatusCode::Invalid_Field);

        break;
    }
  }

  // We can finish immediately
  data.subsystem->complete(this);
}

void SetFeature::getStatList(std::vector<Stat> &, std::string) noexcept {}

void SetFeature::getStatValues(std::vector<double> &) noexcept {}

void SetFeature::resetStatValues() noexcept {}

void SetFeature::createCheckpoint(std::ostream &out) noexcept {
  Command::createCheckpoint(out);
}

void SetFeature::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
