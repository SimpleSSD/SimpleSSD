// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_DEF_HH__
#define __SIMPLESSD_HIL_NVME_DEF_HH__

#include <cinttypes>
#include <cstring>

#include "util/algorithm.hh"

namespace SimpleSSD::HIL::NVMe {

#define NSID_NONE 0x00000000
#define NSID_LOWEST 0x00000001
#define NSID_ALL 0xFFFFFFFF

#define OCSSD_VENDOR 0x1D1D
#define OCSSD_SSVID_1_2 0x0102
#define OCSSD_SSVID_2_0 0x0200

// Controller-wide Unique Command ID
#define MAKE_CCID(qid, cid) MAKE32(qid, cid)

// Global (SSD-wide) Unique Command ID
#define MAKE_GCID(ctrlid, ccid) MAKE64(ctrlid, ccid)

const uint32_t nLBAFormat = 4;
const uint32_t lbaFormat[nLBAFormat] = {
    0x02090000,  // 512B + 0, Good performance
    0x020A0000,  // 1KB + 0, Good performance
    0x010B0000,  // 2KB + 0, Better performance
    0x000C0000,  // 4KB + 0, Best performance
};
const uint32_t lbaSize[nLBAFormat] = {
    512,   // 512B
    1024,  // 1KB
    2048,  // 2KB
    4096,  // 4KB
};

enum class QueuePriority : uint8_t {
  Urgent,
  High,
  Medium,
  Low,
};

enum class Register : uint64_t {
  ControllerCapabilities = 0x00,
  ControllerCapabilitiesH = 0x04,
  Version = 0x08,
  InterruptMaskSet = 0x0C,
  InterruptMaskClear = 0x10,
  ControllerConfiguration = 0x14,
  ControllerStatus = 0x1C,
  NVMSubsystemReset = 0x20,
  AdminQueueAttributes = 0x24,
  AdminSQBaseAddress = 0x28,
  AdminSQBaseAddressH = 0x2C,
  AdminCQBaseAddress = 0x30,
  AdminCQBaseAddressH = 0x34,
  ControllerMemoryBufferLocation = 0x38,
  ControllerMemoryBufferSize = 0x3C,
  BootPartitionInformation = 0x40,
  BootPartitionReadSelect = 0x44,
  BootPartitionMemoryBufferLocation = 0x48,
  ControllerMemoryBufferMemorySpaceControll = 0x50,
  ControllerMemorybufferStatus = 0x58,
  PersistentMemoryCapabilities = 0xE00,
  PersistentMemoryRegionControl = 0xE04,
  PersistentMemoryRegionStatus = 0xE08,
  PersistentMemoryRegionElasticityBufferSize = 0xE0C,
  PersistentMemoryRegionSustainedWriteThroughput = 0xE10,
  PersistentMemoryRegionControllerMemorySpaceControl = 0xE14,

  DoorbellBegin = 0x1000,
};

enum class Arbitration : uint8_t {
  RoundRobin,
  WeightedRoundRobin,
};

enum class AdminCommand : uint8_t {
  DeleteIOSQ = 0x00,
  CreateIOSQ = 0x01,
  GetLogPage = 0x02,
  DeleteIOCQ = 0x04,
  CreateIOCQ = 0x05,
  Identify = 0x06,
  Abort = 0x08,
  SetFeatures = 0x09,
  GetFeatures = 0x0A,
  AsyncEventRequest = 0x0C,
  NamespaceManagement = 0x0D,
  FirmwareCommit = 0x10,
  FirmwareDownload = 0x11,
  DeviceSelfTest = 0x14,
  NamespaceAttachment = 0x15,
  KeepAlive = 0x18,
  DirectiveSend = 0x19,
  DirectiveReceive = 0x1A,
  VirtualizationManagement = 0x1C,
  NVMeMISend = 0x1D,
  NVMeMIReceive = 0x1E,
  DoorbellBufferConfig = 0x7C,

  /** NVM Command Set Specific Admin Command **/
  FormatNVM = 0x80,
  SecuritySend = 0x81,
  SecurityReceive = 0x82,
  Sanitize = 0x84,
  GetLBAStatus = 0x86,

  /** OpenChannel SSD 1.2 **/
  DeviceIdentification = 0xE2,
  SetBadBlockTable = 0xF1,
  GetBadBlockTable = 0xF2,

  /** OpenChannel SSD 2.0 **/
  Geometry = 0xE2
};

enum class IOCommand : uint8_t {
  /** NVM Command Set **/
  Flush = 0x00,
  Write = 0x01,
  Read = 0x02,
  WriteUncorrectable = 0x04,
  Compare = 0x05,
  WriteZeroes = 0x08,
  DatasetManagement = 0x09,
  Verify = 0x0C,
  ReservationRegister = 0x0D,
  ReservationReport = 0x0E,
  ReservationAcquire = 0x11,
  ReservationRelease = 0x15,

  /** Key Value Command Set **/
  Store = 0x01,     // Write
  Retrieve = 0x02,  // Read
  Delete = 0x10,
  Exist = 0x14,
  List = 0x06,

  /** Zoned Namespace Command Set **/
  ZoneManagementSend = 0x79,
  ZoneManagementReceive = 0x7A,
  ZoneAppend = 0x7D,

  /** OpenChannel SSD 1.2 **/
  PhysicalBlockErase = 0x90,
  PhysicalPageWrite,
  PhysicalPageRead,
  PhysicalPageRawWrite = 0x95,
  PhysicalPageRawRead,

  /** OpenChannel SSD 2.0 **/
  VectorChunkReset = 0x90,
  VectorChunkWrite,
  VectorChunkRead,
  VectorChunkCopy
};

enum class LogPageID : uint8_t {
  None,
  ErrorInformation,
  SMARTInformation,
  FirmwareSlotInformation,
  ChangedNamespaceList,
  CommandsSupportedAndEffects,
  DeviceSelfTest,
  TelemetryHostInitiated,
  TelemetryControllerInitiated,
  EnduranceGroupInformation,
  PredictableLatencyPerNVMSet,
  PredictableLatencyEventAggregate,
  AsymmetricNamespaceAccess,
  PersistentEventLog,
  LBAStatusInformation,
  EnduranceGroupEventAggregate,
  ReservationNotification = 0x80,
  SanitizeStatus,

  // OpenChannel SSD
  ChunkInformation = 0xCA
};

enum class IdentifyStructure : uint8_t {
  Namespace,   //!< For specified NSID or common namespace capabilities
  Controller,  //!< For controller processing the command
  ActiveNamespaceList,
  NamespaceIdentificationDescriptorList,  //!< For specified NSID
  NVMSetList,
  IOCommandSetSpecificNamespace,
  IOCommandSetSpecificController,
  IOCommandSetSpecificActiveNamespaceList,
  AllocatedNamespaceList = 0x10,
  AllocatedNamespace,
  AttachedControllerList,  //!< For specified NSID
  ControllerList,          //!< For NVM Subsystem
  PrimaryControllerCapabilities,
  SecondaryControllerList,
  NamespaceGranularityList,
  UUIDList,
  IOCommandSetSpecificAllocatedNamespaceList = 0x1A,
  IOCommandSetSpecificAllocatedNamespace,
  IOCommandSet,
};

enum class CommandSetIdentifier : uint8_t {
  NVM,
  KeyValue,
  ZonedNamespace,

  Invalid = 0xFF,
};

enum class FeatureID : uint8_t {
  Arbitration = 0x01,
  PowerManagement,
  LBARangeType,
  TemperatureThreshold,
  ErrorRecovery,
  VolatileWriteCache,
  NumberOfQueues,
  InterruptCoalescing,
  InterruptVectorConfiguration,
  WriteAtomicityNormal,
  AsynchronousEventConfiguration,
  AutoPowerStateTransition,
  HostMemoryBuffer,
  Timestamp,
  KeepAliveTimer,
  HostControlledThermalManagement,
  NonOperationalPowerStateConfig,
  ReadRecoveryLevelConfig,
  PredictableLatencyModeConfig,
  PredictableLatencyModeWindow,
  LBAStatusInformationReportInterval,
  HostBehaviorSupport,
  SanitizeConfig,
  EnduranceGroupEventConfiguration,
  SoftwareProgressMarker = 0x80,
  HostIdentifier,
  ReservationNotificationMask,
  ReservationPersistence,
  NamespaceWriteProtectionConfig,

  // OpenChannel SSD
  MediaFeedback = 0xCA
};

enum class AsyncEventType : uint8_t {
  ErrorStatus,
  HealthStatus,
  Notice,
  NVMCommandSetSpecificStatus = 6,
};

enum class ErrorStatusCode : uint8_t {
  WriteToInvalidDoorbell,
  InvalidDoorbellValue,
  DiagnosticFailure,
  PersistentInternalError,
  TransientInternalError,
  FirmwareImageLoadError,
};

enum class HealthStatusCode : uint8_t {
  NVMSubsystemReliability,
  TemperatureThreshold,
  SpareBelowThreshold,
};

enum class NoticeCode : uint8_t {
  NamespaceAttributeChanged,
  FirmwareActivationStarting,
  TelemetryLogChanged,
  AsymmetricNamespaceAccessChange,
  PredictableLatencyEventAggregateLogChange,
  LBAStatusInformationAlert,
  EnduranceGroupEventAggregateLogPageChange,
};

enum class StatusType : uint8_t {
  GenericCommandStatus,
  CommandSpecificStatus,
  MediaAndDataIntegrityErrors,
  PathRelatedStatus,
};

enum class GenericCommandStatusCode : uint8_t {
  /** Generic Command Status **/
  Success,
  Invalid_Opcode,
  Invalid_Field,
  CommandIDConflict,
  DataTransferError,
  Abort_PowerLossNotification,
  InternalError,
  Abort_Requested,
  Abort_SQDeletion,
  Abort_FailedFusedCommand,
  Abort_MissingFusedCommand,
  Invalid_NamespaceOrFormat,
  Abort_CommandSequenceError,
  Invalid_SGLDescriptor,
  Invalid_NumberOfSGLDescriptors,
  Invalid_DataSGLLength,
  Invalid_MetadataSGLLength,
  Invalid_SGLDescriptorType,
  Invalid_UseOfControllerMemoryBuffer,
  Invalid_PRPOffset,
  AtomicWriteUnitExceeded,
  OperationDenied,
  Invalid_SGLOffset,
  // Invalid_SGLSubType,
  HostIdentifierInconsistentFormat = 0x18,
  KeepAliveTimerExpired,
  Invalid_KeepAliveTimeout,
  Abort_PreemptAndAbort,
  SanitizeFailed,
  SanitizeInProgress,
  Invalid_SGLDataBlockGranularity,
  CommandNotSupportedForQueueInCMB,
  NamespaceIsWriteProtected,
  CommandInterrupted,
  TransientTransportError,

  /** I/O Command Status **/
  LBAOutOfRange = 0x80,
  CapacityExceeded,
  NamespaceNotReady,
  ReservationConflict,
  FormatInProgress,
};

enum class CommandSpecificStatusCode : uint8_t {
  /** Generic Command Errors **/
  Invalid_CompletionQueue,
  Invalid_QueueIdentifier,
  Invalid_QueueSize,
  AbortCommandLimitExceeded,
  AsynchronousEventRequestLimitExceeded = 0x05,
  Invalid_FirmwareSlot,
  Invalid_FirmwareImage,
  Invalid_InterruptVector,
  Invalid_LogPage,
  Invalid_Format,
  FirmwareActivationRequiresConventionalReset,
  Invalid_QueueDeletion,
  FeatureIdentifierNotSaveable,
  FeatureNotChangeable,
  FeatureNotNamespaceSpecific,
  FirmwareActivationRequiresNVMSubsystemReset,
  FirmwareActivationRequiresControllerLevelReset,
  FirmwareActivationRequiresMaximumTimeViolation,
  FirmwareActivationProhibited,
  OverlappingRange,
  NamespaceInsufficientCapacity,
  NamespaceIdentifierUnavailable,
  NamespaceAlreadyAttached = 0x18,
  NamespaceIsPrivate,
  NamespaceNotAttached,
  ThinProvisioningNotSupported,
  Invalid_ControllerList,
  DeviceSelfTestInProgress,
  BootPartitionWriteProhibited,
  Invalid_ControllerIdentifier,
  Invalid_SecondaryControllerState,
  Invalid_NumberOfControllerResources,
  Invalid_ResourceIdentifier,
  SanitizeProhibitedWhilePersistentMemoryRegionIsEnabled,
  Invalid_ANAGroupIdentifier,
  ANAAttachFailed,
  IOCommandSetNotSupported = 0x29,
  IOCommandSetNotEnabled,
  IOCommandSetCombinationRejected,
  Invalid_IOCommandSet,

  /** NVM Command Errors **/
  ConflictingAttributes = 0x80,
  InvalidProtectionInformation,
  WriteToReadOnlyRange,

  /** Key Value Command Errors **/
  InvalidValueSize = 0x85,
  InvalidKeySize,
  KeyNotExists,
  UnrecoveredError,
  KeyExists,

  /** Zoned Namespace Command Errors **/
  ZoneBoundaryError = 0xB8,
  ZoneIsFull,
  ZoneIsReadOnly,
  ZoneIsOffline,
  ZoneInvalidWrite,
  TooManyActiveZones,
  TooManyOpenZones,
  InvalidZoneStateTransition,
};

enum class MediaAndDataIntegrityErrorCode : uint8_t {
  /** I/O Command Media Errors **/
  WriteFault = 0x80,
  UnrecoveredReadError,
  EndToEndGuardCheckError,
  EndToEndApplicationTagCheckError,
  EndToEndReferenceTagCheckError,
  CompareFailure,
  AccessDenied,
  DeallocatedOrUnwrittenLogicalBlock,
};

enum class PathRelatedStatusCode : uint8_t {
  InternalPathError,
  AsymmetricAccessPersistentLoss,
  AsymmetricAccessInaccessible,
  AsymmetricAccessTransition,
  ControllerPathingError = 0x60,
  HostPathingError = 0x70,
  CommandAbortedByHost,
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
