/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2012 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2004 - 2010, Intel Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "PhyDxeUtil.h"
#include "EmacDxeUtil.h"

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>

/**
	Phy initialization config.
	1.detece phy devices
	2.phy devices config

	@param PhyDriver		A point to Phy dirver structureM
	@param MacBaseAddress 	GMAC register base address

    @retval EFI_SUCCESS            The phy interface was started.
	@retval EFI_DEVICE_ERROR       The command could not be sent to the phy interface.
**/
EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS   Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  //
  // initialize the phyaddr
  //
  PhyDriver->PhyAddr = 0;
  PhyDriver->PhyCurrentLink = LINK_DOWN;
  PhyDriver->PhyOldLink = LINK_DOWN;

  Status = PhyDetectDevice (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  PhyConfig (PhyDriver, MacBaseAddress);

  return EFI_SUCCESS;
}


/**
	Detect phy devices.

	@param PhyDriver		  A point to Phy dirver structureM
	@param MacBaseAddress     GMAC register base address

	@retval EFI_SUCCESS			 The phy interface was detected.
	@retval EFI_DEVICE_ERROR	 Fail to detect ethernet phy.
**/
EFI_STATUS
EFIAPI
PhyDetectDevice (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  UINT32       PhyAddr;
  EFI_STATUS   Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  for (PhyAddr = 0; PhyAddr < 32; PhyAddr++) {
    Status = PhyReadId (PhyAddr, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      continue;
    }

    PhyDriver->PhyAddr = PhyAddr;
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "SNP:PHY: Fail to detect Ethernet PHY!\r\n"));
  return EFI_NOT_FOUND;

}

/**
	Phy devices config,common config ,AR8035 or KSZ9031 config.
	1.phy soft reset
	2.phy special config,AR8035 or KSZ9031
	3.Configure AN and Advertise

	@param PhyDriver			A point to Phy dirver structureM
	@param MacBaseAddress 		GMAC register base address

	@retval EFI_SUCCESS		    Success to config phy.
	@retval EFI_DEVICE_ERROR	Fail to config phy.
**/
EFI_STATUS
EFIAPI
PhyConfig (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  )
{
  EFI_STATUS  Status;
  UINT32 value;
  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  Status = PhySoftReset (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }
  #ifdef PHY_RTL8211F
        DEBUG ((DEBUG_INFO, "SNP:PHY: begin config phy RTL8211\r\n"));
        PhyWrite(PhyDriver->PhyAddr, PHY_SPECIAL_PHY_CTLR, LCR_PAGE, MacBaseAddress);
        PhyWrite(PhyDriver->PhyAddr, LCR_REG, 0xC102, MacBaseAddress);
        PhyWrite(PhyDriver->PhyAddr, EEELCR_REG, 0x0000, MacBaseAddress);
        PhyWrite(PhyDriver->PhyAddr, PHY_SPECIAL_PHY_CTLR, 0x0000, MacBaseAddress);
       
  #endif
  #ifdef PHY_AR8035
	  DEBUG ((DEBUG_INFO, "SNP:PHY: begin config phy AR8035!\r\n"));
	  // ar8035 mmd reg 0x0B bit15 set 0,close hibernate
	  PhyWrite(PhyDriver->PhyAddr, 0x1d, 0x0B,MacBaseAddress);
	  Status = PhyRead(PhyDriver->PhyAddr,0x1e,&value,MacBaseAddress);
	  PhyWrite(PhyDriver->PhyAddr, 0x1e, (value&0x7FFF),MacBaseAddress);
	  Status = PhyRead(PhyDriver->PhyAddr,0x1e,&value,MacBaseAddress);


  #endif
  #ifdef PHY_KSZ9031
  	  DEBUG ((DEBUG_INFO, "SNP:PHY: begin config phy AR8035!\r\n"));
  	  //
	  // Configure TX/RX Skew
      //
	  PhyConfigSkew (PhyDriver, MacBaseAddress);
      //
	  // Read back and display Skew settings
      //
	  PhyDisplayConfigSkew (PhyDriver, MacBaseAddress);
      //
	  // Configure AN FLP Burst Trasmit timing interval
      //
	  PhyConfigFlpBurstTiming (PhyDriver, MacBaseAddress);
	  PhyDisplayFlpBurstTiming (PhyDriver, MacBaseAddress);
  #endif
  // Configure AN and Advertise
  PhyAutoNego (PhyDriver, MacBaseAddress);

  return EFI_SUCCESS;
}

/**
	Perform PHY software reset.

	@param PhyDriver			  A point to Phy dirver structure
	@param MacBaseAddress 	 	  GMAC register base address

	@retval EFI_SUCCESS		  	  Success to soft reset phy.
	@retval EFI_TIMEOUT	  		  timeout to soft reset phy.
**/
EFI_STATUS
EFIAPI
PhySoftReset (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  UINT32        TimeOut;
  UINT32        Data32;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  // PHY Basic Control Register reset
  PhyWrite (PhyDriver->PhyAddr, PHY_BASIC_CTRL, PHYCTRL_RESET, MacBaseAddress);

  // Wait for completion
  TimeOut = 0;
  do {
    // Read PHY_BASIC_CTRL register from PHY
    Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_CTRL, &Data32, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYCTRL_RESET become zero
    if ((Data32 & PHYCTRL_RESET) == 0) {
      break;
    }
    MicroSecondDelay(1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: ERROR! PhySoftReset timeout\n"));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}


/**
	Read the phy devices ID.

	@param PhyAddr			    The phy address
	@param MacBaseAddress 		GMAC register base address

	@retval EFI_SUCCESS			Success to read phy ID.
**/
EFI_STATUS
EFIAPI
PhyReadId (
  IN UINT32   PhyAddr,
  IN UINTN    MacBaseAddress
  )
{
  EFI_STATUS    Status;
  UINT32        PhyId1;
  UINT32        PhyId2;

  Status = PhyRead (PhyAddr, PHY_ID1, &PhyId1, MacBaseAddress);
  if (EFI_ERROR (Status)) {
      return Status;
  }
  Status = PhyRead (PhyAddr, PHY_ID2, &PhyId2, MacBaseAddress);
  if (EFI_ERROR (Status)) {
      return Status;
  }

  if (PhyId1 == PHY_INVALID_ID || PhyId2 == PHY_INVALID_ID) {
    return EFI_NOT_FOUND;
  }

  DEBUG ((DEBUG_INFO, "SNP:PHY: Ethernet PHY detected. PHY_ID1=0x%04X, PHY_ID2=0x%04X, PHY_ADDR=0x%02X\r\n",
          PhyId1, PhyId2, PhyAddr));
  return EFI_SUCCESS;
}

/**
	Config Phy Skew function.

	@param PhyDriver		  A point to Phy dirver structure
	@param MacBaseAddress 	  GMAC register base address
**/
VOID
EFIAPI
PhyConfigSkew (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_CONTROL_PAD_SKEW_REG,
                        PHY_KSZ9031RN_CONTROL_PAD_SKEW_VALUE,
                        MacBaseAddress);
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_CLK_PAD_SKEW_REG,
                        PHY_KSZ9031RN_CLK_PAD_SKEW_VALUE,
                        MacBaseAddress);
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_DEV_ADDR,
                        PHY_KSZ9031RN_RX_DATA_PAD_SKEW_REG,
                        PHY_KSZ9031RN_RX_DATA_PAD_SKEW_VALUE,
                        MacBaseAddress);
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_DEV_ADDR,
                        PHY_KSZ9031RN_TX_DATA_PAD_SKEW_REG,
                        PHY_KSZ9031RN_TX_DATA_PAD_SKEW_VALUE,
                        MacBaseAddress);
}

/**
	Read Phy Skew function configuration.

	@param PhyDriver		A point to Phy dirver structure
	@param MacBaseAddress 	GMAC register base address
**/
VOID
EFIAPI
PhyDisplayConfigSkew (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  // Display skew configuration
  DEBUG ((DEBUG_INFO, "SNP:PHY: Control Signal Pad Skew = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC,
          PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_CONTROL_PAD_SKEW_REG, MacBaseAddress)));

  DEBUG ((DEBUG_INFO, "SNP:PHY: RGMII Clock Pad Skew    = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC,
          PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_CLK_PAD_SKEW_REG, MacBaseAddress)));

  DEBUG ((DEBUG_INFO, "SNP:PHY: RGMII RX Data Pad Skew  = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC,
          PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_RX_DATA_PAD_SKEW_REG, MacBaseAddress)));

  DEBUG ((DEBUG_INFO, "SNP:PHY: RGMII TX Data Pad Skew  = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC,
          PHY_KSZ9031RN_DEV_ADDR, PHY_KSZ9031RN_TX_DATA_PAD_SKEW_REG, MacBaseAddress)));
}

/**
	Config Phy AN FLP Burst Transmit.

	@param PhyDriver		A point to Phy dirver structure
	@param MacBaseAddress 	GMAC register base address
**/
VOID
EFIAPI
PhyConfigFlpBurstTiming (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_MMD_DEV_ADDR_00,
                        PHY_KSZ9031RN_MMD_D0_FLP_LO_REG,
                        PHY_KSZ9031RN_MMD_D0_FLP_16MS_LO,
                        MacBaseAddress);
  Phy9031ExtendedWrite (PhyDriver,
                        PHY_KSZ9031_MOD_DATA_NO_POST_INC,
                        PHY_KSZ9031RN_MMD_DEV_ADDR_00,
                        PHY_KSZ9031RN_MMD_D0_FLP_HI_REG,
                        PHY_KSZ9031RN_MMD_D0_FLP_16MS_HI,
                        MacBaseAddress);
}

/**
	Read Phy AN FLP Burst Transmit configuration.

	@param PhyDriver	    A point to Phy dirver structure
	@param MacBaseAddress   GMAC register base address
**/
VOID
EFIAPI
PhyDisplayFlpBurstTiming (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  // Display Auto-Negotiation FLP burst transmit timing
  DEBUG ((DEBUG_INFO, "SNP:PHY: AN FLP Burst Transmit - LO = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC,
          PHY_KSZ9031RN_MMD_DEV_ADDR_00, PHY_KSZ9031RN_MMD_D0_FLP_LO_REG, MacBaseAddress)));
  DEBUG ((DEBUG_INFO, "SNP:PHY: AN FLP Burst Transmit - HI = 0x%04X\r\n",
          Phy9031ExtendedRead (PhyDriver, PHY_KSZ9031_MOD_DATA_NO_POST_INC, PHY_KSZ9031RN_MMD_DEV_ADDR_00,
          PHY_KSZ9031RN_MMD_D0_FLP_HI_REG, MacBaseAddress)));
}

/**
	Do phy auto-negotiation.
	1.Read PHY Status
	2.Read PHY Auto-Nego Advertise capabilities register for 10/100 Base-T
	3.Set Advertise capabilities for 10Base-T/10Base-T full-duplex/100Base-T/100Base-T full-duplex
	4.Set Advertise capabilities for 1000 Base-T/1000 Base-T full-duplex
	5.Enable and restart Auto-Negotiation.

	@param PhyDriver	    A point to Phy dirver structure
	@param MacBaseAddress   GMAC register base address

	@retval EFI_SUCCESS		auto-negotiation config success.
	@retval EFI_DEVICE_ERROR	Auto-negotiation is not supported
**/
EFI_STATUS
EFIAPI
PhyAutoNego (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS    Status;
  UINT32        PhyControl;
  UINT32        PhyStatus;
  UINT32        Features;

  DEBUG ((DEBUG_INFO, "SNP:PHY: %a ()\r\n", __FUNCTION__));

  // Read PHY Status
  Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &PhyStatus, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check PHY Status if auto-negotiation is supported
  if ((PhyStatus & PHYSTS_AUTO_CAP) == 0) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: Auto-negotiation is not supported.\n"));
    return EFI_DEVICE_ERROR;
  }

  // Read PHY Auto-Nego Advertise capabilities register for 10/100 Base-T
  Status = PhyRead (PhyDriver->PhyAddr, PHY_AUTO_NEG_ADVERT, &Features, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Set Advertise capabilities for 10Base-T/10Base-T full-duplex/100Base-T/100Base-T full-duplex
  Features |= (PHYANA_10BASET | PHYANA_10BASETFD | PHYANA_100BASETX | PHYANA_100BASETXFD);
  PhyWrite (PhyDriver->PhyAddr, PHY_AUTO_NEG_ADVERT, Features, MacBaseAddress);

  // Read PHY Auto-Nego Advertise capabilities register for 1000 Base-T
  Status = PhyRead (PhyDriver->PhyAddr, PHY_1000BASE_T_CONTROL, &Features, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Set Advertise capabilities for 1000 Base-T/1000 Base-T full-duplex
  Features |= (PHYADVERTISE_1000FULL | PHYADVERTISE_1000HALF);
  PhyWrite (PhyDriver->PhyAddr, PHY_1000BASE_T_CONTROL, Features, MacBaseAddress);

  // Read control register
  Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_CTRL, &PhyControl, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  // Enable Auto-Negotiation
  PhyControl |= PHYCTRL_AUTO_EN;
  // Restart auto-negotiation
  PhyControl |= PHYCTRL_RST_AUTO;
  // Write this configuration
  PhyWrite (PhyDriver->PhyAddr, PHY_BASIC_CTRL, PhyControl, MacBaseAddress);

  return EFI_SUCCESS;
}

/**
	Phy link adjust config.
	1.check phy link status.
	2.read phy link capability.
	3.GMAC adjust config.

	@param PhyDriver		A point to Phy dirver structure
	@param MacBaseAddress   GMAC register base address

	@retval EFI_SUCCESS		phy link up.
	@retval EFI_NOT_READY	phy link down.
**/
EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  UINT32       Speed;
  UINT32       Duplex;
  EFI_STATUS   Status;

  Status = EFI_SUCCESS;
  Speed = SPEED_10;
  Duplex = DUPLEX_HALF;

  DEBUG((EFI_D_ERROR, "%a() Line = %d \n", __FUNCTION__, __LINE__));
  Status = PhyCheckLinkStatus (PhyDriver, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    PhyDriver->PhyCurrentLink = LINK_DOWN;
    DEBUG((EFI_D_ERROR, "%a() Line = %d \n", __FUNCTION__, __LINE__));
	Status = EFI_NOT_READY;
  } else {
    PhyDriver->PhyCurrentLink = LINK_UP;
    DEBUG((EFI_D_ERROR, "%a() Line = %d \n", __FUNCTION__, __LINE__));
	Status = EFI_SUCCESS;
  }

  if (PhyDriver->PhyOldLink != PhyDriver->PhyCurrentLink) {
    if (PhyDriver->PhyCurrentLink == LINK_UP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is up - Network Cable is Plugged\r\n"));
      PhyReadCapability (PhyDriver, &Speed, &Duplex, MacBaseAddress);
      EmacConfigAdjust (Speed, Duplex, MacBaseAddress);
      Status = EFI_SUCCESS;
    } else {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Link is Down - Network Cable is Unplugged?\r\n"));
      Status = EFI_NOT_READY;
    }
  } else if (PhyDriver->PhyCurrentLink == LINK_DOWN) {
    Status = EFI_NOT_READY;
  }

  PhyDriver->PhyOldLink = PhyDriver->PhyCurrentLink;

  return Status;
}

/**
	Check phy link status.
	1.check phy link existed or not.
	2.wait until Auto Negotiation completed.

	@param PhyDriver		A point to Phy dirver structure
	@param MacBaseAddress   GMAC register base address

	@retval EFI_SUCCESS		phy link up.
	@retval EFI_TIMEOUT		phy link down.
**/
EFI_STATUS
EFIAPI
PhyCheckLinkStatus (
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS    Status;
  UINT32        Data32;
  UINTN         TimeOut;
  UINT32        PhyBasicStatus;

  // Get the PHY Status
  Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &PhyBasicStatus, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // if Link is already up then dont need to proceed anymore
  if (PhyBasicStatus & PHYSTS_LINK_STS) {
    return EFI_SUCCESS;
  }

  // Wait until it is up or until Time Out
  TimeOut = 0;
  do {
    // Read PHY_BASIC_STATUS register from PHY
    Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &Data32, MacBaseAddress);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    // Wait until PHYSTS_LINK_STS become one
    if (Data32 & PHYSTS_LINK_STS) {
      // Link is up
      break;
    }
    MicroSecondDelay (1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    // Link is down
    return EFI_TIMEOUT;
  }

  // Wait until autonego process has completed
  TimeOut = 0;
  do {
    // Read PHY_BASIC_STATUS register from PHY
    Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &Data32, MacBaseAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    // Wait until PHYSTS_AUTO_COMP become one
    if (Data32 & PHYSTS_AUTO_COMP) {
      DEBUG ((DEBUG_INFO, "SNP:PHY: Auto Negotiation completed\r\n"));
      break;
    }
    MicroSecondDelay (1);
  } while (TimeOut++ < PHY_TIMEOUT);
  if (TimeOut >= PHY_TIMEOUT) {
    DEBUG ((DEBUG_INFO, "SNP:PHY: Error! Auto Negotiation timeout\n"));
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

/**
	Read phy capability.

	@param PhyDriver		A point to Phy dirver structure
	@param speed			ethernet speed,10M/100M/1000M
	@param duplex			Duplex mode,half/full
	@param MacBaseAddress 	GMAC register base address

	@retval EFI_SUCCESS	    Read success
**/
EFI_STATUS
EFIAPI
PhyReadCapability (
  IN PHY_DRIVER   *PhyDriver,
  OUT UINT32       *Speed,
  OUT UINT32       *Duplex,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS    Status;
  UINT32        PartnerAbilityGb;
  UINT32        AdvertisingGb;
  UINT32        CommonAbilityGb;
  UINT32        PartnerAbility;

  // For 1000 Base-T
  Status = PhyRead (PhyDriver->PhyAddr, PHY_1000BASE_T_STATUS, &PartnerAbilityGb, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PhyRead (PhyDriver->PhyAddr, PHY_1000BASE_T_CONTROL, &AdvertisingGb, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CommonAbilityGb = PartnerAbilityGb & (AdvertisingGb << 2);

  // For 10/100 Base-T

  Status = PhyRead (PhyDriver->PhyAddr, PHY_AUTO_NEG_LINK_ABILITY, &PartnerAbility, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Determine the Speed and Duplex
  if (PartnerAbilityGb & (PHYLPA_1000FULL | PHYLPA_1000HALF)) {
    *Speed = SPEED_1000;
    if (CommonAbilityGb & PHYLPA_1000FULL) {
      *Duplex = DUPLEX_FULL;
    }
	else{
		*Duplex = DUPLEX_HALF;
	}
  }
  else {
  	if (PartnerAbility & (PHYLPA_100FULL | PHYLPA_100HALF)){
		*Speed = SPEED_100;
		if (PartnerAbility & PHYLPA_100FULL) {
		  *Duplex = DUPLEX_FULL;
		}
		else{
		  *Duplex = DUPLEX_HALF;
		}
  	}
	else  {
      *Speed = SPEED_10;
	  	if (PartnerAbility & PHYLPA_10FULL) {
		  *Duplex = DUPLEX_FULL;
		}
		else{
		  *Duplex = DUPLEX_HALF;
		}
    }
  }

  PhyDisplayAbility (*Speed, *Duplex);

  return EFI_SUCCESS;
}

/**
	Display phy capability.

	@param speed			  ethernet speed,10M/100M/1000M
	@param duplex 		  	  Duplex mode,half/full
**/
VOID
EFIAPI
PhyDisplayAbility (
  IN UINT32   Speed,
  IN UINT32   Duplex
  )
{

  DEBUG ((DEBUG_INFO, "SNP:PHY: "));
  switch (Speed) {
    case SPEED_1000:
      DEBUG ((DEBUG_INFO, "1 Gbps - "));
      break;
    case SPEED_100:
      DEBUG ((DEBUG_INFO, "100 Mbps - "));
      break;
    case SPEED_10:
      DEBUG ((DEBUG_INFO, "10 Mbps - "));
      break;
    default:
      DEBUG ((DEBUG_INFO, "Invalid link speed"));
      break;
    }

  switch (Duplex) {
    case DUPLEX_FULL:
      DEBUG ((DEBUG_INFO, "Full Duplex\n"));
      break;
    case DUPLEX_HALF:
      DEBUG ((DEBUG_INFO, "Half Duplex\n"));
      break;
    default:
      DEBUG ((DEBUG_INFO, "Invalid duplex mode\n"));
      break;
    }
}

/**
	Function to read from MII register (PHY Access).

	@param Addr				Phy device physical address
	@param Reg 				Phy register
	@param Data				Read data
	@param MacBaseAddress 	GMAC register base address

	@retval EFI_SUCCESS	    Read success
**/
EFI_STATUS
EFIAPI
PhyRead (
  IN  UINT32   Addr,
  IN  UINT32   Reg,
  OUT UINT32   *Data,
  IN  UINTN    MacBaseAddress
  )
{
  UINT32        MiiConfig;
  UINT32        Count;

  // Check it is a valid Reg
  /* ynfan 20210915 */
  // ASSERT (Reg < 31);

  MiiConfig = ((Addr << MIIADDRSHIFT) & MII_ADDRMSK) |
              ((Reg << MIIREGSHIFT) & MII_REGMSK)|
               MII_CLKRANGE_150_250M |
               MII_BUSY;

  // write this config to register
  MmioWrite32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_ADDRESS_OFST, MiiConfig);

  // Wait for busy bit to clear
  Count = 0;
  while (Count < 10000) {
    if (!(DW_EMAC_GMACGRP_GMII_ADDRESS_GB_GET (MmioRead32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_ADDRESS_OFST)))) {
      *Data = DW_EMAC_GMACGRP_GMII_DATA_GD_GET (MmioRead32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_DATA_OFST));
      return EFI_SUCCESS;
    }
    MemoryFence ();
    Count++;
  };
  DEBUG ((DEBUG_INFO, "SNP:PHY: MDIO busy bit timeout\r\n"));
  return EFI_TIMEOUT;
}

/**
	Function to write to the MII register (PHY Access).

	@param Addr			  Phy device physical address
	@param Reg			  Phy register
	@param Data			  Data to write
	@param MacBaseAddress GMAC register base address

	@retval EFI_SUCCESS	  Write success
**/

// Function to write to the MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyWrite (
  IN UINT32   Addr,
  IN UINT32   Reg,
  IN UINT32   Data,
  IN UINTN    MacBaseAddress
  )
{
  UINT32   MiiConfig;
  UINT32   Count;

  // Check it is a valid Reg
  // ASSERT(Reg < 31);

  MiiConfig = ((Addr << MIIADDRSHIFT) & MII_ADDRMSK) |
              ((Reg << MIIREGSHIFT) & MII_REGMSK)|
               MII_WRITE |
               MII_CLKRANGE_150_250M |
               MII_BUSY;
  // Write the desired value to the register first
  MmioWrite32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_DATA_OFST, (Data & 0xFFFF));

  // write this config to register
  MmioWrite32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_ADDRESS_OFST, MiiConfig);

  // Wait for busy bit to clear
  Count = 0;
  while (Count < 1000) {
    if (!(DW_EMAC_GMACGRP_GMII_ADDRESS_GB_GET (MmioRead32 (MacBaseAddress + DW_EMAC_GMACGRP_GMII_ADDRESS_OFST)))) {
      return EFI_SUCCESS;
    }
    MemoryFence ();
    Count++;
  };

  return EFI_TIMEOUT;
}

/**
	Function to write to KSZ9031 MMD register (PHY Access).

	@param PhyDriver		A point to Phy dirver structure
	@param Mode				MMD operation mode
	@param DevAddr			Phy device physical address
	@param Regnum			Phy register
	@param val				Data to write
	@param MacBaseAddress 	GMAC register base address

	@retval EFI_SUCCESS		Write success
**/
EFI_STATUS
EFIAPI
Phy9031ExtendedWrite (
  IN PHY_DRIVER   *PhyDriver,
  IN UINT32       Mode,
  IN UINT32       DevAddr,
  IN UINT32       Regnum,
  IN UINT16       Val,
  IN UINTN        MacBaseAddress
  )
{
  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_CTRL_REG, DevAddr, MacBaseAddress);
  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_REGDATA_REG, Regnum, MacBaseAddress);
  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_CTRL_REG, (Mode << 14) | DevAddr, MacBaseAddress);
  return PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_REGDATA_REG, Val, MacBaseAddress);
}

/**
	Function to write to KSZ9031 MMD register (PHY Access).

	@param PhyDriver		    A point to Phy dirver structure
	@param Mode			  		MMD operation mode
	@param DevAddr		  		Phy device physical address
	@param Regnum			  	Phy register
	@param MacBaseAddress   	GMAC register base address

	@retval Data32			 	Read Data
	@retval 0					Read failed
**/
UINT32
EFIAPI
Phy9031ExtendedRead (
  IN PHY_DRIVER   *PhyDriver,
  IN UINT32       Mode,
  IN UINT32       DevAddr,
  IN UINT32       Regnum,
  IN UINTN        MacBaseAddress
  )
{
  EFI_STATUS    Status;
  UINT32        Data32;

  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_CTRL_REG, DevAddr, MacBaseAddress);
  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_REGDATA_REG, Regnum, MacBaseAddress);
  PhyWrite (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_CTRL_REG, (Mode << 14) | DevAddr, MacBaseAddress);

  Status = PhyRead (PhyDriver->PhyAddr, PHY_KSZ9031RN_MMD_REGDATA_REG, &Data32, MacBaseAddress);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  return Data32;
}

/**
	Function to update the media state.
	Read the link state,compare the PhyOldLink state and PhyCurrentLink state,if the state is changed,
	update the media link state.And if the state is changed to link-up,read the phy capability and
	config adjust.

	@param PhyDriver		  A point to Phy dirver structure
	@param MacBaseAddress 	  GMAC register base address

	@retval EFI_SUCCESS		  link-up
	@retval EFI_TIMEOUT		  link-down
**/
EFI_STATUS
EFIAPI
UpdateMediaState(
  IN PHY_DRIVER   *PhyDriver,
  IN UINTN        MacBaseAddress

)
{
	UINT32	   Speed;
	UINT32	   Duplex;
	EFI_STATUS   Status = EFI_SUCCESS;
	UINT32 PhyBasicStatus;
	UINT32 linkStatus;
	UINTN         TimeOut;
	UINT32	Data32;
	UINT32	ANState;

	Status = EFI_SUCCESS;
	Speed = SPEED_10;
	Duplex = DUPLEX_HALF;

	Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &PhyBasicStatus, MacBaseAddress);
	if (EFI_ERROR (Status)) {
	  return Status;
	}
	if (PhyBasicStatus & PHYSTS_LINK_STS) {
		linkStatus = LINK_UP;
    }
	else{
		linkStatus = LINK_DOWN;
	}
	if(linkStatus == PhyDriver->PhyOldLink){
		PhyDriver->PhyCurrentLink = linkStatus;
		PhyDriver->PhyOldLink = PhyDriver->PhyCurrentLink;
		return EFI_SUCCESS;
	}
	else{
		PhyDriver->PhyCurrentLink = linkStatus;
	   PhyDriver->PhyOldLink = PhyDriver->PhyCurrentLink;
	}
	if(linkStatus == LINK_UP){
		// Wait until autonego process has completed
		TimeOut = 0;
		ANState = 0;
		do {
			// Read PHY_BASIC_STATUS register from PHY
			Status = PhyRead (PhyDriver->PhyAddr, PHY_BASIC_STATUS, &Data32, MacBaseAddress);
			if (EFI_ERROR(Status)) {
			  return Status;
			}
			// Wait until PHYSTS_AUTO_COMP become one
			if (Data32 & PHYSTS_AUTO_COMP) {
			  DEBUG ((DEBUG_INFO, "SNP:PHY: Auto Negotiation completed\r\n"));
			  ANState = 1;
			  break;
			}
			MicroSecondDelay (1);
		} while (TimeOut++ < 10000);
		if (ANState == 0) {
			DEBUG ((DEBUG_INFO, "SNP:PHY: Error! Auto Negotiation timeout\n"));
			Status =  EFI_TIMEOUT;
		}
		else{
			PhyDriver->PhyCurrentLink = linkStatus;
			DEBUG((EFI_D_INFO,"Speed and Duplex config!\n"));
			PhyReadCapability (PhyDriver, &Speed, &Duplex, MacBaseAddress);
    		EmacConfigAdjust (Speed, Duplex, MacBaseAddress);
			Status = EFI_SUCCESS;
		}

	}
	return Status;
}

