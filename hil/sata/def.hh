// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_SATA_DEF__
#define __SIMPLESSD_HIL_SATA_DEF__

#include <cinttypes>

namespace SimpleSSD::HIL::SATA {

// AHCI Generic Host Controller Register
union GenericHostControllerRegister {
  uint8_t data[0x100];
  struct {
    uint32_t capability;
    uint32_t globalHBAControl;
    uint32_t interruptStatus;
    uint32_t portsImplemented;
    uint32_t AHCIVersion;
    uint32_t commandCompletionCoalescingControl;
    uint32_t commandCompletionCoalescingPorts;
    uint32_t enclosureManagementLocation;
    uint32_t enclosureManagementControl;
    uint32_t HBACapabilityExtended;
    uint32_t handoffControlAndStatus;
  };
};

// AHCI Port Register
union PortRegister {
  uint8_t data[0x80];
  struct {
    uint64_t commandListBaseAddress;
    uint64_t FISBaseAddress;
    uint32_t interruptStatus;
    uint32_t interruptEnable;
    uint32_t commandAndStatus;
    uint32_t reserved1;
    uint32_t taskFileData;
    uint32_t signature;
    uint32_t status;
    uint32_t control;
    uint32_t error;
    uint32_t active;
    uint32_t commandIssue;
    uint32_t notification;
    uint32_t switchingControl;
    uint32_t deviceSleep;
  };
};

// Index of AHCI GHC Register
enum : uint8_t {
  REG_CAP = 0x00,
  REG_GHC = 0x04,
  REG_IS = 0x08,
  REG_PI = 0x0C,
  REG_VS = 0x10,
  REG_CCC_CTL = 0x14,
  REG_CCC_PORTS = 0x18,
  REG_EM_LOC = 0x1C,
  REG_EM_CTL = 0x20,
  REG_CAP2 = 0x24,
  REG_BOHC = 0x28,
};

// Index of AHCI Port Register
enum : uint8_t {
  REG_P0CLB = 0x00,
  REG_P0CLBU = 0x04,
  REG_P0FB = 0x08,
  REG_P0FBU = 0x0C,
  REG_P0IS = 0x10,
  REG_P0IE = 0x14,
  REG_P0CMD = 0x18,
  REG_P0TFD = 0x20,
  REG_P0SIG = 0x24,
  REG_P0SSTS = 0x28,
  REG_P0SCTL = 0x2C,
  REG_P0SERR = 0x30,
  REG_P0SACT = 0x34,
  REG_P0CI = 0x38,
  REG_P0SNTF = 0x3C,
  REG_P0FBS = 0x40,
  REG_P0DEVSLP = 0x44
};

// Register bits
enum : uint32_t {
  // GHC > GHC (Generic HBA Control)
  HOST_RESET = (1 << 0),              /* reset controller; self-clear */
  HOST_IRQ_EN = (1 << 1),             /* global IRQ enable */
  HOST_MRSM = (1 << 2),               /* MSI Revert to Single Message */
  HOST_AHCI_EN = ((uint32_t)1 << 31), /* AHCI enabled */

  // GHC > EM_CTL
  EM_CTL_RST = (1 << 9),    /* Reset */
  EM_CTL_TM = (1 << 8),     /* Transmit Message */
  EM_CTL_MR = (1 << 0),     /* Message Received */
  EM_CTL_ALHD = (1 << 26),  /* Activity LED */
  EM_CTL_XMT = (1 << 25),   /* Transmit Only */
  EM_CTL_SMB = (1 << 24),   /* Single Message Buffer */
  EM_CTL_SGPIO = (1 << 19), /* SGPIO messages supported */
  EM_CTL_SES = (1 << 18),   /* SES-2 messages supported */
  EM_CTL_SAFTE = (1 << 17), /* SAF-TE messages supported */
  EM_CTL_LED = (1 << 16),   /* LED messages supported */

  // Port > IS/IE
  PORT_IRQ_COLD_PRES = ((uint32_t)1 << 31), /* cold presence detect */
  PORT_IRQ_TF_ERR = (1 << 30),              /* task file error */
  PORT_IRQ_HBUS_ERR = (1 << 29),            /* host bus fatal error */
  PORT_IRQ_HBUS_DATA_ERR = (1 << 28),       /* host bus data error */
  PORT_IRQ_IF_ERR = (1 << 27),              /* interface fatal error */
  PORT_IRQ_IF_NONFATAL = (1 << 26),         /* interface non-fatal error */
  PORT_IRQ_OVERFLOW = (1 << 24),            /* xfer exhausted available S/G */
  PORT_IRQ_BAD_PMP = (1 << 23),             /* incorrect port multiplier */

  PORT_IRQ_PHYRDY = (1 << 22),     /* PhyRdy changed */
  PORT_IRQ_DEV_ILCK = (1 << 7),    /* device interlock */
  PORT_IRQ_CONNECT = (1 << 6),     /* port connect change status */
  PORT_IRQ_SG_DONE = (1 << 5),     /* descriptor processed */
  PORT_IRQ_UNK_FIS = (1 << 4),     /* unknown FIS rx'd */
  PORT_IRQ_SDB_FIS = (1 << 3),     /* Set Device Bits FIS rx'd */
  PORT_IRQ_DMAS_FIS = (1 << 2),    /* DMA Setup FIS rx'd */
  PORT_IRQ_PIOS_FIS = (1 << 1),    /* PIO Setup FIS rx'd */
  PORT_IRQ_D2H_REG_FIS = (1 << 0), /* D2H Register FIS rx'd */

  // Port > CMD
  PORT_CMD_ASP = (1 << 27),     /* Aggressive Slumber/Partial */
  PORT_CMD_ALPE = (1 << 26),    /* Aggressive Link PM enable */
  PORT_CMD_ATAPI = (1 << 24),   /* Device is ATAPI */
  PORT_CMD_FBSCP = (1 << 22),   /* FBS Capable Port */
  PORT_CMD_ESP = (1 << 21),     /* External Sata Port */
  PORT_CMD_HPCP = (1 << 18),    /* HotPlug Capable Port */
  PORT_CMD_PMP = (1 << 17),     /* PMP attached */
  PORT_CMD_LIST_ON = (1 << 15), /* cmd list DMA engine running */
  PORT_CMD_FIS_ON = (1 << 14),  /* FIS DMA engine running */
  PORT_CMD_FIS_RX = (1 << 4),   /* Enable FIS receive DMA engine */
  PORT_CMD_CLO = (1 << 3),      /* Command list override */
  PORT_CMD_POWER_ON = (1 << 2), /* Power up device */
  PORT_CMD_SPIN_UP = (1 << 1),  /* Spin up device */
  PORT_CMD_START = (1 << 0),    /* Enable port DMA engine */

  // ATA Status
  ATA_BUSY = (1 << 7),  /* BSY status bit */
  ATA_DRDY = (1 << 6),  /* device ready */
  ATA_DF = (1 << 5),    /* device fault */
  ATA_DSC = (1 << 4),   /* drive seek complete */
  ATA_DRQ = (1 << 3),   /* data request i/o */
  ATA_CORR = (1 << 2),  /* corrected data error */
  ATA_SENSE = (1 << 1), /* sense code available */
  ATA_ERR = (1 << 0),   /* have an error */

  // ATA Error
  ATA_ICRC = (1 << 7),    /* interface CRC error */
  ATA_BBK = ATA_ICRC,     /* pre-EIDE: block marked bad */
  ATA_UNC = (1 << 6),     /* uncorrectable media error */
  ATA_MC = (1 << 5),      /* media changed */
  ATA_IDNF = (1 << 4),    /* ID not found */
  ATA_MCR = (1 << 3),     /* media change requested */
  ATA_ABORTED = (1 << 2), /* command aborted */
  ATA_TRK0NF = (1 << 1),  /* track 0 not found */
  ATA_AMNF = (1 << 0),    /* address mark not found */
};

union CommandHeader {
  uint8_t data[32];
  struct {
    uint16_t flags;
    uint16_t prdtLength;
    uint32_t prdByteCount;
    uint64_t commandTableBaseAddress;
    uint32_t reserved[4];
  };
};

// FIS Type
enum class FISType : uint8_t {
  Host2Device = 0x27,
  Device2Host = 0x34,
  DMAActivate = 0x39,
  DMASetup = 0x41,
  Data = 0x46,
  BuiltInSelfTest = 0x58,
  PIOSetup = 0x5F,
  DeviceBits = 0xA1,
};

// Register - H2D
struct Host2Device {
  uint8_t type;
  uint8_t flag;
  uint8_t command;
  uint8_t featureL;
  uint8_t lbaL[3];
  uint8_t device;
  uint8_t lbaH[3];
  uint8_t featureH;
  uint8_t countL;
  uint8_t countH;
  uint8_t icc;
  uint8_t control;
};

// Register - D2H
struct Device2Host {
  uint8_t type;
  uint8_t flag;
  uint8_t status;
  uint8_t error;
  uint8_t lbaL[3];
  uint8_t device;
  uint8_t lbaH[3];
  uint8_t reserved;
  uint8_t countL;
  uint8_t countH;
};

// Set Device Bits
struct SetDeviceBits {
  uint8_t type;
  uint8_t flag;
  uint8_t status;
  uint8_t command;
  uint32_t payload;
};

// DMA Activate
struct DMAActivate {
  uint8_t type;
  uint8_t flag;
};

// DMA Setup
struct DMASetup {
  uint8_t type;
  uint8_t flag;
  uint16_t reserved1;
  uint32_t reserved2[3];
  uint32_t bufferOffset;
  uint32_t transferCount;
};

// PIO Setup
struct PIOSetup {
  uint8_t type;
  uint8_t flag;
  uint8_t status;
  uint8_t error;
  uint8_t lbaL[3];
  uint8_t device;
  uint8_t lbaH[3];
  uint8_t reserved1;
  uint8_t countL;
  uint8_t countH;
  uint8_t reserved2;
  uint8_t e_status;
  uint16_t transferCount;
};

// FIS
union FIS {
  uint8_t data[64];
  union {
    Host2Device host2Device;
    Device2Host device2Host;
    SetDeviceBits setDeviceBits;
    DMAActivate dmaActivate;
    DMASetup dmaSetup;
    PIOSetup pioSetup;
  };
};

// ACS-2 IDENTIFY DEVICE
enum {
  ATA_ID_CONFIG = 0,
  ATA_ID_SERNO = 10,
  ATA_ID_FW_REV = 23,
  ATA_ID_PROD = 27,
  ATA_ID_MAX_MULTSECT = 47,
  ATA_ID_DWORD_IO = 48,
  ATA_ID_CAPABILITY = 49,
  ATA_ID_FIELD_VALID = 53,
  ATA_ID_MULTSECT = 59,
  ATA_ID_LBA_CAPACITY = 60,
  ATA_ID_MWDMA_MODES = 63,
  ATA_ID_PIO_MODES = 64,
  ATA_ID_EIDE_DMA_MIN = 65,
  ATA_ID_EIDE_DMA_TIME = 66,
  ATA_ID_EIDE_PIO = 67,
  ATA_ID_EIDE_PIO_IORDY = 68,
  ATA_ID_QUEUE_DEPTH = 75,
  ATA_ID_SATA_CAPABILITY = 76,
  ATA_ID_SATA_CAPABILITY_2 = 77,
  ATA_ID_FEATURE_SUPP = 78,
  ATA_ID_MAJOR_VER = 80,
  ATA_ID_COMMAND_SET_1 = 82,
  ATA_ID_COMMAND_SET_2 = 83,
  ATA_ID_CFSSE = 84,
  ATA_ID_CFS_ENABLE_1 = 85,
  ATA_ID_CFS_ENABLE_2 = 86,
  ATA_ID_CSF_DEFAULT = 87,
  ATA_ID_UDMA_MODES = 88,
  ATA_ID_HW_CONFIG = 93,
  ATA_ID_SPG = 98,
  ATA_ID_LBA_CAPACITY_2 = 100,
  ATA_ID_SECTOR_SIZE = 106,
  ATA_ID_WWN = 108,
  ATA_ID_LOGICAL_SECTOR_SIZE = 117, /* and 118 */
  ATA_ID_COMMAND_SET_3 = 119,
  ATA_ID_COMMAND_SET_4 = 120,
  ATA_ID_LAST_LUN = 126,
  ATA_ID_DLF = 128,
  ATA_ID_DATA_SET_MGMT = 169,
};

// ACS-2 Command Set
enum class Command : uint8_t {
  DatasetManagement = 0x06,
  ExecuteDeviceDiagnostics = 0x90,
  FlushCache = 0xE7,
  FlushCacheExt = 0xEA,
  IdentifyDevice = 0xEC,
  ReadDMA = 0xC8,
  ReadDMAExt = 0x25,
  ReadFPDMAQueued = 0x60,
  ReadSector = 0x20,
  ReadSectorExt = 0x24,
  ReadVerifySector = 0x40,
  ReadVerifySectorExt = 0x42,
  SetFeature = 0xEF,
  SetMultiplyMode = 0xC6,
  WriteDMA = 0xCA,
  WriteDMAExt = 0x35,
  WriteFPDMAQueued = 0x61,
  WriteSector = 0x30,
  WriteSectorExt = 0x34,
};

// ACS-2 SET FEATURE Fields
enum class Feature : uint8_t {
  EnableVolatileCache = 0x02,
  SetXferMode,
};

}  // namespace SimpleSSD::HIL::SATA

#endif
