#include "etherboot.h"
#include "timer.h"
#include "pci.h"
#include "isa.h"
#include "disk.h"

#define BSY_SET_DURING_SPINUP 1
/*
 *   UBL, The Universal Talkware Boot Loader 
 *    Copyright (C) 2000 Universal Talkware Inc.
 *    Copyright (C) 2002 Eric Biederman
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version. 
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *
 */
struct controller {
	uint16_t cmd_base;
	uint16_t ctrl_base;
};

struct harddisk_info {
	struct controller *ctrl;
	uint16_t heads;
	uint16_t cylinders;
	uint16_t sectors_per_track;
	uint8_t  model_number[41];
	uint8_t  slave;
	sector_t sectors;
	int  address_mode;	/* am i lba (0x40) or chs (0x00) */
#define ADDRESS_MODE_CHS   0
#define ADDRESS_MODE_LBA   1
#define ADDRESS_MODE_LBA48 2
	int drive_exists;
	int slave_absent;
	int basedrive;
};


#define IDE_SECTOR_SIZE 0x200

#define IDE_BASE0             (0x1F0u) /* primary controller */
#define IDE_BASE1             (0x170u) /* secondary */
#define IDE_BASE2             (0x0F0u) /* third */
#define IDE_BASE3             (0x070u) /* fourth */

#define IDE_REG_EXTENDED_OFFSET   (0x204u)

#define IDE_REG_DATA(base)           ((ctrl)->cmd_base + 0u) /* word register */
#define IDE_REG_ERROR(base)          ((ctrl)->cmd_base + 1u)
#define IDE_REG_PRECOMP(base)        ((ctrl)->cmd_base + 1u)
#define IDE_REG_FEATURE(base)        ((ctrl)->cmd_base + 1u)
#define IDE_REG_SECTOR_COUNT(base)   ((ctrl)->cmd_base + 2u)
#define IDE_REG_SECTOR_NUMBER(base)  ((ctrl)->cmd_base + 3u)
#define IDE_REG_LBA_LOW(base)        ((ctrl)->cmd_base + 3u)
#define IDE_REG_CYLINDER_LSB(base)   ((ctrl)->cmd_base + 4u)
#define IDE_REG_LBA_MID(base)	     ((ctrl)->cmd_base + 4u)
#define IDE_REG_CYLINDER_MSB(base)   ((ctrl)->cmd_base + 5u)
#define IDE_REG_LBA_HIGH(base)	     ((ctrl)->cmd_base + 5u)
#define IDE_REG_DRIVEHEAD(base)      ((ctrl)->cmd_base + 6u)
#define IDE_REG_DEVICE(base)	     ((ctrl)->cmd_base + 6u)
#define IDE_REG_STATUS(base)         ((ctrl)->cmd_base + 7u)
#define IDE_REG_COMMAND(base)        ((ctrl)->cmd_base + 7u)
#define IDE_REG_ALTSTATUS(base)      ((ctrl)->ctrl_base + 2u)
#define IDE_REG_DEVICE_CONTROL(base) ((ctrl)->ctrl_base + 2u)

struct ide_pio_command
{
	uint8_t feature;
	uint8_t sector_count;
	uint8_t lba_low;
	uint8_t lba_mid;
	uint8_t lba_high;
	uint8_t device;
#       define IDE_DH_DEFAULT (0xA0)
#       define IDE_DH_HEAD(x) ((x) & 0x0F)
#       define IDE_DH_MASTER  (0x00)
#       define IDE_DH_SLAVE   (0x10)
#       define IDE_DH_LBA     (0x40)
#       define IDE_DH_CHS     (0x00)
	uint8_t command;
	uint8_t sector_count2;
	uint8_t lba_low2;
	uint8_t lba_mid2;
	uint8_t lba_high2;
};

#define IDE_DEFAULT_COMMAND { 0xFFu, 0x01, 0x00, 0x0000, IDE_DH_DEFAULT }

#define IDE_ERR_ICRC	0x80	/* ATA Ultra DMA bad CRC */
#define IDE_ERR_BBK	0x80	/* ATA bad block */
#define IDE_ERR_UNC	0x40	/* ATA uncorrected error */
#define IDE_ERR_MC	0x20	/* ATA media change */
#define IDE_ERR_IDNF	0x10	/* ATA id not found */
#define IDE_ERR_MCR	0x08	/* ATA media change request */
#define IDE_ERR_ABRT	0x04	/* ATA command aborted */
#define IDE_ERR_NTK0	0x02	/* ATA track 0 not found */
#define IDE_ERR_NDAM	0x01	/* ATA address mark not found */

#define IDE_STATUS_BSY	0x80	/* busy */
#define IDE_STATUS_RDY	0x40	/* ready */
#define IDE_STATUS_DF	0x20	/* device fault */
#define IDE_STATUS_WFT	0x20	/* write fault (old name) */
#define IDE_STATUS_SKC	0x10	/* seek complete */
#define IDE_STATUS_DRQ	0x08	/* data request */
#define IDE_STATUS_CORR	0x04	/* corrected */
#define IDE_STATUS_IDX	0x02	/* index */
#define IDE_STATUS_ERR	0x01	/* error (ATA) */
#define IDE_STATUS_CHK	0x01	/* check (ATAPI) */

#define IDE_CTRL_HD15	0x08	/* bit should always be set to one */
#define IDE_CTRL_SRST	0x04	/* soft reset */
#define IDE_CTRL_NIEN	0x02	/* disable interrupts */


/* Most mandtory and optional ATA commands (from ATA-3), */

#define IDE_CMD_CFA_ERASE_SECTORS            0xC0
#define IDE_CMD_CFA_REQUEST_EXT_ERR_CODE     0x03
#define IDE_CMD_CFA_TRANSLATE_SECTOR         0x87
#define IDE_CMD_CFA_WRITE_MULTIPLE_WO_ERASE  0xCD
#define IDE_CMD_CFA_WRITE_SECTORS_WO_ERASE   0x38
#define IDE_CMD_CHECK_POWER_MODE1            0xE5
#define IDE_CMD_CHECK_POWER_MODE2            0x98
#define IDE_CMD_DEVICE_RESET                 0x08
#define IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC    0x90
#define IDE_CMD_FLUSH_CACHE                  0xE7
#define IDE_CMD_FORMAT_TRACK                 0x50
#define IDE_CMD_IDENTIFY_DEVICE              0xEC
#define IDE_CMD_IDENTIFY_DEVICE_PACKET       0xA1
#define IDE_CMD_IDENTIFY_PACKET_DEVICE       0xA1
#define IDE_CMD_IDLE1                        0xE3
#define IDE_CMD_IDLE2                        0x97
#define IDE_CMD_IDLE_IMMEDIATE1              0xE1
#define IDE_CMD_IDLE_IMMEDIATE2              0x95
#define IDE_CMD_INITIALIZE_DRIVE_PARAMETERS  0x91
#define IDE_CMD_INITIALIZE_DEVICE_PARAMETERS 0x91
#define IDE_CMD_NOP                          0x00
#define IDE_CMD_PACKET                       0xA0
#define IDE_CMD_READ_BUFFER                  0xE4
#define IDE_CMD_READ_DMA                     0xC8
#define IDE_CMD_READ_DMA_QUEUED              0xC7
#define IDE_CMD_READ_MULTIPLE                0xC4
#define IDE_CMD_READ_SECTORS                 0x20
#define IDE_CMD_READ_SECTORS_EXT             0x24
#define IDE_CMD_READ_VERIFY_SECTORS          0x40
#define IDE_CMD_RECALIBRATE                  0x10
#define IDE_CMD_SEEK                         0x70
#define IDE_CMD_SET_FEATURES                 0xEF
#define IDE_CMD_SET_MAX_ADDR_EXT             0x24
#define IDE_CMD_SET_MULTIPLE_MODE            0xC6
#define IDE_CMD_SLEEP1                       0xE6
#define IDE_CMD_SLEEP2                       0x99
#define IDE_CMD_STANDBY1                     0xE2
#define IDE_CMD_STANDBY2                     0x96
#define IDE_CMD_STANDBY_IMMEDIATE1           0xE0
#define IDE_CMD_STANDBY_IMMEDIATE2           0x94
#define IDE_CMD_WRITE_BUFFER                 0xE8
#define IDE_CMD_WRITE_DMA                    0xCA
#define IDE_CMD_WRITE_DMA_QUEUED             0xCC
#define IDE_CMD_WRITE_MULTIPLE               0xC5
#define IDE_CMD_WRITE_SECTORS                0x30
#define IDE_CMD_WRITE_VERIFY                 0x3C

/* IDE_CMD_SET_FEATURE sub commands */
#define IDE_FEATURE_CFA_ENABLE_8BIT_PIO                     0x01
#define IDE_FEATURE_ENABLE_WRITE_CACHE                      0x02
#define IDE_FEATURE_SET_TRANSFER_MODE                       0x03
#define IDE_FEATURE_ENABLE_POWER_MANAGEMENT                 0x05
#define IDE_FEATURE_ENABLE_POWERUP_IN_STANDBY               0x06
#define IDE_FEATURE_STANDBY_SPINUP_DRIVE                    0x07
#define IDE_FEATURE_CFA_ENABLE_POWER_MODE1                  0x0A
#define IDE_FEATURE_DISABLE_MEDIA_STATUS_NOTIFICATION       0x31
#define IDE_FEATURE_ENABLE_AUTOMATIC_ACOUSTIC_MANAGEMENT    0x42
#define IDE_FEATURE_SET_MAXIMUM_HOST_INTERFACE_SECTOR_TIMES 0x43
#define IDE_FEATURE_DISABLE_READ_LOOKAHEAD                  0x55
#define IDE_FEATURE_ENABLE_RELEASE_INTERRUPT                0x5D
#define IDE_FEATURE_ENABLE_SERVICE_INTERRUPT                0x5E
#define IDE_FEATURE_DISABLE_REVERTING_TO_POWERON_DEFAULTS   0x66
#define IDE_FEATURE_CFA_DISABLE_8BIT_PIO                    0x81
#define IDE_FEATURE_DISABLE_WRITE_CACHE                     0x82
#define IDE_FEATURE_DISABLE_POWER_MANAGEMENT                0x85
#define IDE_FEATURE_DISABLE_POWERUP_IN_STANDBY              0x86
#define IDE_FEATURE_CFA_DISABLE_POWER_MODE1                 0x8A
#define IDE_FEATURE_ENABLE_MEDIA_STATUS_NOTIFICATION        0x95
#define IDE_FEATURE_ENABLE_READ_LOOKAHEAD                   0xAA
#define IDE_FEATURE_DISABLE_AUTOMATIC_ACOUSTIC_MANAGEMENT   0xC2
#define IDE_FEATURE_ENABLE_REVERTING_TO_POWERON_DEFAULTS    0xCC
#define IDE_FEATURE_DISABLE_SERVICE_INTERRUPT               0xDE



struct controller    controller;
struct harddisk_info harddisk_info[2];

static int await_ide(int (*done)(struct controller *ctrl), 
	struct controller *ctrl, unsigned long timeout)
{
	int result;
	for(;;) {
		result = done(ctrl);
		if (result) {
			return 0;
		}
		poll_interruptions();
		if ((timeout == 0) || (currticks() > timeout)) {
			break;
		}
	}
	return -1;
}

/* The maximum time any IDE command can last 31 seconds,
 * So if any IDE commands takes this long we know we have problems.
 */
#define IDE_TIMEOUT (32*TICKS_PER_SEC)

static int not_bsy(struct controller *ctrl)
{
	return !(inb(IDE_REG_STATUS(ctrl)) & IDE_STATUS_BSY);
}
#if  !BSY_SET_DURING_SPINUP
static int timeout(struct controller *ctrl)
{
	return 0;
}
#endif

static int ide_software_reset(struct controller *ctrl)
{
	/* Wait a little bit in case this is immediately after
	 * hardware reset.
	 */
	mdelay(2);
	/* A software reset should not be delivered while the bsy bit
	 * is set.  If the bsy bit does not clear in a reasonable
	 * amount of time give up.
	 */
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	/* Disable Interrupts and reset the ide bus */
	outb(IDE_CTRL_HD15 | IDE_CTRL_SRST | IDE_CTRL_NIEN, 
		IDE_REG_DEVICE_CONTROL(ctrl));
	udelay(5);
	outb(IDE_CTRL_HD15 | IDE_CTRL_NIEN, IDE_REG_DEVICE_CONTROL(ctrl));
	mdelay(2);
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	return 0;
}

static void pio_set_registers(
	struct controller *ctrl, const struct ide_pio_command *cmd)
{
	uint8_t device;
	/* Disable Interrupts */
	outb(IDE_CTRL_HD15 | IDE_CTRL_NIEN, IDE_REG_DEVICE_CONTROL(ctrl));

	/* Possibly switch selected device */
	device = inb(IDE_REG_DEVICE(ctrl));
	outb(cmd->device,          IDE_REG_DEVICE(ctrl));
	if ((device & (1UL << 4)) != (cmd->device & (1UL << 4))) {
		/* Allow time for the selected drive to switch,
		 * The linux ide code suggests 50ms is the right
		 * amount of time to use here.
		 */
		mdelay(50); 
	}
	outb(cmd->feature,         IDE_REG_FEATURE(ctrl));
	outb(cmd->sector_count2,   IDE_REG_SECTOR_COUNT(ctrl));
	outb(cmd->sector_count,    IDE_REG_SECTOR_COUNT(ctrl));
	outb(cmd->lba_low2,        IDE_REG_LBA_LOW(ctrl));
	outb(cmd->lba_low,         IDE_REG_LBA_LOW(ctrl));
	outb(cmd->lba_mid2,        IDE_REG_LBA_MID(ctrl));
	outb(cmd->lba_mid,         IDE_REG_LBA_MID(ctrl));
	outb(cmd->lba_high2,       IDE_REG_LBA_HIGH(ctrl));
	outb(cmd->lba_high,        IDE_REG_LBA_HIGH(ctrl));
	outb(cmd->command,         IDE_REG_COMMAND(ctrl));
}


static int pio_non_data(struct controller *ctrl, const struct ide_pio_command *cmd)
{
	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	pio_set_registers(ctrl, cmd);
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	/* FIXME is there more error checking I could do here? */
	return 0;
}

static int pio_data_in(struct controller *ctrl, const struct ide_pio_command *cmd,
	void *buffer, size_t bytes)
{
	unsigned int status;

	/* FIXME handle commands with multiple blocks */
	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}

	/* How do I tell if INTRQ is asserted? */
	pio_set_registers(ctrl, cmd);
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	status = inb(IDE_REG_STATUS(ctrl));
	if (!(status & IDE_STATUS_DRQ)) {
		return -1;
	}
	insw(IDE_REG_DATA(ctrl), buffer, bytes/2);
	status = inb(IDE_REG_STATUS(ctrl));
	if (status & IDE_STATUS_DRQ) {
		return -1;
	}
	return 0;
}

#if 0
static int pio_packet(struct controller *ctrl, int in,
	const void *packet, int packet_len,
	void *buffer, int buffer_len)
{
	const uint8_t *pbuf;
	unsigned int status;
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	/* Wait until the busy bit is clear */
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	pio_set_registers(ctrl, cmd);
	ndelay(400);
	if (await_ide(not_bsy, ctrl, currticks() + IDE_TIMEOUT) < 0) {
		return -1;
	}
	status = inb(IDE_REG_STATUS(ctrl));
	if (!(status & IDE_STATUS_DRQ)) {
		return -1;
	}
	while(packet_len > 1) {
		outb(*pbuf, IDE_REG_DATA(ctrl));
		pbuf++;
		packet_len -= 1;
	}
	inb(IDE_REG_ALTSTATUS(ctrl));
	if (await_ide){}
	/*FIXME finish this function */
	
	
}
#endif

static inline int ide_read_sector_chs(
	struct harddisk_info *info, void *buffer, unsigned long sector)
{
	struct ide_pio_command cmd;
	unsigned int track;
	unsigned int offset;
	unsigned int cylinder;
		
	memset(&cmd, 0, sizeof(cmd));
	cmd.sector_count = 1;

	track = sector / info->sectors_per_track;
	/* Sector number */
	offset = 1 + (sector % info->sectors_per_track);
	cylinder = track / info->heads;
	cmd.lba_low = offset;
	cmd.lba_mid = cylinder & 0xff;
	cmd.lba_high = (cylinder >> 8) & 0xff;
	cmd.device = IDE_DH_DEFAULT |
		IDE_DH_HEAD(track % info->heads) |
		info->slave |
		IDE_DH_CHS;
	cmd.command = IDE_CMD_READ_SECTORS;
	return pio_data_in(info->ctrl, &cmd, buffer, IDE_SECTOR_SIZE);
}

static inline int ide_read_sector_lba(
	struct harddisk_info *info, void *buffer, unsigned long sector)
{
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.sector_count = 1;
	cmd.lba_low = sector & 0xff;
	cmd.lba_mid = (sector >> 8) & 0xff;
	cmd.lba_high = (sector >> 16) & 0xff;
	cmd.device = IDE_DH_DEFAULT |
		((sector >> 24) & 0x0f) |
		info->slave | 
		IDE_DH_LBA;
	cmd.command = IDE_CMD_READ_SECTORS;
	return pio_data_in(info->ctrl, &cmd, buffer, IDE_SECTOR_SIZE);
}

static inline int ide_read_sector_lba48(
	struct harddisk_info *info, void *buffer, sector_t sector)
{
	struct ide_pio_command cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.sector_count = 1;
	cmd.lba_low = sector & 0xff;
	cmd.lba_mid = (sector >> 8) & 0xff;
	cmd.lba_high = (sector >> 16) & 0xff;
	cmd.lba_low2 = (sector >> 24) & 0xff;
	cmd.lba_mid2 = (sector >> 32) & 0xff;
	cmd.lba_high2 = (sector >> 40) & 0xff;
	cmd.device =  info->slave | IDE_DH_LBA;
	cmd.command = IDE_CMD_READ_SECTORS_EXT;
	return pio_data_in(info->ctrl, &cmd, buffer, IDE_SECTOR_SIZE);
}


static int ide_read(struct disk *disk, sector_t sector)
{
	struct harddisk_info *info = disk->priv;
	int result;

	/* Report the buffer is empty */
	disk->sector = 0;
	disk->bytes = 0;
	if (sector > info->sectors) {
		return -1;
	}
	if (info->address_mode == ADDRESS_MODE_CHS) {
		result = ide_read_sector_chs(info, disk->buffer, sector);
	}
	else if (info->address_mode == ADDRESS_MODE_LBA) {
		result = ide_read_sector_lba(info, disk->buffer, sector);
	}
	else if (info->address_mode == ADDRESS_MODE_LBA48) {
		result = ide_read_sector_lba48(info, disk->buffer, sector);
	}
	else {
		result = -1;
	}
	/* On success report the buffer has data */
	if (result != -1) {
		disk->bytes = IDE_SECTOR_SIZE;
		disk->sector = sector;
	}
	return result;
}

static int init_drive(struct harddisk_info *info, struct controller *ctrl, int slave,
	int basedrive, unsigned char *buffer)
{
	uint16_t* drive_info;
	struct ide_pio_command cmd;
	int i;

	info->ctrl = ctrl;
	info->heads = 0u;
	info->cylinders = 0u;
	info->sectors_per_track = 0u;
	info->address_mode = IDE_DH_CHS;
	info->sectors = 0ul;
	info->drive_exists = 0;
	info->slave_absent = 0;
	info->slave = slave?IDE_DH_SLAVE: IDE_DH_MASTER;
	info->basedrive = basedrive;

#if 0
	printf("Testing for disk %d\n", info->basedrive);
#endif

	/* Select the drive that we are testing */
	outb(IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave, 
		IDE_REG_DEVICE(ctrl));
	mdelay(50);

	/* Test to see if the drive registers exist,
	 * In many cases this quickly rules out a missing drive.
	 */
	for(i = 0; i < 4; i++) {
		outb(0xaa + i, (ctrl->cmd_base) + 2 + i);
	}
	for(i = 0; i < 4; i++) {
		if (inb((ctrl->cmd_base) + 2 + i) != 0xaa + i) {
			return 1;
		}
	}
	for(i = 0; i < 4; i++) {
		outb(0x55 + i, (ctrl->cmd_base) + 2 + i);
	}
	for(i = 0; i < 4; i++) {
		if (inb((ctrl->cmd_base) + 2 + i) != 0x55 + i) {
			return 1;
		}
	}
#if 0
	printf("Probing for disk %d\n", info->basedrive);
#endif
	
	memset(&cmd, 0, sizeof(cmd));
	cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave;
	cmd.command = IDE_CMD_IDENTIFY_DEVICE;

	
	if (pio_data_in(ctrl, &cmd, buffer, IDE_SECTOR_SIZE) < 0) {
		/* Well, if that command didn't work, we probably don't have drive. */
		return 1;
	}
	

	/* Now suck the data out */
	drive_info = (uint16_t *)buffer;
	if (drive_info[2] == 0x37C8) {
		/* If the response is incomplete spin up the drive... */
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS |
			info->slave;
		cmd.feature = IDE_FEATURE_STANDBY_SPINUP_DRIVE;
		if (pio_non_data(ctrl, &cmd) < 0) {
			/* If the command doesn't work give up on the drive */
			return 1;
		}
		
	}
	if ((drive_info[2] == 0x37C8) || (drive_info[2] == 0x8C73)) {
		/* The response is incomplete retry the drive info command */
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS |
			info->slave;
		cmd.command = IDE_CMD_IDENTIFY_DEVICE;
		if(pio_data_in(ctrl, &cmd, buffer, IDE_SECTOR_SIZE) < 0) {
			/* If the command didn't work give up on the drive. */
			return 1;
		}
	}
	if ((drive_info[2] != 0x37C8) &&
		(drive_info[2] != 0x738C) &&
		(drive_info[2] != 0x8C73) &&
		(drive_info[2] != 0xC837) &&
		(drive_info[2] != 0x0000)) {
		printf("Invalid IDE Configuration: %hx\n", drive_info[2]);
		return 1;
	}
	for(i = 27; i < 47; i++) {
		info->model_number[((i-27)<< 1)] = (drive_info[i] >> 8) & 0xff;
		info->model_number[((i-27)<< 1)+1] = drive_info[i] & 0xff;
	}
	info->model_number[40] = '\0';
	info->drive_exists = 1;

	/* See if LBA is supported */
	if (drive_info[49] & (1 << 9)) {
		info->address_mode = ADDRESS_MODE_LBA;
		info->sectors = (drive_info[61] << 16) | (drive_info[60]);
		/* Enable LBA48 mode if it is present */
		if (drive_info[83] & (1 <<10)) {
			/* Should LBA48 depend on LBA? */
			printf("LBA48 mode\n");
			info->address_mode = ADDRESS_MODE_LBA48;
			info->sectors = 
				(((sector_t)drive_info[103]) << 48) |
				(((sector_t)drive_info[102]) << 32) |
				(((sector_t)drive_info[101]) << 16) |
				(((sector_t)drive_info[100]) <<  0);
		}
	} else {
		info->address_mode = ADDRESS_MODE_CHS;
		info->heads = drive_info[3];
		info->cylinders = drive_info[1];
		info->sectors_per_track = drive_info[6];
		info->sectors = 
			info->sectors_per_track *
			info->heads *
			info->cylinders;
		printf( "%s sectors_per_track=[%d], heads=[%d], cylinders=[%d]\n",
			__FUNCTION__,
			info->sectors_per_track,
			info->heads,
			info->cylinders);
	}
	/* See if we have a slave */
	if (!info->slave && (((drive_info[93] >> 14) & 3) == 1)) {
		info->slave_absent = !(drive_info[93] & (1 << 5));
	}
	/* See if we need to put the device in CFA power mode 1 */
	if ((drive_info[160] & ((1 << 15) | (1 << 13)| (1 << 12))) ==
		((1 << 15) | (1 << 13)| (1 << 12))) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.device = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | info->slave;
		cmd.feature = IDE_FEATURE_CFA_ENABLE_POWER_MODE1;
		if (pio_non_data(ctrl, &cmd) < 0) {
			/* If I need to power up the drive, and I can't
			 * give up.
			 */
			printf("Cannot power up CFA device\n");
			return 1;
		}
	}
	printf("disk%d %dk cap: %hx\n",
		info->basedrive,
		(unsigned long)(info->sectors >> 1),
		drive_info[49]);
	return 0;
}

static int init_controller(struct controller *ctrl, int basedrive, unsigned char *buffer) 
{
	struct harddisk_info *info;

	/* Intialize the harddisk_info structures */
	memset(harddisk_info, 0, sizeof(harddisk_info));

	/* Put the drives ide channel in a know state and wait
	 * for the drives to spinup.  
	 *
	 * In practice IDE disks tend not to respond to commands until
	 * they have spun up.  This makes IDE hard to deal with
	 * immediately after power up, as the delays can be quite
	 * long, so we must be very careful here.
	 *
	 * There are two pathological cases that must be dealt with:
	 *
	 * - The BSY bit not being set while the IDE drives spin up.
	 *   In this cases only a hard coded delay will work.  As
	 *   I have not reproduced it, and this is out of spec for
	 *   IDE drives the work around can be enabled by setting
	 *   BSY_SET_DURING_SPINUP to 0.
	 *
	 * - The BSY bit floats high when no drives are plugged in.
	 *   This case will not be detected except by timing out but
	 *   we avoid the problems by only probing devices we are
	 *   supposed to boot from.  If we don't do the probe we
	 *   will not experience the problem.
	 *
	 * So speed wise I am only slow if the BSY bit is not set
	 * or not reported by the IDE controller during spinup, which
	 * is quite rare.
	 * 
	 */
#if !BSY_SET_DURING_SPINUP
	if (await_ide(timeout, ctrl, IDE_TIMEOUT) < 0) {
		return -1;
	}
#endif
	if (ide_software_reset(ctrl) < 0) {
		return -1;
	}

	/* Note: I have just done a software reset.  It may be
	 * reasonable to just read the boot time signatures 
	 * off of the drives to see if they are present.
	 *
	 * For now I will go with just sending commands to the drives
	 * and assuming filtering out missing drives by detecting registers
	 * that won't set and commands that fail to execute properly.
	 */

	/* Now initialize the individual drives */
	info = &harddisk_info[0];
	init_drive(info, ctrl, 0, basedrive, buffer);
	if (info->drive_exists && !info->slave_absent) {
		basedrive++;
		info++;
		init_drive(info, ctrl, 1, basedrive, buffer);
	}

	return 0;
}

static void ide_disable(struct dev *dev)
{
	struct disk *disk = (struct disk *)dev;
	struct harddisk_info *info = disk->priv;
	ide_software_reset(info->ctrl);
}

#ifdef CONFIG_PCI
static int ide_pci_probe(struct dev *dev, struct pci_device *pci)
{
	struct disk *disk = (struct disk *)dev;
	struct harddisk_info *info;
	int index;

	adjust_pci_device(pci);
	
	index = dev->index + 1;
	if (dev->how_probe == PROBE_NEXT) {
		index++;
	}
	for(; index < 4; index++) {
		unsigned mask;
		mask = (index < 2)? (1 << 0) : (1 << 2);
		if ((pci->class & mask) == 0) {
			/* IDE special pci mode */
			uint16_t base;
			base = (index < 2)?IDE_BASE0:IDE_BASE1;
			controller.cmd_base  = base;
			controller.ctrl_base = base + IDE_REG_EXTENDED_OFFSET;
		} else {
			/* IDE normal pci mode */
			unsigned cmd_reg, ctrl_reg;
			uint32_t cmd_base, ctrl_base;
			if (index < 2) {
				cmd_reg  = PCI_BASE_ADDRESS_0;
				ctrl_reg = PCI_BASE_ADDRESS_1;
			} else {
				cmd_reg  = PCI_BASE_ADDRESS_2;
				ctrl_reg = PCI_BASE_ADDRESS_3;
			}
			pcibios_read_config_dword(pci->bus, pci->devfn, cmd_reg, &cmd_base);
			pcibios_read_config_dword(pci->bus, pci->devfn, ctrl_reg, &ctrl_base);
			controller.cmd_base  = cmd_base  & ~3;
			controller.ctrl_base = ctrl_base & ~3;
		}
		if (((index & 1) == 0) || (dev->how_probe == PROBE_AWAKE)) {
			if (init_controller(&controller, disk->drive, disk->buffer) < 0) {
				/* nothing behind the controller */
				continue;
			}
		}
		info = &harddisk_info[index & 1];
		if (!info->drive_exists) {
			/* unknown drive */
			continue;
		}
		disk->hw_sector_size   = IDE_SECTOR_SIZE;
		disk->sectors_per_read = 1;
		disk->sectors          = info->sectors;
		dev->index   = index;
		dev->disable = ide_disable;
		disk->read   = ide_read;
		disk->priv   = info;
		
		return 1;
	}
	/* past all of the drives */
	dev->index = 0;
	return 0;
}
#define PCI_DEVICE_ID_INTEL_82801CA_11	0x248b
static struct pci_id ide_controllers[] = {
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801CA_11,    "PIIX4" },
#if 0  /* Currently I don't need any entries in this table so ignore it */
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82371FB_0,     "PIIX" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82371FB_1,     "PIIX" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82371MX,       "MPIIX" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82371SB_1,     "PIIX3" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82371AB,       "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801AB_1,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82443MX_1,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801AA_1,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82372FB_1,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82451NX,       "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801BA_9,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801BA_8,     "PIIX4" },
{ PCI_VENDOR_ID_INTEL,       PCI_DEVICE_ID_INTEL_82801CA_10,    "PIIX4" },
{ PCI_VENDOR_ID_VIA,         PCI_DEVICE_ID_VIA_82C561,          "VIA_IDE" },
{ PCI_VENDOR_ID_VIA,         PCI_DEVICE_ID_VIA_82C576_1,        "VP_IDE" },
{ PCI_VENDOR_ID_VIA,         PCI_DEVICE_ID_VIA_82C586_1,        "VP_IDE" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20246,       "PDC20246" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20262,       "PDC20262" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20265,       "PDC20265" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20267,       "PDC20267" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20268,       "PDC20268" },
{ PCI_VENDOR_ID_PROMISE,     PCI_DEVICE_ID_PROMISE_20268R,      "PDC20268" },
{ PCI_VENDOR_ID_PCTECH,      PCI_DEVICE_ID_PCTECH_RZ1000,       "RZ1000" },
{ PCI_VENDOR_ID_PCTECH,      PCI_DEVICE_ID_PCTECH_RZ1001,       "RZ1001" },
{ PCI_VENDOR_ID_PCTECH,      PCI_DEVICE_ID_PCTECH_SAMURAI_IDE,  "SAMURAI" },
{ PCI_VENDOR_ID_CMD,         PCI_DEVICE_ID_CMD_640,             "CMD640" },
{ PCI_VENDOR_ID_CMD,         PCI_DEVICE_ID_CMD_643,             "CMD646" },
{ PCI_VENDOR_ID_CMD,         PCI_DEVICE_ID_CMD_646,             "CMD648" },
{ PCI_VENDOR_ID_CMD,         PCI_DEVICE_ID_CMD_648,             "CMD643" },
{ PCI_VENDOR_ID_CMD,         PCI_DEVICE_ID_CMD_649,             "CMD649" },
{ PCI_VENDOR_ID_SI,          PCI_DEVICE_ID_SI_5513,             "SIS5513" },
{ PCI_VENDOR_ID_OPTI,        PCI_DEVICE_ID_OPTI_82C621,         "OPTI621" },
{ PCI_VENDOR_ID_OPTI,        PCI_DEVICE_ID_OPTI_82C558,         "OPTI621V" },
{ PCI_VENDOR_ID_OPTI,        PCI_DEVICE_ID_OPTI_82C825,         "OPTI621X" },
{ PCI_VENDOR_ID_TEKRAM,      PCI_DEVICE_ID_TEKRAM_DC290,        "TRM290" },
{ PCI_VENDOR_ID_NS,          PCI_DEVICE_ID_NS_87410,            "NS87410" },
{ PCI_VENDOR_ID_NS,          PCI_DEVICE_ID_NS_87415,            "NS87415" },
{ PCI_VENDOR_ID_HOLTEK2,     PCI_DEVICE_ID_HOLTEK2_6565,        "HT6565" },
{ PCI_VENDOR_ID_ARTOP,       PCI_DEVICE_ID_ARTOP_ATP850UF,      "AEC6210" },
{ PCI_VENDOR_ID_ARTOP,       PCI_DEVICE_ID_ARTOP_ATP860,        "AEC6260" },
{ PCI_VENDOR_ID_ARTOP,       PCI_DEVICE_ID_ARTOP_ATP860R,       "AEC6260R" },
{ PCI_VENDOR_ID_WINBOND,     PCI_DEVICE_ID_WINBOND_82C105,      "W82C105" },
{ PCI_VENDOR_ID_UMC,         PCI_DEVICE_ID_UMC_UM8673F,         "UM8673F" },
{ PCI_VENDOR_ID_UMC,         PCI_DEVICE_ID_UMC_UM8886A,         "UM8886A" },
{ PCI_VENDOR_ID_UMC,         PCI_DEVICE_ID_UMC_UM8886BF,        "UM8886BF" },
{ PCI_VENDOR_ID_TTI,         PCI_DEVICE_ID_TTI_HPT343,          "HPT34X" },
{ PCI_VENDOR_ID_TTI,         PCI_DEVICE_ID_TTI_HPT366,          "HPT366" },
{ PCI_VENDOR_ID_AL,          PCI_DEVICE_ID_AL_M5229,            "ALI15X3" },
{ PCI_VENDOR_ID_CONTAQ,      PCI_DEVICE_ID_CONTAQ_82C693,       "CY82C693" },
{ 0x3388,                    0x8013,                            "HINT_IDE" },
{ PCI_VENDOR_ID_CYRIX,       PCI_DEVICE_ID_CYRIX_5530_IDE,      "CS5530" },
{ PCI_VENDOR_ID_AMD,         PCI_DEVICE_ID_AMD_COBRA_7401,      "AMD7401" },
{ PCI_VENDOR_ID_AMD,         PCI_DEVICE_ID_AMD_VIPER_7409,      "AMD7409" },
{ PCI_VENDOR_ID_AMD,         PCI_DEVICE_ID_AMD_VIPER_7411,      "AMD7411" },
{ PCI_VENDOR_ID_PDC,         PCI_DEVICE_ID_PDC_1841,            "PDCADMA" },
{ PCI_VENDOR_ID_EFAR,        PCI_DEVICE_ID_EFAR_SLC90E66_1,     "SLC90E66" },
{ PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_OSB4IDE, "OSB4" },
{ PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_CSB5IDE, "OSB5" },
{ PCI_VENDOR_ID_ITE,         PCI_DEVICE_ID_ITE_IT8172G,         "ITE8172G" },
#endif
};

static struct pci_driver ide_driver __pci_driver = {
	.type      = DISK_DRIVER,
	.name      = "IDE",
	.probe     = ide_pci_probe,
	.ids       = ide_controllers,
	.id_count  = sizeof(ide_controllers)/sizeof(ide_controllers),
	.class     = PCI_CLASS_STORAGE_IDE,
};
#endif

/* The isa driver works but it causes disks to show up twice.
 * comment it out for now.
 */
#if 0 && defined(CONFIG_ISA)
static int ide_isa_probe(struct dev * dev, unsigned short *probe_addrs)
{
	struct disk *disk = (struct disk *)dev;
	int index;
	unsigned short addr;
	struct harddisk_info *info;

	index = dev->index +1;
	if (dev->how_probe == PROBE_AWAKE) {
		index--;
	}
	for(; (index >= 0) && (addr = probe_addrs[index >> 1]); index += 2) {
		if ((index & 1) == 0) {
			controller.cmd_base = addr;
			controller.ctrl_base = addr + IDE_REG_EXTENDED_OFFSET;
			if (init_controller(&controller, disk->drive, disk->buffer) < 0) {
				/* nothing behind the controller */
				continue;
			}
		}
		info = &harddisk_info[index & 1];
		if (!info->drive_exists) {
			/* unknown drive */
			return 0;
		}
		disk->sectors_per_read = 1;
		disk->sectors = info->sectors;
		dev->index   = index;
		dev->disable = ide_disable;
		disk->read   = ide_read;
		disk->priv   = info;
		
		return 1;
	}
	/* past all of the drives */
	dev->index = -1;
	return 0;
}

static unsigned short ide_base[] = {
	IDE_BASE0,
	IDE_BASE1, 
	IDE_BASE2, 
	IDE_BASE3, 
	0
};
static struct isa_driver ide_isa_driver __isa_driver = {
	.type    = DISK_DRIVER,
	.name    = "IDE/ISA",
	.probe   = ide_isa_probe,
	.ioaddrs = ide_base,
};

#endif
